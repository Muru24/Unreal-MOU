#include "Item/RadioItem.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"

ARadioItem::ARadioItem()
{
	// 3D 음향 재생용 오디오 컴포넌트. 메시(루트)에 붙여 라디오 위치를 따라다니게 한다.
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->bAutoActivate = false;   // 손에 들 때(OnEquipped) 직접 재생
	AudioComponent->bAllowSpatialization = true; // 3D 공간 음향(거리 감쇠)
}

void ARadioItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARadioItem, CurrentTrackIndex);
	DOREPLIFETIME(ARadioItem, bIsPlaying);
}

void ARadioItem::OnRep_CurrentTrackIndex()
{
	// 곡이 바뀌면 재생 중일 때만 새 곡을 재생 (정지 상태면 그대로).
	if (bIsPlaying)
	{
		PlayCurrentTrack();
	}
}

void ARadioItem::OnRep_bIsPlaying()
{
	// 재생/정지 상태가 바뀌면 각 클라이언트에서 반영.
	if (bIsPlaying)
	{
		PlayCurrentTrack();
	}
	else
	{
		StopPlaying();
	}
}

// [RADIO-001] 현재 인덱스의 곡을 재생
void ARadioItem::PlayCurrentTrack()
{
	if (!AudioComponent)
	{
		return;
	}

	if (!Playlist.IsValidIndex(CurrentTrackIndex) || !Playlist[CurrentTrackIndex])
	{
		AudioComponent->Stop();
		return;
	}

	USoundBase* DesiredSound = Playlist[CurrentTrackIndex];

	// 이미 "같은 곡"을 재생 중이면 다시 Play하지 않는다.
	// (Play()를 재호출하면 처음부터 리셋됨 → 바닥에서 재생 중인 라디오를 다시 집었을 때
	//  곡이 초기화되던 버그 방지. 서버는 PickUp에서 직접 재생하고 bIsPlaying이 true→true라
	//  클라 OnRep은 안 불려, 서버에서만 리셋되던 증상이었다)
	if (AudioComponent->IsPlaying() && AudioComponent->GetSound() == DesiredSound)
	{
		return;
	}

	AudioComponent->SetSound(DesiredSound);
	AudioComponent->Play();
}

// [RADIO-002] 재생 정지
void ARadioItem::StopPlaying()
{
	if (AudioComponent)
	{
		AudioComponent->Stop();
	}
}

// 인벤토리에서 꺼내 손에 장착: 재생 ON (서버가 상태 설정 → OnRep이 각 클라 재생).
void ARadioItem::OnEquipped_Implementation(AActor* Equipper)
{
	Super::OnEquipped_Implementation(Equipper);

	if (HasAuthority())
	{
		bIsPlaying = true;
	}
	// 서버 자신은 OnRep이 안 불리므로 직접 반영.
	if (bIsPlaying)
	{
		PlayCurrentTrack();
	}
}

// 인벤토리에 수납: 재생 OFF.
void ARadioItem::OnUnequipped_Implementation(AActor* Equipper)
{
	if (HasAuthority())
	{
		bIsPlaying = false;
	}
	StopPlaying();
	Super::OnUnequipped_Implementation(Equipper);
}

// 좌클릭: 다음 곡으로 전환. 서버 권위로 인덱스를 바꾸면 OnRep으로 모든 클라가 곡을 바꾼다.
void ARadioItem::OnUse_Implementation()
{
	// 전환은 항상 서버에서 처리. 클라에서 불리면 ServerNextTrack으로 위임. (WeaponItemBase 패턴)
	if (!HasAuthority())
	{
		ServerNextTrack();
		return;
	}

	if (Playlist.Num() == 0)
	{
		return;
	}

	// 다음 곡으로 순환.
	CurrentTrackIndex = (CurrentTrackIndex + 1) % Playlist.Num();

	// 서버 자신(호스트)에서도 곡을 바꿔야 한다. OnRep은 서버 자신에겐 안 불리므로 직접 재생.
	if (bIsPlaying)
	{
		PlayCurrentTrack();
	}
}

// [RADIO-003] 클라이언트 -> 서버 "다음 곡" 위임
void ARadioItem::ServerNextTrack_Implementation()
{
	OnUse_Implementation();
}

// 집을 때: 소유권 설정(클라 RPC 가능) + 재생 ON.
void ARadioItem::PickUp_Implementation(AActor* Picker)
{
	Super::PickUp_Implementation(Picker);

	if (HasAuthority())
	{
		if (Picker)
		{
			SetOwner(Picker);
		}
		bIsPlaying = true;
	}
	// 서버 자신 직접 반영.
	if (bIsPlaying)
	{
		PlayCurrentTrack();
	}
}

// 놓기: 소유권만 해제하고 재생은 유지한다(바닥에 놓아도 계속 나오는 스피커처럼).
// 재생 중 그대로 두면 AudioComponent가 라디오 위치를 따라다니며 계속 재생된다.
void ARadioItem::Drop_Implementation(FVector DropLocation, AActor* Dropper)
{
	if (HasAuthority())
	{
		SetOwner(nullptr);
	}
	Super::Drop_Implementation(DropLocation, Dropper);
}

// 던지기: 소유권만 해제하고 재생은 유지.
void ARadioItem::Throw_Implementation(FVector ThrowVelocity, AActor* Thrower)
{
	if (HasAuthority())
	{
		SetOwner(nullptr);
	}
	Super::Throw_Implementation(ThrowVelocity, Thrower);
}
