#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Base/PackageBase.h"
#include "PackageSpawner.generated.h"

class USceneComponent;
class UBillboardComponent;
class UArrowComponent;

USTRUCT(BlueprintType)
struct FPackageSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner")
	TSubclassOf<APackageBase> PackageClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPackagesSpawned, const TArray<APackageBase*>&, SpawnedList);

UCLASS()
class TEAMPROJECT_MOU_API APackageSpawner : public AActor
{
	GENERATED_BODY()

public:
	APackageSpawner();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorBillboard;

	UPROPERTY()
	TObjectPtr<UArrowComponent> EditorArrow;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Count")
	bool bUseCountWeights = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Count", meta = (EditCondition = "bUseCountWeights"))
	TArray<float> SpawnCountWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Count", meta = (EditCondition = "!bUseCountWeights", ClampMin = "0", ClampMax = "10"))
	int32 MinSpawnCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Count", meta = (EditCondition = "!bUseCountWeights", ClampMin = "0", ClampMax = "10"))
	int32 MaxSpawnCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Pool")
	TArray<FPackageSpawnEntry> PackageEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Pool")
	bool bAllowDuplicateClasses = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Placement")
	bool bAutoSpawnOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Placement", meta = (ClampMin = "0.0", Units = "s"))
	float AutoSpawnDelay = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Placement", meta = (ClampMin = "0.0"))
	float MultiSpawnSpacing = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Placement")
	bool bRandomYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Placement")
	bool bSnapToGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Package Spawner|Placement", meta = (EditCondition = "bSnapToGround", ClampMin = "10.0"))
	float GroundTraceDistance = 300.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Package Spawner|Runtime")
	TArray<TObjectPtr<APackageBase>> SpawnedPackages;

	FTimerHandle AutoSpawnTimerHandle;

public:
	UPROPERTY(BlueprintAssignable, Category = "Package Spawner|Events")
	FOnPackagesSpawned OnPackagesSpawned;

	UFUNCTION(BlueprintCallable, Category = "Package Spawner")
	void SpawnPackages();

	UFUNCTION(BlueprintCallable, Category = "Package Spawner")
	void ClearSpawnedPackages();

	UFUNCTION(BlueprintPure, Category = "Package Spawner")
	TArray<APackageBase*> GetSpawnedPackages() const;

private:
	int32 DetermineSpawnCount() const;
	TSubclassOf<APackageBase> PickRandomPackageClass(const TArray<TSubclassOf<APackageBase>>& ExcludedClasses) const;
	FVector CalculateSpawnLocation(int32 Index, int32 TotalCount) const;
	FVector SnapLocationToGround(const FVector& InLocation) const;
};
