#include "Rooms/Rooms.h"
#include <cstdlib>
#include <iostream>
#include <limits>

static void Check(bool Condition, const char* Message)
{
    if (!Condition) { std::cerr << Message << '\n'; std::exit(1); }
}

int main()
{
    using namespace MOU;
    std::vector<HostCandidate> Candidates(1);
    uint32_t RoomId = 0, ChangedRoom = 0;
    bool Lan = false, AllReady = false, Closed = false;
    Check(Rooms::Create(100, "host", Candidates, true, "appearance", false, "", 4, RoomId)
        == ERoomResult::Success, "create");
    Check(Rooms::Join(RoomId, 101, "guest", "", Candidates, Lan) == ERoomResult::Success, "join");
    CharacterCustomization Red;
    Red.BodyColor[1] = 0; Red.BodyColor[2] = 0;
    Red.DecalIndex = 4; Red.TilingX = 3; Red.RoughnessA = 0.8f;
    Check(Rooms::SetCustomization(101, RoomId, Red) == ERoomResult::Success, "set appearance");
    Check(Rooms::SetCustomization(999, RoomId, Red) == ERoomResult::NotInRoom, "outsider accepted");
    Check(Rooms::SetCustomization(101, RoomId + 1, Red) == ERoomResult::NotInRoom, "stale room accepted");
    auto Invalid = Red;
    Invalid.Metallic = std::numeric_limits<float>::quiet_NaN();
    Check(Rooms::SetCustomization(101, RoomId, Invalid) == ERoomResult::InvalidRequest, "NaN accepted");
    Invalid = Red; Invalid.DecalIndex = 6;
    Check(Rooms::SetCustomization(101, RoomId, Invalid) == ERoomResult::InvalidRequest, "invalid decal accepted");
    Invalid = Red; Invalid.TilingY = 0;
    Check(Rooms::SetCustomization(101, RoomId, Invalid) == ERoomResult::InvalidRequest, "invalid tiling accepted");
    // Late arrivals receive the existing appearance in the full room snapshot.
    Check(Rooms::Join(RoomId, 102, "late", "", Candidates, Lan) == ERoomResult::Success, "late join");
    std::vector<RoomMemberInfo> Members;
    std::vector<uint64_t> Notify;
    Check(Rooms::GetMembers(RoomId, Members, AllReady, Notify), "snapshot");
    for (const auto& M : Members)
    {
        if (M.UserId == 101)
            Check(M.SlotIndex == 1 && M.Customization.BodyColor[1] == 0 &&
                M.Customization.DecalIndex == 4 && M.Customization.TilingX == 3 &&
                M.Customization.RoughnessA == 0.8f, "appearance/seat lost");
        else Check(M.Customization.BodyColor[1] == 1, "another member changed");
    }
    Rooms::Leave(101, ChangedRoom, Closed, Notify);
    Check(Rooms::Join(RoomId, 103, "replacement", "", Candidates, Lan) == ERoomResult::Success, "refill");
    Rooms::GetMembers(RoomId, Members, AllReady, Notify);
    for (const auto& M : Members)
        if (M.UserId == 103) Check(M.SlotIndex == 1 && M.Customization.BodyColor[1] == 1, "seat leaked appearance");
    Rooms::SetReady(102, true, ChangedRoom);
    Rooms::SetReady(103, true, ChangedRoom);
    Check(Rooms::SetCustomization(100, RoomId, Red) == ERoomResult::Success, "host appearance");
    Check(Rooms::StartGame(100, ChangedRoom, Candidates, Lan, Notify) == ERoomResult::Success, "start");
    Rooms::GetMembers(RoomId, Members, AllReady, Notify);
    for (const auto& M : Members)
        if (M.UserId == 100) Check(M.Customization.DecalIndex == 4 && M.Customization.BodyColor[1] == 0,
            "start cleared appearance");
    Check(Rooms::SetCustomization(100, RoomId, Red) == ERoomResult::AlreadyStarted, "edit after start accepted");
    Rooms::Leave(100, ChangedRoom, Closed, Notify);
    std::cout << "RoomCustomizationTest passed\n";
}
