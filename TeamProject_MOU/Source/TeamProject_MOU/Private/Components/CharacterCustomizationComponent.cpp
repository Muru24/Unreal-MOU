#include "Components/CharacterCustomizationComponent.h"
#include "Server/ServerSubsystem.h"
#include "Server/Net/CustomizationWire.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

UCharacterCustomizationComponent::UCharacterCustomizationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCharacterCustomizationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCharacterCustomizationComponent, CustomizationData);
}

void UCharacterCustomizationComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (!CustomizationDataAsset)
	{
		CustomizationDataAsset = NewObject<UCustomizationDataAsset>(this, TEXT("DefaultCustomizationDataAsset"));
	}

	InitDynamicMaterials();

	ApplyDataToMaterials(CustomizationData);
	ApplyLocalCustomization();
}

void UCharacterCustomizationComponent::ApplyLocalCustomization()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!HasBegunPlay() || !Character || !Character->IsLocallyControlled()) return;
	if (UServerSubsystem* Server = UServerSubsystem::Get(this))
	{
		const auto Data = Server->GetLocalCustomization();
		ApplyDataToMaterials(Data);
		ServerSetCustomizationData(Data);
	}
}

void UCharacterCustomizationComponent::SetPreviewMesh(USkeletalMeshComponent* Mesh)
{
	if (PreviewMesh.Get() == Mesh) return;
	PreviewMesh = Mesh;
	ReinitializeAndApply();
}

void UCharacterCustomizationComponent::InitDynamicMaterials()
{
	BodyMaterialInstances.Empty();

	USkeletalMeshComponent* MeshComp = PreviewMesh.Get();
	if (!MeshComp && OwnerCharacter.IsValid()) MeshComp = OwnerCharacter->GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const int32 NumMaterials = MeshComp->GetNumMaterials();
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		UMaterialInterface* ExistingMat = MeshComp->GetMaterial(i);
		if (UMaterialInstanceDynamic* ExistingDMI = Cast<UMaterialInstanceDynamic>(ExistingMat))
		{
			BodyMaterialInstances.Add(ExistingDMI);
		}
		else if (ExistingMat)
		{
			UMaterialInstanceDynamic* NewDMI = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
			if (NewDMI)
			{
				BodyMaterialInstances.Add(NewDMI);
			}
		}
	}
}

void UCharacterCustomizationComponent::ReinitializeAndApply()
{
	InitDynamicMaterials();
	ApplyDataToMaterials(CustomizationData);
}

void UCharacterCustomizationComponent::ApplyPreview(const FCharacterCustomizationData& InPreviewData)
{
	ApplyDataToMaterials(InPreviewData);
}

void UCharacterCustomizationComponent::ConfirmAndApplyCustomization(const FCharacterCustomizationData& InNewData)
{
	if (!MOU::IsValidCustomization(MOUCustomization::ToWire(InNewData))) return;
	if (UServerSubsystem* Server = UServerSubsystem::Get(this)) Server->CacheLocalCustomization(InNewData);
	// 1. 로컬 즉시 반영
	ApplyDataToMaterials(InNewData);

	// 2. 로컬 디스크에 영구 저장
	SaveCustomizationToDisk(InNewData);

	// 3. 서버로 동기화 전송 (다른 플레이어에게 복제)
	ServerSetCustomizationData(InNewData);
}

void UCharacterCustomizationComponent::ServerSetCustomizationData_Implementation(const FCharacterCustomizationData& NewData)
{
	if (!MOU::IsValidCustomization(MOUCustomization::ToWire(NewData))) return;
	CustomizationData = NewData;

	// 서버(호스트)에서도 머티리얼 즉시 반영
	ApplyDataToMaterials(CustomizationData);
	OnCustomizationDataChanged.Broadcast(CustomizationData);
}

void UCharacterCustomizationComponent::OnRep_CustomizationData()
{
	ApplyDataToMaterials(CustomizationData);
	OnCustomizationDataChanged.Broadcast(CustomizationData);
}

void UCharacterCustomizationComponent::ApplyDataToMaterials(const FCharacterCustomizationData& InData)
{
	if (BodyMaterialInstances.Num() == 0)
	{
		InitDynamicMaterials();
	}

	if (!CustomizationDataAsset)
	{
		return;
	}

	const FName BodyColorParam = CustomizationDataAsset->BodyColorParamName;
	const FName MetallicParam = CustomizationDataAsset->MetallicParamName;
	const FName RoughnessBParam = CustomizationDataAsset->RoughnessBParamName;
	const FName RoughnessAParam = CustomizationDataAsset->RoughnessAParamName;
	const FName RoughnessAFallback = CustomizationDataAsset->RoughnessAFallbackParamName;
	const FName DecalsParam = CustomizationDataAsset->DecalsParamName;
	const FName DecalsColorParam = CustomizationDataAsset->DecalsColorParamName;
	const FName TilingXParam = CustomizationDataAsset->TilingXParamName;
	const FName TilingYParam = CustomizationDataAsset->TilingYParamName;

	UTexture2D* DecalTexture = CustomizationDataAsset->GetDecalTexture(InData.DecalIndex);

	for (UMaterialInstanceDynamic* DMI : BodyMaterialInstances)
	{
		if (DMI)
		{
			// 1. Base 파라미터 적용
			DMI->SetVectorParameterValue(BodyColorParam, InData.BodyColor);
			DMI->SetScalarParameterValue(MetallicParam, InData.Metallic);
			DMI->SetScalarParameterValue(RoughnessBParam, InData.RoughnessB);
			DMI->SetScalarParameterValue(RoughnessAParam, InData.RoughnessA);
			DMI->SetScalarParameterValue(RoughnessAFallback, InData.RoughnessA);

			// 2. Decals 파라미터 적용
			if (DecalTexture)
			{
				DMI->SetTextureParameterValue(DecalsParam, DecalTexture);
			}
			DMI->SetVectorParameterValue(DecalsColorParam, InData.DecalsColor);
			DMI->SetScalarParameterValue(TilingXParam, InData.TilingX);
			DMI->SetScalarParameterValue(TilingYParam, InData.TilingY);
		}
	}
}

bool UCharacterCustomizationComponent::LoadCustomizationFromDisk(FCharacterCustomizationData& OutData)
{
	if (UGameplayStatics::DoesSaveGameExist(UCustomizationSaveGame::SaveSlotName, UCustomizationSaveGame::SaveUserIndex))
	{
		if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(UCustomizationSaveGame::SaveSlotName, UCustomizationSaveGame::SaveUserIndex))
		{
			if (UCustomizationSaveGame* CustomizationSave = Cast<UCustomizationSaveGame>(Loaded))
			{
				OutData = CustomizationSave->SavedCustomization;
				return true;
			}
		}
	}
	return false;
}

bool UCharacterCustomizationComponent::SaveCustomizationToDisk(const FCharacterCustomizationData& InData)
{
	UCustomizationSaveGame* SaveObj = Cast<UCustomizationSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UCustomizationSaveGame::StaticClass()));

	if (SaveObj)
	{
		SaveObj->SavedCustomization = InData;
		return UGameplayStatics::SaveGameToSlot(SaveObj, UCustomizationSaveGame::SaveSlotName, UCustomizationSaveGame::SaveUserIndex);
	}
	return false;
}
