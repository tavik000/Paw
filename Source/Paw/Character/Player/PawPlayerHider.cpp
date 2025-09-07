#include "PawPlayerHider.h"

#include "EnhancedInputComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "../../Core/System/PawLightDetectionSubsystem.h"
#include "Engine/AssetManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PawPlayerSeeker_Ghost.h"
#include "../../Environment/GameplayElement/Common/Interface/PawCollideBreakableInterface.h"
#include "Components/CapsuleComponent.h"


// ================================================================
// Constructor & Core Engine Overrides
// ================================================================

APawPlayerHider::APawPlayerHider()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Team & Basic Setup
	TeamId = ETeamId::Hider;

	// Health System Defaults
	Health = 100.0f;
	MaxHealth = 100.0f;
	bIsDead = false;
	LitDamageAmount = 0.5f;
	ShadowHealAmount = 0.25f;
	CaptureDamageAmount = 0.25f;
	HealthEffectInterval = 0.03f;
	DieTimer = 5.0f;

	// Light Detection System
	bIsInLight = true;
	bIsSpotLighted = false;

	// Light Detection Performance
	LightDetectionTickRate = 0.1f;
	StaggerOffsetMultiplier = 0.05f;

	// Stealth System
	bIsInvisible = false;
	StealthOpacity = 0.5f;
	OpacityParameterName = FName("Opacity");

	// Capture System
	bIsCaptured = false;

	// Multiplayer Properties
	bIsLocalPlayer = false;
	PlayerIndex = -1;

	// Movement System
	bIsWalking = false;

	// UI System
	HUD = nullptr;

	// Role Conversion System
	SeekerGhostClass = APawPlayerSeeker_Ghost::StaticClass();

	// Jump System Defaults
	HangTimeGravityScale = 1.4f;
	JumpVFXVelocityScale = 0.5f;
	LandVFXVelocityScale = 1.0f;
	LandingVolumeDivisor = 700.0f;
	HangTimeVelocityZMin = -100.0f;
	HangTimeVelocityZMax = 100.0f;
	AirControlInputTolerance = 0.0001f;
	JumpSFXVolumeMin = 0.1f;
	JumpSFXVolumeMax = 1.0f;
	DefaultGravityScale = 3.7f;

	// Cached assets initialized to nullptr
	VanishSound = nullptr;
	JumpSound = nullptr;
	JumpVFX = nullptr;
	LandVFX = nullptr;
	LandForceFeedback = nullptr;
}

void APawPlayerHider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Jump System: Max Hang Time and Air Control
	if (GetCharacterMovement())
	{
		const FVector Velocity = GetCharacterMovement()->Velocity;

		// Max Hang Time: Z <= HangTimeVelocityZMax AND Z >= HangTimeVelocityZMin
		if (Velocity.Z <= HangTimeVelocityZMax && Velocity.Z >= HangTimeVelocityZMin)
		{
			GetCharacterMovement()->GravityScale = HangTimeGravityScale;
		}
		else
		{
			GetCharacterMovement()->GravityScale = DefaultGravityScale;
		}

		// Air Control: Is Falling AND Has Input AND Locally Controlled
		const bool bIsFalling = GetCharacterMovement()->IsFalling();
		const FVector LastInput = GetLastMovementInputVector();
		const bool bHasInput = !LastInput.IsNearlyZero(AirControlInputTolerance);
		const bool bIsLocallyControlled = IsLocallyControlled();

		if (bIsFalling && !bHasInput && bIsLocallyControlled)
		{
			ServerRequestCancelHorizontalVelocity();
		}
	}
}

void APawPlayerHider::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Toggle walking
		EnhancedInputComponent->BindAction(ToggleWalkAction, ETriggerEvent::Started, this, &ThisClass::ToggleWalk);
	}
}

void APawPlayerHider::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// VFX/SFX for all clients
	MulticastPlayLandEffects();

	// Force feedback only for local player
	if (IsLocallyControlled())
	{
		PlayLandForceFeedback();
	}
}

void APawPlayerHider::Jump()
{
	Super::Jump();
	MulticastPlayJumpEffects();
}

// ================================================================
// Engine Overrides
// ================================================================

void APawPlayerHider::BeginPlay()
{
	Super::BeginPlay();

	InitializeMaterials();

	// Cache default gravity scale (set in BP)
	if (GetCharacterMovement())
	{
		DefaultGravityScale = GetCharacterMovement()->GravityScale;
		SprintSpeed = GetCharacterMovement()->MaxWalkSpeed;
	}

	// Start async loading of jump assets
	LoadAssetsAsync();

	// Bind collision event for breaking objects
	if (IsValid(GetCapsuleComponent()))
	{
		GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &APawPlayerHider::OnHit);
	}

	// Start lit damage timer immediately (always running)
	if (HasAuthority())
	{
		StartHealthEffectTimer();
		// Register with light detection subsystem
		if (UWorld* World = GetWorld(); IsValid(World))
		{
			if (UPawLightDetectionSubsystem* LightSubsystem = World->GetSubsystem<UPawLightDetectionSubsystem>())
			{
				LightSubsystem->RegisterHider(this);
				UE_LOG(LogTemp, Log, TEXT("PawPlayerHider: Registered with light detection subsystem"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("PawPlayerHider: Failed to get light detection subsystem"));
			}
		}
	}


	// Create HUD for player-controlled pawns
	if (IsLocallyControlled())
	{
		Client_CreateHUD();
	}

	// Bind to possession change events to update visuals when players convert to seekers
	if (UWorld* World = GetWorld())
	{
		// Bind to all PlayerController possession events to detect team changes
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APlayerController* PC = Iterator->Get(); IsValid(PC))
			{
				PC->OnPossessedPawnChanged.AddDynamic(this, &APawPlayerHider::HandlePossessionChanged);
				UE_LOG(LogTemp, Log, TEXT("PawPlayerHider: Bound to OnPossessedPawnChanged for PlayerController %s"),
				       *PC->GetName());
			}
		}
	}
}

