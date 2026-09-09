#pragma once

#include "CoreMinimal.h"
#include "Base/EventObjectBase.h"
#include "Gimmick/SluiceGate.h"
#include "SluiceWheel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSluiceWheelTurned, float, CurrentProgress, float, DeltaAngle);

UCLASS()
class TEAMPROJECT_MOU_API ASluiceWheel : public AEventObjectBase
{
	GENERATED_BODY()

public:
	ASluiceWheel();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BasePlatformMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RotatingWheelMesh;

	// 손잡이 메쉬 컴포넌트 (지정하지 않으면 RotatingWheelMesh 자식 중 'Handle' 컴포넌트 자동 탐색)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	// --- Target Sluice Gate ---
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SluiceWheel|Target")
	TObjectPtr<ASluiceGate> TargetGate;

	// --- Wheel Rotation Settings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SluiceWheel|Settings")
	float FullTurnsForFullOpen = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SluiceWheel|Settings")
	float TurnSpeedDegreesPerSec = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SluiceWheel|Settings")
	bool bStartFullyOpen = true;

	// --- Handle & Grip Settings ---
	// 손잡이 바의 로컬 연장 축 (기본값: Y축)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SluiceWheel|Handle")
	TEnumAsByte<EAxis::Type> HandleBarAxis = EAxis::Y;

	// 손잡이 중심으로부터 양쪽 손잡이까지의 거리 (기본값: 130cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SluiceWheel|Handle")
	float HandleRadius = 130.0f;

	// 손잡이 바와 캐릭터 사이의 간격 (기본값: 45cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SluiceWheel|Handle")
	float PlayerDistanceToBar = 45.0f;

	// 바라보는 방향으로 W키 전진 입력을 주었을 때만 휠 회전 허용 여부 (기본값: true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SluiceWheel|Handle")
	bool bPushOnlyWithW = true;

	// --- State & Replication ---
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTurnProgress, BlueprintReadOnly, Category = "SluiceWheel|State")
	float CurrentTurnProgress = 1.0f;

	UFUNCTION()
	void OnRep_CurrentTurnProgress();

	UFUNCTION(BlueprintCallable, Category = "SluiceWheel")
	void SetTurnProgress(float NewProgress);

	UFUNCTION(Server, Reliable)
	void ServerSetTurnProgress(float NewProgress);

	UFUNCTION(BlueprintPure, Category = "SluiceWheel")
	float GetTurnProgress() const { return CurrentTurnProgress; }

	UPROPERTY(BlueprintAssignable, Category = "SluiceWheel|Events")
	FOnSluiceWheelTurned OnWheelTurned;

	UFUNCTION(BlueprintImplementableEvent, Category = "SluiceWheel|Events")
	void OnWheelRotated(float Progress, float CurrentYaw);

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	virtual void AddPusher(AMainCharacter* Pusher) override;
	virtual void RemovePusher(AMainCharacter* Pusher) override;

	// 주어진 캐릭터 위치 기준 가장 가까운 손잡이 잡기 트랜스폼(위치, 접선 회전, 접선 벡터, CCW 여부) 계산
	bool CalculateGripTransform(const FVector& PusherLocation, const FVector& PusherForward, FVector& OutSnapLocation, FRotator& OutSnapRotation, FVector& OutTangent, bool& bOutIsCCW) const;

	// 푸셔를 정밀 손잡이 위치로 스냅
	void SnapPusherToHandle(AMainCharacter* Pusher);

protected:
	UPROPERTY(Transient)
	float VisualWheelYaw = 0.0f;

	// 각 푸셔가 시계 반대방향(CCW)으로 밀고 있는지 여부 추적
	UPROPERTY(Transient)
	TMap<TObjectPtr<AMainCharacter>, bool> PusherIsCCWMap;

	// 각 푸셔가 잡았을 때의 중심축 기준 고정 반경(Radius) 추적 (원심력 밀림 방지)
	UPROPERTY(Transient)
	TMap<TObjectPtr<AMainCharacter>, float> PusherLockedRadiusMap;

	void UpdateWheelVisuals(float DeltaTime);
};
