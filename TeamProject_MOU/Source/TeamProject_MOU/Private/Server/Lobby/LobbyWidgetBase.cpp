// MOU 로비 - 메인메뉴 + 대기실 UI 구현.
//
// 이 파일은 소켓/패킷을 전혀 모른다.
//   상태 조회: UServerSubsystem (연결 상태 / 내 신원 / 방 번호 / 대기실 명단)
//   서버 왕복: 방에 들어가기 전에는 ULobbyFlowCoordinator 가 응답 수명을 관리하고,
//              들어간 뒤의 방 동작은 UServerSubsystem API 가 한다.
//
// [화면을 바꾸는 곳은 RefreshUI() 하나뿐이다]
//   버튼 라벨, 활성화 여부, 명단 표시를 전부 거기서 결정한다.
//   여기저기서 SetText 를 부르기 시작하면 "준비하기" 라고 적힌 버튼이
//   방을 만드는 식의 어긋남이 반드시 생긴다.

#include "Server/Lobby/LobbyWidgetBase.h"

#include "Server/Lobby/LobbyFlowCoordinator.h"
#include "Server/Lobby/LobbyPageWidgetBase.h"
#include "Server/Lobby/RoomCreateWidgetBase.h"
#include "Server/Lobby/RoomListWidgetBase.h"
#include "Server/Net/NatPortMappingSubsystem.h"
#include "Server/ServerSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

ULobbyWidgetBase::ULobbyWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------
// 수명 주기
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
	else if (WidgetTree != nullptr && LobbyScreenStack == nullptr)
	{
		// 구형 WBP의 단일 패널은 새 페이지 구조와 함께 쓸 수 없다. 디자이너가
		// LobbyScreenStack을 추가하기 전까지 런타임 스택을 루트로 사용한다.
		LobbyScreenStack = WidgetTree->ConstructWidget<UWidgetSwitcher>(
			UWidgetSwitcher::StaticClass(), TEXT("LobbyScreenStack_Runtime"));
		WidgetTree->RootWidget = LobbyScreenStack;
	}
}

void ULobbyWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// NativeConstruct 는 뷰포트에 다시 붙을 때마다 불릴 수 있어 중복 구독을 막는다.
	if (!bSubscribed)
	{
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (ULobbyFlowCoordinator* Flow = GameInstance->GetSubsystem<ULobbyFlowCoordinator>())
			{
				Flow->OnConnectionStateChanged.AddUObject(this, &ULobbyWidgetBase::HandleChatStateChanged);
				Flow->OnLoginCompleted.AddUObject(this, &ULobbyWidgetBase::HandleLoginCompleted);
				Flow->OnRoomMembersChanged.AddUObject(this, &ULobbyWidgetBase::HandleRoomMembersChanged);
				Flow->OnRoomClosed.AddUObject(this, &ULobbyWidgetBase::HandleRoomClosed);
				Flow->OnGameStarted.AddUObject(this, &ULobbyWidgetBase::HandleGameStarted);
				Flow->OnHostReady.AddUObject(this, &ULobbyWidgetBase::HandleHostReady);
				Flow->OnTravelFailed.AddUObject(this, &ULobbyWidgetBase::HandleTravelFailed);
				Flow->OnRoomEntered.AddUObject(this, &ULobbyWidgetBase::HandleFlowRoomEntered);
				bSubscribed = true;
			}
		}
	}

	if (bManageMouseCursor)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			// GameAndUI : 로비 뒤에서 게임이 돌고 있어도 입력이 완전히 막히지 않는다.
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			PC->SetShowMouseCursor(true);
		}
	}

	// ★ 여행 설정을 서브시스템에 넘긴다. (2026-08-29)
	//   값의 주인은 여전히 이 WBP 다(디자이너가 여기서 고친다). 다만 실제로 떠나는
	//   일은 서브시스템이 한다 — 이 위젯은 게임이 시작되면 닫힐 수 있고, 그러면
	//   출발 신호를 받을 사람이 사라지기 때문이다.
	//   여기서 넘겨두면 위젯이 죽어도 설정은 남는다.
	if (UServerSubsystem* Chat = GetServerSubsystem())
	{
		Chat->ConfigureTravel(HostMapName, bAutoTravelOnGameStart, bPreloadMapWhileWaiting);
	}

	// 서브시스템은 레벨을 넘어가도 살아있으므로, 이 위젯이 새로 만들어졌을 때
	// 이미 방 안일 수 있다. 지금 상태를 물어보고 거기에 맞춰 연다.
	if (const UServerSubsystem* Chat = GetServerSubsystem())
	{
		UIState = (Chat->GetCurrentRoomId() != 0)
			? EMOULobbyUIState::WaitingRoom
			: EMOULobbyUIState::MainMenu;
	}

	InitializePageStack();
	RefreshUI();
}

