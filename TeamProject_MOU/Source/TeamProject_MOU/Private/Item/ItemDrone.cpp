#include "Item/ItemDrone.h"
#include "Base/ItemBase.h"
#include "Components/CarryingComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

AItemDrone::AItemDrone()
{
	PrimaryActorTick.bCanEverTick = true;

	// [멀티플레이] 상태·위치 동기화. 위치는 서버 팔로우 결과를 그대로 복제한다.
	bReplicates = true;
	SetReplicateMovement(true);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	// 드론은 공중에 떠 있는 상태를 기본으로 하므로 물리 시뮬레이션은 끈다.
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetSimulatePhysics(false);
	// 플레이어를 막지 않도록 겹침만 처리 (원하면 BP에서 프로필 변경 가능)
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 아이템 거치 지점
	ItemHoldPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ItemHoldPoint"));
	ItemHoldPoint->SetupAttachment(MeshComponent);
}

void AItemDrone::BeginPlay()
{
	Super::BeginPlay();
}

void AItemDrone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AItemDrone, FollowTarget);
	DOREPLIFETIME(AItemDrone, StoredItem);
}

void AItemDrone::OnRep_FollowTarget()
{
	// 클라이언트에서 팔로우 대상이 바뀌었을 때의 훅 (현재 특별 처리는 없음)
}

// [DRONE-004] 팔로우 목표 위치 계산
FVector AItemDrone::CalcTargetLocation(float DeltaTime) const
{
	if (!FollowTarget)
	{
		return GetActorLocation();
	}

	// 플레이어 로컬 공간 기준으로 오프셋을 적용해 월드 위치를 구한다.
	const FTransform TargetTransform = FollowTarget->GetActorTransform();
	FVector Target = TargetTransform.TransformPosition(FollowOffset);

	// 위아래 보빙 (BobbingPhase는 Tick에서 누적)
	if (BobbingAmplitude > 0.0f)
	{
		Target.Z += FMath::Sin(BobbingPhase) * BobbingAmplitude;
	}

	return Target;
}

void AItemDrone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 팔로우는 서버(권위)에서만 계산하고, 결과 위치는 SetReplicateMovement로 클라에 전파된다.
	if (!HasAuthority() || !FollowTarget)
	{
		return;
	}

	BobbingPhase += DeltaTime * BobbingSpeed;

	const FVector TargetLoc = CalcTargetLocation(DeltaTime);
	const FVector NewLoc = FMath::VInterpTo(GetActorLocation(), TargetLoc, DeltaTime, FollowInterpSpeed);
	SetActorLocation(NewLoc);

	// 드론이 플레이어를 바라보도록 회전 (수평만)
	FVector LookDir = FollowTarget->GetActorLocation() - NewLoc;
	LookDir.Z = 0.0f;
	if (!LookDir.IsNearlyZero())
	{
		const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), LookDir.Rotation(), DeltaTime, FollowInterpSpeed);
		SetActorRotation(NewRot);
	}
}

// ---------------------------------------------------------
// [상호작용]
// ---------------------------------------------------------
bool AItemDrone::CanInteract_Implementation(AActor* Interactor) const
{
	ACharacter* Character = Cast<ACharacter>(Interactor);
	if (!Character)
	{
		return false;
	}

	UCarryingComponent* CarryingComp = Character->FindComponentByClass<UCarryingComponent>();
	if (!CarryingComp)
	{
		return false;
	}

	AItemBase* HandItem = Cast<AItemBase>(CarryingComp->GetCarriedActor());

	// 손에 (맡길 수 있는) 아이템이 있고 드론이 비어있으면 맡기기 가능,
	// 빈손이고 드론에 보관 아이템이 있으면 회수 가능.
	if (HandItem && !StoredItem)
	{
		return HandItem->bCanBeStoredInInventory && HandItem->CanBeDropped();
	}
	if (!HandItem && StoredItem)
	{
		return true;
	}

	return false;
}

