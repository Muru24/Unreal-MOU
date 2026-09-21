#include "Session/Session.h"

#include <algorithm>

namespace MOU
{
	SessionPtr SessionManager::Add(SocketHandle Sock)
	{
		SessionPtr Session = std::make_shared<ClientSession>();
		Session->Sock = Sock;

		std::lock_guard<std::mutex> Lock(Mutex);
		Sessions.push_back(Session);
		return Session;
	}

	void SessionManager::Remove(const SessionPtr& Session)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		if (Session->bAuthed)
		{
			const auto Owner = AccountOwners.find(Session->UserId);
			if (Owner != AccountOwners.end() && Owner->second == Session.get())
			{
				AccountOwners.erase(Owner);
			}
		}

		const auto It = std::find(Sessions.begin(), Sessions.end(), Session);
		if (It != Sessions.end())
		{
			Sessions.erase(It);
		}

		// 목록에서 빠진 뒤에 닫는다. 락을 잡은 채로 처리하므로
		// 다른 스레드가 ForEach 로 순회 중인 소켓을 닫아버리는 일은 없다.
		if (Session->Sock != kInvalidSocket)
		{
			CloseSocket(Session->Sock);
			Session->Sock = kInvalidSocket;
		}
	}

	bool SessionManager::TryClaimAccount(const SessionPtr& Session, uint64_t UserId,
	                                     const std::string& Name, int32_t TeamId)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		if (UserId == 0 || Session->bAuthed ||
		    std::find(Sessions.begin(), Sessions.end(), Session) == Sessions.end() ||
		    AccountOwners.find(UserId) != AccountOwners.end())
		{
			return false;
		}

		Session->UserId = UserId;
		Session->Name = Name;
		Session->TeamId = TeamId;
		Session->bAuthed = true;
		AccountOwners.emplace(UserId, Session.get());
		return true;
	}

	uint64_t SessionManager::AssignUserId()
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		return NextUserId++;
	}

	size_t SessionManager::Count()
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		return Sessions.size();
	}
}
