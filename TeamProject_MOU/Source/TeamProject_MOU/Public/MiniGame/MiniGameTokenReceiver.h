#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MiniGameTokenReceiver.generated.h"

UINTERFACE(BlueprintType)
class TEAMPROJECT_MOU_API UMiniGameTokenReceiver : public UInterface
{
	GENERATED_BODY()
};

class TEAMPROJECT_MOU_API IMiniGameTokenReceiver
{
	GENERATED_BODY()

public:
	// 미니게임 토큰 사용 요청
	// 머신이 현재 플레이어의 토큰 사용을 허용했다면 true 반환
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "MiniGame|Token")
	bool TryActivateMiniGameToken(
		AActor* UserActor,
		int32 GrantedUses
	);
};