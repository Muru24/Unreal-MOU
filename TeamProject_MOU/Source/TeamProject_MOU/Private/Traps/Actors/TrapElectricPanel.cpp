#include "Traps/Actors/TrapElectricPanel.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "Traps/Components/TrapTriggerComponent.h"
#include "Traps/Components/TrapPayloadComponent.h"
#include "Traps/Data/TrapDataAsset.h"
#include "Components/StatusComponent.h"
#include "Base/CharacterBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

ATrapElectricPanel::ATrapElectricPanel()
{
	PanelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PanelMesh"));
	PanelMesh->SetupAttachment(RootScene);

	if (BaseTriggerBox)
	{
		BaseTriggerBox->SetupAttachment(PanelMesh);
		BaseTriggerBox->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
		BaseTriggerBox->InitBoxExtent(FVector(100.0f, 100.0f, 40.0f));
	}

	SparkFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SparkFX"));
	SparkFX->SetupAttachment(PanelMesh);
	SparkFX->SetRelativeLocation(FVector(0.0f, 0.0f, 15.0f));
	SparkFX->bAutoActivate = false;

	if (PayloadComponent)
	{
		PayloadComponent->DefaultHazardType = ETrapHazardType::ElectricShock;
	}

	if (TriggerComponent)
	{
		TriggerComponent->TriggerType = ETrapTriggerType::PressurePlate;
		TriggerComponent->PeriodicInterval = 4.0f;
	}

	ElectricActiveDuration = 3.0f;
	ShockTickInterval = 0.2f;
}

void ATrapElectricPanel::BeginPlay()
{
	Super::BeginPlay();
}

void ATrapElectricPanel::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		ClearAllElectrocutedActors();
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(ShockTickTimerHandle);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ATrapElectricPanel::OnStateEntered(ETrapState NewState)
{
	Super::OnStateEntered(NewState);

	// 1. 나이아가라 이펙트(SparkFX) 상태 동기화 (서버 및 모든 클라이언트)
	if (SparkFX)
	{
		if (NewState == ETrapState::Active || NewState == ETrapState::Warning)
		{
			SparkFX->Activate(true);
		}
		else
		{
			SparkFX->Deactivate();
		}
	}

	// 2. 서버 권한 상태별 타이머 및 감전 로직
	if (HasAuthority())
	{
		if (NewState == ETrapState::Active)
		{
			// n초 동안 Active 상태를 유지하도록 부모의 StateTimerHandle 재설정
			if (GetWorld())
			{
				float Duration = ElectricActiveDuration > 0.0f ? ElectricActiveDuration : (TrapData ? TrapData->ActiveDuration : 1.0f);
				GetWorld()->GetTimerManager().SetTimer(StateTimerHandle, this, &ATrapElectricPanel::OnActiveTimerExpired, Duration, false);

				DamagedActors.Empty();
				ElectrocutedActors.Empty();

				// 즉시 1차 오버랩 대상 감전 적용
				TickActiveElectricShock();

				// 활성화 기간 동안 주기적으로 오버랩 검사 가동 (지속 감전 태그 부여 및 새로 들어온 대상 처리)
				GetWorld()->GetTimerManager().SetTimer(ShockTickTimerHandle, this, &ATrapElectricPanel::TickActiveElectricShock, ShockTickInterval, true);
			}
		}
		else if (NewState == ETrapState::Cooldown || NewState == ETrapState::Idle || NewState == ETrapState::Disarmed)
		{
			if (GetWorld())
			{
				GetWorld()->GetTimerManager().ClearTimer(ShockTickTimerHandle);
			}

			// 활성화 종료 시 감전 태그 일괄 회수 -> 정상 보행 복귀
			ClearAllElectrocutedActors();
		}
	}
}

void ATrapElectricPanel::ExecuteTrapPayload()
{
	if (HasAuthority())
	{
		TickActiveElectricShock();
	}
}

void ATrapElectricPanel::TickActiveElectricShock()
{
	if (!HasAuthority() || CurrentState != ETrapState::Active || !GetWorld())
	{
		return;
	}

	TArray<AActor*> OverlappedActors;
	if (BaseTriggerBox)
	{
		BaseTriggerBox->GetOverlappingActors(OverlappedActors);
	}
	else if (TriggerComponent)
	{
		OverlappedActors = TriggerComponent->GetOverlappingActors();
	}

	for (AActor* Target : OverlappedActors)
	{
		if (Target && Target != this)
		{
			ProcessTargetElectricShock(Target);
		}
	}
}

void ATrapElectricPanel::ProcessTargetElectricShock(AActor* TargetActor)
{
	if (!TargetActor || TargetActor == this)
	{
		return;
	}

	ACharacterBase* TargetChar = Cast<ACharacterBase>(TargetActor);
	if (!TargetChar)
	{
		return;
	}

	TWeakObjectPtr<AActor> WeakTarget(TargetActor);

	// 1. 대미지는 활성화 1회당 대상별 최초 1회만 인가 (다단히트 즉사 방지)
	if (!DamagedActors.Contains(WeakTarget))
	{
		DamagedActors.Add(WeakTarget);

		if (PayloadComponent)
		{
			PayloadComponent->ExecutePayloadOnActor(TargetActor, TrapData);
		}
	}

	// 2. 활성화(Active) 기간 동안 대상에게 지속적으로 감전 태그 부여 및 유지 (길막 속박)
	if (UStatusComponent* StatusComp = TargetChar->GetStatusComponent())
	{
		static const FGameplayTag ElectricTag = FGameplayTag::RequestGameplayTag(FName("State.ElectricShock"), false);
		static const FGameplayTag CCElectricTag = FGameplayTag::RequestGameplayTag(FName("State.CC.Electric"), false);

		if (ElectricTag.IsValid() && !StatusComp->HasStatusTag(ElectricTag))
		{
			StatusComp->AddStatusTag(ElectricTag);
		}
		if (CCElectricTag.IsValid() && !StatusComp->HasStatusTag(CCElectricTag))
		{
			StatusComp->AddStatusTag(CCElectricTag);
		}

		ElectrocutedActors.Add(WeakTarget);
	}
}

void ATrapElectricPanel::ClearAllElectrocutedActors()
{
	static const FGameplayTag ElectricTag = FGameplayTag::RequestGameplayTag(FName("State.ElectricShock"), false);
	static const FGameplayTag CCElectricTag = FGameplayTag::RequestGameplayTag(FName("State.CC.Electric"), false);

	for (const TWeakObjectPtr<AActor>& WeakActor : ElectrocutedActors)
	{
		if (WeakActor.IsValid())
		{
			if (ACharacterBase* Char = Cast<ACharacterBase>(WeakActor.Get()))
			{
				if (UStatusComponent* StatusComp = Char->GetStatusComponent())
				{
					if (ElectricTag.IsValid() && StatusComp->HasStatusTag(ElectricTag))
					{
						StatusComp->RemoveStatusTag(ElectricTag);
					}
					if (CCElectricTag.IsValid() && StatusComp->HasStatusTag(CCElectricTag))
					{
						StatusComp->RemoveStatusTag(CCElectricTag);
					}
				}
			}
		}
	}

	ElectrocutedActors.Empty();
	DamagedActors.Empty();
}
