#include "Server/Lobby/LobbyPageWidgetBase.h"
#include "Server/Lobby/RoomPlayerSlotWidgetBase.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Server/ServerSubsystem.h"
#include "Components/CharacterCustomizationComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UVerticalBox* BuildPagePanel(UWidgetTree* Tree, const TCHAR* RootName, const FVector2D& Size)
	{
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), RootName);
		Tree->RootWidget = Canvas;

		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.94f));
		Border->SetPadding(FMargin(20.f));
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Border);
		Slot->SetAnchors(FAnchors(0.5f));
		Slot->SetAlignment(FVector2D(0.5f));
		Slot->SetSize(Size);

		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Border->AddChild(Box);
		return Box;
	}

	void AddRow(UVerticalBox* Box, UWidget* Widget, float BottomPadding = 8.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Widget))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
		}
	}

	UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Name, const TCHAR* Text)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		AddRow(Box, Label);
		return Label;
	}

	UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Box, const TCHAR* Name, const TCHAR* Text, UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(FString(Name) + TEXT("Label")));
		Label->SetText(FText::FromString(Text));
		Button->AddChild(Label);
		AddRow(Box, Button);
		if (OutLabel != nullptr)
		{
			*OutLabel = Label;
		}
		return Button;
	}

	void SetMessageText(UTextBlock* Target, const FString& Text, bool bIsError)
	{
		if (Target == nullptr)
		{
			return;
		}
		Target->SetText(FText::FromString(Text));
		Target->SetColorAndOpacity(FSlateColor(bIsError
			? FLinearColor(1.f, 0.45f, 0.45f)
			: FLinearColor(0.75f, 0.75f, 0.75f)));
	}
}

void ULobbyMainWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbyMainWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (CreateRoomButton) { CreateRoomButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleCreateRoomClicked); }
	if (JoinRoomButton) { JoinRoomButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleJoinRoomClicked); }
	if (SettingsButton) { SettingsButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleSettingsClicked); }
	if (QuitGameButton) { QuitGameButton->OnClicked.AddUniqueDynamic(this, &ULobbyMainWidgetBase::HandleQuitGameClicked); }
}

void ULobbyMainWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbyMainRoot"), FVector2D(380.f, 440.f));
	TitleText = AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("MOU 로비"));
	StatusText = AddText(WidgetTree, Box, TEXT("StatusText"), TEXT("서버 상태 확인 중..."));
	CreateRoomButton = AddButton(WidgetTree, Box, TEXT("CreateRoomButton"), TEXT("방 만들기"));
	JoinRoomButton = AddButton(WidgetTree, Box, TEXT("JoinRoomButton"), TEXT("방 참여하기"));
	SettingsButton = AddButton(WidgetTree, Box, TEXT("SettingsButton"), TEXT("환경설정"));
	QuitGameButton = AddButton(WidgetTree, Box, TEXT("QuitGameButton"), TEXT("게임 종료"));
	MessageText = AddText(WidgetTree, Box, TEXT("MessageText"), TEXT(""));
}

void ULobbyMainWidgetBase::Refresh(const UServerSubsystem* Server)
{
	const bool bLoggedIn = Server != nullptr && Server->GetConnectionState() == EChatConnectionState::LoggedIn;
	if (CreateRoomButton) { CreateRoomButton->SetIsEnabled(bLoggedIn); }
	if (JoinRoomButton) { JoinRoomButton->SetIsEnabled(bLoggedIn); }
	if (StatusText)
	{
		FString Status = TEXT("서버에 연결되어 있지 않습니다.");
		if (Server != nullptr)
		{
			switch (Server->GetConnectionState())
			{
			case EChatConnectionState::LoggedIn: Status = FString::Printf(TEXT("%s 님으로 접속 중"), *Server->GetLoginResult().Name); break;
			case EChatConnectionState::Connected: Status = TEXT("서버에 연결됨. 로그인 대기 중..."); break;
			case EChatConnectionState::Connecting: Status = TEXT("서버에 연결하는 중..."); break;
			default: break;
			}
		}
		StatusText->SetText(FText::FromString(Status));
	}
}

