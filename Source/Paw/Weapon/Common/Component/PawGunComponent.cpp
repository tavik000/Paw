// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawGunComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Kismet/GameplayStatics.h"
#include "Paw/Character/Player/PawFPSPlayer.h"
#include "Paw/Weapon/Projectile/PawProjectileBase.h"

UPawGunComponent::UPawGunComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
	SetIsReplicatedByDefault(true);
}

bool UPawGunComponent::AttachWeapon(APawBattleCharacter* TargetCharacter)
{
	if (!Super::AttachWeapon(TargetCharacter))
	{
		return false;
	}

	FPSPlayer = Cast<APawFPSPlayer>(TargetCharacter);

	if (!IsValid(FPSPlayer))
	{
		return false;
	}

	// Attach the weapon to the First Person Character
	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget,
	                                                EAttachmentRule::KeepWorld, true);
	AttachToComponent(FPSPlayer->GetArmMesh(), AttachmentRules, FName(TEXT("GripPoint")));

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<
			UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(
			PlayerController->InputComponent))
		{
			// Fire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UPawGunComponent::Fire);
		}
	}

	return true;
}

void UPawGunComponent::Fire()
{
	if (IsCoolDown)
	{
		// TODO: Play a sound or something to indicate that the weapon is on cooldown
		return;
	}

	if (Character == nullptr || Character->GetController() == nullptr)
	{
		return;
	}

	if (!IsValid(FPSPlayer))
	{
		return;
	}

	if (ProjectileClass == nullptr)
	{
		return;
	}
	
	// Try and fire a projectile
	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
	const FVector Start = GetOwner()->GetActorLocation();
	const FVector End = Start + (SpawnRotation.Vector() * 10000);
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(GetOwner());
	GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, CollisionParams);
	const float Distance = FVector::Dist(Start, HitResult.ImpactPoint);
	const bool IsFacingDownOrTooClose = Distance < FireClosetDistance || SpawnRotation.Pitch <= FacingDownPitch;

	FVector SpawnLocation = GetOwner()->GetActorLocation();
	if (IsFacingDownOrTooClose)
	{
		SpawnLocation = SpawnLocation + GetOwner()->GetActorForwardVector() * TooCloseAdjustOffset;
	}
	

	// Spawn the projectile at the muzzle
	ServerSpawnProjectile(SpawnLocation, SpawnRotation);

	// Try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}

	// Try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = FPSPlayer->GetArmMesh()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}


	IsCoolDown = true;
	GetWorld()->GetTimerManager().SetTimer(FireCoolDownTimerHandle, this, &UPawGunComponent::ResetCoolDown,
	                                       FireCoolDown, false);
}

void UPawGunComponent::ServerSpawnProjectile_Implementation(FVector SpawnLocation, FRotator SpawnRotation)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (ProjectileClass == nullptr)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (IsValid(World))
	{
		//Set Spawn Collision Handling Override
		FActorSpawnParameters ActorSpawnParams;
		ActorSpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		APawProjectileBase* SpawnProjectile = World->SpawnActor<APawProjectileBase>(
			ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
	}
}

void UPawGunComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UPawGunComponent::ResetCoolDown()
{
	IsCoolDown = false;
}
