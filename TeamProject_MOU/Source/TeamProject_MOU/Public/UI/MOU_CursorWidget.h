#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOU_CursorWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * UMOU_CursorWidget
 * 인게임 소프트웨어 마우스 커서 전용 위젯 베이스 클래스
 * - Slate 레벨에서 마우스 좌클릭 상태를 0ms 지연 없이 실시간 감지
 * - 클릭 시 화살표 끝(0,0) 피벗 기준 쫀득한 스케일 압축(Squish) 및 네온 발광(Glow) 효과 제공
 * - 핫스팟 오프셋(HotspotOffset)을 통해 화살표 끝부분과 실제 클릭점 일치 및 여유 범위 튜닝 지원
 * - 텍스처 교체(Default <-> Clicked) 또는 컬러 틴트 보간 동시 지원
 */
UCLASS()
class TEAMPROJECT_MOU_API UMOU_CursorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMOU_CursorWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

public:
	// ---------------------------------------------------------
	// [핫스팟 / 클릭 기준점 오프셋 조정]
	// ---------------------------------------------------------

	// 실제 마우스 클릭 지점(위젯 중심)과 화살표 끝점을 일치시키기 위한 오프셋 (픽셀 단위)
	// 언리얼 엔진은 소프트웨어 커서의 정중앙(Center)을 실제 마우스 클릭 위치로 잡습니다.
	// 화살표 끝이 이미지 좌상단에 있다면, 양수(+X, +Y) 값(예: X=30~45, Y=30~45 전후)을 주어 이미지를 우하단으로 이동시키면
	// 화살표 끝이 디버그 십자선(실제 클릭 지점)에 정확히 일치하게 됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	FVector2D HotspotOffset = FVector2D::ZeroVector;

	// ---------------------------------------------------------
	// [디버그: 실제 클릭 위치(0, 0) 시각화]
	// ---------------------------------------------------------

	// 실제 마우스 클릭이 발생하는 지점(0, 0)에 십자선과 조준점 디버그 마커를 화면에 표시할지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor|Debug")
	bool bShowDebugHotspot = true;

	// 평상시 디버그 마커 색상 (기본: 눈에 잘 띄는 빨간색)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor|Debug")
	FLinearColor DebugHotspotColor = FLinearColor::Red;

	// 마우스 좌클릭 시 디버그 마커 색상 (기본: 형광 연두색 - 클릭 즉시 반응 확인)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor|Debug")
	FLinearColor DebugClickedColor = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);

	// 디버그 십자선 크기 (반경 픽셀)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor|Debug", meta = (ClampMin = "4.0", ClampMax = "50.0"))
	float DebugCrosshairSize = 14.0f;

	// ---------------------------------------------------------
	// [위젯 컴포넌트 바인딩]
	// ---------------------------------------------------------

	// 커서 이미지를 표시할 UMG Image 위젯 (지정 안 하면 위젯 트리의 첫 번째 Image 자동 탐색)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Cursor")
	TObjectPtr<UImage> Image_Cursor;

	// ---------------------------------------------------------
	// [커서 텍스처 설정]
	// ---------------------------------------------------------

	// 평상시 커서 텍스처 (지정 시 자동 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	TObjectPtr<UTexture2D> Texture_Default;

	// 클릭 중 표시할 커서 텍스처 (지정 시 클릭 중 자동 교체)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	TObjectPtr<UTexture2D> Texture_Clicked;

	// ---------------------------------------------------------
	// [비주얼 피드백 파라미터 (발광 및 스케일)]
	// ---------------------------------------------------------

	// 평상시 틴트 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	FLinearColor NormalTint = FLinearColor::White;

	// 클릭 시 발광(Glow) 틴트 색상 (RGB > 1.0 시 눈부신 네온 발광)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	FLinearColor ClickedTint = FLinearColor(1.6f, 1.9f, 2.3f, 1.0f);

	// 클릭 시 압축 스케일 배율 (0.9 = 90% 크기로 쫀득하게 눌림)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float ClickedScale = 0.90f;

	// 클릭/해제 시 애니메이션 보간 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	float InterpSpeed = 25.0f;

	// 클릭 시 스케일 압축 사용 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	bool bEnableClickSquish = true;

	// 클릭 시 발광 효과 사용 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cursor")
	bool bEnableGlow = true;

	// ---------------------------------------------------------
	// [블루프린트 연동 이벤트]
	// ---------------------------------------------------------

	UFUNCTION(BlueprintImplementableEvent, Category = "Cursor|Event")
	void OnCursorPressed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Cursor|Event")
	void OnCursorReleased();

private:
	bool bWasPressed = false;
	float CurrentScale = 1.0f;
	FLinearColor CurrentColor = FLinearColor::White;
};
