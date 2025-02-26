﻿// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

#include "PawWeaponBase.generated.h"

class APawBattleCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipMulticastDelegate, APawBattleCharacter*, TargetCharacter);

UCLASS()
class PAW_API APawWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	APawWeaponBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void EquipToOwner();

	UPROPERTY(BlueprintAssignable)
	FOnEquipMulticastDelegate OnEquip;
};
