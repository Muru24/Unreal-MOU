#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/CustomizationTypes.h"
#include "CharacterCustomizationWidget.generated.h"

class AMainCharacter;
class UColorPickerWidget;
class UCharacterCustomizationComponent;
class UCustomizationDataAsset;

/**
 * 캐릭터 외형 커스터마이징 UMG 위젯의 C++ 베이스 클래스
 */
UCLASS()
class TEAMPROJECT_MOU_API UCharacterCustomizationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintPure, Category = "Customization|UI")
	int32 GetAvailablePresetCount() const;

	// ---------------------------------------------------------
	// [실시간 프리뷰 조작 함수 (슬라이더/컬러피커 연동용)]
	// ---------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetBodyColor(FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetMetallic(float InMetallic);

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetRoughnessB(float InRoughnessB);

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetRoughnessA(float InRoughnessA);

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetDecalIndex(int32 InIndex);

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetDecalsColor(FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetTilingX(float InTilingX);

	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void SetTilingY(float InTilingY);

	// ---------------------------------------------------------
	// [컬러 피커 팝업 연동 (토글 지원)]
	// ---------------------------------------------------------
	// 바디 컬러 피커 팝업 열기/닫기 토글 (이미 열려있으면 닫힘)
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	UColorPickerWidget* OpenBodyColorPicker();

	// 데칼 컬러 피커 팝업 열기/닫기 토글 (이미 열려있으면 닫힘)
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	UColorPickerWidget* OpenDecalColorPicker();

	// 현재 열려있는 컬러 피커 팝업 닫기
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void CloseColorPicker();

	// 현재 컬러 피커가 열려있는지 여부
	UFUNCTION(BlueprintPure, Category = "Customization|UI")
	bool IsColorPickerOpen() const;

	// 컬러 피커 위젯 클래스 (WBP_ColorPicker 할당)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|UI")
	TSubclassOf<class UColorPickerWidget> ColorPickerWidgetClass;

	// ---------------------------------------------------------
	// [확정 / 취소 / 프리셋 조작 버튼]
	// ---------------------------------------------------------
	// 변경사항 저장 및 서버 동기화, UI 닫기
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	virtual void ConfirmAndSave();

	// 변경사항 롤백 및 UI 닫기
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	virtual void CancelAndExit();

	// 기본값으로 되돌리기
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void ResetToDefault();

	// 특정 프리셋 적용
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	void ApplyPreset(int32 PresetIndex);

	// 마우스 드래그를 통한 캐릭터 좌우 회전 (DeltaX 입력)
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	virtual void RotateCharacter(float DeltaX);

	// 현재 편집 중인 커스터마이징 데이터 반환
	UFUNCTION(BlueprintPure, Category = "Customization|UI")
	const FCharacterCustomizationData& GetCurrentCustomizationData() const { return CurrentData; }

	// 사용 가능한 데칼 개수 반환
	UFUNCTION(BlueprintPure, Category = "Customization|UI")
	int32 GetAvailableDecalCount() const;

	// 특정 인덱스의 데칼 텍스처 반환
	UFUNCTION(BlueprintCallable, Category = "Customization|UI")
	class UTexture2D* GetDecalTexture(int32 Index) const;

protected:
	virtual void InitializeCustomization();
	virtual void UpdatePreview();
	UCustomizationDataAsset* GetEditingDataAsset() const;
	void CloseColorPickers();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Customization|UI")
	TObjectPtr<UCustomizationDataAsset> EditingDataAsset;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UColorPickerWidget>> OpenColorPickers;
	// 현재 UI에서 조절 중인 실시간 데이터
	UPROPERTY(BlueprintReadOnly, Category = "Customization|UI")
	FCharacterCustomizationData CurrentData;

	// 진입 시점의 원본 데이터 (취소 시 롤백용)
	UPROPERTY(BlueprintReadOnly, Category = "Customization|UI")
	FCharacterCustomizationData OriginalData;

	// 캐싱된 대상 캐릭터 및 컴포넌트
	UPROPERTY(BlueprintReadOnly, Category = "Customization|UI")
	TWeakObjectPtr<AMainCharacter> CachedCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Customization|UI")
	TWeakObjectPtr<UCharacterCustomizationComponent> CachedCustomizationComp;

	// 마우스 드래그 회전 감도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization|UI")
	float DragRotationSpeed = 0.5f;

	// 현재 활성화된 컬러 피커 팝업 위젯 (토글 및 중복 방지용)
	UPROPERTY(BlueprintReadOnly, Category = "Customization|UI")
	TObjectPtr<class UColorPickerWidget> ActiveColorPicker = nullptr;

	// 현재 열려있는 피커 종류 (0: 없음, 1: 바디 컬러, 2: 데칼 컬러)
	UPROPERTY(BlueprintReadOnly, Category = "Customization|UI")
	int32 ActiveColorPickerType = 0;

	// UI 위젯의 초기 슬라이더/버튼 값 세팅을 알리는 Blueprint 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Customization|UI")
	void OnCustomizationDataInitialized(const FCharacterCustomizationData& InitialData);


};