void AItemDrone::Interact_Implementation(AActor* Interactor)
{
	ACharacter* Character = Cast<ACharacter>(Interactor);
	if (!Character)
	{
		return;
	}

	// 상호작용은 서버 권위에서만 상태를 변경한다.
	// (InteractionComponent/GA_Interact가 서버 흐름으로 Interact를 호출하는 구조)
	if (!HasAuthority())
	{
		return;
	}

	UCarryingComponent* CarryingComp = Character->FindComponentByClass<UCarryingComponent>();
	if (!CarryingComp)
	{
		return;
	}

	AItemBase* HandItem = Cast<AItemBase>(CarryingComp->GetCarriedActor());

	if (HandItem && !StoredItem)
	{
		StoreItemFromHand(Character);
	}
	else if (!HandItem && StoredItem)
	{
		RetrieveItemToHand(Character);
	}
}

FText AItemDrone::GetInteractPrompt_Implementation() const
{
	if (StoredItem)
	{
		return NSLOCTEXT("Interaction", "DroneRetrievePrompt", "드론에서 아이템 꺼내기");
	}
	return NSLOCTEXT("Interaction", "DroneStorePrompt", "드론에 아이템 맡기기");
}

// [DRONE-001] 손 -> 드론 (맡기기)
void AItemDrone::StoreItemFromHand(ACharacter* Interactor)
{
	UCarryingComponent* CarryingComp = Interactor->FindComponentByClass<UCarryingComponent>();
	if (!CarryingComp)
	{
		return;
	}

	AItemBase* HandItem = Cast<AItemBase>(CarryingComp->GetCarriedActor());
	if (!HandItem)
	{
		return;
	}

	// 첫 상호작용한 플레이어를 팔로우 대상으로 삼는다 (이미 지정돼 있으면 유지).
	if (!FollowTarget)
	{
		FollowTarget = Interactor;
	}

	// 손을 비운다 (CarryingComponent가 CarriedActor 복제/무게 갱신까지 처리).
	CarryingComp->ClearCarriedItem();

	// 드론에 보관 상태로 등록하고 거치 지점에 부착.
	StoredItem = HandItem;
	MulticastAttachToDrone(HandItem);
}

// [DRONE-002] 드론 -> 손 (회수)
void AItemDrone::RetrieveItemToHand(ACharacter* Interactor)
{
	if (!StoredItem)
	{
		return;
	}

	UCarryingComponent* CarryingComp = Interactor->FindComponentByClass<UCarryingComponent>();
	if (!CarryingComp)
	{
		return;
	}

	AItemBase* Item = StoredItem;
	StoredItem = nullptr;

	// 드론 거치에서 떼어낸다.
	Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 인벤토리에서 꺼낼 때와 동일하게, EquipItem(단순 부착) 전에 OnEquipped로
	// 표시/충돌 상태를 먼저 복구해야 BoundingBox 오프셋 계산과 렌더링이 정상 동작한다.
	// (UInventoryComponent::RequestSlotAction의 꺼내기 흐름과 동일)
	Item->MulticastOnEquipped(Interactor);

	// 손에 쥐어준다. EquipItem이 CarriedActor 설정·무게 갱신·MulticastEquipItem까지 담당.
	CarryingComp->EquipItem(Item);
}

// [DRONE-003] 아이템을 드론 거치 지점에 부착 (모든 클라 동기화)
void AItemDrone::MulticastAttachToDrone_Implementation(AItemBase* Item)
{
	if (!Item || !ItemHoldPoint)
	{
		return;
	}

	// 인벤토리 수납과 동일하게 충돌·물리를 비활성화한 뒤(단, 화면에는 보이도록) 부착한다.
	// OnUnequipped는 액터를 숨기므로 직접 상태를 세팅한다.
	Item->SetActorHiddenInGame(false);
	if (Item->MeshComponent)
	{
		Item->MeshComponent->SetSimulatePhysics(false);
		Item->MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Item->AttachToComponent(ItemHoldPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
}
