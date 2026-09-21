#include "AuthHandler/AuthHandler.h"
#include "ServerContext/ServerContext.h"
#include "ServerLog/ServerLog.h"
#include "SocialHandler/SocialHandler.h"
#include "Accounts/Accounts.h"
#include "Friends/Friends.h"
#include "Framing.h"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


namespace MOU::ServerRuntime
{

	ELoginResult ToLoginResult(EAccountResult R)
	{
		switch (R)
		{
		case EAccountResult::Success:       return ELoginResult::Success;
		case EAccountResult::NotFound:      return ELoginResult::AccountNotFound;
		case EAccountResult::WrongPassword: return ELoginResult::WrongPassword;
		case EAccountResult::DuplicateId:   return ELoginResult::DuplicateId;
		case EAccountResult::InvalidFormat: return ELoginResult::InvalidFormat;
		default:                            return ELoginResult::ServerError;
		}
	}


	const char* AccountResultName(EAccountResult R)
	{
		switch (R)
		{
		case EAccountResult::Success:       return "성공";
		case EAccountResult::NotFound:      return "없는 아이디";
		case EAccountResult::WrongPassword: return "비밀번호 불일치";
		case EAccountResult::DuplicateId:   return "이미 있는 아이디";
		case EAccountResult::InvalidFormat: return "형식 위반";
		default:                            return "서버 오류";
		}
	}


	void SendLoginFailure(const SessionPtr& Session, ELoginResult Reason)
	{
		LoginAckBody Ack{};
		Ack.bSuccess      = 0;
		Ack.Result        = static_cast<uint8_t>(Reason);
		Ack.ServerVersion = kProtocolVersion;
		SendPacket(Session->Sock, EOpcode::LoginAck, &Ack, sizeof(Ack));
	}


	bool HandleLoginReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		// Version 은 LoginReqBody 의 첫 필드다.
		// 구조체 크기가 안 맞더라도 이 2바이트만은 읽어서 정확한 사유를 돌려준다.
		if (BodySize < sizeof(uint16_t))
		{
			ServerLog::Print("[거부] LoginReq 가 너무 짧다 (%u바이트)\n", BodySize);
			SendLoginFailure(Session, ELoginResult::InvalidRequest);
			return false;
		}

		uint16_t ClientVersion = 0;
		std::memcpy(&ClientVersion, Body, sizeof(ClientVersion));

		if (ClientVersion != kProtocolVersion)
		{
			ServerLog::Print("[거부] 프로토콜 버전 불일치. 클라이언트=%u, 서버=%u"
			            " (양쪽을 같은 커밋으로 다시 빌드할 것)\n",
			            ClientVersion, kProtocolVersion);
			SendLoginFailure(Session, ELoginResult::VersionMismatch);
			return false;
		}

		if (BodySize < sizeof(LoginReqBody))
		{
			ServerLog::Print("[거부] LoginReq 크기 부족 (%u < %u)\n",
			            BodySize, static_cast<uint32_t>(sizeof(LoginReqBody)));
			SendLoginFailure(Session, ELoginResult::InvalidRequest);
			return false;
		}

		LoginReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const std::string LoginId  = ReadFixedString(Req.LoginId,  kMaxLoginIdLen);
		const std::string Password = ReadFixedString(Req.Password, kMaxPasswordLen);

		// 계정 검증. UserId 는 이제 서버가 세는 번호가 아니라 accounts.id 다.
		// 그래서 같은 계정으로 재접속하면 언제나 같은 번호가 나온다.
		uint64_t    AccountId = 0;
		std::string Nickname;
		const EAccountResult AuthResult =
			Accounts::Authenticate(LoginId, Password, AccountId, Nickname);

		if (AuthResult != EAccountResult::Success)
		{
			ServerLog::Print("[거부] 로그인 실패: id=%s 사유=%s\n",
			            LoginId.c_str(), AccountResultName(AuthResult));
			SendLoginFailure(Session, ToLoginResult(AuthResult));
			// 연결은 유지한다. 사용자가 비번을 고쳐 다시 시도할 수 있어야 한다.
			return true;
		}

		if (!Context().Sessions.TryClaimAccount(Session, AccountId, Nickname, Req.TeamId))
		{
			ServerLog::Print("[거부] 이미 접속 중인 계정: id=%s, UserId=%llu\n",
			            LoginId.c_str(), static_cast<unsigned long long>(AccountId));
			SendLoginFailure(Session, ELoginResult::AlreadyOnline);
			return true;
		}

