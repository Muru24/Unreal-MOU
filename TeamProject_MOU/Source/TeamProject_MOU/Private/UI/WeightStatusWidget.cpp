#include "UI/WeightStatusWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

UWeightStatusWidget::UWeightStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWeightStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TargetColor = DefaultColor;
	CurrentColor = DefaultColor;
	ApplyGradeAppearance(EWeightGrade::Light);
}

void UWeightStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 1. 비율 부드러운 보간 (Interpolation)
	CurrentWeightRatio = FMath::FInterpTo(CurrentWeightRatio, TargetWeightRatio, InDeltaTime, InterpSpeed);

	FLinearColor FinalFrameColor = DefaultColor;
	FLinearColor FinalBarColor = DefaultColor;

	// 2. 초과 상태(100% 이상: Overload1, Overload2, Overload3 또는 CurrentWeightRatio >= 1.0f) 시 펄스(Blink/Pulse) 네온 연출 유지
	const bool bIsOverloaded = (CurrentGrade == EWeightGrade::Overload1 || CurrentGrade == EWeightGrade::Overload2 || CurrentGrade == EWeightGrade::Overload3 || CurrentWeightRatio >= 1.0f);
	if (bIsOverloaded)
	{
		PulseTime += InDeltaTime * PulseSpeed;
		float PulseAlpha = (FMath::Sin(PulseTime) + 1.0f) * 0.5f; // 0.0 ~ 1.0

		FinalFrameColor = FMath::Lerp(DefaultColor, OverloadWarningColor, PulseAlpha);
		FinalBarColor = FMath::Lerp(DefaultColor, OverloadWarningColor, PulseAlpha);
	}
	else
	{
		PulseTime = 0.0f;
	}

	// 3. 프로그레스 바 갱신 (0.0 ~ 1.0)
	if (ProgressBar_Weight)
	{
		// 프로그레스 바 게이지 채우기 (100% 이상도 1.0으로 꽉 참)
		ProgressBar_Weight->SetPercent(FMath::Clamp(CurrentWeightRatio, 0.0f, 1.0f));
		ProgressBar_Weight->SetFillColorAndOpacity(FinalBarColor);
	}

	// 4. 외곽 프레임 색상 틴트 적용 (기본 색상 유지, 초과 시 경고 깜빡임)
	if (Image_Frame)
	{
		Image_Frame->SetColorAndOpacity(FinalFrameColor);
	}

	// 5. 로봇 얼굴 표정: 상태에 따른 표정 변경 없음, 초과 상태 경고 깜빡임만 적용
	if (Image_FacePortrait)
	{
		if (bIsOverloaded)
		{
			Image_FacePortrait->SetColorAndOpacity(FinalFrameColor);
		}
		else
		{
			Image_FacePortrait->SetColorAndOpacity(FLinearColor::White);
		}
	}

	// 6. 퍼센트 텍스트 갱신
	if (Text_WeightPercent)
	{
		int32 DisplayPercent = FMath::RoundToInt(CurrentWeightRatio * 100.0f);
		Text_WeightPercent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), DisplayPercent)));
		Text_WeightPercent->SetColorAndOpacity(FSlateColor(FinalFrameColor));
	}
}

void UWeightStatusWidget::UpdateWeight(float NewCurrentWeight, float NewMaxWeight)
{
	TargetCurrentWeight = FMath::Max(0.0f, NewCurrentWeight);
	TargetMaxWeight = FMath::Max(1.0f, NewMaxWeight);

	TargetWeightRatio = TargetCurrentWeight / TargetMaxWeight;

	CalculateWeightGrade(TargetWeightRatio);
}

void UWeightStatusWidget::ForceUpdateWeight(float NewCurrentWeight, float NewMaxWeight)
{
	UpdateWeight(NewCurrentWeight, NewMaxWeight);
	CurrentWeightRatio = TargetWeightRatio;
	CurrentColor = DefaultColor;
}

void UWeightStatusWidget::CalculateWeightGrade(float Ratio)
{
	EWeightGrade NewGrade = UCharacterVisualDataAsset::CalculateWeightGrade(Ratio);

	if (CurrentGrade != NewGrade)
	{
		CurrentGrade = NewGrade;
		ApplyGradeAppearance(CurrentGrade);
	}
}

void UWeightStatusWidget::ApplyGradeAppearance(EWeightGrade Grade)
{
	// 상태에 따른 표정 아이콘 및 색상 변경 비활성화 (기본 색상 및 기본 표정 유지)
}
