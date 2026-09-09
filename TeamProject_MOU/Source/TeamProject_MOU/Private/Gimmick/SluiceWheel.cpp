#include "Gimmick/SluiceWheel.h"
#include "Gimmick/SluiceGate.h"
#include "Player/MainCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"

ASluiceWheel::ASluiceWheel()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	bIsPushable = true;
	bCustomPusherAlignment = true;
	RequiredPushers = 1;
	MaxPushDistance = 350.0f;
	PushDetachDistance = 200.0f;
	MinRequiredGroundPoints = 0;
	bShowDebugGroundTrace = false;
	ItemWeight = 0.0f;

	HandleBarAxis = EAxis::Y;
	HandleRadius = 130.0f;
	PlayerDistanceToBar = 45.0f;
	bPushOnlyWithW = true;

	BasePlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BasePlatformMesh"));
	BasePlatformMesh->SetupAttachment(RootComponent);

	RotatingWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RotatingWheelMesh"));
	RotatingWheelMesh->SetupAttachment(BasePlatformMesh);
}

void ASluiceWheel::BeginPlay()
{
	Super::BeginPlay();

	SetPhysicsSimulateEnabled(false);
	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(false);
	}

	// HandleMesh가 미지정된 경우 RotatingWheelMesh 자식 중 'Handle' 컴포넌트 자동 탐색
	if (!HandleMesh && RotatingWheelMesh)
	{
		TArray<USceneComponent*> ChildComponents;
		RotatingWheelMesh->GetChildrenComponents(true, ChildComponents);
		for (USceneComponent* Child : ChildComponents)
		{
			if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
			{
				if (SMC->GetName().Contains(TEXT("Handle")))
				{
					HandleMesh = SMC;
					break;
				}
			}
		}

		if (!HandleMesh)
		{
			for (USceneComponent* Child : ChildComponents)
			{
				if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
				{
					HandleMesh = SMC;
					break;
				}
			}
		}
	}

	CurrentTurnProgress = bStartFullyOpen ? 1.0f : 0.0f;
	VisualWheelYaw = CurrentTurnProgress * FullTurnsForFullOpen * 360.0f;

	if (RotatingWheelMesh)
	{
		RotatingWheelMesh->SetRelativeRotation(FRotator(0.0f, VisualWheelYaw, 0.0f));
	}

	if (TargetGate && HasAuthority())
	{
		TargetGate->SetTargetOpenRatio(CurrentTurnProgress);
	}
}

void ASluiceWheel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASluiceWheel, CurrentTurnProgress);
}

bool ASluiceWheel::CalculateGripTransform(
	const FVector& PusherLocation,
	const FVector& PusherForward,
	FVector& OutSnapLocation,
	FRotator& OutSnapRotation,
	FVector& OutTangent,
	bool& bOutIsCCW) const
{
	FVector WheelCenter = GetActorLocation();

	// 1. 플레이어가 서 있는 현재 위치에서 그대로 시작 (손잡이 끝으로 순간이동하지 않음)
	OutSnapLocation = PusherLocation;

	FVector RadialFromCenter = PusherLocation - WheelCenter;
	RadialFromCenter.Z = 0.0f;
	float CurrentRadius = RadialFromCenter.Size();

	// 중심 기둥 바로 안쪽으로 파고든 극단적인 경우에만 최소 안전 거리 확보
	if (CurrentRadius < 50.0f)
	{
		CurrentRadius = 50.0f;
		if (RadialFromCenter.IsNearlyZero())
		{
			RadialFromCenter = GetActorForwardVector() * CurrentRadius;
		}
		else
		{
			RadialFromCenter = RadialFromCenter.GetSafeNormal() * CurrentRadius;
		}
		OutSnapLocation = WheelCenter + RadialFromCenter;
		OutSnapLocation.Z = PusherLocation.Z;
	}

	FVector RadialDir = RadialFromCenter.GetSafeNormal();

	// 선택된 위치에서의 접선 방향 (반시계 CCW vs 시계 CW)
	FVector TangentCCW = FVector::CrossProduct(FVector::UpVector, RadialDir).GetSafeNormal();
	FVector TangentCW = -TangentCCW;

	// 플레이어가 현재 바라보고 있는 방향과 접선 방향의 내적 비교하여 전진 진행 방향 결정
	FVector Facing2D = PusherForward;
	Facing2D.Z = 0.0f;
	Facing2D = Facing2D.GetSafeNormal();

	float DotCCW = FVector::DotProduct(Facing2D, TangentCCW);
	float DotCW = FVector::DotProduct(Facing2D, TangentCW);

	if (DotCCW >= DotCW)
	{
		OutTangent = TangentCCW;
		bOutIsCCW = true;
	}
	else
	{
		OutTangent = TangentCW;
		bOutIsCCW = false;
	}

	// 캐릭터 회전은 바라보는 접선 전방으로 정렬
	OutSnapRotation = OutTangent.Rotation();

	return true;
}

