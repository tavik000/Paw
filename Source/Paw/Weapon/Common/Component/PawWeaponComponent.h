// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

#include "PawWeaponComponent.generated.h"

class APawBattleCharacter;
/**
 * 
 */
UCLASS()
class PAW_API UPawWeaponComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UPawWeaponComponent();

	UFUNCTION(BlueprintCallable, Category="Weapon")
	virtual bool AttachWeapon(APawBattleCharacter* TargetCharacter);
	
protected:
	/** The Character holding this weapon*/
	APawBattleCharacter* Character;
};
