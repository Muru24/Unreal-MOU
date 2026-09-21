#include "RoomHandler/RoomHandler.h"
#include "ServerContext/ServerContext.h"
#include "ServerLog/ServerLog.h"
#include "EndpointService/EndpointService.h"
#include "RelayRouteService/RelayRouteService.h"
#include "SessionMessaging/SessionMessaging.h"
#include "SocialHandler/SocialHandler.h"
#include "Rooms/Rooms.h"
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

	void BroadcastRoomMembers(uint32_t RoomId)
	{
		std::vector<RoomMemberInfo> Members;
		std::vector<uint64_t>       Recipients;
		bool bAllReady = false;

		if (!Rooms::GetMembers(RoomId, Members, bAllReady, Recipients))
		{
			return;   // 이미 사라진 방
		}

		RoomMemberListBody Head{};
		Head.RoomId    = RoomId;
		Head.Count     = static_cast<uint8_t>(Members.size());
		Head.bAllReady = bAllReady ? 1 : 0;

		SendToUsers(Recipients, EOpcode::RoomMemberList,
		            &Head, sizeof(Head),
		            Members.empty() ? nullptr : Members.data(),
		            static_cast<uint32_t>(Members.size() * sizeof(RoomMemberInfo)));
	}


	void NotifyRoomClosed(const std::vector<uint64_t>& Recipients,
	                      uint32_t RoomId, ERoomCloseReason Reason)
	{
		RoomClosedBody Body{};
		Body.RoomId = RoomId;
		Body.Reason = static_cast<uint8_t>(Reason);

		SendToUsers(Recipients, EOpcode::RoomClosed, &Body, sizeof(Body), nullptr, 0);
	}


	void LeaveRoomAndNotify(const SessionPtr& Session)
	{
		uint32_t RoomId = 0;
		bool bRoomClosed = false;
		std::vector<uint64_t> Recipients;

		Rooms::Leave(Session->UserId, RoomId, bRoomClosed, Recipients);
		if (RoomId == 0)
		{
			return;   // 어느 방에도 없었다
		}

		// relay route 는 방 멤버십과 같은 수명이다. 방장이 나가면 모두, 참여자가
		// 나가면 그 사람의 포트 쌍만 즉시 닫아 늦은 UDP가 다음 방에 섞이지 않게 한다.
		if (bRoomClosed)
		{
			ReleaseRelayRoutesForRoom(RoomId);
		}
		else
		{
			ReleaseRelayRouteForGuest(RoomId, Session->UserId);
		}

		if (bRoomClosed)
		{
			ServerLog::Print("[방 삭제] #%u 방장 %s(%llu) 가 나갔다. 남은 %zu명에게 통보. 남은 방 %zu개\n",
			            RoomId, Session->Name.c_str(),
			            static_cast<unsigned long long>(Session->UserId),
			            Recipients.size(), Rooms::Count());
			NotifyRoomClosed(Recipients, RoomId, ERoomCloseReason::HostLeft);

			// ★ 방이 사라졌으니 남아 있던 사람들은 다시 "온라인" 이다 (M4).
			//   게임이 시작된 방이었다면 이들은 "게임중" 으로 보이고 있었고,
			//   여기서 되돌리지 않으면 **친구 목록에 영영 게임중으로 남는다.**
			BroadcastPresenceFor(Recipients, EPresence::Online);
		}
		else
		{
			ServerLog::Print("[방 퇴장] #%u 에서 %s(%llu) 가 나갔다\n",
			            RoomId, Session->Name.c_str(),
			            static_cast<unsigned long long>(Session->UserId));
			BroadcastRoomMembers(RoomId);
		}

		// ★ 나간 사람 본인의 상태는 **여기서 알리지 않는다.** 호출자가 정한다:
		//     · 스스로 나감  -> HandleRoomLeaveReq 가 Online 을 알린다
		//     · 접속 종료    -> ClientThread 가 Offline 을 알린다
		//   여기서 Online 을 보내면 접속 종료 경로에서 Online -> Offline 두 개가
		//   연달아 나가 친구 화면이 깜빡인다.
	}


	bool HandleRoomCreateReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		auto Reply = [&](ERoomResult R, uint32_t RoomId)
		{
			RoomCreateAckBody Ack{};
			Ack.RoomId   = RoomId;
			Ack.bSuccess = (R == ERoomResult::Success) ? 1 : 0;
			Ack.Result   = static_cast<uint8_t>(R);
			return SendPacket(Session->Sock, EOpcode::RoomCreateAck, &Ack, sizeof(Ack));
		};

		if (!Session->bAuthed)
		{
			return Reply(ERoomResult::NotAuthed, 0);
		}
		if (BodySize < sizeof(RoomCreateReqBody))
		{
			return Reply(ERoomResult::InvalidRequest, 0);
		}

		RoomCreateReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const std::string Title = ReadFixedString(Req.Title, kMaxRoomTitleLen);
		// 비밀번호는 널 종료가 없는 고정 4바이트다. 길이를 지정해 그대로 읽는다.
		const std::string Password(Req.Password, kRoomPasswordLen);

		// 공인 주소는 서버가 관측하고, 사설 주소는 호스트가 신고한 값을 검사해서 받는다.
		// 판단은 전부 BuildHostCandidates 안에 있다.
		const std::string ReportedLan = ReadFixedString(Req.LanAddress, kMaxAddressLen);
		const std::vector<HostCandidate> Candidates =
			BuildHostCandidates(Session->PeerAddress, ReportedLan, Req.HostPort);

		uint32_t NewRoomId = 0;
		const ERoomResult R = Rooms::Create(
			Session->UserId, Session->Name, Candidates, Session->bLanOnly,
			Title, Req.bHasPassword != 0, Password, Req.MaxPlayers, NewRoomId);

		if (R == ERoomResult::Success)
		{
			ServerLog::Print("[방 생성] #%u \"%s\" 방장=%s(%llu) %s\n",
			            NewRoomId, Title.c_str(), Session->Name.c_str(),
			            static_cast<unsigned long long>(Session->UserId),
			            Req.bHasPassword ? "[비번]" : "");
			for (const HostCandidate& C : Candidates)
			{
				ServerLog::Print("[방 생성]   후보 %s:%u (%s)\n", C.Address, C.Port,
				            C.Kind == static_cast<uint8_t>(EHostAddrKind::Lan) ? "LAN" : "공인");
			}

			// 프로브를 돌리지 않은 클라이언트도 있을 수 있다(구버전, 프로브 실패).
			// 그때는 "모름" 이고, 방은 예전처럼 아무 제한 없이 동작한다.
			ServerLog::Print("[방 생성]   외부 접속: %s\n",
			            !Session->bHasReachabilityReport ? "확인 안 됨"
			            : (Session->bLanOnly ? "**불가** (같은 LAN 전용 방)" : "가능"));
		}
		else
		{
			ServerLog::Print("[거부] 방 생성 실패: %s (사유 %u)\n", Title.c_str(), static_cast<unsigned>(R));
		}

		const bool bSent = Reply(R, NewRoomId);

		// Ack 를 먼저 보내고 명단을 보낸다. 그래야 클라이언트가 RoomId 를 알게 된 뒤에
		// 명단이 도착해서, 어느 방 명단인지 헷갈릴 일이 없다.
		if (R == ERoomResult::Success)
		{
			BroadcastRoomMembers(NewRoomId);
		}
		return bSent;
	}


	bool HandleRoomListReq(const SessionPtr& Session, const char*, uint32_t)
	{
		if (!Session->bAuthed)
		{
			// 로그인하지 않은 사람에게는 목록을 주지 않는다.
			RoomListAckBody Empty{};
			Empty.Count = 0;
			return SendPacket(Session->Sock, EOpcode::RoomListAck, &Empty, sizeof(Empty));
		}

		std::vector<RoomInfo> List;
		Rooms::ListWaiting(List, kMaxRoomsInList);

		RoomListAckBody Head{};
		Head.Count = static_cast<uint16_t>(List.size());

		// 고정 헤더 + 가변 배열. ChatBroadcast 와 같은 2조각 전송 방식이다.
		return SendPacket2(Session->Sock, EOpcode::RoomListAck,
		                   &Head, sizeof(Head),
		                   List.empty() ? nullptr : reinterpret_cast<const char*>(List.data()),
		                   static_cast<uint32_t>(List.size() * sizeof(RoomInfo)));
	}


	bool HandleRoomJoinReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		RoomJoinAckBody Ack{};

		auto Reply = [&](ERoomResult R)
		{
			Ack.bSuccess = (R == ERoomResult::Success) ? 1 : 0;
			Ack.Result   = static_cast<uint8_t>(R);
			return SendPacket(Session->Sock, EOpcode::RoomJoinAck, &Ack, sizeof(Ack));
		};

		if (!Session->bAuthed)
		{
			return Reply(ERoomResult::NotAuthed);
		}
		if (BodySize < sizeof(RoomJoinReqBody))
		{
			return Reply(ERoomResult::InvalidRequest);
		}

		RoomJoinReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		const std::string Password(Req.Password, kRoomPasswordLen);

		std::vector<HostCandidate> Candidates;
		bool bLanOnly = false;
		const ERoomResult R = Rooms::Join(Req.RoomId, Session->UserId, Session->Name,
		                                  Password, Candidates, bLanOnly);

		Ack.RoomId = Req.RoomId;
		if (R == ERoomResult::Success)
		{
			Ack.CandidateCount = FillCandidates(Ack.Candidates, Candidates);
			Ack.bLanOnly       = bLanOnly ? 1 : 0;
			ServerLog::Print("[방 참여] #%u <- %s(%llu), 주소 후보 %u개 전달\n",
			            Req.RoomId, Session->Name.c_str(),
			            static_cast<unsigned long long>(Session->UserId),
			            static_cast<unsigned>(Ack.CandidateCount));
		}
		else
		{
			ServerLog::Print("[거부] 방 참여 실패: #%u <- %s (사유 %u)\n",
			            Req.RoomId, Session->Name.c_str(), static_cast<unsigned>(R));
		}

		const bool bSent = Reply(R);

		// 들어온 사람에게도, 이미 있던 사람에게도 갱신된 명단이 필요하다.
		// Ack 다음에 보내야 새 참여자가 RoomId 를 안 상태로 명단을 받는다.
		if (R == ERoomResult::Success)
		{
			BroadcastRoomMembers(Req.RoomId);
		}
		return bSent;
	}


	bool HandleRoomStateUpdate(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(RoomStateUpdateBody))
		{
			return true;   // 조용히 무시한다. 상태 갱신은 응답이 없는 단방향 통지다
		}

		RoomStateUpdateBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		// Req.CurrentPlayers 는 읽지 않는다. v5 부터 인원은 서버가 직접 센다.
		const ERoomResult R = Rooms::UpdateState(
			Req.RoomId, Session->UserId, static_cast<ERoomState>(Req.State));

		if (R == ERoomResult::Success)
		{
			ServerLog::Print("[방 갱신] #%u 상태 %s\n", Req.RoomId,
			            Req.State == static_cast<uint8_t>(ERoomState::InGame) ? "게임중" : "대기중");
		}
		return true;
	}


	bool HandleRoomLeaveReq(const SessionPtr& Session, const char*, uint32_t)
	{
		if (Session->bAuthed)
		{
			// v4 까지는 방장 전용이었다. 이제 참여자도 이걸로 대기실에서 나간다.
			LeaveRoomAndNotify(Session);

			// 스스로 나갔으므로 다시 "온라인" 이다 (M4).
			// 게임중이던 방에서 나온 경우 이게 없으면 계속 게임중으로 보인다.
			BroadcastPresence(Session, EPresence::Online);
		}
		return true;
	}


	bool HandleRoomCustomizationReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize != sizeof(RoomCustomizationReqBody)) return true;
		RoomCustomizationReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));
		RoomCustomizationAckBody Ack{};
		Ack.RoomId = Req.RoomId;
		Ack.RequestId = Req.RequestId;
		Ack.Data = Req.Data;
		const auto Result = Rooms::SetCustomization(Session->UserId, Req.RoomId, Req.Data);
		Ack.Result = static_cast<uint8_t>(Result);
		SendToUsers({Session->UserId}, EOpcode::RoomCustomizationAck, &Ack, sizeof(Ack), nullptr, 0);
		if (Result == ERoomResult::Success) BroadcastRoomMembers(Req.RoomId);
		return true;
	}

	bool HandleRoomReadyReq(const SessionPtr& Session, const char* Body, uint32_t BodySize)
	{
		if (!Session->bAuthed || BodySize < sizeof(RoomReadyReqBody))
		{
			return true;   // 준비 토글도 응답 없는 단방향이다. 결과는 명단으로 돌아온다
		}

		RoomReadyReqBody Req{};
		std::memcpy(&Req, Body, sizeof(Req));

		uint32_t RoomId = 0;
		const ERoomResult R = Rooms::SetReady(Session->UserId, Req.bReady != 0, RoomId);

		if (R == ERoomResult::Success)
		{
			ServerLog::Print("[준비] #%u %s(%llu) -> %s\n", RoomId, Session->Name.c_str(),
			            static_cast<unsigned long long>(Session->UserId),
			            Req.bReady ? "준비완료" : "준비해제");
			// 방장의 "게임 시작" 버튼이 켜질지 말지가 여기서 갈린다.
			// 명단에 bAllReady 가 실려 나가므로 전원이 같은 판정을 본다.
			BroadcastRoomMembers(RoomId);
		}
		return true;
	}


	bool HandleRoomStartReq(const SessionPtr& Session, const char*, uint32_t)
	{
		if (!Session->bAuthed)
		{
			return true;
		}

		uint32_t    RoomId = 0;
		std::vector<HostCandidate> Candidates;
		std::vector<uint64_t> Recipients;

		bool bLanOnly = false;
		const ERoomResult R = Rooms::StartGame(Session->UserId, RoomId,
		                                       Candidates, bLanOnly, Recipients);
		if (R != ERoomResult::Success)
		{
			ServerLog::Print("[거부] 게임 시작 실패: %s(%llu) (사유 %u)\n", Session->Name.c_str(),
			            static_cast<unsigned long long>(Session->UserId), static_cast<unsigned>(R));
			return true;
		}

		ServerLog::Print("[게임 시작] #%u 방장=%s, 주소 후보 %zu개, 인원 %zu명\n",
		            RoomId, Session->Name.c_str(), Candidates.size(), Recipients.size());

		RoomStartBody Start{};
		Start.RoomId         = RoomId;
		Start.CandidateCount = FillCandidates(Start.Candidates, Candidates);
		Start.bLanOnly       = bLanOnly ? 1 : 0;

		// ★ 홀펀칭 대상: 방장을 뺀 참여자들의 **관측된** 공인 게임 엔드포인트. (v10)
		//   방장은 OpenLevel 직전에 이 주소들로 한 발씩 쏴서 자기 NAT 에 구멍을 낸다.
		//   등록을 안 한 참여자는 여기 안 들어간다 — 그 사람은 예전처럼 붙어야 하고,
		//   방장 네트워크가 인바운드를 받는 경우에만 성공한다.
		{
			uint8_t PunchCount = 0;
			Context().Sessions.ForEach([&](const SessionPtr& Member)
			{
				if (PunchCount >= kMaxPlayersInRoom || Member->UserId == Session->UserId)
				{
					return;
				}
				if (Member->GameEndpointPort == 0 || Member->GameEndpointAddress.empty())
				{
					return;
				}
				// 이 방의 멤버만. Recipients 에 방장도 들어 있어 위에서 걸렀다.
				bool bInThisRoom = false;
				for (const uint64_t Id : Recipients)
				{
					if (Id == Member->UserId) { bInThisRoom = true; break; }
				}
				if (!bInThisRoom)
				{
					return;
				}

				PeerEndpoint& Target = Start.PunchTargets[PunchCount++];
				CopyFixedString(Target.Address, kMaxAddressLen, Member->GameEndpointAddress);
				Target.Port = Member->GameEndpointPort;
			});
			Start.PunchTargetCount = PunchCount;

			ServerLog::Print("[게임 시작]   홀펀칭 대상 %u명\n", static_cast<unsigned>(PunchCount));
			for (uint8_t i = 0; i < PunchCount; ++i)
			{
				ServerLog::Print("[게임 시작]     %s:%u\n",
				            Start.PunchTargets[i].Address, Start.PunchTargets[i].Port);
			}
		}


		// 직접 후보는 전원에게 같지만, relay capability 는 절대로 브로드캐스트하지
		// 않는다. 방장만 host-facing 경로 전부를 받고, 참여자 RoomStart 는 0으로 둔다.
		const bool bRelayAllocated = AllocateRelayRoutes(RoomId, Session->UserId, Recipients);
		if (bRelayAllocated)
		{
			std::vector<RelayHostRoute> HostRoutes = GetHostRelayRoutes(RoomId);
			if (!Context().RelayLanIp.empty() && IsPrivateAddress(Session->PeerAddress))
			{
				for (RelayHostRoute& Route : HostRoutes)
				{
					SetRelayAddress(Route, Context().RelayLanIp);
				}
			}
			Start.RelayRouteCount = static_cast<uint8_t>(std::min<std::size_t>(HostRoutes.size(), kMaxRelayRoutes));
			for (uint8_t Index = 0; Index < Start.RelayRouteCount; ++Index)
			{
				Start.RelayRoutes[Index] = HostRoutes[Index];
			}
			if (Start.RelayRouteCount > 0)
			{
				ServerLog::Print("[릴레이] #%u 방장에 %u개 host-facing 경로를 전달했다.\n",
				            RoomId, static_cast<unsigned>(Start.RelayRouteCount));
			}
		}
		else if (Context().Relay && Context().Relay->IsRunning())
		{
			ServerLog::Print("[릴레이] #%u 경로 할당 실패. 이번 방은 직접 연결만 시도한다.\n", RoomId);
		}

		// 방장에게만 capability 가 채워진 body 를 보낸다.
		SendPacket(Session->Sock, EOpcode::RoomStart, &Start, sizeof(Start));

		// 나머지 멤버에게는 relay 필드가 모두 0인 같은 RoomStart 를 보낸다.
		std::vector<uint64_t> GuestRecipients;
		GuestRecipients.reserve(Recipients.size());
		for (const uint64_t UserId : Recipients)
		{
			if (UserId != Session->UserId)
			{
				GuestRecipients.push_back(UserId);
			}
		}
		Start.RelayRouteCount = 0;
		std::memset(Start.RelayRoutes, 0, sizeof(Start.RelayRoutes));
		SendToUsers(GuestRecipients, EOpcode::RoomStart, &Start, sizeof(Start), nullptr, 0);

		// ★ 방이 InGame 이 됐으므로 이 방 사람들의 친구에게 "게임중" 을 알린다 (M4).
		//   Recipients 는 방 멤버 전원이다 - 방장도 포함된다.
		BroadcastPresenceFor(Recipients, EPresence::InGame);
		return true;
	}


	bool HandleRoomHostReadyReq(const SessionPtr& Session, const char*, uint32_t)
	{
		if (!Session->bAuthed)
		{
			return true;
		}

		uint32_t    RoomId = 0;
		std::vector<HostCandidate> Candidates;
		std::vector<uint64_t> Recipients;

		bool bLanOnly = false;
		const ERoomResult R = Rooms::MarkHostReady(Session->UserId, RoomId,
		                                           Candidates, bLanOnly, Recipients);
		if (R != ERoomResult::Success)
		{
			ServerLog::Print("[거부] 호스트 준비 신고 실패: %s(%llu) (사유 %u)\n", Session->Name.c_str(),
			            static_cast<unsigned long long>(Session->UserId), static_cast<unsigned>(R));
			return true;
		}

		ServerLog::Print("[호스트 준비] #%u 리슨서버가 열렸다. 참여자 %zu명에게 출발 신호 (주소 후보 %zu개)\n",
		            RoomId, Recipients.size(), Candidates.size());

		// GuestPort/token 은 참여자마다 다르므로, RoomHostReady 는 더 이상 하나를
		// 브로드캐스트할 수 없다. direct 후보는 같고 Relay 만 개인화한다.
		for (const uint64_t GuestUserId : Recipients)
		{
			RoomHostReadyBody Ready{};
			Ready.RoomId         = RoomId;
			Ready.CandidateCount = FillCandidates(Ready.Candidates, Candidates);
			Ready.bLanOnly       = bLanOnly ? 1 : 0;
			if (const SessionPtr Guest = FindAuthedSession(GuestUserId))
			{
				GetGuestRelayRoute(RoomId, GuestUserId, Ready.Relay);
				if (!Context().RelayLanIp.empty() && IsPrivateAddress(Guest->PeerAddress))
				{
					SetRelayAddress(Ready.Relay, Context().RelayLanIp);
				}
				SendPacket(Guest->Sock, EOpcode::RoomHostReady, &Ready, sizeof(Ready));
			}
		}
		return true;
	}

}
