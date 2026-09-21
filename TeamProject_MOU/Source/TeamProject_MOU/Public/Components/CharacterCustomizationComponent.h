#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CustomizationTypes.h"
#include "CharacterCustomizationComponent.generated.h"

class ACharacter;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCustomizationDataChanged, const FCharacterCustomizationData&, NewData);

/**
 * 캐릭터의 머티리얼 파라미터 커스터마이징 및 멀티플레이 동기화를 전담하는 컴포넌트
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TEAMPROJECT_MOU_API UCharacterCustomizationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterCustomizationComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 커스터마이징 변경 시 브로드캐스트되는 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Customization")
	FOnCustomizationDataChanged OnCustomizationDataChanged;

	// 현재 동기화된 커스터마이징 데이터 반환
	UFUNCTION(BlueprintPure, Category = "Customization")
	const FCharacterCustomizationData& GetCustomizationData() const { return CustomizationData; }

	// 현재 데이터 에셋 반환
	UFUNCTION(BlueprintPure, Category = "Customization")
	UCustomizationDataAsset* GetCustomizationDataAsset() const { return CustomizationDataAsset; }

	// UI 슬라이더 조작 시 로컬 DMI에만 실시간으로 즉시 반영 (서버 통신 X)
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyPreview(const FCharacterCustomizationData& InPreviewData);

	// 커스터마이징 적용 및 서버로 동기화 요청 (확정 버튼 클릭 시 호출)
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ConfirmAndApplyCustomization(const FCharacterCustomizationData& InNewData);

	// 로컬 세이브 파일에서 커스터마이징 불러오기
	UFUNCTION(BlueprintCallable, Category = "Customization|Save")
	bool LoadCustomizationFromDisk(FCharacterCustomizationData& OutData);

	// 로컬 세이브 파일에 커스터마이징 저장
	UFUNCTION(BlueprintCallable, Category = "Customization|Save")
	bool SaveCustomizationToDisk(const FCharacterCustomizationData& InData);

	// 머티리얼 DMI 수동 재초기화 및 현재 데이터 재적용
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ReinitializeAndApply();

	/** Local preview only: supports a SceneCapture actor with its own skeletal mesh. */
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetPreviewMesh(USkeletalMeshComponent* Mesh);

	/** Called again after possession/client restart because BeginPlay can precede ownership. */
	void ApplyLocalCustomization();


	// 서버로 커스터마이징 데이터 전송 RPC
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Customization|Network")
	void ServerSetCustomizationData(const FCharacterCustomizationData& NewData);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	TObjectPtr<UCustomizationDataAsset> CustomizationDataAsset;

	// 네트워크 복제되는 현재 커스터마이징 데이터
	UPROPERTY(ReplicatedUsing = OnRep_CustomizationData, VisibleAnywhere, BlueprintReadOnly, Category = "Customization")
	FCharacterCustomizationData CustomizationData;

	UFUNCTION()
	void OnRep_CustomizationData();

	// 캐싱된 신체 동적 머티리얼 인스턴스 목록
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMaterialInstances;

private:
	// 동적 머티리얼 인스턴스 초기화
	void InitDynamicMaterials();

	// 실제 DMI에 파라미터 적용
	void ApplyDataToMaterials(const FCharacterCustomizationData& InData);

	TWeakObjectPtr<ACharacter> OwnerCharacter;
	TWeakObjectPtr<USkeletalMeshComponent> PreviewMesh;
};
