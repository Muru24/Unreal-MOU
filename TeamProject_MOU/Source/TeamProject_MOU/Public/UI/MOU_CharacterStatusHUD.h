#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOU_CharacterStatusHUD.generated.h"

class UImage;
class UProgressBar;
class UTexture2D;
class UWidget;
class AMainCharacter;

UENUM(BlueprintType)
enum class ECharacterStatusState : uint8
{
	Happy,
	OK,
	Warning,
	Critical,
	Offline
};

/**
 * Character Status HUD
 * Handles HP, Stamina circular bars and dynamically changes center portrait
 */
UCLASS()
class TEAMPROJECT_MOU_API UMOU_CharacterStatusHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Set Target HP (0.0 ~ 1.0) for interpolation
	UFUNCTION(BlueprintCallable, Category = "UI|Status")
	void SetTargetHP(float NewHPPercent);

	// Set Target Stamina (0.0 ~ 1.0) for interpolation
	UFUNCTION(BlueprintCallable, Category = "UI|Status")
	void SetTargetStamina(float NewStaminaPercent);

	// Immediately set values without interpolation
	UFUNCTION(BlueprintCallable, Category = "UI|Status")
	void ForceUpdateStatus(float NewHPPercent, float NewStaminaPercent);

	// Bind status updates to a specific character (e.g. for spectating)
	UFUNCTION(BlueprintCallable, Category = "UI|Status")
	void BindToCharacter(AMainCharacter* InCharacter);

	UFUNCTION(BlueprintPure, Category = "UI|Status")
	AMainCharacter* GetBoundCharacter() const { return BoundCharacter.Get(); }

protected:
	// -- UI Components --

	UPROPERTY(meta = (BindWidget))
	UProgressBar* ProgressBar_HP;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* ProgressBar_Stamina;

	UPROPERTY(meta = (BindWidget))
	UImage* Image_CenterPortrait;

	// -- Portrait Textures --

	UPROPERTY(EditDefaultsOnly, Category = "UI|Status|Portrait")
	UTexture2D* Tex_Happy;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Status|Portrait")
	UTexture2D* Tex_OK;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Status|Portrait")
	UTexture2D* Tex_Warning;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Status|Portrait")
	UTexture2D* Tex_Critical;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Status|Portrait")
	UTexture2D* Tex_Offline;

	// -- Interpolation Setting --

	UPROPERTY(EditDefaultsOnly, Category = "UI|Status|Interpolation")
	float CatchUpInterpSpeed = 5.0f;

	// -- Dynamic HUD Sway & Parallax Settings --

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SwayContainer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Sway")
	bool bEnableSway = true;

	// 카메라 회전(Yaw/Pitch)에 따른 흔들림 감도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Sway")
	FVector2D SwaySensitivity = FVector2D(2.0f, 2.0f);

	// 캐릭터 이동 속도에 따른 미세 반동 감도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Sway")
	float MovementSwayIntensity = 3.0f;

	// 최대 허용 흔들림 거리 (픽셀 단위)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Sway")
	float MaxSwayOffset = 25.0f;

	// 목표 오프셋 추적 보간 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Sway")
	float SwayInterpSpeed = 10.0f;

	// 마우스 정지 시 원점 복귀 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Sway")
	float SwayReturnSpeed = 6.0f;

	// 좌우 흔들림 시 회전 기울기 계수 (Degree)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Sway")
	float TiltAngleMultiplier = 0.06f;

	// -- Parallax Settings --

	// 중앙 초상화 레이어 입체 패럴랙스 활성화 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Parallax")
	bool bEnablePortraitParallax = true;

	// 중앙 초상화 이동 배율 (1.0 초과 시 더 많이 움직여 앞쪽에 있는 것처럼 보임)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Status|Parallax")
	float PortraitParallaxMultiplier = 1.25f;

private:
	float TargetHPPercent = 1.0f;
	float CurrentHPPercent = 1.0f;

	float TargetStaminaPercent = 1.0f;
	float CurrentStaminaPercent = 1.0f;

	ECharacterStatusState CurrentState = ECharacterStatusState::Happy;

	UPROPERTY()
	TWeakObjectPtr<AMainCharacter> BoundCharacter;

	FVector2D CurrentSwayOffset = FVector2D::ZeroVector;
	FVector2D TargetSwayOffset = FVector2D::ZeroVector;
	FRotator PreviousControlRotation = FRotator::ZeroRotator;
	bool bHasPreviousRotation = false;

	void UpdateHUDSway(float InDeltaTime);
	void UpdatePortraitState(float InCurrentHP);
	void SetPortraitTextureByState(ECharacterStatusState NewState);
};