void APawPlayerHider::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from light detection subsystem
	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (UPawLightDetectionSubsystem* LightSubsystem = World->GetSubsystem<UPawLightDetectionSubsystem>())
			{
				LightSubsystem->UnregisterHider(this);
			}
		}
	}

	// Clear all timer handles to prevent memory leaks
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		FTimerManager& TimerManager = World->GetTimerManager();

		// Clear health effect timer
		if (HealthEffectTimerHandle.IsValid())
		{
			TimerManager.ClearTimer(HealthEffectTimerHandle);
		}


		// Clear die timer
		if (DieTimerHandle.IsValid())
		{
			TimerManager.ClearTimer(DieTimerHandle);
		}
	}

	// Release streamable asset handle
	if (AssetsHandle.IsValid())
	{
		AssetsHandle->ReleaseHandle();
		AssetsHandle.Reset();
	}

	// Clear HUD widget reference
	if (IsValid(HUD))
	{
		HUD->RemoveFromParent();
		HUD = nullptr;
	}

	// Unbind from possession change events
	if (UWorld* World = GetWorld())
	{
		// Unbind from all PlayerController possession events
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APlayerController* PC = Iterator->Get(); IsValid(PC))
			{
				PC->OnPossessedPawnChanged.RemoveDynamic(this, &APawPlayerHider::HandlePossessionChanged);
			}
		}
	}

	ConversionAuraVFX = nullptr;
	ConversionBurstVFX = nullptr;

	Super::EndPlay(EndPlayReason);
}

void APawPlayerHider::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APawPlayerHider, Health);
	DOREPLIFETIME(APawPlayerHider, bIsDead);
	DOREPLIFETIME(APawPlayerHider, bIsInLight);
	DOREPLIFETIME(APawPlayerHider, bIsSpotLighted);
	DOREPLIFETIME(APawPlayerHider, bIsInvisible);
	DOREPLIFETIME(APawPlayerHider, bIsCaptured);
	DOREPLIFETIME(APawPlayerHider, bIsLocalPlayer);
	DOREPLIFETIME(APawPlayerHider, PlayerIndex);
	DOREPLIFETIME(APawPlayerHider, bIsWalking);
}

bool APawPlayerHider::CanMove()
{
	return !bIsCaptured;
}

bool APawPlayerHider::CanJump()
{
	return !bIsCaptured;
}

// ================================================================
// Movement System
// ================================================================
void APawPlayerHider::ToggleWalk()
{
	if (!HasAuthority())
	{
		ServerToggleWalk();
		return;
	}

	bIsWalking = !bIsWalking;

	// Server needs to update its own speed since RepNotify doesn't execute on server
	if (GetCharacterMovement())
	{
		if (bIsWalking)
		{
			GetCharacterMovement()->MaxWalkSpeed = SlowWalkSpeed;
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
		}
	}
}

// ================================================================
// Blueprint Callable API - Health System
// ================================================================

void APawPlayerHider::TakeHealthDamage(float DamageAmount)
{
	if (HasAuthority() && IsAlive())
	{
		Health = FMath::Clamp(Health - DamageAmount, 0.0f, MaxHealth);
		OnHealthChanged(Health, MaxHealth);
		OnHpChanged.Broadcast(GetHealthPercentage());

		if (Health <= 0 && !bIsDead)
		{
			Die();
		}
	}
	else if (!HasAuthority())
	{
		ServerTakeHealthDamage(DamageAmount);
	}
}

void APawPlayerHider::Heal(float HealAmount)
{
	if (HasAuthority() && IsAlive())
	{
		Health = FMath::Clamp(Health + HealAmount, 0.0f, MaxHealth);
		OnHealthChanged(Health, MaxHealth);
		OnHpChanged.Broadcast(GetHealthPercentage());
	}
}

void APawPlayerHider::Die()
{
	if (HasAuthority())
	{
		bIsDead = true;
		Health = 0.0f;

		// Stop health effects timer
		StopHealthEffectTimer();

		MulticastSpawnConversionAuraEffect();

		// Broadcast death started
		OnDeathStarted.Broadcast();

		// Disable input for this player
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->GetPawn()->DisableInput(PC);
		}

		// Start die timer
		GetWorldTimerManager().SetTimer(DieTimerHandle, [this]()
		{
			// Safety check: ensure object is still valid
			if (!IsValid(this) || !HasAuthority())
			{
				return;
			}

			MulticastSpawnConversionBurstEffect();
			OnDeathFinished.Broadcast();
			Client_HandleConvertSeekerUI();
			ServerConvertToSeeker();
		}, DieTimer, false);
	}

	// Handle UI changes on client
	Client_HandleOnDeathStartedUI();
}

// ================================================================
// Blueprint Callable API - Light Detection System
// ================================================================


void APawPlayerHider::SetInLight(bool bNewInLight)
{
	if (HasAuthority() && bIsInLight != bNewInLight)
	{
		bIsInLight = bNewInLight;
		OnLightExposureChanged(bNewInLight);

		// Update invisibility state based on combined light exposure
		UpdateInvisibilityState();
	}
}

void APawPlayerHider::SetSpotLighted(bool bSpotLighted)
{
	if (HasAuthority() && bIsSpotLighted != bSpotLighted)
	{
		bIsSpotLighted = bSpotLighted;
	}
}

// ================================================================
// Blueprint Callable API - Stealth System
// ================================================================

void APawPlayerHider::ActivateInvisibility()
{
	if (HasAuthority() && !bIsInvisible)
	{
		bIsInvisible = true;
		MulticastUpdateStealthVisuals();

		OnInvisibilityChanged(true);
	}
}

