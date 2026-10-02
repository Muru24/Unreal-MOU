#include "UI/MOU_CursorWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"

UMOU_CursorWidget::UMOU_CursorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 소프트웨어 커서는 UI 클릭 입력을 가로채지 않도록 HitTestInvisible로 설정
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMOU_CursorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. UMG에서 바인딩 변수명이 Image_Cursor가 아니더라도(예: Image_18) 첫 번째 UImage 자동 탐색
	if (!Image_Cursor && WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (!Image_Cursor)
			{
				if (UImage* FoundImage = Cast<UImage>(Widget))
				{
					Image_Cursor = FoundImage;
				}
			}
		});
	}

	// 2. 루트 위젯의 자체 Translation은 0으로 고정
	SetRenderTranslation(FVector2D::ZeroVector);

	// 3. 언리얼 엔진은 소프트웨어 커서 위젯의 정중앙(0.5, 0.5)을 마우스 커서 위치에 배치합니다.
	// 클릭 스케일 압축 시 클릭 지점이 흔들리지 않도록 피벗을 중심 (0.5, 0.5)으로 설정합니다.
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	CurrentColor = NormalTint;
	CurrentScale = 1.0f;

	if (Image_Cursor)
	{
		Image_Cursor->SetRenderTranslation(HotspotOffset);
		Image_Cursor->SetColorAndOpacity(NormalTint);

		if (Texture_Default)
		{
			Image_Cursor->SetBrushFromTexture(Texture_Default);
		}
	}
}

void UMOU_CursorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 위젯 생성 시점에 Image를 못 찾았을 경우 대비한 재탐색 폴백
	if (!Image_Cursor && WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (!Image_Cursor)
			{
				if (UImage* FoundImage = Cast<UImage>(Widget))
				{
					Image_Cursor = FoundImage;
				}
			}
		});
	}

	// HotspotOffset 실시간 반영 (디테일 패널에서 값을 변경해도 즉각 화면에 이동 반영)
	if (Image_Cursor)
	{
		Image_Cursor->SetRenderTranslation(HotspotOffset);
	}

	// Slate 레벨에서 좌클릭 상태를 0ms 지연 없이 실시간 감지 (GameAndUI, UIOnly 모두 호환)
	bool bIsPressed = false;
	if (FSlateApplication::IsInitialized())
	{
		bIsPressed = FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
	}

	// 클릭 상태 전환 감지
	if (bIsPressed != bWasPressed)
	{
		bWasPressed = bIsPressed;

		if (bIsPressed)
		{
			// 클릭 텍스처가 지정되어 있으면 교체
			if (Image_Cursor && Texture_Clicked)
			{
				Image_Cursor->SetBrushFromTexture(Texture_Clicked);
			}

			OnCursorPressed();
		}
		else
		{
			// 기본 텍스처로 복원
			if (Image_Cursor && Texture_Default)
			{
				Image_Cursor->SetBrushFromTexture(Texture_Default);
			}

			OnCursorReleased();
		}
	}

	// 1. 스케일 압축(Squish) 보간 처리 (피벗이 (0.5, 0.5)이므로 클릭점은 유지되고 화살표 몸체만 쫀득하게 압축)
	float TargetScale = (bIsPressed && bEnableClickSquish) ? ClickedScale : 1.0f;
	CurrentScale = FMath::FInterpTo(CurrentScale, TargetScale, InDeltaTime, InterpSpeed);
	SetRenderScale(FVector2D(CurrentScale, CurrentScale));

	// 2. 발광(Glow) / 틴트 색상 보간 처리
	FLinearColor TargetColor = (bIsPressed && bEnableGlow) ? ClickedTint : NormalTint;
	CurrentColor = FMath::CInterpTo(CurrentColor, TargetColor, InDeltaTime, InterpSpeed);

	if (Image_Cursor)
	{
		Image_Cursor->SetColorAndOpacity(CurrentColor);
	}
}

int32 UMOU_CursorWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// 디버그 핫스팟 표시 활성화 시: Slate가 마우스 위치로 판정하는 실제 클릭 지점(위젯 정중앙)에 십자선 마커 출력
	if (bShowDebugHotspot)
	{
		const int32 DebugLayer = MaxLayerId + 10;
		const FLinearColor ActiveColor = bWasPressed ? DebugClickedColor : DebugHotspotColor;
		const float LineThickness = 2.0f;

		// Slate 엔진이 마우스 클릭 위치에 배치하는 실제 클릭 지점 (위젯의 정중앙)
		const FVector2f ClickHotspot = UE::Slate::CastToVector2f(AllottedGeometry.GetLocalSize()) * 0.5f;

		// 1. 가로 십자선
		TArray<FVector2f> HorizLine;
		HorizLine.Add(FVector2f(ClickHotspot.X - DebugCrosshairSize, ClickHotspot.Y));
		HorizLine.Add(FVector2f(ClickHotspot.X + DebugCrosshairSize, ClickHotspot.Y));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			DebugLayer,
			AllottedGeometry.ToPaintGeometry(),
			HorizLine,
			ESlateDrawEffect::None,
			ActiveColor,
			true,
			LineThickness
		);

		// 2. 세로 십자선
		TArray<FVector2f> VertLine;
		VertLine.Add(FVector2f(ClickHotspot.X, ClickHotspot.Y - DebugCrosshairSize));
		VertLine.Add(FVector2f(ClickHotspot.X, ClickHotspot.Y + DebugCrosshairSize));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			DebugLayer,
			AllottedGeometry.ToPaintGeometry(),
			VertLine,
			ESlateDrawEffect::None,
			ActiveColor,
			true,
			LineThickness
		);

		// 3. 중심 타겟 박스 (클릭 시 크기가 살짝 커져 클릭 피드백 시각화)
		const float BoxR = bWasPressed ? 5.5f : 3.5f;
		TArray<FVector2f> CenterBox;
		CenterBox.Add(FVector2f(ClickHotspot.X - BoxR, ClickHotspot.Y - BoxR));
		CenterBox.Add(FVector2f(ClickHotspot.X + BoxR, ClickHotspot.Y - BoxR));
		CenterBox.Add(FVector2f(ClickHotspot.X + BoxR, ClickHotspot.Y + BoxR));
		CenterBox.Add(FVector2f(ClickHotspot.X - BoxR, ClickHotspot.Y + BoxR));
		CenterBox.Add(FVector2f(ClickHotspot.X - BoxR, ClickHotspot.Y - BoxR));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			DebugLayer,
			AllottedGeometry.ToPaintGeometry(),
			CenterBox,
			ESlateDrawEffect::None,
			ActiveColor,
			true,
			LineThickness
		);

		MaxLayerId = DebugLayer;
	}

	return MaxLayerId;
}