void ULobbyMainWidgetBase::SetMessage(const FString& Text, bool bIsError) { SetMessageText(MessageText, Text, bIsError); }
void ULobbyMainWidgetBase::HandleCreateRoomClicked() { OnCreateRoom.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleJoinRoomClicked() { OnJoinRoom.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleSettingsClicked() { OnOpenSettings.ExecuteIfBound(); }
void ULobbyMainWidgetBase::HandleQuitGameClicked() { OnQuitGame.ExecuteIfBound(); }

void URoomLobbyWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void URoomLobbyWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	EnsurePlayerSlots();
	if (ReadyButton) { ReadyButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleReadyClicked); }
	if (StartGameButton) { StartGameButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleStartClicked); }
	if (CustomizeButton) { CustomizeButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleCustomizeClicked); }
	if (LeaveRoomButton) { LeaveRoomButton->OnClicked.AddUniqueDynamic(this, &URoomLobbyWidgetBase::HandleLeaveClicked); }
}

void URoomLobbyWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("RoomLobbyRoot"), FVector2D(560.f, 660.f));
	TitleText = AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("방 대기실"));
	StatusText = AddText(WidgetTree, Box, TEXT("StatusText"), TEXT(""));
	PlayerSlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("PlayerSlotGrid"));
	AddRow(Box, PlayerSlotGrid, 12.f);
	CastChecked<UVerticalBoxSlot>(PlayerSlotGrid->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* ReadyLabel = nullptr;
	UTextBlock* StartLabel = nullptr;
	ReadyButton = AddButton(WidgetTree, Box, TEXT("ReadyButton"), TEXT("준비하기"), &ReadyLabel);
	StartGameButton = AddButton(WidgetTree, Box, TEXT("StartGameButton"), TEXT("게임 시작"), &StartLabel);
	ReadyButtonLabel = ReadyLabel;
	StartGameButtonLabel = StartLabel;
	CustomizeButton = AddButton(WidgetTree, Box, TEXT("CustomizeButton"), TEXT("커스터마이징"));
	LeaveRoomButton = AddButton(WidgetTree, Box, TEXT("LeaveRoomButton"), TEXT("방 나가기"));
	MessageText = AddText(WidgetTree, Box, TEXT("MessageText"), TEXT(""));
}

void URoomLobbyWidgetBase::Refresh(const UServerSubsystem* Server)
{
	if (Server == nullptr)
	{
		return;
	}
	const bool bIsHost = Server->IsRoomHost();
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("방 #%d — %s"),
			Server->GetCurrentRoomId(), bIsHost ? TEXT("방장") : TEXT("참여자"))));
	}
	if (ReadyButton)
	{
		ReadyButton->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ReadyButton->SetIsEnabled(!bIsHost);
	}
	if (ReadyButtonLabel)
	{
		ReadyButtonLabel->SetText(FText::FromString(Server->IsSelfReady() ? TEXT("준비 해제") : TEXT("준비하기")));
	}
	if (StartGameButton)
	{
		StartGameButton->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		StartGameButton->SetIsEnabled(bIsHost && Server->AreAllMembersReady());
	}
	if (StartGameButtonLabel)
	{
		StartGameButtonLabel->SetText(FText::FromString(Server->AreAllMembersReady()
			? TEXT("게임 시작") : TEXT("게임 시작 (준비 대기 중)")));
	}
	RebuildMemberList(Server);
}

