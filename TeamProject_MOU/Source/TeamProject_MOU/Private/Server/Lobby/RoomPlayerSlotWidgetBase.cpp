#include "Server/Lobby/RoomPlayerSlotWidgetBase.h"
#include "Components/CharacterCustomizationComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void URoomPlayerSlotWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetMinDesiredWidth(200.f);
		Size->SetMinDesiredHeight(140.f);
		WidgetTree->RootWidget = Size;
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(FLinearColor(0.08f, 0.14f, 0.22f, 1.f));
		Background->SetPadding(FMargin(12.f));
		Size->AddChild(Background);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
		Background->AddChild(Layers);
		UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
		Empty->SetText(FText::FromString(TEXT("참가자 대기 중")));
		EmptyPanel = Empty;
		Layers->AddChild(Empty);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		OccupiedPanel = Content;
		Layers->AddChild(Content);
		NicknameText = WidgetTree->ConstructWidget<UTextBlock>();
		HostImage = WidgetTree->ConstructWidget<UImage>();
		ReadyImage = WidgetTree->ConstructWidget<UImage>();
		UTextBlock* Self = WidgetTree->ConstructWidget<UTextBlock>();
		Self->SetText(FText::FromString(TEXT("나")));
		SelfHighlight = Self;
		Content->AddChild(NicknameText);
		Content->AddChild(HostImage);
		Content->AddChild(ReadyImage);
		Content->AddChild(Self);
	}
	RefreshVisuals();
}

void URoomPlayerSlotWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	// Restore current member state after Blueprint PreConstruct/Construct and on reattachment.
	RefreshVisuals();
}

void URoomPlayerSlotWidgetBase::SetMember(const FMOURoomMember& InMember, bool bInIsSelf)
{
	if (bOccupied && Member.UserId == InMember.UserId && Member.Name == InMember.Name &&
		Member.bReady == InMember.bReady && Member.bIsHost == InMember.bIsHost &&
		Member.SlotIndex == InMember.SlotIndex && Member.Customization == InMember.Customization && bIsSelf == bInIsSelf)
	{
		return;
	}
	Member = InMember;
	bOccupied = true;
	bIsSelf = bInIsSelf;
	if (!PreviewComponent.IsValid()) FindPreviewActorForSlot(Member.SlotIndex);
	RefreshVisuals();
	OnSlotChanged();
}

void URoomPlayerSlotWidgetBase::ClearMember()
{
	if (!bOccupied) { return; }
	Member = FMOURoomMember();
	bOccupied = false;
	bIsSelf = false;
	bHasAppliedCustomization = false;
	RefreshVisuals();
	OnSlotChanged();
}

void URoomPlayerSlotWidgetBase::SetPreviewComponent(UCharacterCustomizationComponent* Component)
{
	PreviewComponent = Component;
	bHasAppliedCustomization = false;
	RefreshVisuals();
}

UCharacterCustomizationComponent* URoomPlayerSlotWidgetBase::GetOrCreatePreviewComponent(AActor* Actor)
{
	if (!Actor) return nullptr;
	auto* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh) return nullptr;
	auto* Component = Actor->FindComponentByClass<UCharacterCustomizationComponent>();
	if (!Component)
	{
		Component = NewObject<UCharacterCustomizationComponent>(Actor);
		Component->SetIsReplicated(false);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
	}
	Component->SetPreviewMesh(Mesh);
	return Component;
}

void URoomPlayerSlotWidgetBase::SetPreviewActor(AActor* Actor)
{
	if (auto* Component = GetOrCreatePreviewComponent(Actor)) SetPreviewComponent(Component);
}

AActor* URoomPlayerSlotWidgetBase::FindLobbyPreviewActor(const UObject* WorldContextObject, int32 SlotIndex)
{
	if (SlotIndex < 0 || !WorldContextObject) return nullptr;
	UClass* PreviewClass = LoadClass<AActor>(nullptr,
		TEXT("/Game/02_JSY/MainLobby/LobbyCharacter/BP_LobbyCharacterPreview.BP_LobbyCharacterPreview_C"));
	if (!PreviewClass) return nullptr;
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(WorldContextObject, PreviewClass, Actors);
	for (AActor* Actor : Actors)
	{
		if (!Actor) continue;
		const auto* Property = FindFProperty<FNumericProperty>(Actor->GetClass(), TEXT("PreviewSlotIndex"));
		if (Property && Property->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor)) == SlotIndex)
			return Actor;
	}
	return nullptr;
}

void URoomPlayerSlotWidgetBase::FindPreviewActorForSlot(int32 SlotIndex)
{
	SetPreviewActor(FindLobbyPreviewActor(this, SlotIndex));
}

void URoomPlayerSlotWidgetBase::RefreshVisuals()
{
	if (PreviewComponent.IsValid())
	{
		if (AActor* Actor = PreviewComponent->GetOwner()) Actor->SetActorHiddenInGame(!bOccupied);
		if (bOccupied && (!bHasAppliedCustomization || LastAppliedCustomization != Member.Customization))
		{
			PreviewComponent->ApplyPreview(Member.Customization);
			LastAppliedCustomization = Member.Customization;
			bHasAppliedCustomization = true;
		}
	}

	if (EmptyPanel) { EmptyPanel->SetVisibility(bOccupied ? ESlateVisibility::Collapsed : ESlateVisibility::Visible); }
	if (OccupiedPanel) { OccupiedPanel->SetVisibility(bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (NicknameText) { NicknameText->SetText(FText::FromString(Member.Name)); }
	if (HostImage)
	{
		HostImage->SetVisibility(bOccupied && Member.bIsHost
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (SelfHighlight) { SelfHighlight->SetVisibility(bOccupied && bIsSelf ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (ReadyImage)
	{
		ReadyImage->SetVisibility(bOccupied && !Member.bIsHost && Member.bReady
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
}
