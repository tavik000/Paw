// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawCharacterWeaponComponent.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Paw/Character/Player/PawBattleCharacter.h"
#include "Paw/Weapon/Common/PawWeaponBase.h"


UPawCharacterWeaponComponent::UPawCharacterWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
}


void UPawCharacterWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	CreateDefaultWeapon();
}


void UPawCharacterWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UPawCharacterWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (EquippedWeapon.IsValid())
	{
		EquippedWeapon->Destroy();
	}
}

APawWeaponBase* UPawCharacterWeaponComponent::GetEquipWeapon() const
{
	return EquippedWeapon.Get();
}

void UPawCharacterWeaponComponent::CreateDefaultWeapon()
{
	CreateEquipWeapon(DefaultWeaponClass);
}

void UPawCharacterWeaponComponent::CreateEquipWeapon(UClass* WeaponClass)
{
	if (!WeaponClass)
	{
		return;
	}

	if (EquippedWeapon.IsValid())
	{
		if (EquippedWeapon->GetClass() != WeaponClass)
		{
			DeleteEquipWeapon();
		}
		else
		{
			// Already equipped
			return;
		}
	}

	if (APawBattleCharacter* OwnerCharacter = Cast<APawBattleCharacter>(GetOwner()))
	{
		FTransform SpawnTransform(OwnerCharacter->GetActorRotation(), OwnerCharacter->GetActorLocation());

		if (APawWeaponBase* Spawning = Cast<APawWeaponBase>(
			UGameplayStatics::BeginDeferredActorSpawnFromClass(
				OwnerCharacter, WeaponClass, SpawnTransform, ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
				OwnerCharacter)))
		{
			APawWeaponBase* SpawnInstance = CastChecked<APawWeaponBase>(
				UGameplayStatics::FinishSpawningActor(Spawning, SpawnTransform));
			if (IsValid(SpawnInstance))
			{
				EquippedWeapon = SpawnInstance;
				EquippedWeapon->Equip(OwnerCharacter);
			}
		}
	}
}

void UPawCharacterWeaponComponent::DeleteEquipWeapon()
{
	if (EquippedWeapon.IsValid())
	{
		EquippedWeapon->Destroy();
		EquippedWeapon.Reset();
	}
}

void UPawCharacterWeaponComponent::ChangeEquipWeapon(UClass* ChangeWeaponClass)
{
	if (!EquippedWeapon.IsValid())
	{
		return;
	}
	if (EquippedWeapon->GetClass() == ChangeWeaponClass)
	{
		return;
	}
	CreateEquipWeapon(ChangeWeaponClass);
}
