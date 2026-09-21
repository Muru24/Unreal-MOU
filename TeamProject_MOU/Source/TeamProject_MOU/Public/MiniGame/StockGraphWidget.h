// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StockGraphWidget.generated.h"

/**
 * 
 */
UCLASS()
class TEAMPROJECT_MOU_API UStockGraphWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 그래프에 새로운 좌표를 추가
	UFUNCTION(BlueprintCallable, Category = "Stock|Graph")
	void AddGraph(FVector2D NewPoint);

	// 현재 그래프의 모든 좌표를 초기화
	UFUNCTION(BlueprintCallable, Category = "Stock|Graph")
	void ResetStockGraph();
	
	// 서버 시작 시간을 기준으로 그래프 진행 시작
	UFUNCTION(BlueprintCallable, Category = "Stock|Graph")
	void StartStockGraph(float InStopMultiplier, float InStartServerTime);

	// 그래프 진행 정지
	UFUNCTION(BlueprintCallable, Category = "Stock|Graph")
	void StopStockGraph();

	// 현재 배율 반환 및 WBP_StockScreen의 배율 Text 갱신에 사용
	UFUNCTION(BlueprintPure, Category = "Stock|Graph")
	float GetCurrentMultiplier() const { return CurrentMultiplier; }

	// 현금화 한 배율 위치에 마커 표시
	UFUNCTION(BlueprintCallable, Category = "Stock|Graph")
	void SetCashOutMarker(float InMultiplier);

	// 현금화 마커 초기화
	UFUNCTION(BlueprintCallable, Category = "Stock|Graph")
	void ResetCashOutMarker();
protected:
	// 위젯 생성 시 호출
	virtual void NativeConstruct() override;

	// 위젯 제거 시 timer 정리
	virtual void NativeDestruct() override;

	// 저장된 그래프 좌표들을 실제 선으로 그리는 함수
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool ParentEnabled
	)const override;

private:
	// 일정 시간마다 새로운 그래프 좌표 생성
	void UpdateStockGraph();
private:
	// 그래프를 구성하는 좌표 배열
	UPROPERTY()
	TArray<FVector2D> GraphPoint;

	// 최소 종료 배율
	UPROPERTY(EditDefaultsOnly, Category = "Stock|Graph")
	float MinStopMultiplier = 1.0f;

	// 최대 종료 배율
	UPROPERTY(EditDefaultsOnly, Category = "Stock|Graph")
	float MaxStopMultiplier = 3.0f;

	// 1.0x에서 3.00x까지 도달하는 전체 시간
	UPROPERTY(EditDefaultsOnly, Category = "Stock|Graph", meta = (ClampMin = "0.1"))
	float GraphDuration = 4.0f;

	// 그래프의 급등 정도
	// 값이 높을수록 초반은 완만하고 후반에 급격하게 상승
	UPROPERTY(EditDefaultsOnly, Category = "Stock|Graph", meta = (ClampMin = "0.1"))
	float CurvePower = 2.2f;

	// 현재 그래프 X 위치
	float CurrentX = 100.0f;

	// 현재 그래프 Y 위치
	float CurrentY = 600.0f;

	// 현재 진행 중인 배율
	float CurrentMultiplier = 1.0f;

	// 이번 라운드에서 그래프가 멈출 배율
	float StopMultiplier = 1.0f;

	// 이번 라운드가 서버에서 시작된 시간
	float RoundStartServerTime = 0.0f;

	// 그래프가 시작된 후 경과 시간
	float ElapsedTime = 0.0f;

	// 그래프 갱신 주기
	float UpdateInterval = 0.05f;

	// 그래프가 끝나는 X 위치
	float MaxX = 1500.0f;

	// 그래프 진행 여부
	bool GraphRunning = false;

	// 현금화 지점 표시 여부
	bool ShowCashOutMarker = false;

	// 현금화한 그래프 위치
	FVector2D CashOutMarkerPoint = FVector2D::ZeroVector;

	// 그래프 갱신 Timer
	FTimerHandle GraphTimerHandle;

	// 실제 그래프 선에서 현금화 위치 갱신
	void UpdateCashOutMarkerPoint();

	// 현금화한 배율 저장
	float CashOutMarkerMultiplier = 0.0f;
};
