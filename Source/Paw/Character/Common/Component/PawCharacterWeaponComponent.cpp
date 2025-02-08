// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawCharacterWeaponComponent.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Paw/Character/Player/PawBattleCharacter.h"
#include "Paw/Weapon/Common/PawWeaponBase.h"


UPawCharacterWeaponComponent::UPawCharacterWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}


void UPawCharacterWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	ServerCreateDefaultWeapon();
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

void UPawCharacterWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPawCharacterWeaponComponent, EquippedWeapon);
}

APawWeaponBase* UPawCharacterWeaponComponent::GetEquipWeapon() const
{
	return EquippedWeapon.Get();
}

void UPawCharacterWeaponComponent::ServerCreateDefaultWeapon_Implementation()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	ServerCreateEquipWeapon(DefaultWeaponClass);
}

void UPawCharacterWeaponComponent::ServerCreateEquipWeapon_Implementation(UClass* WeaponClass)
{
	// Server only
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

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
		APawWeaponBase* SpawnWeapon = GetWorld()->SpawnActor<APawWeaponBase>(WeaponClass, SpawnTransform);
		if (IsValid(SpawnWeapon))
		{
			EquippedWeapon = SpawnWeapon;
			EquippedWeapon->SetReplicates(true);
			EquippedWeapon->SetReplicateMovement(true);
			EquippedWeapon->SetOwner(OwnerCharacter);
			OnRep_EquippedWeapon();
		}
	}
}

void UPawCharacterWeaponComponent::OnRep_EquippedWeapon()
{
	EquippedWeapon->EquipToOwner();
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
	ServerCreateEquipWeapon(ChangeWeaponClass);
}