void ULobbyWidgetBase::NativeDestruct()
{
	ResetPageStack();

	if (bSubscribed)
	{
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (ULobbyFlowCoordinator* Flow = GameInstance->GetSubsystem<ULobbyFlowCoordinator>())
			{
				Flow->OnConnectionStateChanged.RemoveAll(this);
				Flow->OnLoginCompleted.RemoveAll(this);
				Flow->OnRoomMembersChanged.RemoveAll(this);
				Flow->OnRoomClosed.RemoveAll(this);
				Flow->OnGameStarted.RemoveAll(this);
				Flow->OnHostReady.RemoveAll(this);
				Flow->OnTravelFailed.RemoveAll(this);
				Flow->OnRoomEntered.RemoveAll(this);
			}
		}
		bSubscribed = false;
	}

	if (bManageMouseCursor)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// 기본 레이아웃 조립 (WBP 가 없을 때만)
//
//   CanvasPanel (화면 전체)
//     └ Border (화면 정중앙 380x420, 반투명 검정)
//         └ VerticalBox
//             ├ TitleText          "MOU 로비" / "대기실"
//             ├ StatusText         닉네임 / 방 번호
//             ├ MemberListBox      대기실 명단 (메인메뉴에서는 접힘)
//             ├ PrimaryButton      방 만들기 / 준비하기 / 게임 시작
//             ├ SecondaryButton    참여하기 / 커스터마이징
//             ├ TertiaryButton     게임 종료 / 나가기
//             └ MessageText        안내 / 실패 사유
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::BuildDefaultLayout()
{
	LobbyScreenStack = WidgetTree->ConstructWidget<UWidgetSwitcher>(
		UWidgetSwitcher::StaticClass(), TEXT("LobbyScreenStack"));
	WidgetTree->RootWidget = LobbyScreenStack;
}

void ULobbyWidgetBase::InitializePageStack()
{
	if (LobbyScreenStack == nullptr)
	{
		return;
	}

	if (PageStack.Num() != 0)
	{
		return;
	}

	MainLobbyWidget = CreateMainLobbyPage();
	if (MainLobbyWidget == nullptr)
	{
		return;
	}
	PushPage(MainLobbyWidget);

	if (UIState == EMOULobbyUIState::WaitingRoom)
	{
		RoomLobbyWidget = CreateRoomLobbyPage();
		PushPage(RoomLobbyWidget);
	}
}

ULobbyMainWidgetBase* ULobbyWidgetBase::CreateMainLobbyPage()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return nullptr;
	}
	UClass* PageClass = MainLobbyWidgetClass ? MainLobbyWidgetClass.Get() : ULobbyMainWidgetBase::StaticClass();
	ULobbyMainWidgetBase* Page = CreateWidget<ULobbyMainWidgetBase>(PC, PageClass);
	if (Page != nullptr)
	{
		Page->OnCreateRoom.BindUObject(this, &ULobbyWidgetBase::OpenRoomCreate);
		Page->OnJoinRoom.BindUObject(this, &ULobbyWidgetBase::OpenRoomList);
		Page->OnOpenSettings.BindUObject(this, &ULobbyWidgetBase::OpenSettings);
		Page->OnQuitGame.BindUObject(this, &ULobbyWidgetBase::QuitGame);
	}
	return Page;
}

URoomLobbyWidgetBase* ULobbyWidgetBase::CreateRoomLobbyPage()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return nullptr;
	}
	UClass* PageClass = RoomLobbyWidgetClass ? RoomLobbyWidgetClass.Get() : URoomLobbyWidgetBase::StaticClass();
	URoomLobbyWidgetBase* Page = CreateWidget<URoomLobbyWidgetBase>(PC, PageClass);
	if (Page != nullptr)
	{
		Page->OnToggleReady.BindUObject(this, &ULobbyWidgetBase::ToggleReady);
		Page->OnStartGame.BindUObject(this, &ULobbyWidgetBase::RequestStartGame);
		Page->OnCustomize.BindUObject(this, &ULobbyWidgetBase::OpenCustomize);
		Page->OnLeaveRoom.BindUObject(this, &ULobbyWidgetBase::LeaveRoom);
	}
	return Page;
}

