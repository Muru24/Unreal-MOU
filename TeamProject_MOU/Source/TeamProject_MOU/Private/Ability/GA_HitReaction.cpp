#include "Ability/GA_HitReaction.h"
#include "Player/MainCharacter.h"
#include "Components/CharacterVisualComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"

UGA_HitReaction::UGA_HitReaction()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// 태그 설정
	FGameplayTag HitReactionTag = FGameplayTag::RequestGameplayTag(FName("Ability.Player.HitReaction"), false);
	FGameplayTagContainer AssetTagsContainer;
	if (HitReactionTag.IsValid())
	{
		AssetTagsContainer.AddTag(HitReactionTag);
	}
	SetAssetTags(AssetTagsContainer);

	// 이미 사망, 그로기, 넉다운, 기절 상태인 경우 피격 애니메이션 발동 차단
	FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("State.Player.Dead"), false);
	FGameplayTag GroggyTag = FGameplayTag::RequestGameplayTag(FName("State.Player.Groggy"), false);
	FGameplayTag KnockdownTag = FGameplayTag::RequestGameplayTag(FName("State.Player.Knockdown"), false);
	FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName("State.Stunned"), false);

	if (DeadTag.IsValid()) ActivationBlockedTags.AddTag(DeadTag);
	if (GroggyTag.IsValid()) ActivationBlockedTags.AddTag(GroggyTag);
	if (KnockdownTag.IsValid()) ActivationBlockedTags.AddTag(KnockdownTag);
	if (StunnedTag.IsValid()) ActivationBlockedTags.AddTag(StunnedTag);

	// 어빌리티 실행 중 캐릭터에게 부여되는 태그 (모든 클라이언트로 자동 복제되어 표정 및 피격 상태 동기화)
	FGameplayTag HitStateTag = FGameplayTag::RequestGameplayTag(FName("State.Player.HitReaction"), false);
	if (HitStateTag.IsValid())
	{
		ActivationOwnedTags.AddTag(HitStateTag);
	}
}

void UGA_HitReaction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AMainCharacter* MainChar = Cast<AMainCharacter>(GetCharacterFromActorInfo());
	if (!MainChar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 1. 등록된 피격 몽타주 중 50 : 50 (무작위 균등) 확률로 1개 선택
	UAnimMontage* SelectedMontage = nullptr;
	TArray<UAnimMontage*> ValidMontages;
	for (const TObjectPtr<UAnimMontage>& Montage : HitReactionMontages)
	{
		if (Montage)
		{
			ValidMontages.Add(Montage.Get());
		}
	}

	if (ValidMontages.Num() > 0)
	{
		int32 SelectedIndex = FMath::RandRange(0, ValidMontages.Num() - 1);
		SelectedMontage = ValidMontages[SelectedIndex];
	}

	// 2. 네트워크 복제 몽타주 태스크 실행
	float AnimDuration = FallbackDuration;
	if (SelectedMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			SelectedMontage,
			1.0f,
			NAME_None,
			false,
			1.0f);

		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UGA_HitReaction::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_HitReaction::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UGA_HitReaction::OnMontageInterrupted);
			MontageTask->ReadyForActivation();
		}

		if (SelectedMontage->GetPlayLength() > 0.0f)
		{
			AnimDuration = SelectedMontage->GetPlayLength();
		}
	}

	// 3. 얼굴 표정 오버라이드
	if (UCharacterVisualComponent* VisualComp = MainChar->FindComponentByClass<UCharacterVisualComponent>())
	{
		static const FGameplayTag HitStateTag = FGameplayTag::RequestGameplayTag(FName("State.Player.HitReaction"), false);
		VisualComp->SetTagTemporaryOverride(HitStateTag, AnimDuration);
	}

	// 4. 서버에서 안전 타이머 가동 (몽타주 미지정 또는 네트워크 유실 대비)
	if (HasAuthority(&ActivationInfo))
	{
		GetWorld()->GetTimerManager().SetTimer(
			FallbackTimerHandle,
			this,
			&UGA_HitReaction::OnFallbackDurationExpired,
			AnimDuration,
			false);
	}
}

void UGA_HitReaction::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HitReaction::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_HitReaction::OnFallbackDurationExpired()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_HitReaction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FallbackTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
