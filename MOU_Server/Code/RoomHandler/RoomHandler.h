#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	void BroadcastRoomMembers(uint32_t RoomId);
	void NotifyRoomClosed(const std::vector<uint64_t>& Recipients,
	                      uint32_t RoomId, ERoomCloseReason Reason);
	void LeaveRoomAndNotify(const SessionPtr& Session);
	bool HandleRoomCreateReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomListReq(const SessionPtr& Session, const char*, uint32_t);
	bool HandleRoomJoinReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomStateUpdate(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomLeaveReq(const SessionPtr& Session, const char*, uint32_t);
	bool HandleRoomCustomizationReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomReadyReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomStartReq(const SessionPtr& Session, const char*, uint32_t);
	bool HandleRoomHostReadyReq(const SessionPtr& Session, const char*, uint32_t);
}