void ULobbyWidgetBase::PushPage(UUserWidget* Page)
{
	if (LobbyScreenStack == nullptr || Page == nullptr || IsTopPage(Page))
	{
		return;
	}
	if (Page->GetParent() != LobbyScreenStack)
	{
		LobbyScreenStack->AddChild(Page);
	}
	PageStack.Add(Page);
	LobbyScreenStack->SetActiveWidget(Page);
}

bool ULobbyWidgetBase::PopPage()
{
	if (LobbyScreenStack == nullptr || PageStack.Num() <= 1)
	{
		return false;
	}
	if (UUserWidget* Top = PageStack.Pop())
	{
		Top->RemoveFromParent();
	}
	LobbyScreenStack->SetActiveWidget(PageStack.Last());
	return true;
}

void ULobbyWidgetBase::PopToMainMenu()
{
	while (PageStack.Num() > 1)
	{
		PopPage();
	}
	RoomCreateWidget = nullptr;
	RoomListWidget = nullptr;
	RoomLobbyWidget = nullptr;
	SettingsWidget = nullptr;
	CustomizeWidget = nullptr;
}

void ULobbyWidgetBase::ResetPageStack()
{
	if (LobbyScreenStack != nullptr)
	{
		LobbyScreenStack->ClearChildren();
	}
	PageStack.Reset();
	RoomCreateWidget = nullptr;
	RoomListWidget = nullptr;
	RoomLobbyWidget = nullptr;
	SettingsWidget = nullptr;
	CustomizeWidget = nullptr;
	MainLobbyWidget = nullptr;
}

bool ULobbyWidgetBase::IsTopPage(const UUserWidget* Page) const
{
	return Page != nullptr && PageStack.Num() > 0 && PageStack.Last() == Page;
}

// ---------------------------------------------------------------------------
// 화면 갱신 — 상태를 화면으로 옮기는 유일한 함수
// ---------------------------------------------------------------------------

UServerSubsystem* ULobbyWidgetBase::GetServerSubsystem() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UServerSubsystem>();
	}
	return nullptr;
}

void ULobbyWidgetBase::RefreshUI()
{
	const UServerSubsystem* Server = GetServerSubsystem();
	if (MainLobbyWidget != nullptr) { MainLobbyWidget->Refresh(Server); }
	if (RoomLobbyWidget != nullptr) { RoomLobbyWidget->Refresh(Server); }
}

void ULobbyWidgetBase::RebuildMemberList()
{
	if (RoomLobbyWidget != nullptr) { RoomLobbyWidget->Refresh(GetServerSubsystem()); }
}

void ULobbyWidgetBase::SetMessage(const FString& Text, bool bIsError)
{
	if (RoomLobbyWidget != nullptr && IsTopPage(RoomLobbyWidget))
	{
		RoomLobbyWidget->SetMessage(Text, bIsError);
		return;
	}
	if (MainLobbyWidget != nullptr) { MainLobbyWidget->SetMessage(Text, bIsError); }
}

// ---------------------------------------------------------------------------
// 버튼 — 상태에 따라 다른 일을 한다
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::HandlePrimaryClicked()
{
	if (UIState == EMOULobbyUIState::MainMenu)
	{
		OpenRoomCreate();
		return;
	}

	const UServerSubsystem* Chat = GetServerSubsystem();
	if (Chat != nullptr && Chat->IsRoomHost())
	{
		RequestStartGame();
	}
	else
	{
		ToggleReady();
	}
}

void ULobbyWidgetBase::HandleSecondaryClicked()
{
	if (UIState == EMOULobbyUIState::MainMenu)
	{
		OpenRoomList();
	}
	else
	{
		OpenCustomize();
	}
}

void ULobbyWidgetBase::HandleTertiaryClicked()
{
	if (UIState == EMOULobbyUIState::MainMenu)
	{
		QuitGame();
	}
	else
	{
		LeaveRoom();
	}
}

