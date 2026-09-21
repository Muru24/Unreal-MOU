#include "UI/MOU_CharacterStatusHUD.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "Math/UnrealMathUtility.h"
#include "Player/MainCharacter.h"
#include "Base/BaseAttributeSet.h"
#include "GameFramework/PlayerController.h"

void UMOU_CharacterStatusHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Initial State Update
	SetPortraitTextureByState(ECharacterStatusState::Happy);
}

void UMOU_CharacterStatusHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateHUDSway(InDeltaTime);

	if (BoundCharacter.IsValid())
	{
		if (UBaseAttributeSet* Attr = BoundCharacter->BaseAttribute)
		{
			const float MaxHP = FMath::Max(1.0f, Attr->GetMaxHealth());
			const float CurrentHP = FMath::Clamp(Attr->GetHealth(), 0.0f, MaxHP);
			SetTargetHP(CurrentHP / MaxHP);

			const float MaxStamina = FMath::Max(1.0f, Attr->GetMaxStemina());
			const float CurrentStamina = FMath::Clamp(Attr->GetStemina(), 0.0f, MaxStamina);
			SetTargetStamina(CurrentStamina / MaxStamina);
		}
	}

	bool bHPUpdated = false;

	// Catch-Up effect for HP
	if (!FMath::IsNearlyEqual(CurrentHPPercent, TargetHPPercent, 0.001f))
	{
		CurrentHPPercent = FMath::FInterpTo(CurrentHPPercent, TargetHPPercent, InDeltaTime, CatchUpInterpSpeed);
		
		if (ProgressBar_HP)
		{
			ProgressBar_HP->SetPercent(CurrentHPPercent);
		}
		
		bHPUpdated = true;
	}
	else if (CurrentHPPercent != TargetHPPercent)
	{
		// 목표치 도달 시 정확한 값으로 스냅 (0.0009f 등에 머무는 현상 방지)
		CurrentHPPercent = TargetHPPercent;
		if (ProgressBar_HP)
		{
			ProgressBar_HP->SetPercent(CurrentHPPercent);
		}
		bHPUpdated = true;
	}

	// Catch-Up effect for Stamina
	if (!FMath::IsNearlyEqual(CurrentStaminaPercent, TargetStaminaPercent, 0.001f))
	{
		CurrentStaminaPercent = FMath::FInterpTo(CurrentStaminaPercent, TargetStaminaPercent, InDeltaTime, CatchUpInterpSpeed);
		
		if (ProgressBar_Stamina)
		{
			ProgressBar_Stamina->SetPercent(CurrentStaminaPercent);
		}
	}
	else if (CurrentStaminaPercent != TargetStaminaPercent)
	{
		CurrentStaminaPercent = TargetStaminaPercent;
		if (ProgressBar_Stamina)
		{
			ProgressBar_Stamina->SetPercent(CurrentStaminaPercent);
		}
	}

	// Only update portrait state if HP has visually changed
	if (bHPUpdated)
	{
		UpdatePortraitState(CurrentHPPercent);
	}
}

void UMOU_CharacterStatusHUD::SetTargetHP(float NewHPPercent)
{
	TargetHPPercent = FMath::Clamp(NewHPPercent, 0.0f, 1.0f);
}

void UMOU_CharacterStatusHUD::SetTargetStamina(float NewStaminaPercent)
{
	TargetStaminaPercent = FMath::Clamp(NewStaminaPercent, 0.0f, 1.0f);
}

void UMOU_CharacterStatusHUD::ForceUpdateStatus(float NewHPPercent, float NewStaminaPercent)
{
	TargetHPPercent = FMath::Clamp(NewHPPercent, 0.0f, 1.0f);
	CurrentHPPercent = TargetHPPercent;
	
	TargetStaminaPercent = FMath::Clamp(NewStaminaPercent, 0.0f, 1.0f);
	CurrentStaminaPercent = TargetStaminaPercent;

	if (ProgressBar_HP)
	{
		ProgressBar_HP->SetPercent(CurrentHPPercent);
	}

	if (ProgressBar_Stamina)
	{
		ProgressBar_Stamina->SetPercent(CurrentStaminaPercent);
	}

	UpdatePortraitState(CurrentHPPercent);
}

void UMOU_CharacterStatusHUD::BindToCharacter(AMainCharacter* InCharacter)
{
	BoundCharacter = InCharacter;
	if (InCharacter && InCharacter->BaseAttribute)
	{
		UBaseAttributeSet* Attr = InCharacter->BaseAttribute;
		const float MaxHP = FMath::Max(1.0f, Attr->GetMaxHealth());
		const float CurrentHP = FMath::Clamp(Attr->GetHealth(), 0.0f, MaxHP);
		const float MaxStamina = FMath::Max(1.0f, Attr->GetMaxStemina());
		const float CurrentStamina = FMath::Clamp(Attr->GetStemina(), 0.0f, MaxStamina);
		ForceUpdateStatus(CurrentHP / MaxHP, CurrentStamina / MaxStamina);
	}
}

