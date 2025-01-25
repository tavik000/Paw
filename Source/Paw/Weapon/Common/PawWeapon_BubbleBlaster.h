// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawWeaponBase.h"
#include "PawWeapon_BubbleBlaster.generated.h"

class UPawGunComponent;

UCLASS()
class PAW_API APawWeapon_BubbleBlaster : public APawWeaponBase
{
	GENERATED_BODY()

public:
	APawWeapon_BubbleBlaster();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void Equip(APawBattleCharacter* TargetCharacter) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UPawGunComponent> GunComponent;
};