// ---------------------------------------------------------------------------
// 대기실 동작
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::ToggleReady()
{
	UServerSubsystem* Chat = GetServerSubsystem();
	if (Chat == nullptr || Chat->GetCurrentRoomId() == 0)
	{
		return;
	}

	// 낙관적으로 화면을 먼저 바꾸지 않는다. 서버가 갱신된 명단을 돌려주면
	// OnRoomMembersChanged 에서 RefreshUI 가 돌면서 버튼 글자가 바뀐다.
	// 이렇게 해야 화면과 서버가 어긋나지 않는다.
	Chat->SetReady(!Chat->IsSelfReady());
}

void ULobbyWidgetBase::RequestStartGame()
{
	UServerSubsystem* Chat = GetServerSubsystem();
	if (Chat == nullptr || !Chat->IsRoomHost())
	{
		return;
	}

	if (!Chat->AreAllMembersReady())
	{
		SetMessage(UServerSubsystem::GetRoomResultText(EMOURoomResultBP::NotAllReady), true);
		return;
	}

	SetMessage(TEXT("게임을 시작합니다..."), false);
	Chat->StartGame();
}

void ULobbyWidgetBase::LeaveRoom()
{
	if (UServerSubsystem* Chat = GetServerSubsystem())
	{
		Chat->LeaveRoom();
	}
	ReturnToMainMenu(/*bRoomClosed=*/false);
	SetMessage(TEXT("방에서 나왔습니다."), false);
}

void ULobbyWidgetBase::OpenCustomize()
{
	if (!IsTopPage(RoomLobbyWidget))
	{
		return;
	}
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}
	UClass* WidgetClass = CustomizeWidgetClass ? CustomizeWidgetClass.Get() : ULobbyCustomizeWidgetBase::StaticClass();
	CustomizeWidget = CreateWidget<ULobbyCustomizeWidgetBase>(PC, WidgetClass);
	if (CustomizeWidget == nullptr)
	{
		SetMessage(TEXT("커스터마이징 화면을 만들지 못했습니다."), true);
		return;
	}
	CustomizeWidget->OnBack.BindUObject(this, &ULobbyWidgetBase::HandleCustomizeClosed);
	PushPage(CustomizeWidget);
	OnCustomizeRequested();
}

void ULobbyWidgetBase::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

// ---------------------------------------------------------------------------
// 상태 전환
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::EnterWaitingRoom(int32 RoomId, bool bIsHost)
{
	UIState = EMOULobbyUIState::WaitingRoom;

	// ★ 게임 포트를 지금 확보하고 서버에 등록한다. (v10)
	//
	//   [왜 여기인가]
	//     대기실에 앉아 있는 동안이 유일하게 여유 있는 시점이다. 게임 시작 뒤에는
	//     방장이 곧 OpenLevel 을 하고, 그 전에 punch 대상이 이미 정해져 있어야 한다.
	//
	//   [왜 참여자도 하는가]
	//     punch 를 받는 쪽이 참여자다. 서버가 참여자의 공인 엔드포인트를 관측해
	//     두지 않으면 방장은 어디로 쏠지 모른다.
	//     방장도 등록해 둔다 — 다음 판에 참여자가 될 수 있고, 그때 다시 하려면
	//     또 대기실을 거쳐야 하기 때문이다.
	if (UServerSubsystem* Chat = GetServerSubsystem())
	{
		Chat->RegisterGameEndpoint(HostPort);
	}

	if (RoomLobbyWidget == nullptr)
	{
		RoomLobbyWidget = CreateRoomLobbyPage();
		PushPage(RoomLobbyWidget);
	}
	RefreshUI();

	OnEnteredWaitingRoom(RoomId, bIsHost);
}

void ULobbyWidgetBase::ReturnToMainMenu(bool bRoomClosed)
{
	UIState = EMOULobbyUIState::MainMenu;
	MyRoomPassword.Empty();
	JoinedRoomPassword.Empty();

	// ★ 방을 떠났으니 공유기에 열어둔 포트를 닫는다.
	//   방을 나가는 모든 경로(직접 나가기 / 방장 이탈로 쫓겨남)가 이 함수를 지나므로
	//   여기 한 곳에만 두면 빠뜨릴 일이 없다.
	//   참여자였다면 애초에 연 포트가 없어서 아무 일도 일어나지 않는다.
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UNatPortMappingSubsystem* Nat = GameInstance->GetSubsystem<UNatPortMappingSubsystem>())
		{
			Nat->ReleasePortMapping();
		}
	}

	// v5 까지는 여기서 예약해둔 여행 타이머를 취소했다. 이제 예약이 없다 —
	// 참여자는 방장의 리슨서버가 실제로 뜬 뒤에 오는 신호를 받고서야 떠나고,
	// 방을 나가면 서버가 그 신호를 보내지 않기 때문이다.

	PopToMainMenu();
	RefreshUI();

	OnLeftWaitingRoom(bRoomClosed);
}

