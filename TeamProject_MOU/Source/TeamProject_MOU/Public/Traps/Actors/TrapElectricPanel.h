#pragma once

#include "CoreMinimal.h"
#include "Traps/Actors/TrapBase.h"
#include "TrapElectricPanel.generated.h"

class UStaticMeshComponent;
class UNiagaraComponent;

/**
 * ATrapElectricPanel
 * 바닥 패널에서 일정 시간(n초) 동안 고전압 전기를 방전하여 길을 막는 전기 함정입니다.
 * - 발동(Active) 단계: 설정된 ElectricActiveDuration(n초) 동안 푸른 번개 방전(SparkFX) 지속.
 * - 범위 내 대상에게 최초 1회 대미지 + 손전등 배터리 방전 + 물품 강제 드랍.
 * - 발동 중인 n초 동안 범위 내 대상에게 지속적으로 감전(State.ElectricShock) 태그를 부여하여 이동을 차단(길막).
 * - 발동 종료 시 감전 태그를 안전하게 회수하여 정상 이동 복구.
 */
UCLASS()
class TEAMPROJECT_MOU_API ATrapElectricPanel : public ATrapBase
{
	GENERATED_BODY()

public:
	ATrapElectricPanel();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PanelMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> SparkFX;

	// ---------------------------------------------------------
	// [전기 함정 고유 설정]
	// ---------------------------------------------------------

	/** 방전(Active) 유지 시간 (초) - 이 시간 동안 지속적으로 길을 막고 SparkFX가 켜져 있습니다 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Electric|Config", meta = (ClampMin = "0.5"))
	float ElectricActiveDuration = 3.0f;

	/** 발동 중 오버랩 대상 감전 갱신 주기 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Electric|Config", meta = (ClampMin = "0.05"))
	float ShockTickInterval = 0.2f;

	/** 발동 시 손전등 배터리 방전량 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Electric|Config")
	float BatteryDrain = 30.0f;

	/** 비/물/오일 지형 연계 시 방전 범위 확장 배율 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Electric|Config")
	float WetZoneRadiusMultiplier = 1.5f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnStateEntered(ETrapState NewState) override;
	virtual void ExecuteTrapPayload() override;

	/** 활성화(Active) 상태 동안 주기적으로 오버랩 액터들을 감전 및 1회 대미지 처리 */
	void TickActiveElectricShock();

	/** 단일 대상 감전 처리 */
	void ProcessTargetElectricShock(AActor* TargetActor);

	/** 활성화 종료 시 감전 상태를 일괄 해제 */
	void ClearAllElectrocutedActors();

protected:
	FTimerHandle ShockTickTimerHandle;

	/** 이번 활성화 주기에서 이미 대미지를 입은 액터 목록 (1회 피해 보장) */
	TSet<TWeakObjectPtr<AActor>> DamagedActors;

	/** 현재 감전 상태(State.ElectricShock)가 부여되어 유지 중인 액터 목록 */
	TSet<TWeakObjectPtr<AActor>> ElectrocutedActors;
};