void APawPlayerHider::DeactivateInvisibility()
{
	if (HasAuthority() && bIsInvisible)
	{
		bIsInvisible = false;
		MulticastUpdateStealthVisuals();
		OnInvisibilityChanged(false);
	}
}

void APawPlayerHider::UpdateInvisibilityState()
{
	if (!HasAuthority())
	{
		return;
	}

	// Check combined light state (environment lights + spotlights)
	bool bCurrentlyLit = bIsInLight || bIsSpotLighted;
	bool bCanBecomeInvisible = !bCurrentlyLit && !bIsCaptured;

	// Automatically become invisible if in shadows and not captured
	if (bCanBecomeInvisible && !bIsInvisible)
	{
		ActivateInvisibility();
	}
	// Become visible if lit or captured
	else if (bIsInvisible && (bCurrentlyLit || bIsCaptured))
	{
		DeactivateInvisibility();
	}
}

void APawPlayerHider::PlayVanishSound()
{
	if (IsValid(VanishSound))
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), VanishSound, GetActorLocation());
	}
}

void APawPlayerHider::SpawnConversionAura()
{
	if (IsValid(ConversionAuraSound))
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ConversionAuraSound, GetActorLocation());
	}

	if (ConversionAuraVFXAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("SpawnConversionAuraVFX: ConversionAuraVFXAsset is not set"));
		return;
	}

	UClass* VFXClass = ConversionAuraVFXAsset.Get();
	if (!IsValid(VFXClass))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("SpawnConversionAuraVFX: ConversionAuraVFXAsset class is not loaded"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// Find ground location using line trace
	FVector StartLocation = GetActorLocation();
	FVector EndLocation = StartLocation - FVector(0.0f, 0.0f, 1000.0f); // Trace 1000 units down

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	FHitResult HitResult;
	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECC_WorldStatic,
		QueryParams
	);

	FVector GroundLocation;
	if (bHit)
	{
		// Use the hit location as ground position
		GroundLocation = HitResult.Location;
	}
	else
	{
		// Fallback to capsule-based calculation if trace fails
		GroundLocation = GetActorLocation();
		if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			GroundLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	ConversionAuraVFX = World->SpawnActor<AActor>(VFXClass, GroundLocation, GetActorRotation());
}

void APawPlayerHider::SpawnConversionBurstVFX()
{
	if (ConversionBurstVFXAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("SpawnConversionBurstVFX: ConversionBurstVFXAsset is not set"));
		return;
	}

	UClass* VFXClass = ConversionBurstVFXAsset.Get();
	if (!IsValid(VFXClass))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("SpawnConversionBurstVFX: ConversionBurstVFXAsset class is not loaded"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// Find ground location using line trace
	const FVector StartLocation = GetActorLocation();
	const FVector EndLocation = StartLocation - FVector(0.0f, 0.0f, 1000.0f); // Trace 1000 units down
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		StartLocation,
		EndLocation,
		ECC_WorldStatic,
		QueryParams
	);

	FVector GroundLocation;
	if (bHit)
	{
		GroundLocation = HitResult.Location;
	}
	else
	{
		// Fallback to capsule-based calculation if trace fails
		GroundLocation = GetActorLocation();
		if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			GroundLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	ConversionBurstVFX = World->SpawnActor<AActor>(VFXClass, GroundLocation, GetActorRotation());
}

void APawPlayerHider::UpdateStealthVisuals()
{
	// Early validation - check if actor is being destroyed or mesh is invalid
	if (!IsValid(this) || IsActorBeingDestroyed() || !IsValid(GetMesh()))
	{
		UE_LOG(LogTemp, Warning, TEXT("UpdateStealthVisuals: Actor or mesh is not valid for %s"), *GetName());
		return;
	}

	// Validate cached materials before proceeding with visual updates
	ValidateAndRefreshMaterials();

	// Handle visible state
	if (!bIsInvisible)
	{
		RestoreOriginalMaterials();
		if (!OpacityParameterName.IsNone() && IsValid(GetMesh()))
		{
			GetMesh()->SetScalarParameterValueOnMaterials(OpacityParameterName, 1.0f);
		}
		return;
	}

	PlayVanishSound();

	// Handle invisible state
	ApplyInvisibilityMaterials();
	if (!OpacityParameterName.IsNone() && IsValid(GetMesh()))
	{
		const float OpacityValue = GetOpacityForViewerTeam();
		GetMesh()->SetScalarParameterValueOnMaterials(OpacityParameterName, OpacityValue);
	}
}

float APawPlayerHider::GetOpacityForViewerTeam() const
{
	// Seekers see full invisibility, Hiders see partial opacity
	if (IsViewerOnSeekerTeam())
	{
		return 0.0f;
	}

	return StealthOpacity; // Default 0.5f for Hiders and fallback
}

// ================================================================
// Blueprint Callable API - UI System
// ================================================================

UUserWidget* APawPlayerHider::GetHUDSafe() const
{
	// Only return HUD on locally controlled clients
	if (HasValidHUD())
	{
		return HUD;
	}
	return nullptr;
}

bool APawPlayerHider::HasValidHUD() const
{
	// Only check HUD on locally controlled clients
	return IsLocallyControlled() && IsValid(HUD);
}

bool APawPlayerHider::IsEventDispatcherReady() const
{
	// Event dispatchers are ready when the object is fully initialized
	// Check if we're not in construction phase and object is valid
	return IsValid(this) && !HasAnyFlags(RF_NeedInitialization | RF_NeedLoad) && GetWorld() != nullptr;
}

void APawPlayerHider::TriggerHpChangedManually()
{
	// Manually trigger the HP changed event for UI binding
	if (IsEventDispatcherReady())
	{
		OnHpChanged.Broadcast(GetHealthPercentage());
	}
}