void ULobbyWidgetBase::HandleChatStateChanged(EChatConnectionState NewState, const FString& /*Detail*/)
{
	// 연결이 끊기면 서브시스템이 방 상태를 비운다. 화면도 메인메뉴로 되돌린다.
	if (NewState == EChatConnectionState::Disconnected && UIState == EMOULobbyUIState::WaitingRoom)
	{
		ReturnToMainMenu(/*bRoomClosed=*/true);
		SetMessage(TEXT("서버와의 연결이 끊겨 방에서 나왔습니다."), true);
		return;
	}
	RefreshUI();
}

void ULobbyWidgetBase::HandleLoginCompleted(const FChatLoginResult& /*Result*/)
{
	RefreshUI();
}

void ULobbyWidgetBase::HandleRoomMembersChanged(int32 /*RoomId*/, const TArray<FMOURoomMember>& /*Members*/, bool /*bAllReady*/)
{
	// 명단 자체는 서브시스템이 들고 있다. 여기서는 다시 그리기만 한다.
	RefreshUI();
}

void ULobbyWidgetBase::HandleRoomClosed(int32 RoomId, EMOURoomCloseReasonBP /*Reason*/)
{
	if (UIState != EMOULobbyUIState::WaitingRoom)
	{
		return;
	}

	ReturnToMainMenu(/*bRoomClosed=*/true);
	SetMessage(FString::Printf(
		TEXT("방장이 나가서 방 #%d 이(가) 사라졌습니다."), RoomId), true);
}

void ULobbyWidgetBase::HandleGameStarted(const FMOURoomJoinResult& Host, bool bIsHost)
{
	const FString RoomPassword = bIsHost ? MyRoomPassword : JoinedRoomPassword;

	// 참여자에게는 "이동합니다" 가 아니라 "기다립니다" 라고 말해야 한다.
	// 실제로 떠나는 것은 HandleHostReady 이고, 그 사이에 몇 초가 흐를 수 있다.
	// 화면이 멈춘 것처럼 보이지 않게 지금 무엇을 기다리는지 적어준다.
	SetMessage(bIsHost
		? TEXT("게임을 시작합니다. 리슨서버를 엽니다...")
		: TEXT("게임이 시작됐습니다. 방장이 서버를 여는 중입니다..."), false);

	OnGameStarted(Host, bIsHost, RoomPassword);

	// ★ 여기서 더 이상 여행하지 않는다. (2026-08-29)
	//   방장의 OpenLevel 도, 참여자의 맵 미리 올리기도 전부 UServerSubsystem 이
	//   이 델리게이트를 쏜 직후에 이어서 한다. 이 위젯은 안내만 한다.
	//   자세한 이유는 UServerSubsystem::ConfigureTravel 위 주석 참고.
}

void ULobbyWidgetBase::HandleHostReady(const FMOURoomJoinResult& Host)
{
	// ★ 예전에는 여기에 "대기실이 아니면 return" 이 있었다. 지웠다. (2026-08-29)
	//   그 조건 때문에, 게임 시작과 함께 화면이 바뀌어 UIState 가 달라진 참여자는
	//   출발 신호를 받고도 조용히 무시했다. 이제 떠나는 일은 서브시스템이 하므로
	//   이 함수가 무엇을 하든 이동은 보장된다. 여기서는 안내만 한다.
	SetMessage(FString::Printf(TEXT("호스트 %s 로 이동합니다..."),
		*Host.ToDisplayString()), false);

	OnHostReady(Host, JoinedRoomPassword);

	// ClientTravel 은 UServerSubsystem::TravelToHost 가 한다.
	// bAutoTravelOnGameStart 를 껐다면 BP 가 원하는 시점에 그 함수를 부르면 된다.
}

