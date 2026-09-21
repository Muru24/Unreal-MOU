// Fill out your copyright notice in the Description page of Project Settings.


#include "MiniGame/StockGraphWidget.h"
#include "Rendering/DrawElements.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"

void UStockGraphWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 기존 그래프 데이터 초기화
	GraphPoint.Empty();
}

void UStockGraphWidget::NativeDestruct()
{
	// 위젯이 제거될 때 Timer 정리
	StopStockGraph();

	Super::NativeDestruct();
}

void UStockGraphWidget::StartStockGraph(float InStopMultiplier, float InStartServerTime)
{
	// 이전에 실행 중인 그래프와 Timer 정리
	StopStockGraph();

	// 기존 그래프 초기화
	ResetStockGraph();

	// 이전 라운드 현금화 마커 초기화
	ResetCashOutMarker();

	// 그래프 시작 위치 설정
	CurrentX = 100.0f;
	CurrentY = 600.0f;

	// 배율 및 경과 시간 초기화
	CurrentMultiplier = MinStopMultiplier;
	ElapsedTime = 0.0f;

	// 서버에서 라운드가 시작된 시간 저장
	RoundStartServerTime = InStartServerTime;

	// StockMachine에서 전달받은 배율 저장
	StopMultiplier = FMath::Clamp(
		FMath::RoundToInt(InStopMultiplier * 100.0f) / 100.0f, 
		MinStopMultiplier, 
		MaxStopMultiplier
	);

	// 첫번째 그래프 좌표 추가
	AddGraph(FVector2D(CurrentX, CurrentY));

	// 최소배율이 1.0x가 나온 경우 그래프가 진행하지 않고 즉시 종료
	if (StopMultiplier <= MinStopMultiplier)
	{
		CurrentMultiplier = MinStopMultiplier;
		GraphRunning = false;
		return;
	}

	// 그래프 진행 상태 활성화
	GraphRunning = true;

	// 일정한 주기로 그래프 갱신
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			GraphTimerHandle, this, &UStockGraphWidget::UpdateStockGraph, UpdateInterval, true
		);
	}
}

void UStockGraphWidget::StopStockGraph()
{
	// 그래프 진행 정지
	GraphRunning = false;

	// 실행 중인 Timer 제거
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GraphTimerHandle);
	}
}

void UStockGraphWidget::UpdateStockGraph()
{
	// 그래프가 진행 중이 아니면 갱신하지 않음.
	if (!GraphRunning)
	{
		return;
	}
	
	// 잘못된 설정값으로 인한 0 나누기 방지
	if (GraphDuration <= 0.0f ||
		MaxStopMultiplier <= MinStopMultiplier)
	{
		StopStockGraph();
		return;
	}

	// 서버 기준 시간으로 실제 라운드 경과 시간 계산
	if (const AGameStateBase* GameState = GetWorld()->GetGameState<AGameStateBase>())
	{
		ElapsedTime = FMath::Max(
			0.0f,
			GameState->GetServerWorldTimeSeconds() - RoundStartServerTime
		);
	}
	else
	{
		// GameState를 얻지 못한 경우 기존 Timer 방식 사용
		ElapsedTime += UpdateInterval;
	}

	// 전체 그래프 진행률
	// GraphDuration 동안 0.0 → 1.0으로 증가
	const float Progress = FMath::Clamp(ElapsedTime / GraphDuration, 0.0f, 1.0f);

	// 초반은 완만하고 후반으로 갈수록 급격하게 상승
	const float CurveAlpha = FMath::Pow(Progress, CurvePower);

	// 모든 라운드에서 동일한 X 진행
	CurrentX = FMath::Lerp(100.0f, MaxX, Progress);

	// 모든 라운드에서 동일한 1배 → 3배 상승 곡선 계산
	const float CalculateMultiplier = FMath::Lerp(MinStopMultiplier, MaxStopMultiplier, CurveAlpha);

	// 이번 라운드의 랜덤 종료 배율까지만 허용
	CurrentMultiplier = FMath::Min(CalculateMultiplier, StopMultiplier);

	// 현재 배율을 0~1 범위로 변환
	const float NormalizedMultiplier =
		(CurrentMultiplier - MinStopMultiplier) /
		(MaxStopMultiplier - MinStopMultiplier);

	// 배율에 맞춰 Y 위치 결정
	CurrentY = FMath::Lerp(600.0f, 120.0f, NormalizedMultiplier);

	// 현재 그래프 좌표 추가
	AddGraph(FVector2D(CurrentX, CurrentY));

	// 이번 판의 랜덤 종료 배율에 도달하면 정지
	if (CurrentMultiplier >= StopMultiplier)
	{
		CurrentMultiplier = StopMultiplier;
		StopStockGraph();
		return;
	}
}