// ================================================================
// Networking - RPC Implementations
// ================================================================

void APawPlayerHider::ServerSetTeamId_Implementation(ETeamId NewTeamId)
{
	TeamId = NewTeamId;
}

void APawPlayerHider::ServerTakeHealthDamage_Implementation(float DamageAmount)
{
	TakeHealthDamage(DamageAmount);
}

void APawPlayerHider::ServerSetCaptured_Implementation(bool NewIsCaptured)
{
	bIsCaptured = NewIsCaptured;
}

void APawPlayerHider::ServerConvertToSeeker_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	// Get the player controller before conversion
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!IsValid(PC))
	{
		UE_LOG(LogTemp, Error, TEXT("ServerConvertToSeeker: No valid PlayerController found for %s"), *GetName());
		return;
	}

	// Validate seeker class is configured
	if (!IsValid(SeekerGhostClass))
	{
		UE_LOG(LogTemp, Error, TEXT("ServerConvertToSeeker: No SeekerGhostClass configured for %s"), *GetName());
		return;
	}

	// Get current transform for spawning the seeker
	FTransform CurrentTransform = GetActorTransform();

	// Spawn the new seeker ghost at the current location
	APawPlayerSeeker_Ghost* NewSeeker = GetWorld()->SpawnActor<APawPlayerSeeker_Ghost>(
		SeekerGhostClass,
		CurrentTransform.GetLocation(),
		CurrentTransform.GetRotation().Rotator()
	);

	if (!IsValid(NewSeeker))
	{
		UE_LOG(LogTemp, Error, TEXT("ServerConvertToSeeker: Failed to spawn seeker ghost for %s"), *GetName());
		return;
	}

	// Configure networking for the new seeker
	NewSeeker->SetReplicates(true);
	NewSeeker->SetReplicateMovement(true);

	// Transfer ownership and possession
	NewSeeker->SetOwner(PC);
	PC->Possess(NewSeeker);

	NewSeeker->SetTeamId(ETeamId::Seeker);
	SetTeamId(ETeamId::Seeker);

	UE_LOG(LogTemp, Log, TEXT("ServerConvertToSeeker: Successfully converted %s to seeker ghost"),
	       *GetActorNameOrLabel());

	// Note: Hider visual updates are now handled by the new seeker's BeginPlay()
	// This eliminates timing issues with network replication

	// Clean up all timers before destroying to prevent memory leaks
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		FTimerManager& TimerManager = World->GetTimerManager();

		if (HealthEffectTimerHandle.IsValid())
		{
			TimerManager.ClearTimer(HealthEffectTimerHandle);
		}


		if (DieTimerHandle.IsValid())
		{
			TimerManager.ClearTimer(DieTimerHandle);
		}
	}

	// Destroy the hider character
	Destroy();
}

void APawPlayerHider::ServerRequestCancelHorizontalVelocity_Implementation()
{
	if (GetCharacterMovement())
	{
		FVector CurrentVelocity = GetCharacterMovement()->Velocity;
		CurrentVelocity.X = 0.0f;
		CurrentVelocity.Y = 0.0f;
		// Z unchanged
		GetCharacterMovement()->Velocity = CurrentVelocity;
	}
}

void APawPlayerHider::ServerToggleWalk_Implementation()
{
	ToggleWalk();
}


void APawPlayerHider::MulticastUpdateStealthVisuals_Implementation()
{
	// Network safety checks before updating visuals
	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		UE_LOG(LogTemp, Warning, TEXT("MulticastUpdateStealthVisuals: Actor is invalid or being destroyed for %s"),
		       IsValid(this) ? *GetName() : TEXT("Invalid Actor"));
		return;
	}

	// Ensure we have a valid world and are not in invalid network state
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->bIsTearingDown)
	{
		UE_LOG(LogTemp, Warning, TEXT("MulticastUpdateStealthVisuals: World is invalid or tearing down for %s"),
		       *GetName());
		return;
	}

	// Check if materials are currently being modified by another system
	if (USkeletalMeshComponent* MeshComp = GetMesh(); IsValid(MeshComp))
	{
		// Ensure mesh component is in a valid state for material changes
		if (!MeshComp->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Warning, TEXT("MulticastUpdateStealthVisuals: MeshComponent is invalid for %s"),
			       *GetName());
			return;
		}
	}

	UpdateStealthVisuals();
}

void APawPlayerHider::MulticastOnDeath_Implementation()
{
	OnDeathStarted.Broadcast();
}

void APawPlayerHider::ClientUpdateHealth_Implementation(float NewHealth)
{
	Health = NewHealth;
	OnHealthChanged(Health, MaxHealth);
	OnHpChanged.Broadcast(GetHealthPercentage());
}

void APawPlayerHider::Client_CreateHUD_Implementation()
{
	// Early validation - only create HUD for player-controlled pawns
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!IsValid(PC))
	{
		return;
	}

	// Early validation - check if HUD widget class is configured
	if (!IsValid(HUDWidgetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("HUDWidgetClass is not set for %s"), *GetName());
		return;
	}

	// Setup HUD widget and configure child widgets
	SetupHUDWidget(PC);
}

void APawPlayerHider::Client_HandleOnDeathStartedUI_Implementation()
{
	DeleteHpBar();
	ShowYouDieText();
}

void APawPlayerHider::Client_HandleConvertSeekerUI_Implementation()
{
	HideYouDieText();
	SetCrosshairVisibility(true);
}

void APawPlayerHider::MulticastPlayJumpEffects_Implementation()
{
	PlayJumpSound();
	SpawnJumpVFX();
}

void APawPlayerHider::MulticastPlayLandEffects_Implementation()
{
	const float LandingVelocity = FMath::Abs(GetCharacterMovement()->Velocity.Z);
	const float Volume = FMath::Clamp(LandingVelocity / LandingVolumeDivisor, JumpSFXVolumeMin, JumpSFXVolumeMax);

	PlayLandSound(Volume);
	SpawnLandVFX();
}