// ---------------------------------------------------------------------------
// 스택 페이지 열고 닫기
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::OpenRoomCreate()
{
	if (UIState != EMOULobbyUIState::MainMenu || !IsTopPage(MainLobbyWidget))
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	UClass* WidgetClass = RoomCreateWidgetClass ? RoomCreateWidgetClass.Get() : URoomCreateWidgetBase::StaticClass();
	RoomCreateWidget = CreateWidget<URoomCreateWidgetBase>(PC, WidgetClass);
	if (RoomCreateWidget == nullptr)
	{
		SetMessage(TEXT("방 생성 창을 만들지 못했습니다."), true);
		return;
	}

	// 커서는 로비가 이미 관리하고 있다. 자식이 또 만지면 닫힐 때 커서가 사라진다.
	RoomCreateWidget->bManageMouseCursor = false;
	// 성공 뒤에도 스택 기록으로 남겨두고, 방에서 나갈 때 PopToMainMenu 한다.
	RoomCreateWidget->bRemoveOnSuccess = false;
	RoomCreateWidget->HostPort = HostPort;
	RoomCreateWidget->OnRoomCreateCancelled.BindUObject(this, &ULobbyWidgetBase::HandleRoomCreateCancelled);

	PushPage(RoomCreateWidget);
}

void ULobbyWidgetBase::OpenRoomList()
{
	if (UIState != EMOULobbyUIState::MainMenu || !IsTopPage(MainLobbyWidget))
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	UClass* WidgetClass = RoomListWidgetClass ? RoomListWidgetClass.Get() : URoomListWidgetBase::StaticClass();
	RoomListWidget = CreateWidget<URoomListWidgetBase>(PC, WidgetClass);
	if (RoomListWidget == nullptr)
	{
		SetMessage(TEXT("방 목록 창을 만들지 못했습니다."), true);
		return;
	}

	RoomListWidget->bManageMouseCursor = false;
	RoomListWidget->bRemoveOnSuccess = false;
	RoomListWidget->OnRoomListClosed.BindUObject(this, &ULobbyWidgetBase::HandleRoomListClosed);

	PushPage(RoomListWidget);
}

void ULobbyWidgetBase::OpenSettings()
{
	if (UIState != EMOULobbyUIState::MainMenu || !IsTopPage(MainLobbyWidget))
	{
		return;
	}
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}
	UClass* WidgetClass = SettingsWidgetClass ? SettingsWidgetClass.Get() : ULobbySettingsWidgetBase::StaticClass();
	SettingsWidget = CreateWidget<ULobbySettingsWidgetBase>(PC, WidgetClass);
	if (SettingsWidget == nullptr)
	{
		SetMessage(TEXT("환경설정 화면을 만들지 못했습니다."), true);
		return;
	}
	SettingsWidget->OnBack.BindUObject(this, &ULobbyWidgetBase::HandleSettingsClosed);
	PushPage(SettingsWidget);
}