void ASluiceWheel::SnapPusherToHandle(AMainCharacter* Pusher)
{
	if (!Pusher)
	{
		return;
	}

	FVector SnapLoc;
	FRotator SnapRot;
	FVector Tangent;
	bool bIsCCW = true;

	if (CalculateGripTransform(Pusher->GetActorLocation(), Pusher->GetActorForwardVector(), SnapLoc, SnapRot, Tangent, bIsCCW))
	{
		Pusher->SetActorLocation(SnapLoc, false);
		Pusher->SetActorRotation(SnapRot);
		Pusher->LockedPushDirection = Tangent;
		Pusher->LastCharacterLocation = SnapLoc;
		Pusher->PushInitialDistToBox = 0.0f;

		FVector WheelCenter = GetActorLocation();
		float LockedRadius = FVector::Dist2D(SnapLoc, WheelCenter);
		PusherLockedRadiusMap.Add(Pusher, LockedRadius);

		FVector NewLocalAnchor = GetActorTransform().InverseTransformPosition(SnapLoc);
		PusherLocalAnchorMap.Add(Pusher, NewLocalAnchor);
		Pusher->PushLocalAnchor = NewLocalAnchor;

		PusherIsCCWMap.Add(Pusher, bIsCCW);
	}
}

void ASluiceWheel::AddPusher(AMainCharacter* Pusher)
{
	Super::AddPusher(Pusher);

	if (Pusher)
	{
		SnapPusherToHandle(Pusher);
	}
}

void ASluiceWheel::RemovePusher(AMainCharacter* Pusher)
{
	Super::RemovePusher(Pusher);

	if (Pusher)
	{
		PusherIsCCWMap.Remove(Pusher);
		PusherLockedRadiusMap.Remove(Pusher);
	}
}

