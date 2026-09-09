#include "Item/VehicleBase.h"

#include "Base/CharacterBase.h"
#include "ChaosVehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"

// [VEHICLE-001] 생성자: 카메라/상호작용 볼륨 구성 및 기본값
AVehicleBase::AVehicleBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// 운전 카메라 붐: 차량 메시에 붙어 차량 방향을 그대로 따라간다(정면 고정).
	// bUsePawnControlRotation 을 끄면 마우스로 카메라를 돌릴 수 없어 "정면 고정"이 된다.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 650.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 150.0f);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bInheritYaw = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// F키 상호작용 감지 범위. 실제 크기는 차량마다 에디터에서 조정.
	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(GetMesh());
	InteractionVolume->SetBoxExtent(FVector(250.0f, 150.0f, 100.0f));
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Overlap);

	// 차량은 네트워크에서 서버 권위로 위치가 복제되어야 한다.
	bReplicates = true;
	SetReplicateMovement(true);
}

void AVehicleBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 좌석 점유 상태를 모든 클라이언트에 복제한다.
	DOREPLIFETIME(AVehicleBase, Seats);
}

void AVehicleBase::BeginPlay()
{
	Super::BeginPlay();
}

// [VEHICLE-002] 좌석 복제 콜백: 클라이언트에서 좌석 변화 연출 훅 호출
void AVehicleBase::OnRep_Seats()
{
	OnSeatsChanged();
}

// [VEHICLE-010] F키 상호작용 가능 여부: 빈 좌석이 하나라도 있으면 탑승 가능
bool AVehicleBase::CanInteract_Implementation(AActor* Interactor) const
{
	const ACharacterBase* Character = Cast<ACharacterBase>(Interactor);
	if (!Character)
	{
		return false;
	}

	// 이미 이 차량에 타고 있으면(하차는 별도 입력) 상호작용 대상에서 제외
	if (GetSeatIndexOf(Character) != INDEX_NONE)
	{
		return false;
	}

	return FindNearestFreeSeat(Interactor) != INDEX_NONE;
}

// [VEHICLE-011] F키 상호작용 실행: 서버에 탑승 요청 (드론과 동일하게 서버 권위 처리)
void AVehicleBase::Interact_Implementation(AActor* Interactor)
{
	ACharacterBase* Character = Cast<ACharacterBase>(Interactor);
	if (!Character)
	{
		return;
	}

	// 상호작용은 로컬 클라에서도 호출되므로, 상태 변경은 서버 RPC로 위임한다.
	ServerRequestEnter(Character);
}

FText AVehicleBase::GetInteractPrompt_Implementation() const
{
	return FText::FromString(TEXT("탑승"));
}

// [VEHICLE-020] 가장 가까운 빈 좌석 찾기 (운전석 우선 없음 - 순수 거리 기준)
int32 AVehicleBase::FindNearestFreeSeat(const AActor* ForActor) const
{
	if (!ForActor)
	{
		return INDEX_NONE;
	}

	const FVector FromLoc = ForActor->GetActorLocation();
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();

	const USkeletalMeshComponent* MeshComp = GetMesh();

	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (Seats[i].Occupant != nullptr)
		{
			continue; // 이미 점유된 좌석
		}

		// 좌석 소켓 위치 기준 거리. 소켓이 없으면 차량 위치로 대체.
		FVector SeatLoc = GetActorLocation();
		if (MeshComp && Seats[i].SeatSocketName != NAME_None && MeshComp->DoesSocketExist(Seats[i].SeatSocketName))
		{
			SeatLoc = MeshComp->GetSocketLocation(Seats[i].SeatSocketName);
		}

		const float DistSq = FVector::DistSquared(FromLoc, SeatLoc);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = i;
		}
	}

	return BestIndex;
}