bool ULobbyWidgetBase::NavigateBack()
{
	if (IsTopPage(RoomCreateWidget))
	{
		RoomCreateWidget->CancelCreate();
		return true;
	}
	if (IsTopPage(RoomListWidget))
	{
		RoomListWidget->CloseList();
		return true;
	}
	if (IsTopPage(SettingsWidget))
	{
		HandleSettingsClosed();
		return true;
	}
	if (IsTopPage(CustomizeWidget))
	{
		CustomizeWidget->CancelAndExit();
		return true;
	}
	if (IsTopPage(RoomLobbyWidget))
	{
		LeaveRoom();
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// 흐름 관리자와 자식 창에서 올라오는 결과
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::HandleFlowRoomEntered(int32 RoomId, bool bIsHost, const FString& RoomPassword)
{
	if (bIsHost)
	{
		MyRoomPassword = RoomPassword;
		JoinedRoomPassword.Empty();
	}
	else
	{
		JoinedRoomPassword = RoomPassword;
		MyRoomPassword.Empty();
	}

	EnterWaitingRoom(RoomId, bIsHost);
	SetMessage(bIsHost
		? TEXT("방을 열었습니다. 참여자가 모두 준비하면 시작할 수 있습니다.")
		: TEXT("방에 들어왔습니다. 준비를 누르면 방장이 시작할 수 있습니다."), false);
}

void ULobbyWidgetBase::HandleRoomCreateCancelled()
{
	if (IsTopPage(RoomCreateWidget))
	{
		PopPage();
	}
	RoomCreateWidget = nullptr;
}

void ULobbyWidgetBase::HandleRoomListClosed()
{
	if (IsTopPage(RoomListWidget))
	{
		PopPage();
	}
	RoomListWidget = nullptr;
}

void ULobbyWidgetBase::HandleSettingsClosed()
{
	if (IsTopPage(SettingsWidget))
	{
		PopPage();
	}
	SettingsWidget = nullptr;
}

void ULobbyWidgetBase::HandleCustomizeClosed()
{
	if (IsTopPage(CustomizeWidget))
	{
		PopPage();
	}
	CustomizeWidget = nullptr;
}

// ---------------------------------------------------------------------------
// 여행 실패 안내
//
// 여기서부터가 로비 서버의 관할 밖이다. 방 목록은 "주소록" 일 뿐이고,
// 실제 게임 트래픽은 참여자가 호스트의 리슨서버에 직접 붙어서 오간다.
//
// ★ 실제로 떠나는 코드는 2026-08-29 에 UServerSubsystem 으로 옮겼다.
//   위젯은 레벨 이동과 함께 파괴되므로, 여기서 출발 신호를 기다리면
//   게임 시작 시 위젯을 닫는 순간 아무도 안 떠난다.
//   이 파일에는 "사용자에게 무엇을 보여줄 것인가" 만 남는다.
// ---------------------------------------------------------------------------

void ULobbyWidgetBase::HandleTravelFailed(const FString& Reason)
{
	// 대기실에 있든 이미 메인메뉴로 돌아왔든 알려준다. 접속 실패는 어느
	// 화면에서 받든 사용자가 알아야 하는 정보다.
	SetMessage(Reason, /*bIsError=*/true);

	// 대기실에 남아 있으면 "이동합니다..." 상태로 굳어 있으므로 풀어준다.
	// 방 자체는 서버에 살아 있으니 메인메뉴로 쫓아내지는 않는다 —
	// 방장이 포워딩을 고치고 다시 시작하면 그대로 다시 붙을 수 있다.
	RefreshUI();
}

// ---------------------------------------------------------------------------
// 콘솔 명령 - 게임 플로우에 로비를 붙이기 전에 UI 를 검증하기 위한 것.
//
//   MOU.Lobby.Show   로비를 띄운다
//   MOU.Lobby.Hide   로비를 화면에서 제거한다
//
// 게임에서 정식으로 쓸 때는 로그인 성공 뒤에 자동으로 뜬다
// (ULoginWidgetBase 의 bShowLobbyWidgetOnSuccess).
// ---------------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
namespace
{
	/**
	 * 콘솔로 띄운 로비를 월드별로 기억한다. ChatWidgetBase 의 GDebugChatWidgets 와 같은 이유다 —
	 * PIE 창을 여러 개 띄우면 창마다 월드가 따로 생기므로 전역 하나로는 두 번째 창을 못 띄운다.
	 */
	TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<ULobbyWidgetBase>> GDebugLobbyWidgets;

	void PruneDebugLobbyWidgets()
	{
		for (auto It = GDebugLobbyWidgets.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || !It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GLobbyShowCommand(
		TEXT("MOU.Lobby.Show"),
		TEXT("로비를 띄운다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World)
			{
				PruneDebugLobbyWidgets();
				if (GDebugLobbyWidgets.Contains(World))
				{
					return;   // 이 창에는 이미 떠 있다
				}

				APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
				if (PC == nullptr)
				{
					return;
				}

				ULobbyWidgetBase* Widget = CreateWidget<ULobbyWidgetBase>(PC, ULobbyWidgetBase::StaticClass());
				if (Widget != nullptr)
				{
					Widget->AddToViewport();
					GDebugLobbyWidgets.Add(World, Widget);
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GLobbyHideCommand(
		TEXT("MOU.Lobby.Hide"),
		TEXT("로비를 화면에서 제거한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World)
			{
				PruneDebugLobbyWidgets();
				if (const TWeakObjectPtr<ULobbyWidgetBase>* Found = GDebugLobbyWidgets.Find(World))
				{
					if (ULobbyWidgetBase* Widget = Found->Get())
					{
						Widget->RemoveFromParent();
					}
				}
				GDebugLobbyWidgets.Remove(World);
			}));
}
#endif