void URoomLobbyWidgetBase::EnsurePlayerSlots()
{
	if (!PlayerSlotGrid && MemberListBox)
	{
		PlayerSlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
		MemberListBox->ClearChildren();
		UVerticalBoxSlot* GridSlot = MemberListBox->AddChildToVerticalBox(PlayerSlotGrid);
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (!PlayerSlotGrid) { return; }
	if (PlayerSlots.Num() < 4)
	{
		PlayerSlotGrid->SetSlotPadding(FMargin(6.f));
		UClass* SlotClass = PlayerSlotWidgetClass ? PlayerSlotWidgetClass.Get() : URoomPlayerSlotWidgetBase::StaticClass();
		for (int32 Index = PlayerSlots.Num(); Index < 4; ++Index)
		{
			auto* PlayerSlot = CreateWidget<URoomPlayerSlotWidgetBase>(GetOwningPlayer(), SlotClass);
			if (!PlayerSlot) { return; }
			PlayerSlots.Add(PlayerSlot);
			UUniformGridSlot* GridSlot = PlayerSlotGrid->AddChildToUniformGrid(
				PlayerSlot, Index / FMath::Clamp(SlotColumns, 1, 4), Index % FMath::Clamp(SlotColumns, 1, 4));
			// Canvas-based WBP cards must receive the whole cell, not only their desired size.
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void URoomLobbyWidgetBase::RebuildMemberList(const UServerSubsystem* Server)
{
	if (!Server) { return; }
	EnsurePlayerSlots();
	const auto Members = Server->GetRoomMembers();
	for (int32 Index = 0; Index < PlayerSlots.Num(); ++Index)
	{
		const FMOURoomMember* Found = Members.FindByPredicate(
			[Index](const FMOURoomMember& Member) { return Member.SlotIndex == Index; });
		if (Found) { PlayerSlots[Index]->SetMember(*Found, Found->UserId == Server->GetLoginResult().UserId); }
		else { PlayerSlots[Index]->ClearMember(); }
	}
}

void URoomLobbyWidgetBase::SetMessage(const FString& Text, bool bIsError) { SetMessageText(MessageText, Text, bIsError); }
void URoomLobbyWidgetBase::HandleReadyClicked() { OnToggleReady.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleStartClicked() { OnStartGame.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleCustomizeClicked() { OnCustomize.ExecuteIfBound(); }
void URoomLobbyWidgetBase::HandleLeaveClicked() { OnLeaveRoom.ExecuteIfBound(); }

void ULobbySettingsWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbySettingsWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (BackButton) { BackButton->OnClicked.AddUniqueDynamic(this, &ULobbySettingsWidgetBase::HandleBackClicked); }
}

void ULobbySettingsWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbySettingsRoot"), FVector2D(420.f, 320.f));
	AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("환경설정"));
	AddText(WidgetTree, Box, TEXT("DescriptionText"), TEXT("WBP에서 설정 항목을 배치하세요."));
	BackButton = AddButton(WidgetTree, Box, TEXT("BackButton"), TEXT("뒤로가기"));
}

void ULobbySettingsWidgetBase::HandleBackClicked() { OnBack.ExecuteIfBound(); }

void ULobbyCustomizeWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		BuildDefaultLayout();
	}
}

void ULobbyCustomizeWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	ConnectOwnSlotPreview();
	if (BackButton) { BackButton->OnClicked.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleBackClicked); }
	if (ConfirmButton) { ConfirmButton->OnClicked.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleConfirmClicked); }
	if (ResetButton) { ResetButton->OnClicked.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleResetClicked); }
	if (auto* Server = UServerSubsystem::Get(this))
	{
		Server->OnLobbyCustomizationResult.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandleCustomizationResult);
		Server->OnRoomMembersChanged.AddUniqueDynamic(this, &ULobbyCustomizeWidgetBase::HandlePreviewRoomMembersChanged);
	}
}

