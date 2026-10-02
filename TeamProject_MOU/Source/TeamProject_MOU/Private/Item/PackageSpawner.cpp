#include "Item/PackageSpawner.h"
#include "Base/PackageBase.h"
#include "Components/SceneComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "TimerManager.h"

APackageSpawner::APackageSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	EditorBillboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorBillboard"));
	if (EditorBillboard)
	{
		EditorBillboard->SetupAttachment(RootComponent);
		EditorBillboard->bIsScreenSizeScaled = true;
	}

	EditorArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EditorArrow"));
	if (EditorArrow)
	{
		EditorArrow->SetupAttachment(RootComponent);
		EditorArrow->ArrowColor = FColor(255, 140, 0);
		EditorArrow->ArrowSize = 1.0f;
	}
#endif

	SpawnCountWeights.Add(1.0f);
	SpawnCountWeights.Add(4.0f);
	SpawnCountWeights.Add(2.0f);

	MinSpawnCount = 0;
	MaxSpawnCount = 2;
	MultiSpawnSpacing = 70.0f;
	AutoSpawnDelay = 0.5f;
	GroundTraceDistance = 300.0f;
}

void APackageSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() || !bAutoSpawnOnBeginPlay)
	{
		return;
	}

	if (AutoSpawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			GetWorldTimerManager().SetTimer(
				AutoSpawnTimerHandle, this, &APackageSpawner::SpawnPackages, AutoSpawnDelay, false);
		}));
	}
	else
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &APackageSpawner::SpawnPackages);
	}
}

void APackageSpawner::SpawnPackages()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearSpawnedPackages();

	const int32 CountToSpawn = DetermineSpawnCount();
	if (CountToSpawn <= 0)
	{
		return;
	}

	TArray<TSubclassOf<APackageBase>> SpawnedClasses;
	TArray<APackageBase*> NewlySpawnedPackages;

	for (int32 Index = 0; Index < CountToSpawn; ++Index)
	{
		TSubclassOf<APackageBase> ChosenClass = PickRandomPackageClass(SpawnedClasses);
		if (!ChosenClass)
		{
			continue;
		}

		if (!bAllowDuplicateClasses)
		{
			SpawnedClasses.Add(ChosenClass);
		}

		FVector TargetLocation = CalculateSpawnLocation(Index, CountToSpawn);
		if (bSnapToGround)
		{
			TargetLocation = SnapLocationToGround(TargetLocation);
		}

		FRotator TargetRotation = GetActorRotation();
		if (bRandomYaw)
		{
			TargetRotation.Yaw = FMath::FRandRange(0.0f, 360.0f);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		APackageBase* NewPackage = GetWorld()->SpawnActor<APackageBase>(
			ChosenClass, TargetLocation, TargetRotation, SpawnParams);

		if (IsValid(NewPackage))
		{
			SpawnedPackages.Add(NewPackage);
			NewlySpawnedPackages.Add(NewPackage);
		}
	}

	if (OnPackagesSpawned.IsBound())
	{
		OnPackagesSpawned.Broadcast(NewlySpawnedPackages);
	}
}

void APackageSpawner::ClearSpawnedPackages()
{
	if (!HasAuthority())
	{
		return;
	}

	for (TObjectPtr<APackageBase>& Package : SpawnedPackages)
	{
		if (IsValid(Package))
		{
			Package->Destroy();
		}
	}

	SpawnedPackages.Empty();
}

TArray<APackageBase*> APackageSpawner::GetSpawnedPackages() const
{
	TArray<APackageBase*> ValidPackages;
	for (const TObjectPtr<APackageBase>& Package : SpawnedPackages)
	{
		if (IsValid(Package))
		{
			ValidPackages.Add(Package.Get());
		}
	}
	return ValidPackages;
}

int32 APackageSpawner::DetermineSpawnCount() const
{
	if (bUseCountWeights)
	{
		if (SpawnCountWeights.IsEmpty())
		{
			return 0;
		}

		float TotalWeight = 0.0f;
		for (float W : SpawnCountWeights)
		{
			if (W > 0.0f)
			{
				TotalWeight += W;
			}
		}

		if (TotalWeight <= 0.0f)
		{
			return 0;
		}

		const float RandomPoint = FMath::FRandRange(0.0f, TotalWeight);
		float AccumulatedWeight = 0.0f;

		for (int32 Index = 0; Index < SpawnCountWeights.Num(); ++Index)
		{
			const float W = SpawnCountWeights[Index];
			if (W <= 0.0f)
			{
				continue;
			}

			AccumulatedWeight += W;
			if (RandomPoint <= AccumulatedWeight)
			{
				return Index;
			}
		}

		return 0;
	}

	const int32 SafeMin = FMath::Max(0, MinSpawnCount);
	const int32 SafeMax = FMath::Max(SafeMin, MaxSpawnCount);
	return FMath::RandRange(SafeMin, SafeMax);
}

TSubclassOf<APackageBase> APackageSpawner::PickRandomPackageClass(const TArray<TSubclassOf<APackageBase>>& ExcludedClasses) const
{
	if (PackageEntries.IsEmpty())
	{
		return nullptr;
	}

	float TotalWeight = 0.0f;
	TArray<const FPackageSpawnEntry*> ValidEntries;

	for (const FPackageSpawnEntry& Entry : PackageEntries)
	{
		if (Entry.Weight <= 0.0f)
		{
			continue;
		}

		if (!bAllowDuplicateClasses && Entry.PackageClass && ExcludedClasses.Contains(Entry.PackageClass))
		{
			continue;
		}

		TotalWeight += Entry.Weight;
		ValidEntries.Add(&Entry);
	}

	if (TotalWeight <= 0.0f || ValidEntries.IsEmpty())
	{
		return nullptr;
	}

	const float RandomPoint = FMath::FRandRange(0.0f, TotalWeight);
	float AccumulatedWeight = 0.0f;

	for (const FPackageSpawnEntry* EntryPtr : ValidEntries)
	{
		AccumulatedWeight += EntryPtr->Weight;
		if (RandomPoint <= AccumulatedWeight)
		{
			return EntryPtr->PackageClass;
		}
	}

	return ValidEntries.Last()->PackageClass;
}

FVector APackageSpawner::CalculateSpawnLocation(int32 Index, int32 TotalCount) const
{
	const FVector Origin = GetActorLocation();

	if (TotalCount <= 1)
	{
		return Origin;
	}

	const float OffsetRatio = static_cast<float>(Index) - (static_cast<float>(TotalCount - 1) * 0.5f);
	const FVector RightDir = GetActorRightVector();
	return Origin + (RightDir * (OffsetRatio * MultiSpawnSpacing));
}

FVector APackageSpawner::SnapLocationToGround(const FVector& InLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return InLocation;
	}

	const FVector TraceStart = InLocation + FVector(0.0f, 0.0f, 100.0f);
	const FVector TraceEnd = InLocation - FVector(0.0f, 0.0f, FMath::Max(10.0f, GroundTraceDistance));

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const bool bHit = World->LineTraceSingleByChannel(
		HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	if (bHit && HitResult.bBlockingHit)
	{
		return HitResult.Location + FVector(0.0f, 0.0f, 5.0f);
	}

	return InLocation;
}