void APawPlayerHider::MulticastSpawnConversionAuraEffect_Implementation()
{
	SpawnConversionAura();
}

void APawPlayerHider::MulticastSpawnConversionBurstEffect_Implementation()
{
	SpawnConversionBurstVFX();
}

// ================================================================
// Networking - RepNotify Functions
// ================================================================


void APawPlayerHider::OnRep_Health()
{
	// Trigger health change events on clients when health replicates
	OnHealthChanged(Health, MaxHealth);
	OnHpChanged.Broadcast(GetHealthPercentage());
}

void APawPlayerHider::OnRep_IsInvisible()
{
	// Prevent visual updates during actor destruction to avoid crashes
	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		UE_LOG(LogTemp, Warning, TEXT("OnRep_IsInvisible: Skipping update for %s during destruction"), *GetName());
		return;
	}

	UpdateStealthVisuals();
	OnInvisibilityChanged(bIsInvisible);
}

void APawPlayerHider::OnRep_IsWalking()
{
	if (!IsValid(GetCharacterMovement()))
	{
		UE_LOG(LogTemp, Error, TEXT("OnRep_IsWalking: CharacterMovement is invalid!"));
		return;
	}

	if (bIsWalking)
	{
		GetCharacterMovement()->MaxWalkSpeed = SlowWalkSpeed;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

// ================================================================
// Private Helper Functions - Internal System Functions
// ================================================================

void APawPlayerHider::InitializeMaterials()
{
	if (!IsValid(GetMesh()))
	{
		UE_LOG(LogTemp, Error, TEXT("InitializeMaterials: Mesh is not valid for %s"), *GetName());
		return;
	}

	const int32 MaterialCount = GetMesh()->GetNumMaterials();
	CachedBaseMaterials.Empty();
	CachedBaseMaterials.Reserve(MaterialCount);

	// Cache all materials from the mesh
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		UMaterialInterface* Material = GetMesh()->GetMaterial(MaterialIndex);
		CachedBaseMaterials.Add(Material);

		if (!IsValid(Material))
		{
			UE_LOG(LogTemp, Warning, TEXT("InitializeMaterials: Material at slot %d is null for %s"), MaterialIndex,
			       *GetName());
		}
	}

	// Validate invisible material setup
	if (!IsValid(InvisibleMaterial))
	{
		UE_LOG(LogTemp, Error,
		       TEXT("InitializeMaterials: InvisibleMaterial is not set for %s. Stealth effects will not work properly."
		       ), *GetName());
	}

	// Materials cached successfully
}

void APawPlayerHider::StartHealthEffectTimer()
{
	if (HasAuthority() && IsAlive() && HealthEffectInterval > 0)
	{
		// Start repeating timer for health effects (damage and healing)
		GetWorldTimerManager().SetTimer(HealthEffectTimerHandle, this, &APawPlayerHider::OnHealthEffectTimerTick,
		                                HealthEffectInterval, true, 0.0f);
	}
}

void APawPlayerHider::StopHealthEffectTimer()
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(HealthEffectTimerHandle);
	}
}

void APawPlayerHider::OnHealthEffectTimerTick()
{
	// Safety check: ensure object is still valid
	if (!IsValid(this) || !HasAuthority() || !IsAlive())
	{
		return;
	}

	bool bCurrentlyLit = bIsInLight || bIsSpotLighted;

	if (bIsCaptured)
	{
		// Apply capture damage
		TakeHealthDamage(CaptureDamageAmount);
		return;
	}

	if (bCurrentlyLit)
	{
		// Apply lit damage
		TakeHealthDamage(LitDamageAmount);
	}
	else
	{
		// Apply shadow healing
		Heal(ShadowHealAmount);
	}
	// Do nothing if in shadow but captured
}

// ================================================================
// Private Helper Functions - Stealth Helper Functions
// ================================================================

bool APawPlayerHider::IsViewerOnSeekerTeam() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Iterator->Get();
		if (!IsValid(PC) || !PC->IsLocalController())
		{
			continue;
		}

		APawn* LocalPawn = PC->GetPawn();
		if (!IsValid(LocalPawn))
		{
			continue;
		}

		if (!LocalPawn->GetClass()->ImplementsInterface(UTeamableInterface::StaticClass()))
		{
			continue;
		}

		ETeamId LocalTeam = Execute_GetTeamId(LocalPawn);
		return LocalTeam == ETeamId::Seeker;
	}

	return false;
}

