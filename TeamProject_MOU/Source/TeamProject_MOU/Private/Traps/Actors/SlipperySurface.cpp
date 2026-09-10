#include "Traps/Actors/SlipperySurface.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ASlipperySurface::ASlipperySurface()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	SurfaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SurfaceMesh"));
	SurfaceMesh->SetupAttachment(RootScene);

	SlipperyVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SlipperyVolume"));
	SlipperyVolume->SetupAttachment(SurfaceMesh);
	SlipperyVolume->InitBoxExtent(FVector(200.0f, 200.0f, 30.0f));
	SlipperyVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SlipperyVolume->SetGenerateOverlapEvents(true);
}

void ASlipperySurface::BeginPlay()
{
	Super::BeginPlay();

	if (SlipperyVolume)
	{
		SlipperyVolume->OnComponentBeginOverlap.AddDynamic(this, &ASlipperySurface::HandleSurfaceBeginOverlap);
		SlipperyVolume->OnComponentEndOverlap.AddDynamic(this, &ASlipperySurface::HandleSurfaceEndOverlap);
	}
}

void ASlipperySurface::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 액터 파괴 시 미끄러짐 상태였던 모든 캐릭터의 원래 무브먼트 복원
	for (auto& Pair : OriginalFrictionMap)
	{
		if (Pair.Key)
		{
			RestoreCharacterMovement(Pair.Key);
		}
	}
	OriginalFrictionMap.Empty();
	OriginalBrakingMap.Empty();

	Super::EndPlay(EndPlayReason);
}

void ASlipperySurface::HandleSurfaceBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character && Character->GetCharacterMovement())
	{
		UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
		if (!OriginalFrictionMap.Contains(Character))
		{
			OriginalFrictionMap.Add(Character, MoveComp->GroundFriction);
		}
		if (!OriginalBrakingMap.Contains(Character))
		{
			OriginalBrakingMap.Add(Character, MoveComp->BrakingDecelerationWalking);
		}

		MoveComp->GroundFriction = SlipperyGroundFriction;
		MoveComp->BrakingDecelerationWalking = SlipperyBrakingDeceleration;
	}
}

void ASlipperySurface::HandleSurfaceEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		RestoreCharacterMovement(Character);
	}
}

void ASlipperySurface::RestoreCharacterMovement(ACharacter* Character)
{
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (MoveComp)
	{
		if (float* OrigFriction = OriginalFrictionMap.Find(Character))
		{
			MoveComp->GroundFriction = *OrigFriction;
			OriginalFrictionMap.Remove(Character);
		}
		if (float* OrigBraking = OriginalBrakingMap.Find(Character))
		{
			MoveComp->BrakingDecelerationWalking = *OrigBraking;
			OriginalBrakingMap.Remove(Character);
		}
	}
}