void UStockGraphWidget::UpdateCashOutMarkerPoint()
{
	if (GraphPoint.Num() == 0)
	{
		return;
	}

	// 현금화 배율을 그래프 Y 좌표로 변환
	const float NormalizedMultiplier =
		(CashOutMarkerMultiplier - MinStopMultiplier) /
		(MaxStopMultiplier - MinStopMultiplier);

	const float TargetY =
		FMath::Lerp(600.0f, 120.0f, NormalizedMultiplier);

	// 아직 정확한 지점을 못 찾았을 경우 현재 마지막 점 사용
	CashOutMarkerPoint = GraphPoint.Last();

	// 실제 화면에 그려진 GraphPoint 사이에서
	// CashOut 배율의 Y를 통과하는 구간 탐색
	for (int32 i = 1; i < GraphPoint.Num(); ++i)
	{
		const FVector2D& PrevPoint = GraphPoint[i - 1];
		const FVector2D& CurrentPoint = GraphPoint[i];

		const bool bContainsTargetY =
			(TargetY <= PrevPoint.Y && TargetY >= CurrentPoint.Y) ||
			(TargetY >= PrevPoint.Y && TargetY <= CurrentPoint.Y);

		if (!bContainsTargetY)
		{
			continue;
		}

		const float YDifference =
			CurrentPoint.Y - PrevPoint.Y;

		const float Alpha =
			FMath::IsNearlyZero(YDifference)
			? 0.0f
			: FMath::Clamp(
				(TargetY - PrevPoint.Y) / YDifference,
				0.0f,
				1.0f
			);

		// 실제 그려진 선분 위의 정확한 위치
		CashOutMarkerPoint =
			FMath::Lerp(
				PrevPoint,
				CurrentPoint,
				Alpha
			);

		break;
	}
}

void UStockGraphWidget::AddGraph(FVector2D NewPoint)
{
	// 새로운 그래프 좌표 추가
	GraphPoint.Add(NewPoint);

	// 그래프 변경 내용을 다시 그리도록 갱신
	InvalidateLayoutAndVolatility();
}

void UStockGraphWidget::ResetStockGraph()
{
	// 모든 그래프 좌표 삭제
	GraphPoint.Empty();

	// 초기화된 상태를 화면에 반영
	InvalidateLayoutAndVolatility();
}

void UStockGraphWidget::SetCashOutMarker(float InMultiplier)
{
	CashOutMarkerMultiplier = FMath::Clamp(
		InMultiplier,
		MinStopMultiplier,
		MaxStopMultiplier
	);

	// 현금화 순간 최신 서버 시간 기준으로
	// 그래프를 한 번 즉시 갱신
	if (GraphRunning)
	{
		UpdateStockGraph();
	}


	ShowCashOutMarker = true;

	// 현금화 순간 단 한 번만 실제 그래프 선에서 위치 결정
	UpdateCashOutMarkerPoint();

	InvalidateLayoutAndVolatility();
}

void UStockGraphWidget::ResetCashOutMarker()
{
	ShowCashOutMarker = false;

	CashOutMarkerMultiplier = 0.0f;
	CashOutMarkerPoint = FVector2D::ZeroVector;

	InvalidateLayoutAndVolatility();
}

int32 UStockGraphWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool ParentEnabled) const
{
	// 부모 위젯의 기본 Paint 처리
	const int32 SuperLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, ParentEnabled
	);

	// 점이 2개 미만이면 선을 만들 수 없기에 종료
	if (GraphPoint.Num() < 2)
	{
		return SuperLayer;
	}

	// Slate에서 사용할 FVector2f 배열 생성
	TArray<FVector2f> SlatePoint;
	SlatePoint.Reserve(GraphPoint.Num());

	// FVector2D 좌표를 Slate용 FVector2f로 변환
	for (const FVector2D& Point:GraphPoint)
	{
		SlatePoint.Add(FVector2f(static_cast<float>(Point.X), static_cast<float>(Point.Y)));
	}

	// 부모 위젯보다 한 단계 위 레이어에 그래프 출력
	const int32 GraphLayer = SuperLayer + 1;

	// 저장된 좌표들을 연결하여 하나의 그래프 선으로 출력
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		GraphLayer,
		AllottedGeometry.ToPaintGeometry(),
		SlatePoint,
		ESlateDrawEffect::None,
		FLinearColor::White,
		true,
		5.0f
	);

	// 현금화 지점 마커 표시
	if (ShowCashOutMarker)
	{
		const float MarkerSize = 12.0f;

		TArray<FVector2f> MarkerLine1;
		MarkerLine1.Add(FVector2f(
			CashOutMarkerPoint.X - MarkerSize,
			CashOutMarkerPoint.Y - MarkerSize
		));
		MarkerLine1.Add(FVector2f(
			CashOutMarkerPoint.X + MarkerSize,
			CashOutMarkerPoint.Y + MarkerSize
		));

		TArray<FVector2f> MarkerLine2;
		MarkerLine2.Add(FVector2f(
			CashOutMarkerPoint.X - MarkerSize,
			CashOutMarkerPoint.Y + MarkerSize
		));
		MarkerLine2.Add(FVector2f(
			CashOutMarkerPoint.X + MarkerSize,
			CashOutMarkerPoint.Y - MarkerSize
		));

		const int32 MarkerLayer = GraphLayer + 1;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MarkerLayer,
			AllottedGeometry.ToPaintGeometry(),
			MarkerLine1,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.5f, 0.0f, 1.0f),
			true,
			7.0f
		);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MarkerLayer,
			AllottedGeometry.ToPaintGeometry(),
			MarkerLine2,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.5f, 0.0f, 1.0f),
			true,
			7.0f
		);

		return MarkerLayer;
	}


	return GraphLayer;
}