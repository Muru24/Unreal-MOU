#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "ItemDrone.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class AItemBase;
class ACharacter;

// ---------------------------------------------------------
// [아이템 드론]
// 플레이어가 손에 든 아이템을 F키로 맡겨두는 비행 드론.
// - 맡기기: 손에 아이템 O + 드론 비어있음 -> 드론 ItemHoldPoint에 거치
// - 회수  : 빈손        + 드론 아이템 O  -> 손으로 되돌려줌
// - 팔로우: 소유 플레이어의 오른쪽 어깨 옆을 목표로 부드럽게 따라감(시야 비방해)
// ---------------------------------------------------------
UCLASS()
class TEAMPROJECT_MOU_API AItemDrone : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AItemDrone();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void Tick(float DeltaTime) override;

	// ---------------------------------------------------------
	// [컴포넌트]
	// ---------------------------------------------------------
	// 드론 본체 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	// 맡긴 아이템이 거치될 위치 (요청하신 SceneComponent)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ItemHoldPoint;

	// ---------------------------------------------------------
	// [팔로우 설정]
	// ---------------------------------------------------------
	// 따라다닐 대상 플레이어. 첫 상호작용한 플레이어가 자동 지정되며, 배치 시 미리 지정도 가능.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_FollowTarget, Category = "Drone|Follow")
	TObjectPtr<ACharacter> FollowTarget;

	UFUNCTION()
	void OnRep_FollowTarget();

	// 플레이어 기준 목표 위치 오프셋 (로컬 공간). 기본값: 오른쪽 어깨 옆 + 약간 위.
	// X=뒤쪽(-), Y=오른쪽(+), Z=위(+)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone|Follow")
	FVector FollowOffset = FVector(-40.0f, 70.0f, 60.0f);

	// 목표 위치까지 따라가는 부드러움 (클수록 빠르게 붙음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone|Follow")
	float FollowInterpSpeed = 6.0f;

	// 위아래로 살짝 떠다니는 보빙 진폭 (0이면 비활성)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone|Follow")
	float BobbingAmplitude = 5.0f;

	// 보빙 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone|Follow")
	float BobbingSpeed = 2.0f;

	// ---------------------------------------------------------
	// [보관 상태]
	// ---------------------------------------------------------
	// 현재 드론이 보관 중인 아이템 (없으면 nullptr)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Drone|State")
	TObjectPtr<AItemBase> StoredItem;

	// ---------------------------------------------------------
	// [상호작용 인터페이스 (IInteractableInterface)]
	// ---------------------------------------------------------
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

private:
	// [DRONE-001] 손에 든 아이템을 드론에 맡긴다 (서버 전용 처리 흐름)
	void StoreItemFromHand(ACharacter* Interactor);

	// [DRONE-002] 드론이 보관 중인 아이템을 플레이어 손으로 되돌려준다 (서버 전용 처리 흐름)
	void RetrieveItemToHand(ACharacter* Interactor);

	// [DRONE-003] 아이템을 드론 거치 지점에 부착 (모든 클라 동기화)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAttachToDrone(AItemBase* Item);

	// [DRONE-004] 팔로우 목표 위치 계산 (플레이어 오프셋 + 보빙)
	FVector CalcTargetLocation(float DeltaTime) const;

	// 보빙 위상 누적용
	float BobbingPhase = 0.0f;
};