void ULobbyCustomizeWidgetBase::ConnectOwnSlotPreview()
{
	// The room snapshot is authoritative for the local seat. Never default to slot 0.
	const UServerSubsystem* Server = UServerSubsystem::Get(this);
	if (!GetWorld() || !Server || Server->GetCurrentRoomId() == 0) return;
	const int64 SelfUserId = Server->GetLoginResult().UserId;
	const TArray<FMOURoomMember> Members = Server->GetRoomMembers();
	const FMOURoomMember* Self = Members.FindByPredicate(
		[SelfUserId](const FMOURoomMember& Member) { return Member.UserId == SelfUserId; });
	if (!Self || Self->SlotIndex < 0 || Self->SlotIndex > 3)
	{
		if (PreviewImage) PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		ShowStatus(FText::FromString(TEXT("내 슬롯 정보 수신을 기다리는 중...")), false);
		return;
	}

	// A designer-placed PreviewImage controls placement; a Canvas-only WBP gets a fallback.
	if (!PreviewImage)
	{
		if (auto* Canvas = Cast<UCanvasPanel>(WidgetTree ? WidgetTree->RootWidget : nullptr))
		{
			PreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewImage"));
			if (auto* CanvasSlot = Canvas->AddChildToCanvas(PreviewImage))
			{
				CanvasSlot->SetAnchors(FAnchors(0.02f, 0.12f, 0.38f, 0.92f));
				CanvasSlot->SetOffsets(FMargin(0));
				CanvasSlot->SetZOrder(-1);
			}
		}
	}
	if (!PreviewImage)
	{
		ShowStatus(FText::FromString(TEXT("PreviewImage가 없습니다. WBP에 Image를 추가하세요.")), false);
		return;
	}
	PreviewImage->SetVisibility(ESlateVisibility::Collapsed);

	const int32 SlotIndex = Self->SlotIndex;
	const FString TargetPath = FString::Printf(
		TEXT("/Game/02_JSY/MainLobby/LobbyCharacter/RenderTarget/RT_LobbySlot%d.RT_LobbySlot%d"),
		SlotIndex, SlotIndex);
	UTextureRenderTarget2D* SlotTarget = LoadObject<UTextureRenderTarget2D>(nullptr, *TargetPath);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/02_JSY/MainLobby/LobbyCharacter/M_UI_LobbyCharacter.M_UI_LobbyCharacter"));
	AActor* SlotActor = URoomPlayerSlotWidgetBase::FindLobbyPreviewActor(this, SlotIndex);
	USceneCaptureComponent2D* Capture = SlotActor ? SlotActor->FindComponentByClass<USceneCaptureComponent2D>() : nullptr;
	if (!SlotTarget || !Material || !SlotActor || !Capture)
	{
		ShowStatus(FText::FromString(FString::Printf(
			TEXT("슬롯 %d의 RenderTarget 또는 프리뷰 액터를 찾지 못했습니다."), SlotIndex)), false);
		return;
	}
	UCharacterCustomizationComponent* Component =
		URoomPlayerSlotWidgetBase::GetOrCreatePreviewComponent(SlotActor);
	if (!Component)
	{
		ShowStatus(FText::FromString(TEXT("내 슬롯 프리뷰에 SkeletalMesh가 없습니다.")), false);
		return;
	}

	if (PreviewCapture.IsValid() && PreviewCapture.Get() != Capture)
		PreviewCapture->bCaptureEveryFrame = bPreviewCaptureEveryFrameBeforeEdit;
	LocalSlotIndex = SlotIndex;
	PreviewRenderTarget = SlotTarget;
	if (PreviewCapture.Get() != Capture)
	{
		PreviewCapture = Capture;
		bPreviewCaptureEveryFrameBeforeEdit = Capture->bCaptureEveryFrame;
	}
	Capture->TextureTarget = SlotTarget;
	Capture->bCaptureEveryFrame = true;
	Capture->Activate(true);
	SlotActor->SetActorHiddenInGame(false);
	if (!PreviewUIMaterial) PreviewUIMaterial = UMaterialInstanceDynamic::Create(Material, this);
	// M_UI_LobbyCharacter's texture parameter is PortraitRT. Its default value happens
	// to be RT_LobbySlot0, so using the asset name as the parameter silently showed the host.
	PreviewUIMaterial->SetTextureParameterValue(TEXT("PortraitRT"), SlotTarget);
	PreviewImage->SetBrushFromMaterial(PreviewUIMaterial);
	PreviewImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetPreviewComponent(Component);
	Capture->CaptureScene();
	ShowStatus(FText::GetEmpty(), true);
}

void ULobbyCustomizeWidgetBase::HandlePreviewRoomMembersChanged(
	int32 RoomId, const TArray<FMOURoomMember>& /*Members*/, bool /*bAllReady*/)
{
	if (!PreviewComponent.IsValid())
	{
		if (const UServerSubsystem* Server = UServerSubsystem::Get(this))
			if (RoomId == Server->GetCurrentRoomId()) ConnectOwnSlotPreview();
	}
}

void ULobbyCustomizeWidgetBase::BuildDefaultLayout()
{
	UVerticalBox* Box = BuildPagePanel(WidgetTree, TEXT("LobbyCustomizeRoot"), FVector2D(520.f, 420.f));
	AddText(WidgetTree, Box, TEXT("TitleText"), TEXT("커스터마이징"));
	AddText(WidgetTree, Box, TEXT("DescriptionText"), TEXT("몸 색상 / 금속성 / 거칠기 A·B / 데칼 / 데칼 색상 / 반복 X·Y"));
	ConfirmButton = AddButton(WidgetTree, Box, TEXT("ConfirmButton"), TEXT("적용 및 저장"));
	ResetButton = AddButton(WidgetTree, Box, TEXT("ResetButton"), TEXT("기본값"));
	CustomizationStatusText = AddText(WidgetTree, Box, TEXT("CustomizationStatusText"), TEXT(""));
	BackButton = AddButton(WidgetTree, Box, TEXT("BackButton"), TEXT("뒤로가기"));
}

void ULobbyCustomizeWidgetBase::HandleBackClicked() { CancelAndExit(); }
void ULobbyCustomizeWidgetBase::HandleConfirmClicked() { ConfirmAndSave(); }
void ULobbyCustomizeWidgetBase::HandleResetClicked() { if (!bWaitingForConfirmation) ResetToDefault(); }

void ULobbyCustomizeWidgetBase::InitializeCustomization()
{
	if (!EditingDataAsset) EditingDataAsset = NewObject<UCustomizationDataAsset>(this);
	if (auto* Server = UServerSubsystem::Get(this)) CurrentData = Server->GetLocalCustomization();
	OriginalData = CurrentData;
	OnCustomizationDataInitialized(CurrentData);
	UpdatePreview();
}

