#include "PacketDispatcher/PacketDispatcher.h"
#include "ServerLog/ServerLog.h"
#include "EndpointService/EndpointService.h"
#include "ChatHandler/ChatHandler.h"
#include "AuthHandler/AuthHandler.h"
#include "SocialHandler/SocialHandler.h"
#include "RoomHandler/RoomHandler.h"
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

	bool HandlePacket(const SessionPtr& Session, const PacketHeader& Header,
	                  const std::vector<char>& Body)
	{
		const char* Data = Body.empty() ? nullptr : Body.data();
		const uint32_t Size = static_cast<uint32_t>(Body.size());

		switch (static_cast<EOpcode>(Header.Opcode))
		{
		case EOpcode::LoginReq:  return HandleLoginReq(Session, Data, Size);
		case EOpcode::RegisterReq: return HandleRegisterReq(Session, Data, Size);
		case EOpcode::RoomCreateReq:   return HandleRoomCreateReq(Session, Data, Size);
		case EOpcode::RoomListReq:     return HandleRoomListReq(Session, Data, Size);
		case EOpcode::RoomJoinReq:     return HandleRoomJoinReq(Session, Data, Size);
		case EOpcode::RoomLeaveReq:    return HandleRoomLeaveReq(Session, Data, Size);
		case EOpcode::RoomStateUpdate: return HandleRoomStateUpdate(Session, Data, Size);
		case EOpcode::RoomCustomizationReq: return HandleRoomCustomizationReq(Session, Data, Size);
		case EOpcode::RoomReadyReq:    return HandleRoomReadyReq(Session, Data, Size);
		case EOpcode::RoomStartReq:    return HandleRoomStartReq(Session, Data, Size);
		case EOpcode::RoomHostReadyReq: return HandleRoomHostReadyReq(Session, Data, Size);

		// --- 도달성 프로브 (v9) ---
		case EOpcode::HostProbeReq:        return HandleHostProbeReq(Session, Data, Size);
		case EOpcode::RoomReachabilityReq: return HandleRoomReachabilityReq(Session, Data, Size);
		case EOpcode::ChatSend:  return HandleChatSend(Session, Data, Size);
		case EOpcode::SetDead:   return HandleSetDead(Session, Data, Size);

		// --- 친구 (v7) ---
		case EOpcode::FriendListReq:    return HandleFriendListReq(Session, Data, Size);
		case EOpcode::FriendAddReq:     return HandleFriendAddReq(Session, Data, Size);
		case EOpcode::FriendRespondReq: return HandleFriendRespondReq(Session, Data, Size);
		case EOpcode::FriendRemoveReq:  return HandleFriendRemoveReq(Session, Data, Size);

		// --- 메신저 (v7) ---
		case EOpcode::DirectMessageSend: return HandleDirectMessageSend(Session, Data, Size);
		case EOpcode::DmHistoryReq:      return HandleDmHistoryReq(Session, Data, Size);

		case EOpcode::Heartbeat: return true;
		default:
			ServerLog::Print("[경고] 알 수 없는 오피코드 %u\n", Header.Opcode);
			return false;
		}
	}

}