void APawPlayerHider::ApplyInvisibilityMaterials()
{
	if (!IsValid(InvisibleMaterial))
	{
		return;
	}
	// Cache mesh component pointer to avoid repeated GetMesh() calls
	USkeletalMeshComponent* MeshComp = GetMesh();

	if (!IsValid(MeshComp) || !MeshComp->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid mesh, skipping material restore for %s"), *GetName());
		return;
	}
	if (MeshComp->GetBodyInstance() && !MeshComp->GetBodyInstance()->IsValidBodyInstance())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid physics body, skipping material restore"));
		return;
	}

	const int32 MaterialCount = MeshComp->GetNumMaterials();
	const bool bViewerIsSeeker = IsViewerOnSeekerTeam();

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		// Re-validate mesh component before each SetMaterial call
		if (!IsValid(MeshComp) || !MeshComp->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyInvisibilityMaterials: Mesh became invalid during loop for %s"),
			       *GetName());
			break;
		}

		// Check if the body instance is still valid before material change
		if (MeshComp->GetBodyInstance() && !MeshComp->GetBodyInstance()->IsValidBodyInstance())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("ApplyInvisibilityMaterials: Body instance became invalid during loop for %s"), *GetName());
			break;
		}

		// Apply materials with UE-specific error checking
		UMaterialInterface* MaterialToApply = nullptr;

		if (MaterialIndex == 0)
		{
			// Slot 0 (body): Always use invisible material when invisible
			MaterialToApply = InvisibleMaterial;
		}
		else if (MaterialIndex == 1)
		{
			// Slot 1 (facial): Only invisible for Seekers, visible for Hiders
			if (bViewerIsSeeker)
			{
				MaterialToApply = InvisibleMaterial;
			}
			else
			{
				// Keep original facial material for Hiders
				if (MaterialIndex < CachedBaseMaterials.Num() && IsValid(CachedBaseMaterials[MaterialIndex]))
				{
					MaterialToApply = CachedBaseMaterials[MaterialIndex];
				}
			}
		}
		else
		{
			// Other slots: Use invisible material
			MaterialToApply = InvisibleMaterial;
		}

		// Validate material before applying
		if (IsValid(MaterialToApply) && MaterialToApply->IsValidLowLevel())
		{
			MeshComp->SetMaterial(MaterialIndex, MaterialToApply);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ApplyInvisibilityMaterials: Invalid material to apply at index %d for %s"),
			       MaterialIndex, *GetName());
			// Use default material as fallback
			if (UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
			{
				MeshComp->SetMaterial(MaterialIndex, DefaultMaterial);
			}
		}
	}
}

void APawPlayerHider::RestoreOriginalMaterials()
{
	// Cache mesh component pointer to avoid repeated GetMesh() calls
	USkeletalMeshComponent* MeshComp = GetMesh();

	// Validate mesh before accessing it
	if (!IsValid(MeshComp) || !MeshComp->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("RestoreOriginalMaterials: Mesh is not valid for %s"), *GetName());
		return;
	}

	// Check body instance validity before attempting material changes
	if (MeshComp->GetBodyInstance() && !MeshComp->GetBodyInstance()->IsValidBodyInstance())
	{
		UE_LOG(LogTemp, Warning, TEXT("RestoreOriginalMaterials: Body instance is not valid for %s"), *GetName());
		return;
	}

	const int32 MaterialCount = MeshComp->GetNumMaterials();

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount && MaterialIndex < CachedBaseMaterials.Num(); ++
	     MaterialIndex)
	{
		// Re-validate mesh component before each SetMaterial call
		if (!IsValid(MeshComp) || !MeshComp->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Warning, TEXT("RestoreOriginalMaterials: Mesh became invalid during loop for %s"),
			       *GetName());
			break;
		}

		// Check if the body instance is still valid before material change
		if (MeshComp->GetBodyInstance() && !MeshComp->GetBodyInstance()->IsValidBodyInstance())
		{
			UE_LOG(LogTemp, Warning, TEXT("RestoreOriginalMaterials: Body instance became invalid during loop for %s"),
			       *GetName());
			break;
		}

		// Double-check array bounds before accessing
		if (MaterialIndex >= CachedBaseMaterials.Num())
		{
			UE_LOG(LogTemp, Error,
			       TEXT("RestoreOriginalMaterials: MaterialIndex %d out of bounds (array size: %d) for %s"),
			       MaterialIndex, CachedBaseMaterials.Num(), *GetName());
			break;
		}

		UMaterialInterface* CachedMaterial = CachedBaseMaterials[MaterialIndex];

		// Validate material before accessing its properties
		if (!IsValid(CachedMaterial))
		{
			UE_LOG(LogTemp, Warning, TEXT("RestoreOriginalMaterials: Cached material at index %d is invalid for %s"),
			       MaterialIndex, *GetName());
			continue;
		}

		// Check if material name is valid before calling GetName()
		if (!CachedMaterial->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Error,
			       TEXT("RestoreOriginalMaterials: Cached material at index %d has invalid low level for %s"),
			       MaterialIndex, *GetName());
			continue;
		}

		// Safely get material name with additional validation
		FString MaterialName;
		if (CachedMaterial->GetFName().IsValid())
		{
			MaterialName = CachedMaterial->GetName();
			// Check for None material
			if (MaterialName == TEXT("None") || MaterialName.IsEmpty())
			{
				UE_LOG(LogTemp, Error,
				       TEXT("RestoreOriginalMaterials: Cached material at index %d is None or empty for %s"),
				       MaterialIndex, *GetName());
				continue;
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT("RestoreOriginalMaterials: Cached material at index %d has invalid FName for %s"),
			       MaterialIndex, *GetName());
			continue;
		}

		// Apply material with safer approach
		MeshComp->SetMaterial(MaterialIndex, CachedMaterial);
	}
}

void APawPlayerHider::ValidateAndRefreshMaterials()
{
	// Validate mesh component first
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!IsValid(MeshComp))
	{
		UE_LOG(LogTemp, Warning, TEXT("ValidateAndRefreshMaterials: Invalid mesh component for %s"), *GetName());
		return;
	}

	const int32 CurrentMaterialCount = MeshComp->GetNumMaterials();
	bool bNeedRefreshMaterial = false;

	// Check if material count has changed
	if (CurrentMaterialCount != CachedBaseMaterials.Num())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("ValidateAndRefreshMaterials: Material count mismatch (current: %d, cached: %d) for %s"),
		       CurrentMaterialCount, CachedBaseMaterials.Num(), *GetName());
		bNeedRefreshMaterial = true;
	}

	// Check if any cached materials have become invalid
	if (!bNeedRefreshMaterial)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < CachedBaseMaterials.Num(); ++MaterialIndex)
		{
			UMaterialInterface* CachedMaterial = CachedBaseMaterials[MaterialIndex];
			if (!IsValid(CachedMaterial) || !CachedMaterial->IsValidLowLevel())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("ValidateAndRefreshMaterials: Cached material at index %d is invalid for %s"),
				       MaterialIndex, *GetName());
				bNeedRefreshMaterial = true;
				break;
			}

			// Check if the material name suggests it's been garbage collected
			if (CachedMaterial->GetFName().IsValid())
			{
				FString MaterialName = CachedMaterial->GetName();
				if (MaterialName == TEXT("None") || MaterialName.IsEmpty())
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("ValidateAndRefreshMaterials: Cached material at index %d has invalid name for %s"),
					       MaterialIndex, *GetName());
					bNeedRefreshMaterial = true;
					break;
				}
			}
			else
			{
				bNeedRefreshMaterial = true;
				break;
			}
		}
	}

	// Re-cache materials if needed
	if (bNeedRefreshMaterial)
	{
		UE_LOG(LogTemp, Log, TEXT("ValidateAndRefreshMaterials: Re-caching materials for %s"), *GetName());
		InitializeMaterials();
	}
}