		// ★ 친구 캐시를 여기서 한 번 채운다(Session.h 의 FriendIds 주석).
		//   이 목록은 "접속 상태가 바뀌었을 때 알려줄 대상" 이라, 상태가 바뀔
		//   때마다 DB 를 때리지 않으려고 캐시한다. 실패해도 로그인은 시킨다 —
		//   친구 목록이 비어 보이는 것과 로그인이 안 되는 것은 심각도가 다르다.
		{
			std::vector<uint64_t> FriendIds;
			if (Friends::GetFriendIds(Session->UserId, FriendIds))
			{
				Session->SetFriendIds(std::move(FriendIds));
			}
			else
			{
				ServerLog::Print("[경고] %s 의 친구 목록을 읽지 못했다. 접속 알림이 안 갈 수 있다.\n",
				            Session->Name.c_str());
			}
		}

		LoginAckBody Ack{};
		Ack.UserId        = Session->UserId;
		Ack.TeamId        = Session->TeamId;
		Ack.bSuccess      = 1;
		Ack.Result        = static_cast<uint8_t>(ELoginResult::Success);
		Ack.ServerVersion = kProtocolVersion;
		CopyFixedString(Ack.Name, kMaxNameLen, Session->Name);

		ServerLog::Print("[로그인] %s -> UserId=%llu, Team=%d\n",
		            Session->Name.c_str(),
		            static_cast<unsigned long long>(Session->UserId), Session->TeamId);

		// ★ LoginAck 를 먼저 보내고 알린다. 순서를 바꾸면 친구 쪽에서
		//   "온라인" 을 받았는데 정작 본인은 아직 로그인 절차 중인 창이 생긴다.
		const bool bAckSent = SendPacket(Session->Sock, EOpcode::LoginAck, &Ack, sizeof(Ack));

		// 내가 접속했음을 친구들에게 알린다 (M4).
		BroadcastPresence(Session, EPresence::Online);

		// 오프라인 동안 온 DM 을 내려준다 (M5).
		// 친구 목록(FriendListReq)보다 먼저 갈 수 있는데 문제되지 않는다 —
		// 클라는 DM 을 UserId 로 묶어 두었다가 목록이 오면 붙이면 된다.
		DeliverPendingDirectMessages(Session);

		return bAckSent;
	}


	bool HandleRegisterReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		auto SendResult = [&](ELoginResult Reason)
		{
			RegisterAckBody Ack{};
			Ack.bSuccess      = (Reason == ELoginResult::Success) ? 1 : 0;
			Ack.Result        = static_cast<uint8_t>(Reason);
			Ack.ServerVersion = kProtocolVersion;
			return SendPacket(Session->Sock, EOpcode::RegisterAck, &Ack, sizeof(Ack));
		};

		if (BodySize < sizeof(uint16_t))
		{
			return SendResult(ELoginResult::InvalidRequest);
		}

		uint16_t ClientVersion = 0;
		std::memcpy(&ClientVersion, Body, sizeof(ClientVersion));
		if (ClientVersion != kProtocolVersion)
		{
			ServerLog::Print("[거부] 가입 요청 버전 불일치. 클라이언트=%u, 서버=%u\n",
			            ClientVersion, kProtocolVersion);
			return SendResult(ELoginResult::VersionMismatch);
		}

		if (BodySize < sizeof(RegisterReqBody))
		{
			return SendResult(ELoginResult::InvalidRequest);
		}

		RegisterReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const std::string LoginId  = ReadFixedString(Req.LoginId,  kMaxLoginIdLen);
		const std::string Password = ReadFixedString(Req.Password, kMaxPasswordLen);
		const std::string Nickname = ReadFixedString(Req.Nickname, kMaxNameLen);

		uint64_t NewUserId = 0;
		const EAccountResult R = Accounts::Create(LoginId, Password, Nickname, NewUserId);

		if (R == EAccountResult::Success)
		{
			ServerLog::Print("[가입] %s (닉네임 %s) -> UserId=%llu\n",
			            LoginId.c_str(), Nickname.c_str(),
			            static_cast<unsigned long long>(NewUserId));
		}
		else
		{
			ServerLog::Print("[거부] 가입 실패: id=%s 사유=%s\n",
			            LoginId.c_str(), AccountResultName(R));
		}

		return SendResult(ToLoginResult(R));
	}

}
