#pragma once

#include "CoreMinimal.h"
#include "Base/GameplayAbilityBase.h"
#include "GA_HitReaction.generated.h"

class UAnimMontage;

/**
 * 피격 반응(Hit Reaction)을 담당하는 Gameplay Ability
 * - 500 미만의 가벼운 충격 또는 일반 투척물 충돌 시 발동
 * - 등록된 피격 몽타주 목록 중 50 : 50 확률(또는 균등 랜덤)로 무작위 선택하여 네트워크 복제 재생
 */
UCLASS()
class TEAMPROJECT_MOU_API UGA_HitReaction : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UGA_HitReaction();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/* 피격 몽타주 목록 (2개 등록 시 50 : 50 확률로 무작위 재생) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReaction")
	TArray<TObjectPtr<UAnimMontage>> HitReactionMontages;

	/* 몽타주 재생에 실패하거나 없을 때 사용할 기본 지속시간 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReaction", meta = (ClampMin = "0.1"))
	float FallbackDuration = 0.5f;

	FTimerHandle FallbackTimerHandle;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	void OnFallbackDurationExpired();
};
