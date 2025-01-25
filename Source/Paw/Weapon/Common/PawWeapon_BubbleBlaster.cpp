// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawWeapon_BubbleBlaster.h"

#include "Component/PawGunComponent.h"


APawWeapon_BubbleBlaster::APawWeapon_BubbleBlaster()
{
	PrimaryActorTick.bCanEverTick = true;
	GunComponent = CreateDefaultSubobject<UPawGunComponent>(TEXT("GunComponent"));
	SetRootComponent(GunComponent);
}

void APawWeapon_BubbleBlaster::BeginPlay()
{
	Super::BeginPlay();
}

void APawWeapon_BubbleBlaster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawWeapon_BubbleBlaster::Equip(APawBattleCharacter* TargetCharacter)
{
	if (IsValid(GunComponent))
	{
		GunComponent->AttachWeapon(TargetCharacter);
	}
}