void APawPlayerHider::HandlePossessionChanged(APawn* OldPawn, APawn* NewPawn)
{
	// Only respond to valid possession changes
	if (!IsValid(NewPawn))
	{
		return;
	}

	// Check if a new seeker joined the game
	if (Cast<APawPlayerSeeker>(NewPawn) || Cast<APawPlayerSeeker_Ghost>(NewPawn))
	{
		UE_LOG(LogTemp, Log, TEXT("HandlePossessionChanged: Seeker joined (%s) - updating hider visuals for %s"),
		       *NewPawn->GetName(), *GetName());

		// Update stealth visuals now that team composition has changed
		UpdateStealthVisuals();
	}

	// Optional: Handle seeker leaving (if OldPawn was seeker but NewPawn is not)
	// This could be useful for future game modes where seekers can become hiders
}

// ================================================================
// Private Helper Functions - UI Helper Functions
// ================================================================

void APawPlayerHider::SetupHUDWidget(APlayerController* PC)
{
	HUD = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
	if (!IsValid(HUD))
	{
		return;
	}

	// Add widget to viewport
	HUD->AddToViewport();
	HUD->SetVisibility(ESlateVisibility::Visible);

	// Configure child widgets
	ConfigureCrosshair();
	ConfigureHealthBar();

	// Broadcast initial HP changed event
	OnHpChanged.Broadcast(GetHealthPercentage());
}

void APawPlayerHider::ConfigureCrosshair()
{
	if (!IsValid(HUD))
	{
		return;
	}

	UWidget* CrosshairWidget = HUD->GetWidgetFromName(TEXT("IMG_Crosshair"));
	if (IsValid(CrosshairWidget))
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void APawPlayerHider::ConfigureHealthBar()
{
	if (!IsValid(HUD))
	{
		return;
	}

	UWidget* HealthBarWidget = HUD->GetWidgetFromName(TEXT("WBP_HealthBar"));
	if (!IsValid(HealthBarWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("Client_CreateHUD: HealthBar widget not found in HUD for %s"), *GetName());
		return;
	}

	// Call Blueprint Custom Event to set target Pawn for HealthBar
	UFunction* SetTargetPawnEvent = HealthBarWidget->GetClass()->FindFunctionByName(TEXT("SetTargetPawn"));
	if (!IsValid(SetTargetPawnEvent))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("Client_CreateHUD: SetTargetPawn Custom Event not found on HealthBar widget for %s"), *GetName());
		return;
	}

	struct FSetTargetPawnParams
	{
		APawn* TargetPawn;
	};

	FSetTargetPawnParams Params;
	Params.TargetPawn = this;
	HealthBarWidget->ProcessEvent(SetTargetPawnEvent, &Params);
}

void APawPlayerHider::DeleteHpBar()
{
	if (!IsValid(HUD))
	{
		return;
	}

	UWidget* HealthBarWidget = HUD->GetWidgetFromName(TEXT("WBP_HealthBar"));
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void APawPlayerHider::ShowYouDieText()
{
	if (!IsValid(HUD))
	{
		return;
	}

	// Call Blueprint event to show death text
	UFunction* ShowDeathTextEvent = HUD->GetClass()->FindFunctionByName(TEXT("ShowYouDieText"));
	if (IsValid(ShowDeathTextEvent))
	{
		HUD->ProcessEvent(ShowDeathTextEvent, nullptr);
	}
}

void APawPlayerHider::HideYouDieText()
{
	if (!IsValid(HUD))
	{
		return;
	}

	// Hide death text
	UFunction* ShowDeathTextEvent = HUD->GetClass()->FindFunctionByName(TEXT("HideYouDieText"));
	if (IsValid(ShowDeathTextEvent))
	{
		HUD->ProcessEvent(ShowDeathTextEvent, nullptr);
	}
}

void APawPlayerHider::SetCrosshairVisibility(bool bVisible)
{
	if (!IsValid(HUD))
	{
		return;
	}

	UWidget* CrosshairWidget = HUD->GetWidgetFromName(TEXT("IMG_Crosshair"));
	if (IsValid(CrosshairWidget))
	{
		ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		CrosshairWidget->SetVisibility(NewVisibility);
	}
}

// ================================================================
// Private Helper Functions - Jump System Helper Functions
// ================================================================

void APawPlayerHider::LoadAssetsAsync()
{
	TArray<FSoftObjectPath> AssetsToLoad;

	if (!VanishSoundAsset.IsNull()) AssetsToLoad.Add(VanishSoundAsset.ToSoftObjectPath());
	if (!JumpSoundAsset.IsNull()) AssetsToLoad.Add(JumpSoundAsset.ToSoftObjectPath());
	if (!LandSoundAsset.IsNull()) AssetsToLoad.Add(LandSoundAsset.ToSoftObjectPath());
	if (!JumpVFXAsset.IsNull()) AssetsToLoad.Add(JumpVFXAsset.ToSoftObjectPath());
	if (!LandVFXAsset.IsNull()) AssetsToLoad.Add(LandVFXAsset.ToSoftObjectPath());
	if (!LandForceFeedbackAsset.IsNull()) AssetsToLoad.Add(LandForceFeedbackAsset.ToSoftObjectPath());
	if (!ConversionAuraVFXAsset.IsNull()) AssetsToLoad.Add(ConversionAuraVFXAsset.ToSoftObjectPath());
	if (!ConversionBurstVFXAsset.IsNull()) AssetsToLoad.Add(ConversionBurstVFXAsset.ToSoftObjectPath());
	if (!ConversionAuraSoundAsset.IsNull()) AssetsToLoad.Add(ConversionAuraSoundAsset.ToSoftObjectPath());

	if (AssetsToLoad.Num() > 0)
	{
		AssetsHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			AssetsToLoad,
			FStreamableDelegate::CreateUObject(this, &APawPlayerHider::OnAssetsLoaded)
		);
	}
}

