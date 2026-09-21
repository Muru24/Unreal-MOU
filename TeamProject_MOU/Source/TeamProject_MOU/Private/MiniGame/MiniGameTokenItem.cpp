#include "MiniGame/MiniGameTokenItem.h"

#include "MiniGame/MiniGameTokenReceiver.h"
#include "Kismet/GameplayStatics.h"

AMiniGameTokenItem::AMiniGameTokenItem()
{
	// 토큰 아이템 자체는 한 번만 사용
	MaxUseCount = 1;
	CurrentUseCount = 1;

	// 토큰 하나당 미니게임 플레이 가능 횟수
	GrantedPlayCount = 2;

	// 인벤토리에 보관 가능한 아이템
	bCanBeStoredInInventory = true;
}

bool AMiniGameTokenItem::CanUseToken() const
{
	return CurrentUseCount > 0;
}

AActor* AMiniGameTokenItem::ResolveTokenUser() const
{
	// 인벤토리 보관 상태라면 플레이어에게 Attach되어 있으므로 우선 확인
	if (AActor* AttachedActor = GetAttachParentActor())
	{
		return AttachedActor;
	}

	// ItemBase에서 마지막으로 아이템을 소유했던 플레이어
	if (LastOwner)
	{
		return LastOwner.Get();
	}

	// 별도로 Owner가 설정되어 있는 경우
	if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor;
	}

	return nullptr;
}

void AMiniGameTokenItem::OnUse_Implementation()
{
	// 토큰 및 머신 상태 변경은 서버에서만 처리
	if (!HasAuthority())
	{
		return;
	}

	UseTokenOnServer(ResolveTokenUser());
}

void AMiniGameTokenItem::UseTokenOnServer(AActor* UserActor)
{
	// 토큰 및 머신 상태 변경은 서버에서만 처리
	if (!HasAuthority())
	{
		return;
	}

	// 이미 소비된 토큰
	if (!CanUseToken())
	{
		return;
	}


	if (!UserActor || !GetWorld())
	{
		return;
	}

	// 토큰을 받을 수 있는 모든 미니게임 머신 검색
	TArray<AActor*> MiniGameMachines;

	UGameplayStatics::GetAllActorsWithInterface(
		this,
		UMiniGameTokenReceiver::StaticClass(),
		MiniGameMachines
	);

	for (AActor* Machine : MiniGameMachines)
	{
		if (!IsValid(Machine))
		{
			continue;
		}

		// 실제 사용 가능 여부는 각 머신이 판단
		// StockMachine에서는 UseArea 안에 있는지도 여기서 검사
		const bool bAccepted =
			IMiniGameTokenReceiver::Execute_TryActivateMiniGameToken(
				Machine,
				UserActor,
				GrantedPlayCount
			);

		if (!bAccepted)
		{
			continue;
		}

		// 머신이 토큰 사용을 승인했을 때만 토큰 소비
		CurrentUseCount =
			FMath::Max(0, CurrentUseCount - 1);

		UE_LOG(
			LogTemp,
			Log,
			TEXT("[MiniGameToken] Token accepted. GrantedUses = %d"),
			GrantedPlayCount
		);

		// 추후 Inventory 제거 처리
		OnTokenConsumed(UserActor);

		return;
	}

	// 어떤 머신에서도 승인하지 않은 경우
	// 토큰은 소비되지 않음
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[MiniGameToken] No available machine.")
	);
}
