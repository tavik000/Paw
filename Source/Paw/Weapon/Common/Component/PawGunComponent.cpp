// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawGunComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Paw/Character/Player/PawFPSPlayer.h"
#include "Paw/Core/Utility/Object/PawActorPool.h"
#include "Paw/Weapon/Projectile/PawProjectileBase.h"

UPawGunComponent::UPawGunComponent()
{
	// Default offset from the character location for projectiles to spawn
	SetIsReplicatedByDefault(true);
}

void UPawGunComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	if (!IsValid(ProjectileClass))
	{
		return;
	}

	ProjectilePool = NewObject<UPawActorPool>(this);
	if (!IsValid(ProjectilePool))
	{
		return;
	}
	ProjectilePool->InitializePool(ProjectileClass, PrewarmCount);
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
		if (DryFireSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, DryFireSound, Character->GetActorLocation());
		}
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
	if (const AActor* ActorOwner = GetOwner(); !IsValid(ActorOwner))
	{
		return;
	}
	const APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();

	FVector ActorLocation = FPSPlayer->GetActorLocation();
	FVector CameraForwardVector = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
	FVector SpawnLocation = ActorLocation + CameraForwardVector * MuzzleOffset;

	// Debug SpawnLocation
	// DrawDebugSphere(GetWorld(), SpawnLocation, 20.0f, 12, FColor::Red, false, 5.0f);

	// Ensure minimum ground clearance to prevent spawning inside ground
	constexpr float MinClearanceDistance = 30.0f; // Minimum distance above ground
	FVector ForwardCheckStartLocation = ActorLocation;
	FVector ForwardCheckEndLocation = ActorLocation + CameraForwardVector * 100.0f;
	// Check 100 units forward

	FHitResult InitHit;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // Seeker
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(FPSPlayer);

	if (GetWorld()->LineTraceSingleByObjectType(InitHit, ForwardCheckStartLocation, ForwardCheckEndLocation,
	                                            ObjectQueryParams, QueryParams))
	{
		// Found an obstacle in front, adjust spawn location
		if (!InitHit.GetActor()->ActorHasTag("Projectile"))
		{
			SpawnLocation = ActorLocation + CameraForwardVector * (InitHit.Distance - MinClearanceDistance);

			// DrawDebugSphere(GetWorld(), SpawnLocation, 20.0f, 12, FColor::Green, false, 5.0f);
		}
	}

	FVector GroundCheckStartLocation = SpawnLocation;
	FVector GroundCheckEndLocation = SpawnLocation - FVector(0, 0, 200.0f); // Check 200 units down

	if (GetWorld()->LineTraceSingleByObjectType(InitHit, GroundCheckStartLocation, GroundCheckEndLocation,
	                                            ObjectQueryParams, QueryParams))
	{
		// Found ground below, ensure we're above it
		float DistanceToGround = FVector::Dist(SpawnLocation, InitHit.ImpactPoint);
		if (DistanceToGround < MinClearanceDistance)
		{
			// Adjust spawn location to be above ground
			SpawnLocation = InitHit.ImpactPoint + FVector(0, 0, MinClearanceDistance);
		}
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

	if (const UWorld* World = GetWorld(); IsValid(World))
	{
		//Set Spawn Collision Handling Override
		FActorSpawnParameters ActorSpawnParams;
		ActorSpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ActorSpawnParams.Owner = FPSPlayer;
		ActorSpawnParams.Instigator = FPSPlayer;
		AActor* SpawnProjectile = ProjectilePool->TrySpawnPooledActor(
			SpawnLocation, SpawnRotation, ActorSpawnParams);
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
