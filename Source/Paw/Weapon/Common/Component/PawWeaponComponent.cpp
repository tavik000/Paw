// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawWeaponComponent.h"

#include "Paw/Character/Player/PawBattleCharacter.h"

UPawWeaponComponent::UPawWeaponComponent()
{
	SetIsReplicatedByDefault(true);
}


bool UPawWeaponComponent::AttachWeapon(APawBattleCharacter* TargetCharacter)
{
	Character = TargetCharacter;

	// Check that the character is valid, and has no weapon component yet
	if (Character == nullptr)
	{
		return false;
	}
	if (Character->GetInstanceComponents().FindItemByClass<UPawWeaponComponent>())
	{
		return false;
	}

	return true;
}
