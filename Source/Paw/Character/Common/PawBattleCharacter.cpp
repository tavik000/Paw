// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBattleCharacter.h"

#include "Paw/Character/Common/Component/PawCharacterWeaponComponent.h"


APawBattleCharacter::APawBattleCharacter()
{
	CharacterWeaponComponent = CreateDefaultSubobject<UPawCharacterWeaponComponent>(TEXT("CharacterWeaponComponent"));
}

void APawBattleCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APawBattleCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