void APawPlayerHider::OnAssetsLoaded()
{
	// Safety check: ensure object is still valid
	if (!IsValid(this))
	{
		return;
	}

	VanishSound = VanishSoundAsset.Get();
	if (!VanishSound && !VanishSoundAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load JumpSoundAsset for %s"), *GetName());
	}

	JumpSound = JumpSoundAsset.Get();
	if (!JumpSound && !JumpSoundAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load JumpSoundAsset for %s"), *GetName());
	}

	LandSound = LandSoundAsset.Get();
	if (!LandSound && !LandSoundAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load LandSoundAsset for %s"), *GetName());
	}

	JumpVFX = JumpVFXAsset.Get();
	if (!JumpVFX && !JumpVFXAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load JumpVFXAsset for %s"), *GetName());
	}

	LandVFX = LandVFXAsset.Get();
	if (!LandVFX && !LandVFXAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load LandVFXAsset for %s"), *GetName());
	}

	LandForceFeedback = LandForceFeedbackAsset.Get();
	if (!LandForceFeedback && !LandForceFeedbackAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load LandForceFeedbackAsset for %s"), *GetName());
	}

	if (ConversionAuraVFXAsset.Get() == nullptr && !ConversionAuraVFXAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load ConversionAuraVFXAsset for %s"), *GetName());
	}

	if (ConversionBurstVFXAsset.Get() == nullptr && !ConversionBurstVFXAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load ConversionBurstVFXAsset for %s"), *GetName());
	}

	ConversionAuraSound = ConversionAuraSoundAsset.Get();
	if (ConversionAuraSoundAsset.Get() == nullptr && !ConversionAuraSoundAsset.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load ConversionAuraSoundAsset for %s"), *GetName());
	}
}

void APawPlayerHider::SpawnJumpVFX()
{
	if (!IsValid(JumpVFX))
	{
		if (!JumpVFXAsset.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("JumpVFX not loaded for %s"), *GetName());
		}
		return;
	}

	TObjectPtr<UNiagaraComponent> JumpVFXComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), JumpVFX, GetActorLocation(), GetActorRotation(),
		FVector::OneVector, true, true, ENCPoolMethod::AutoRelease, true);

	if (IsValid(JumpVFXComp))
	{
		JumpVFXComp->SetVariableFloat(FName("VelocityScale"), JumpVFXVelocityScale);
	}
}

void APawPlayerHider::SpawnLandVFX()
{
	if (!IsValid(LandVFX))
	{
		if (!LandVFXAsset.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("LandVFX not loaded for %s"), *GetName());
		}
		return;
	}

	TObjectPtr<UNiagaraComponent> LandVFXComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), LandVFX, GetActorLocation(), GetActorRotation(),
		FVector::OneVector, true, true, ENCPoolMethod::AutoRelease, true);

	if (IsValid(LandVFXComp))
	{
		LandVFXComp->SetVariableFloat(FName("VelocityScale"), LandVFXVelocityScale);
	}
}

void APawPlayerHider::PlayJumpSound()
{
	if (IsValid(JumpSound))
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), JumpSound, GetActorLocation());
	}
}

void APawPlayerHider::PlayLandSound(float Volume)
{
	if (IsValid(LandSound))
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), LandSound, GetActorLocation(), Volume);
	}
}

void APawPlayerHider::PlayLandForceFeedback()
{
	if (!IsValid(LandForceFeedback))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (IsValid(PC))
	{
		FForceFeedbackParameters Params;
		PC->ClientPlayForceFeedback(LandForceFeedback, Params);
	}
}

// ================================================================
// Collision Breaking
// ================================================================

void APawPlayerHider::BreakCollidedObject(AActor* HitActor)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	IPawCollideBreakableInterface* BreakableInterface = Cast<IPawCollideBreakableInterface>(HitActor);
	if (!BreakableInterface)
	{
		return;
	}

	// Check if this object can be broken by hiders
	if (!BreakableInterface->Execute_CanBeBreakByHider(HitActor))
	{
		return;
	}

	// Break the object
	BreakableInterface->Execute_Break(HitActor);
}

void APawPlayerHider::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                            FVector NormalImpulse, const FHitResult& Hit)
{
	// Only process on server
	if (!HasAuthority())
	{
		return;
	}

	// Automatically try to break the hit object
	BreakCollidedObject(OtherActor);
}

// ================================================================
// Private Helper Functions - Light Detection Optimization
// ================================================================


float APawPlayerHider::CalculateStaggeredDelay() const
{
	// Use player index or hash of actor name to create staggered delays
	int32 StaggerIndex = PlayerIndex >= 0 ? PlayerIndex : GetUniqueID();

	// Create offset based on player index (0-4 players = 0.0-0.2s offset)
	float StaggerOffset = (StaggerIndex % 5) * StaggerOffsetMultiplier;

	return StaggerOffset;
}
