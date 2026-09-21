# 방 대기실 플레이어 슬롯

4개의 슬롯을 2열로 생성하고 재사용한다. 서버가 보내는 `SlotIndex`(0~3)에 배치하며 중간 인원이 나가도 다른 사람의 자리는 이동하지 않는다. 새 참가자는 가장 낮은 빈 번호에 배정된다.

## 디자이너 설정

1. `RoomPlayerSlotWidgetBase`를 부모로 `WBP_PlayerSlotWidget`을 만든다.
2. 빈 자리 패널 `EmptyPanel`, 참가자 패널 `OccupiedPanel`을 배치한다.
3. 선택 바인딩 이름: `NicknameText`는 TextBlock, `HostImage`와 `ReadyImage`는 Image, `SelfHighlight`는 자기 자신을 표시할 위젯이다. 각 위젯을 변수로 노출한다. `HostImage`는 방장에게만 보이고, `ReadyImage`는 방장이 아닌 참가자가 준비 완료했을 때만 보인다. 상태 이미지의 Brush는 WBP에서 지정한다.
4. 캐릭터 이미지와 배경은 슬롯 WBP에서 자유롭게 구성한다. `OnSlotChanged` 이벤트에서 `Member`, `bOccupied`, `bIsSelf`로 추가 외형을 갱신할 수 있다. 현재 캐릭터 외형 데이터/3D 미리보기는 구현 범위에 포함하지 않는다.
5. `RoomLobbyWidgetBase`를 부모로 한 방 대기실 WBP에 빈 UniformGridPanel을 만들고 이름을 `PlayerSlotGrid`로 지정한다. 슬롯 자식은 C++에서 추가하므로 디자이너에서 중복 배치하지 않는다.
6. 해당 WBP의 Class Defaults에서 `PlayerSlotWidgetClass`를 `WBP_PlayerSlotWidget`으로, `SlotColumns`를 2로 지정한다. 로비 루트의 방 대기실 페이지 클래스로 이 WBP를 연결한다.

WBP가 없으면 C++ 기본 카드 UI를 사용한다. 기존 `MemberListBox`만 있는 방 대기실 WBP도 내부에 그리드를 생성하는 호환 경로를 사용한다. 방 생성 화면과 방 대기실 화면은 별개 페이지이므로 준비/시작 버튼은 방 대기실에서만 디자인한다.

## 닉네임만 보일 때: 2 × 2 레이아웃 설정

`SlotColumns = 2`는 행/열 번호를 정할 뿐, 위젯 내부의 크기와 앵커까지 수정하지 않는다.
UE 5.8의 `UUniformGridSlot` 기본 정렬은 Left/Top이다. C++에서는 동적으로 추가한 슬롯을
가로/세로 Fill로 지정하고, 대기실 Construct 때 서버 목록 수신 여부와 관계없이 네 자리를 생성한다.
슬롯의 Construct 뒤에는 현재 참가자 상태로 닉네임과 패널/방장/준비 표시를 다시 적용한다.
WBP 디자이너에서는 다음 설정도 필요하다.

### WBP_RoomLobbyWidget

- 부모 클래스: `RoomLobbyWidgetBase`.
- `PageLayout`이 Vertical Box라면 `HeaderArea`와 `FooterArea`의 슬롯 Size는 Auto,
  `PlayersArea`의 슬롯 Size는 Fill(1), 가로/세로 정렬은 Fill로 지정한다.
- `PlayersArea` 안의 `PlayerSlotGrid`도 가로/세로 Fill로 지정한다.
  Canvas 자식인 경우에는 Anchors Min=(0,0), Max=(1,1), Offsets=모두 0,
  Alignment=(0,0), Auto Size=false로 전체 영역을 채운다.
- `PlayerSlotGrid`의 타입은 **Uniform Grid Panel**, 이름은 정확히 `PlayerSlotGrid`로 하고
  Is Variable을 켠다. 디자이너 안에 슬롯 자식을 수동으로 추가하지 않는다.
- Class Defaults: Player Slot Widget Class=`WBP_PlayerSlotWidget`, Slot Columns=2.
- `WBP_LobbyWidget`의 방 대기실 페이지 클래스에도 이 `WBP_RoomLobbyWidget`이 지정되어 있어야 한다.

```text
PlayersArea
└─ PlayerSlotGrid (UniformGridPanel)
   ├─ 런타임 슬롯 0 → Row 0 / Column 0
   ├─ 런타임 슬롯 1 → Row 0 / Column 1
   ├─ 런타임 슬롯 2 → Row 1 / Column 0
   └─ 런타임 슬롯 3 → Row 1 / Column 1
```

### WBP_PlayerSlotWidget: 현재 계층을 유지하는 방법

전체 화면 크기의 RootCanvas 가운데 고정 크기 카드를 배치하면, 그리드가 카드 크기를 줄여 주지는 않는다.
RootCanvas가 받는 한 칸의 공간을 카드가 채우도록 바꾼다.

1. `SlotSize` 선택 → **Slot (Canvas Panel Slot)**:
   Anchors Min=(0,0), Max=(1,1), Offsets Left/Top/Right/Bottom=0,
   Alignment=(0,0), Auto Size=false.