void UMOU_CharacterStatusHUD::UpdatePortraitState(float InCurrentHP)
{
	ECharacterStatusState NewState;

	if (InCurrentHP <= 0.0f)
	{
		NewState = ECharacterStatusState::Offline; // Groggy or Dead
	}
	else if (InCurrentHP <= 0.25f)
	{
		NewState = ECharacterStatusState::Critical; // 0+ ~ 25
	}
	else if (InCurrentHP <= 0.50f)
	{
		NewState = ECharacterStatusState::Warning; // 25+ ~ 50
	}
	else if (InCurrentHP <= 0.75f)
	{
		NewState = ECharacterStatusState::OK; // 50+ ~ 75
	}
	else
	{
		NewState = ECharacterStatusState::Happy; // 75+ ~ 100
	}

	if (NewState != CurrentState)
	{
		SetPortraitTextureByState(NewState);
		CurrentState = NewState;
	}
}

void UMOU_CharacterStatusHUD::SetPortraitTextureByState(ECharacterStatusState NewState)
{
	UTexture2D* TargetPortrait = nullptr;

	switch (NewState)
	{
		case ECharacterStatusState::Happy:
			TargetPortrait = Tex_Happy;
			break;
		case ECharacterStatusState::OK:
			TargetPortrait = Tex_OK;
			break;
		case ECharacterStatusState::Warning:
			TargetPortrait = Tex_Warning;
			break;
		case ECharacterStatusState::Critical:
			TargetPortrait = Tex_Critical;
			break;
		case ECharacterStatusState::Offline:
			TargetPortrait = Tex_Offline;
			break;
	}

	if (Image_CenterPortrait && TargetPortrait)
	{
		Image_CenterPortrait->SetBrushFromTexture(TargetPortrait);
	}
}

void UMOU_CharacterStatusHUD::UpdateHUDSway(float InDeltaTime)
{
	if (!bEnableSway)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		const FRotator CurrentControlRot = PC->GetControlRotation();
		if (bHasPreviousRotation)
		{
			const FRotator DeltaRot = (CurrentControlRot - PreviousControlRotation).GetNormalized();

			// 마우스 회전에 따른 오프셋 산출:
			// Yaw(좌우 마우스 회전): 오른쪽 회전(+Yaw) 시 UI는 관성으로 반대 방향(-X)으로 밀림
			// Pitch(상하 마우스 회전): 위로 회전(+Pitch) 시 UI는 관성으로 반대 방향(+Y, 화면 아래)으로 밀림
			FVector2D AddedSway;
			AddedSway.X = -DeltaRot.Yaw * SwaySensitivity.X;
			AddedSway.Y = DeltaRot.Pitch * SwaySensitivity.Y;

			// 캐릭터 로컬 이동 속도에 따른 미세 관성 흔들림 추가
			if (BoundCharacter.IsValid() && MovementSwayIntensity > 0.0f)
			{
				const FVector Velocity = BoundCharacter->GetVelocity();
				const FVector LocalVel = BoundCharacter->GetActorTransform().InverseTransformVector(Velocity);
				// 좌우 스트레이프(Y) 시 반대 방향으로 밀림
				AddedSway.X -= (LocalVel.Y / 500.0f) * MovementSwayIntensity;
				// 전후 이동(X) 시 상하로 미세 바운스
				AddedSway.Y += (LocalVel.X / 500.0f) * (MovementSwayIntensity * 0.5f);
			}

			// 목표 흔들림 위치 누적 및 최대 반경 클램프
			TargetSwayOffset += AddedSway;
			TargetSwayOffset.X = FMath::Clamp(TargetSwayOffset.X, -MaxSwayOffset, MaxSwayOffset);
			TargetSwayOffset.Y = FMath::Clamp(TargetSwayOffset.Y, -MaxSwayOffset, MaxSwayOffset);
		}
		else
		{
			bHasPreviousRotation = true;
		}
		PreviousControlRotation = CurrentControlRot;
	}

	// 1. 현재 흔들림 위치를 목표치로 부드럽게 보간
	CurrentSwayOffset = FMath::Vector2DInterpTo(CurrentSwayOffset, TargetSwayOffset, InDeltaTime, SwayInterpSpeed);

	// 2. 목표치를 매 프레임 원점(0,0)으로 부드럽게 감쇠 복귀 (마우스 멈춤 시 자연스러운 리턴)
	TargetSwayOffset = FMath::Vector2DInterpTo(TargetSwayOffset, FVector2D::ZeroVector, InDeltaTime, SwayReturnSpeed);

	// 3. 대상 위젯에 Render Transform 적용 (SwayContainer가 지정되지 않은 경우 전체 루트 위젯에 적용)
	UWidget* TargetWidget = SwayContainer ? SwayContainer.Get() : GetRootWidget();
	if (TargetWidget)
	{
		TargetWidget->SetRenderTranslation(CurrentSwayOffset);
		if (TiltAngleMultiplier != 0.0f)
		{
			TargetWidget->SetRenderTransformAngle(CurrentSwayOffset.X * TiltAngleMultiplier);
		}
	}

	// 4. 중앙 초상화 레이어 다층 패럴랙스(입체 깊이감) 적용
	if (bEnablePortraitParallax && Image_CenterPortrait)
	{
		const FVector2D PortraitOffset = CurrentSwayOffset * (PortraitParallaxMultiplier - 1.0f);
		Image_CenterPortrait->SetRenderTranslation(PortraitOffset);
	}
}