void ASluiceWheel::Tick(float DeltaTime)
{
	if (bShowDebugPushDistance)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation(), MaxPushDistance, 32, FColor::Green, false, -1.0f, 0, 2.0f, FVector(0, 1, 0), FVector(1, 0, 0), false);
	}

	if (HasAuthority() && bIsPushable && CurrentPushers.Num() > 0)
	{
		TArray<AMainCharacter*> PushersToDetach;
		for (AMainCharacter* Pusher : CurrentPushers)
		{
			if (!Pusher) continue;

			float DistToCenter = FVector::Dist2D(Pusher->GetActorLocation(), GetActorLocation());
			if (DistToCenter > (MaxPushDistance + 100.0f))
			{
				PushersToDetach.Add(Pusher);
			}
		}

		for (AMainCharacter* Pusher : PushersToDetach)
		{
			Pusher->StopPushMode();
		}

		if (IsReadyToMove())
		{
			float TotalSignedInput = 0.0f;
			int32 ActiveCount = 0;

			for (const TObjectPtr<AMainCharacter>& Pusher : CurrentPushers)
			{
				if (!Pusher) continue;

				float PushInput = Pusher->GetCurrentPushInput();

				// W키(전진, PushInput > 0.1f)를 눌렀을 때만 회전 입력 허용
				if (PushInput > 0.1f)
				{
					bool bCCW = PusherIsCCWMap.Contains(Pusher) ? PusherIsCCWMap[Pusher] : true;
					float DirectionSign = bCCW ? 1.0f : -1.0f;
					TotalSignedInput += DirectionSign * PushInput;
					ActiveCount++;
				}
				else if (!bPushOnlyWithW && PushInput < -0.1f)
				{
					// bPushOnlyWithW가 false인 경우에만 S키 후진 허용
					bool bCCW = PusherIsCCWMap.Contains(Pusher) ? PusherIsCCWMap[Pusher] : true;
					float DirectionSign = bCCW ? 1.0f : -1.0f;
					TotalSignedInput += DirectionSign * PushInput;
					ActiveCount++;
				}
			}

			if (ActiveCount > 0)
			{
				float AvgInput = TotalSignedInput / ActiveCount;
				float TotalMaxDegrees = FMath::Max(1.0f, FullTurnsForFullOpen * 360.0f);
				float DeltaDegrees = AvgInput * TurnSpeedDegreesPerSec * DeltaTime;
				float PrevProgress = CurrentTurnProgress;
				float DeltaProgress = DeltaDegrees / TotalMaxDegrees;

				CurrentTurnProgress = FMath::Clamp(CurrentTurnProgress + DeltaProgress, 0.0f, 1.0f);
				float ActualDeltaDegrees = (CurrentTurnProgress - PrevProgress) * TotalMaxDegrees;

				if (!FMath::IsNearlyZero(ActualDeltaDegrees))
				{
					FVector WheelCenter = GetActorLocation();

					for (const TObjectPtr<AMainCharacter>& Pusher : CurrentPushers)
					{
						if (!Pusher) continue;

						FVector PusherLoc = Pusher->LastCharacterLocation;
						FVector RadialOffset = PusherLoc - WheelCenter;
						RadialOffset.Z = 0.0f;

						float LockedRadius = PusherLockedRadiusMap.Contains(Pusher)
							? PusherLockedRadiusMap[Pusher]
							: RadialOffset.Size();
						if (LockedRadius < 50.0f)
						{
							LockedRadius = 130.0f;
						}

						FVector RadialDir = RadialOffset.GetSafeNormal();
						if (RadialDir.IsNearlyZero())
						{
							RadialDir = GetActorForwardVector();
						}

						FVector NewRadialDir = FRotator(0.0f, ActualDeltaDegrees, 0.0f).RotateVector(RadialDir);
						FVector NewPusherLoc = WheelCenter + NewRadialDir * LockedRadius;
						NewPusherLoc.Z = Pusher->GetActorLocation().Z;

						Pusher->SetActorLocation(NewPusherLoc, false);
						Pusher->LastCharacterLocation = NewPusherLoc;

						FVector NewTangent = FRotator(0.0f, ActualDeltaDegrees, 0.0f).RotateVector(Pusher->LockedPushDirection);
						Pusher->LockedPushDirection = NewTangent;
						Pusher->SetActorRotation(NewTangent.Rotation());

						FVector NewLocalAnchor = GetActorTransform().InverseTransformPosition(NewPusherLoc);
						PusherLocalAnchorMap.Add(Pusher, NewLocalAnchor);
						Pusher->PushLocalAnchor = NewLocalAnchor;

						// 서버 로컬 플레이어 카메라 회전 동기화
						if (Pusher->IsLocallyControlled() && Pusher->GetController())
						{
							FRotator ControlRot = Pusher->GetController()->GetControlRotation();
							ControlRot.Yaw = FRotator::NormalizeAxis(ControlRot.Yaw + ActualDeltaDegrees);
							Pusher->GetController()->SetControlRotation(ControlRot);
						}
					}

					if (TargetGate)
					{
						TargetGate->SetTargetOpenRatio(CurrentTurnProgress);
					}

					OnWheelTurned.Broadcast(CurrentTurnProgress, ActualDeltaDegrees);
					OnWheelRotated(CurrentTurnProgress, VisualWheelYaw);
				}
			}
		}
	}

	// [클라이언트 전용] 로컬 푸셔 실시간 위치/회전/카메라 예측 회전 동기화
	if (!HasAuthority() && bIsPushable)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		AMainCharacter* LocalPusher = PC ? Cast<AMainCharacter>(PC->GetPawn()) : nullptr;

		if (LocalPusher && LocalPusher->bIsPushingMode && LocalPusher->CurrentPushedObject == this)
		{
			float PushInput = LocalPusher->GetCurrentPushInput();
			if (PushInput > 0.1f)
			{
				bool bCCW = PusherIsCCWMap.Contains(LocalPusher) ? PusherIsCCWMap[LocalPusher] : true;
				float DirectionSign = bCCW ? 1.0f : -1.0f;
				float DeltaDegrees = DirectionSign * PushInput * TurnSpeedDegreesPerSec * DeltaTime;

				bool bCanTurn = true;
				if (DirectionSign > 0.0f && CurrentTurnProgress >= 1.0f) bCanTurn = false;
				if (DirectionSign < 0.0f && CurrentTurnProgress <= 0.0f) bCanTurn = false;

				if (bCanTurn && !FMath::IsNearlyZero(DeltaDegrees))
				{
					FVector WheelCenter = GetActorLocation();
					FVector PusherLoc = LocalPusher->LastCharacterLocation;
					FVector RadialOffset = PusherLoc - WheelCenter;
					RadialOffset.Z = 0.0f;

					float LockedRadius = PusherLockedRadiusMap.Contains(LocalPusher)
						? PusherLockedRadiusMap[LocalPusher]
						: RadialOffset.Size();
					if (LockedRadius < 50.0f)
					{
						LockedRadius = 130.0f;
					}

					FVector RadialDir = RadialOffset.GetSafeNormal();
					if (RadialDir.IsNearlyZero())
					{
						RadialDir = GetActorForwardVector();
					}

					FVector NewRadialDir = FRotator(0.0f, DeltaDegrees, 0.0f).RotateVector(RadialDir);
					FVector NewPusherLoc = WheelCenter + NewRadialDir * LockedRadius;
					NewPusherLoc.Z = LocalPusher->GetActorLocation().Z;

					LocalPusher->SetActorLocation(NewPusherLoc, false);
					LocalPusher->LastCharacterLocation = NewPusherLoc;

					FVector NewTangent = FRotator(0.0f, DeltaDegrees, 0.0f).RotateVector(LocalPusher->LockedPushDirection);
					LocalPusher->LockedPushDirection = NewTangent;
					LocalPusher->SetActorRotation(NewTangent.Rotation());

					if (LocalPusher->GetController())
					{
						FRotator ControlRot = LocalPusher->GetController()->GetControlRotation();
						ControlRot.Yaw = FRotator::NormalizeAxis(ControlRot.Yaw + DeltaDegrees);
						LocalPusher->GetController()->SetControlRotation(ControlRot);
					}
				}
			}
		}
	}

	UpdateWheelVisuals(DeltaTime);
}