// [VEHICLE-021] 특정 캐릭터가 앉은 좌석 인덱스 조회
int32 AVehicleBase::GetSeatIndexOf(const ACharacterBase* Character) const
{
	if (!Character)
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (Seats[i].Occupant == Character)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

// [VEHICLE-022] 현재 운전자(운전석 탑승자) 반환
ACharacterBase* AVehicleBase::GetDriver() const
{
	for (const FVehicleSeat& Seat : Seats)
	{
		if (Seat.bIsDriverSeat && Seat.Occupant != nullptr)
		{
			return Seat.Occupant;
		}
	}
	return nullptr;
}

// [VEHICLE-030] 서버: 탑승 요청 처리
void AVehicleBase::ServerRequestEnter_Implementation(ACharacterBase* Requestor)
{
	if (!Requestor)
	{
		return;
	}
	EnterVehicle(Requestor);
}

// [VEHICLE-031] 서버: 하차 요청 처리
void AVehicleBase::ServerRequestExit_Implementation(ACharacterBase* Requestor)
{
	if (!Requestor)
	{
		return;
	}
	ExitVehicle(Requestor);
}

// [VEHICLE-032] 탑승 실제 처리 (서버 권위)
bool AVehicleBase::EnterVehicle(ACharacterBase* NewOccupant)
{
	if (!HasAuthority() || !NewOccupant)
	{
		return false;
	}

	// 이미 탑승 중이면 무시
	if (GetSeatIndexOf(NewOccupant) != INDEX_NONE)
	{
		return false;
	}

	const int32 SeatIndex = FindNearestFreeSeat(NewOccupant);
	if (SeatIndex == INDEX_NONE)
	{
		return false; // 빈 좌석 없음
	}

	SeatCharacter(NewOccupant, SeatIndex);
	return true;
}

// [VEHICLE-033] 하차 실제 처리 (서버 권위)
void AVehicleBase::ExitVehicle(ACharacterBase* Occupant)
{
	if (!HasAuthority() || !Occupant)
	{
		return;
	}

	const int32 SeatIndex = GetSeatIndexOf(Occupant);
	if (SeatIndex == INDEX_NONE)
	{
		return;
	}

	UnseatCharacter(Occupant, SeatIndex);
}

// [VEHICLE-040] 좌석 배정 + Attach + (운전석이면) 차량 Possess (서버 전용)
void AVehicleBase::SeatCharacter(ACharacterBase* Character, int32 SeatIndex)
{
	if (!Seats.IsValidIndex(SeatIndex) || !Character)
	{
		return;
	}

	FVehicleSeat& Seat = Seats[SeatIndex];
	Seat.Occupant = Character;

	// [임시 진단 로그] 어느 좌석에 탔고 운전석 플래그/컨트롤러 상태가 어떤지 확인용.
	UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] SeatCharacter: SeatIndex=%d, bIsDriverSeat=%d, Controller=%s"),
		SeatIndex,
		Seat.bIsDriverSeat ? 1 : 0,
		Character->GetController() ? *Character->GetController()->GetName() : TEXT("NULL"));

	// 모든 머신에서 캐릭터를 좌석 소켓에 붙이고 이동/충돌을 잠근다.
	MulticastAttachOccupant(Character, SeatIndex);

	// 운전석이면 컨트롤러를 차량으로 옮겨(Possess) 운전 입력을 넘겨받는다.
	if (Seat.bIsDriverSeat)
	{
		if (AController* DriverController = Character->GetController())
		{
			// 하차 시 원래 캐릭터로 되돌리기 위해 짝을 기억한다.
			CachedDriverController = DriverController;
			CachedDriverCharacter = Character;

			DriverController->Possess(this);

			UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] Possess 실행됨 -> 이제 이 차량의 Controller=%s"),
				GetController() ? *GetController()->GetName() : TEXT("NULL"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[VEHICLE] 운전석인데 Character->GetController()가 NULL -> Possess 못함"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] 이 좌석은 운전석이 아님 -> Possess 안함 (동승석에 탄 것)"));
	}

	// 서버에서도 좌석 변화 연출 훅 호출 (OnRep 은 클라 전용이므로)
	OnSeatsChanged();
}

// [VEHICLE-041] 좌석 해제 + Detach + (운전자면) 원래 캐릭터 Possess 복귀 (서버 전용)
void AVehicleBase::UnseatCharacter(ACharacterBase* Character, int32 SeatIndex)
{
	if (!Seats.IsValidIndex(SeatIndex) || !Character)
	{
		return;
	}

	FVehicleSeat& Seat = Seats[SeatIndex];
	const bool bWasDriver = Seat.bIsDriverSeat;

	// 운전자였다면 조종 입력을 0으로 정리한 뒤 컨트롤러를 캐릭터로 되돌린다.
	if (bWasDriver && CachedDriverController && CachedDriverCharacter == Character)
	{
		if (UChaosVehicleMovementComponent* Movement = GetVehicleMovementComponent())
		{
			Movement->SetThrottleInput(0.0f);
			Movement->SetBrakeInput(0.0f);
			Movement->SetSteeringInput(0.0f);
			Movement->SetHandbrakeInput(true);
		}

		AController* DriverController = CachedDriverController;
		DriverController->Possess(Character);

		CachedDriverController = nullptr;
		CachedDriverCharacter = nullptr;
	}

	Seat.Occupant = nullptr;

	// 하차 위치: 차량 오른쪽 옆(운전석 반대편이 아니라 일단 차량 옆 안전 위치).
	const FVector ExitLocation = GetActorLocation()
		+ GetActorRightVector() * 200.0f
		+ FVector(0.0f, 0.0f, 50.0f);

	MulticastDetachOccupant(Character, ExitLocation);

	OnSeatsChanged();
}

