// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawWeaponBase.h"


APawWeaponBase::APawWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APawWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawWeaponBase::Equip(APawBattleCharacter* TargetCharacter)
{
	OnEquip.Broadcast(TargetCharacter);
}

