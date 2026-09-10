#pragma once

#include "CoreMinimal.h"
#include "Base/ItemBase.h"
#include "RadioItem.generated.h"

class USoundBase;
class UAudioComponent;

// ---------------------------------------------------------
// [라디오 아이템]
// AItemBase를 상속한 아이템. 손에 들면 노래가 자동 재생되고, 좌클릭하면 다음 곡으로 넘어간다.
// - 장착(OnEquipped)  : 현재 곡 재생 시작
// - 좌클릭(OnUse)      : 다음 곡으로 전환 (서버 권위, 3D 음향으로 근처 모두에게 동기화)
// - 해제(OnUnequipped) : 재생 정지
// 곡 목록은 BP의 Playlist 배열에서 편집한다. 메시는 AItemBase의 MeshComponent 사용.
// ---------------------------------------------------------
UCLASS()
class TEAMPROJECT_MOU_API ARadioItem : public AItemBase
{
	GENERATED_BODY()

public:
	ARadioItem();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// ---------------------------------------------------------
	// [컴포넌트]
	// ---------------------------------------------------------
	// 3D 음향 재생용 오디오 컴포넌트 (라디오 위치에서 재생 → 거리 감쇠, 위치 따라다님)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> AudioComponent;

	// ---------------------------------------------------------
	// [곡 목록]
	// ---------------------------------------------------------
	// 재생할 곡 목록 (BP에서 편집). 좌클릭으로 순서대로 넘어가며, 끝나면 처음으로 순환한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Radio")
	TArray<TObjectPtr<USoundBase>> Playlist;

	// 현재 재생 중인 곡 인덱스 (서버가 정하고 모든 클라에 복제)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentTrackIndex, Category = "Radio")
	int32 CurrentTrackIndex = 0;

	UFUNCTION()
	void OnRep_CurrentTrackIndex();

	// 현재 재생 중인지 여부 (서버가 손에 들림/놓임에 따라 정하고 모든 클라에 복제).
	// 손에 든 트리거(PickUp/OnEquipped)가 경로마다 제각각이라, 재생 여부를 상태로 두고 복제하는 게 가장 견고하다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_bIsPlaying, Category = "Radio")
	bool bIsPlaying = false;

	UFUNCTION()
	void OnRep_bIsPlaying();

	// ---------------------------------------------------------
	// [AItemBase 오버라이드]
	// ---------------------------------------------------------
	// 좌클릭: 다음 곡으로 전환
	virtual void OnUse_Implementation() override;

	// 손에 든/보관되는 상태 전환 훅들. 서버에서 bIsPlaying을 켜고/끈다(OnRep이 각 클라 재생/정지 처리).
	// 바닥에서 줍기(PickUp)·인벤토리에서 꺼내기(OnEquipped)는 재생 ON,
	// 놓기(Drop)·던지기(Throw)·인벤토리 수납(OnUnequipped)은 재생 OFF.
	virtual void OnEquipped_Implementation(AActor* Equipper) override;
	virtual void OnUnequipped_Implementation(AActor* Equipper) override;

	// 집을 때: 소유권 설정(클라가 ServerNextTrack RPC를 보낼 수 있게, [WEAPON-010]) + 재생 ON
	virtual void PickUp_Implementation(AActor* Picker) override;

	// 놓기/던지기: 소유권 해제 + 재생 OFF
	virtual void Drop_Implementation(FVector DropLocation, AActor* Dropper = nullptr) override;
	virtual void Throw_Implementation(FVector ThrowVelocity, AActor* Thrower = nullptr) override;

private:
	// [RADIO-001] 현재 CurrentTrackIndex의 곡을 재생 (없으면 정지). 서버/클라 공용 로컬 재생 처리.
	void PlayCurrentTrack();

	// [RADIO-002] 재생 정지 (로컬)
	void StopPlaying();

	// [RADIO-003] 클라이언트 -> 서버 "다음 곡" 위임 (WeaponItemBase의 ServerFire와 동일 패턴)
	UFUNCTION(Server, Reliable)
	void ServerNextTrack();
};