2. `SlotSize`의 Width Override / Height Override와 고정 Min/Max Desired 크기 제한을 해제한다.
   내부 콘텐츠의 가로/세로 정렬은 Fill로 둔다.
3. `Overlay`의 `Background`와 `SlotOverlay`, `SlotOverlay`의 `EmptyPanel`과
   `OccupiedPanel`은 각 부모 슬롯에서 가로/세로 Fill로 지정한다.
   `OccupiedCanvas`도 부모 영역 전체를 채운다.
4. `OccupiedCanvas` 안의 요소는 카드의 경계에 앵커를 건다. 아래는 높이 40/44를 쓰는 예시다.

   | 위젯 | Anchors Min → Max | Offsets | Alignment |
   |---|---|---|---|
   | NicknameOverlay | (0,0) → (1,0) | Left=0, Top=0, Right=0, Height=40 | (0,0) |
   | StateOverlay | (0,1) → (1,1) | Left=0, Position Y=0, Right=0, Height=44 | (0,1) |
   | PreviewArea | (0,0) → (1,1) | Left=0, Top=40, Right=0, Bottom=44 | (0,0) |

   Auto Size는 끈다. `NicknameOverlay` 안의 닉네임 배경, `StateOverlay` 안의
   `HostImage`/`ReadyImage`도 Fill로 배치한다. `CharacterImage`는 PreviewArea에 맞게
   배치하고, 비율 유지가 필요하면 ScaleBox를 사용한다.
5. `Background`는 EmptyPanel/OccupiedPanel의 바깥에 둬서 빈 자리에도 보이게 한다.
   `EmptyPanel`에 “참가자 대기 중” 텍스트나 빈 자리 이미지를 넣는다.
   네 슬롯 자체의 Visibility는 Visible 또는 Not Hit-Testable로 유지한다.
   빈 자리라고 슬롯 전체를 Collapsed로 숨기지 않는다.

Designer의 Full Screen/Custom 미리보기 크기는 런타임 카드 크기를 고정하는 설정이 아니다.
루트 Canvas를 제거하고 Overlay를 루트로 구성해도 되지만, 위 앵커 설정으로 현재 계층을 유지할 수 있다.

### HostImage와 이미지 리소스 확인

- `HostImage`/`ReadyImage`는 **Image**, `NicknameText`는 **TextBlock**이어야 한다.
  이름과 Is Variable을 확인한다. `NickNameBlock` 같은 장식용 배경 이름은 자유롭게 사용해도 된다.
- `HostImage`의 Brush → Image에 방장 텍스처를 지정한다. Draw As=None이 아니어야 하며,
  Color and Opacity의 Alpha와 Render Opacity는 1로 둔다. 부모 `StateOverlay`도 숨기지 않는다.
- `Background`, `CharacterImage`, 닉네임 배경도 각자 Brush를 지정한다.
  C++은 캐릭터 텍스처나 방장 배지 텍스처를 자동으로 넣지 않는다.
- Visibility 바인딩이나 Construct/OnSlotChanged 그래프가 C++의 상태를 다시 덮어쓰지 않게 한다.
  `HostImage`는 `bOccupied && Member.bIsHost`, `ReadyImage`는
  `bOccupied && !Member.bIsHost && Member.bReady`일 때만 표시된다.
  따라서 방장에게 ReadyImage가 보이지 않는 것은 정상이다.
- 계속 방장만 안 보이면 `OnSlotChanged`에서 Member.bIsHost를 출력해 데이터부터 확인한다.
  true라면 Brush/부모 Visibility/크기 문제를, false라면 서버의 멤버 목록을 확인한다.
  서버의 `Rooms.cpp`는 멤버 UserId와 HostUserId 비교로 이 값을 전송한다.

## 에디터 확인 순서

1. C++ 빌드 후 에디터에서 두 WBP를 Compile/Save한다.
2. 방 생성 직후 2 × 2 자리 중 방장 카드 1개와 빈 자리 3개가 보이는지 확인한다.
3. 두 번째 클라이언트로 참가하여 빈 자리 하나만 참가자 카드로 바뀌는지 확인한다.
4. 참가자 준비/준비 해제로 ReadyImage가 바뀌고, 방장의 HostImage는 유지되는지 확인한다.
5. 참가자가 나가면 해당 자리만 빈 자리로 돌아오는지 확인한다.
6. 작은 창에서도 닉네임/방장 배지가 카드 영역 안에 남아 있는지 확인한다.

## 데이터와 검증

서버 응답 → 기존 FlowCoordinator/방 대기실 갱신 경로 → SlotIndex별 SetMember/ClearMember. 개별 슬롯은 서버 델리게이트를 구독하지 않는다.

프로토콜은 v12이며 RoomMemberInfo는 43바이트다. 서버와 클라이언트를 함께 다시 빌드하여 교체해야 한다.

`RoomSlotsTest`는 중간 퇴장 후 자리 유지, 빈 자리 재사용, 중복 입장, 정원 제한, 준비 변경 및 방장 퇴장을 검증한다. 에디터에서는 별도로 두 클라이언트 이상으로 입장/퇴장/준비 및 WBP 레이아웃을 확인한다.