void ASluiceWheel::SetTurnProgress(float NewProgress)
{
	CurrentTurnProgress = FMath::Clamp(NewProgress, 0.0f, 1.0f);

	if (!HasAuthority())
	{
		ServerSetTurnProgress(NewProgress);
	}

	if (TargetGate)
	{
		TargetGate->SetTargetOpenRatio(CurrentTurnProgress);
	}
}

void ASluiceWheel::ServerSetTurnProgress_Implementation(float NewProgress)
{
	SetTurnProgress(NewProgress);
}

void ASluiceWheel::OnRep_CurrentTurnProgress()
{
	if (TargetGate)
	{
		TargetGate->SetTargetOpenRatio(CurrentTurnProgress);
	}

	OnWheelTurned.Broadcast(CurrentTurnProgress, 0.0f);
	OnWheelRotated(CurrentTurnProgress, CurrentTurnProgress * FullTurnsForFullOpen * 360.0f);
}

void ASluiceWheel::UpdateWheelVisuals(float DeltaTime)
{
	float TargetYaw = CurrentTurnProgress * FullTurnsForFullOpen * 360.0f;
	VisualWheelYaw = (DeltaTime > 0.0f)
		? FMath::FInterpTo(VisualWheelYaw, TargetYaw, DeltaTime, 10.0f)
		: TargetYaw;

	if (RotatingWheelMesh)
	{
		RotatingWheelMesh->SetRelativeRotation(FRotator(0.0f, VisualWheelYaw, 0.0f));
	}
}

bool ASluiceWheel::CanInteract_Implementation(AActor* Interactor) const
{
	if (!bIsPushable || !Interactor)
	{
		return false;
	}

	float Dist2D = FVector::Dist2D(Interactor->GetActorLocation(), GetActorLocation());
	if (Dist2D <= MaxPushDistance)
	{
		return true;
	}

	if (RotatingWheelMesh)
	{
		float CompDist = FVector::Dist2D(Interactor->GetActorLocation(), RotatingWheelMesh->GetComponentLocation());
		if (CompDist <= MaxPushDistance)
		{
			return true;
		}
	}

	if (HandleMesh)
	{
		float HandleDist = FVector::Dist2D(Interactor->GetActorLocation(), HandleMesh->GetComponentLocation());
		if (HandleDist <= MaxPushDistance)
		{
			return true;
		}
	}

	return false;
}

void ASluiceWheel::Interact_Implementation(AActor* Interactor)
{
	if (!bIsPushable || !Interactor)
	{
		return;
	}

	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	if (AMainCharacter* MainChar = Cast<AMainCharacter>(Interactor))
	{
		// 1. 로컬에서 즉시 손잡이 위치로 스냅하여 네트워크 지연 없이 즉각 정렬
		SnapPusherToHandle(MainChar);

		// 2. 푸시 모드 시작
		MainChar->StartPushMode(this);
	}
}
