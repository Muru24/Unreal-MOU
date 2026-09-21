#pragma once

#include "CoreMinimal.h"
#include "Base/ItemBase.h"
#include "MiniGameTokenItem.generated.h"

UCLASS()
class TEAMPROJECT_MOU_API AMiniGameTokenItem : public AItemBase
{
	GENERATED_BODY()

public:
	AMiniGameTokenItem();

	// 현재 토큰 사용 가능 여부
	UFUNCTION(BlueprintPure, Category = "MiniGame|Token")
	bool CanUseToken() const;

	// 이 토큰 1개가 머신에 제공하는 플레이 횟수
	UFUNCTION(BlueprintPure, Category = "MiniGame|Token")
	int32 GetGrantedPlayCount() const
	{
		return GrantedPlayCount;
	}

protected:
	// 좌클릭 토큰 사용
	virtual void OnUse_Implementation() override;

	UFUNCTION(BlueprintCallable, Category = "MiniGame|Token")
	void UseTokenOnServer(AActor* UserActor);

	// 토큰 1개 사용 성공 시 머신에 제공할 플레이 횟수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniGame|Token",
		meta = (ClampMin = "1"))
	int32 GrantedPlayCount = 2;

	// 토큰 소비 성공 후 호출
	// 추후 Inventory에서 토큰 제거 처리 연결용
	UFUNCTION(BlueprintImplementableEvent, Category = "MiniGame|Token")
	void OnTokenConsumed(AActor* UserActor);

private:
	// 현재 토큰을 가지고 있는 플레이어 찾기
	AActor* ResolveTokenUser() const;
};