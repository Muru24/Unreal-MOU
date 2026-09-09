#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlipperySurface.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class ACharacter;

/**
 * ASlipperySurface
 * 빙판(Ice) 또는 기름(Oil Spill) 바닥을 나타내는 상시 환경 지형 액터입니다.
 * - 영역에 진입한 캐릭터의 지면 마찰력(Ground Friction)과 제동 감속도(Braking Deceleration)를 극감시켜 미끄러짐/썰매 관성을 부여합니다.
 * - 영역을 벗어나면 캐릭터의 원래 마찰력 및 제동력을 자동으로 복원합니다.
 */
UCLASS()
class TEAMPROJECT_MOU_API ASlipperySurface : public AActor
{
	GENERATED_BODY()
	
public:	
	ASlipperySurface();

	// ---------------------------------------------------------
	// [컴포넌트]
	// ---------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SurfaceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> SlipperyVolume;

	// ---------------------------------------------------------
	// [지면 마찰 설정]
	// ---------------------------------------------------------

	/** 미끄러운 지면 마찰력 (기본 8.0 -> 0.1로 감소) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slippery|Physics")
	float SlipperyGroundFriction = 0.1f;

	/** 제동 감속도 (Braking Deceleration, 기본 2048 -> 100으로 감소) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slippery|Physics")
	float SlipperyBrakingDeceleration = 100.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleSurfaceBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSurfaceEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void RestoreCharacterMovement(ACharacter* Character);

protected:
	UPROPERTY(Transient)
	TMap<TObjectPtr<ACharacter>, float> OriginalFrictionMap;

	UPROPERTY(Transient)
	TMap<TObjectPtr<ACharacter>, float> OriginalBrakingMap;
};