// [VEHICLE-050] 모든 머신: 캐릭터를 좌석 소켓에 붙이고 이동/충돌 잠금
void AVehicleBase::MulticastAttachOccupant_Implementation(ACharacterBase* Character, int32 SeatIndex)
{
	if (!Character || !Seats.IsValidIndex(SeatIndex))
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	const FName SocketName = Seats[SeatIndex].SeatSocketName;

	if (MeshComp && SocketName != NAME_None && MeshComp->DoesSocketExist(SocketName))
	{
		Character->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	}
	else
	{
		// 소켓 미지정 시 루트에 그냥 붙인다 (에디터 셋업 전 임시).
		Character->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}

	// 캐릭터 이동/충돌 잠금: 탑승 중 걸어다니거나 물리 충돌하지 않도록.
	if (UCharacterMovementComponent* CharMove = Character->GetCharacterMovement())
	{
		CharMove->DisableMovement();
	}
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// [VEHICLE-051] 모든 머신: 캐릭터 Detach + 이동/충돌 복구 + 안전 위치 이동
void AVehicleBase::MulticastDetachOccupant_Implementation(ACharacterBase* Character, FVector ExitLocation)
{
	if (!Character)
	{
		return;
	}

	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (UCharacterMovementComponent* CharMove = Character->GetCharacterMovement())
	{
		CharMove->SetMovementMode(MOVE_Walking);
	}

	// 서버 권위 머신에서만 실제 위치를 옮긴다(복제로 클라 동기화).
	if (Character->HasAuthority())
	{
		Character->SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

// [VEHICLE-060] 운전 입력 바인딩 (컨트롤러가 이 Pawn 을 Possess 한 동안만 유효)
void AVehicleBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// [임시 진단] 이 함수가 호출됐는지부터 확인.
	UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] SetupPlayerInputComponent 호출됨. Controller=%s, InputComp=%s"),
		GetController() ? *GetController()->GetName() : TEXT("NULL"),
		PlayerInputComponent ? *PlayerInputComponent->GetClass()->GetName() : TEXT("NULL"));

	// 운전자 로컬 컨트롤러에 운전용 입력 매핑 컨텍스트를 추가한다.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DrivingMappingContext)
			{
				Subsystem->AddMappingContext(DrivingMappingContext, 1);
				UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] DrivingMappingContext 추가됨"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[VEHICLE] DrivingMappingContext 가 NULL -> BP에서 지정 안됨"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[VEHICLE] EnhancedInput Subsystem 못 찾음"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[VEHICLE] GetController()가 PlayerController 아님 -> IMC 추가 못함"));
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[VEHICLE] EnhancedInputComponent OK. Throttle=%s, Steer=%s, Exit=%s"),
			ThrottleAction ? TEXT("있음") : TEXT("NULL"),
			SteerAction ? TEXT("있음") : TEXT("NULL"),
			ExitAction ? TEXT("있음") : TEXT("NULL"));

		if (ThrottleAction)
		{
			EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AVehicleBase::OnThrottleInput);
			EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AVehicleBase::OnThrottleInput);
		}
		if (SteerAction)
		{
			EIC->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AVehicleBase::OnSteerInput);
			EIC->BindAction(SteerAction, ETriggerEvent::Completed, this, &AVehicleBase::OnSteerInput);
		}
		if (ExitAction)
		{
			// 하차: 서버에 요청. Possess 로 이 Pawn 이 조종 중이므로 GetDriver 로 운전자를 찾는다.
			EIC->BindAction(ExitAction, ETriggerEvent::Started, this, &AVehicleBase::OnExitInput);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[VEHICLE] PlayerInputComponent가 EnhancedInputComponent 아님 -> 액션 바인딩 못함"));
	}
}

// [VEHICLE-061] W/S : 전진/후진. Axis1D 입력값을 꺼내 스로틀/브레이크로 변환.
void AVehicleBase::OnThrottleInput(const FInputActionValue& Value)
{
	UChaosVehicleMovementComponent* Movement = GetVehicleMovementComponent();
	if (!Movement)
	{
		return;
	}

	const float Axis = Value.Get<float>();

	// Axis > 0 : 전진(스로틀), Axis < 0 : 후진(브레이크/리버스)
	if (Axis >= 0.0f)
	{
		Movement->SetThrottleInput(Axis);
		Movement->SetBrakeInput(0.0f);
	}
	else
	{
		Movement->SetThrottleInput(0.0f);
		Movement->SetBrakeInput(-Axis);
	}
}

// [VEHICLE-062] A/D : 조향. 누르는 동안 Triggered 로 값이 계속 들어온다.
void AVehicleBase::OnSteerInput(const FInputActionValue& Value)
{
	if (UChaosVehicleMovementComponent* Movement = GetVehicleMovementComponent())
	{
		Movement->SetSteeringInput(Value.Get<float>());
	}
}

// [VEHICLE-063] 하차 입력: 운전석 탑승자를 찾아 서버에 하차 요청
void AVehicleBase::OnExitInput()
{
	// 이 Pawn 을 Possess 한 운전자가 하차 대상이다.
	if (ACharacterBase* Driver = GetDriver())
	{
		ServerRequestExit(Driver);
	}
}

void AVehicleBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