void ULobbyCustomizeWidgetBase::SetPreviewComponent(UCharacterCustomizationComponent* Component)
{
	if (PreviewComponent.IsValid()) PreviewComponent->ApplyPreview(OriginalData);
	PreviewComponent = Component;
	UpdatePreview();
}

void ULobbyCustomizeWidgetBase::UpdatePreview()
{
	if (bWaitingForConfirmation) return;
	if (PreviewComponent.IsValid()) PreviewComponent->ApplyPreview(CurrentData);
	if (PreviewCapture.IsValid()) PreviewCapture->CaptureScene();
	OnCustomizationPreviewChanged(CurrentData);
}

void ULobbyCustomizeWidgetBase::RotateCharacter(float DeltaX)
{
	if (PreviewComponent.IsValid() && PreviewComponent->GetOwner())
		PreviewComponent->GetOwner()->AddActorLocalRotation(FRotator(0, DeltaX * DragRotationSpeed, 0));
}

void ULobbyCustomizeWidgetBase::ShowStatus(const FText& Message, bool bSuccess)
{
	if (CustomizationStatusText) CustomizationStatusText->SetText(Message);
	OnCustomizationStatus(Message, bSuccess);
}

void ULobbyCustomizeWidgetBase::ConfirmAndSave()
{
	if (bWaitingForConfirmation) return;
	if (LocalSlotIndex == INDEX_NONE || !PreviewComponent.IsValid() || !PreviewRenderTarget)
	{
		ShowStatus(FText::FromString(TEXT("내 슬롯 미리보기가 준비되지 않았습니다. 슬롯 정보와 RenderTarget을 확인하세요.")), false);
		return;
	}
	CloseColorPickers();
	auto* Server = UServerSubsystem::Get(this);
	if (!Server || !Server->SubmitCustomization(CurrentData))
	{
		ShowStatus(FText::FromString(TEXT("외형을 전송할 수 없습니다. 연결/입장 상태 또는 진행 중인 요청을 확인하세요.")), false);
		return;
	}
	bWaitingForConfirmation = true;
	SetIsEnabled(false);
	ShowStatus(FText::FromString(TEXT("외형 적용 중...")), false);
}

void ULobbyCustomizeWidgetBase::HandleCustomizationResult(bool bSuccess, bool bSavedToDisk)
{
	if (!bWaitingForConfirmation) return;
	bWaitingForConfirmation = false;
	SetIsEnabled(true);
	if (!bSuccess)
	{
		ShowStatus(FText::FromString(TEXT("외형 적용 실패. 방 상태/서버 연결을 확인한 뒤 다시 시도하세요.")), false);
		return;
	}
	if (auto* Server = UServerSubsystem::Get(this)) CurrentData = Server->GetLocalCustomization();
	OriginalData = CurrentData;
	UpdatePreview();
	if (!bSavedToDisk)
	{
		ShowStatus(FText::FromString(TEXT("외형은 적용되었습니다. 디스크 저장에 실패하여 다음 실행에는 유지되지 않을 수 있습니다.")), true);
		return;
	}
	OnBack.ExecuteIfBound();
}

void ULobbyCustomizeWidgetBase::CancelAndExit()
{
	if (bWaitingForConfirmation) return;
	CloseColorPickers();
	CurrentData = OriginalData;
	UpdatePreview();
	OnBack.ExecuteIfBound();
}

void ULobbyCustomizeWidgetBase::NativeDestruct()
{
	if (auto* Server = UServerSubsystem::Get(this))
	{
		Server->OnLobbyCustomizationResult.RemoveDynamic(this, &ULobbyCustomizeWidgetBase::HandleCustomizationResult);
		Server->OnRoomMembersChanged.RemoveDynamic(this, &ULobbyCustomizeWidgetBase::HandlePreviewRoomMembersChanged);
		if (PreviewComponent.IsValid()) PreviewComponent->ApplyPreview(Server->GetLocalCustomization());
	}
	if (PreviewCapture.IsValid())
	{
		PreviewCapture->CaptureScene();
		PreviewCapture->bCaptureEveryFrame = bPreviewCaptureEveryFrameBeforeEdit;
	}
	PreviewCapture.Reset();
	PreviewComponent.Reset();
	PreviewRenderTarget = nullptr;
	PreviewUIMaterial = nullptr;
	Super::NativeDestruct();
}
