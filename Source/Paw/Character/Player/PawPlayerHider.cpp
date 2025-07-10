#include "PawPlayerHider.h"
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
	bIsInLight = false;
	bIsSpotLighted = false;

	// Stealth System
	bIsInvisible = false;
	StealthOpacity = 0.5f;
	OpacityParameterName = FName("Opacity");

	// Capture System
	bIsCaptured = false;

	// Multiplayer Properties
	bIsLocalPlayer = false;
	PlayerIndex = -1;

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
	JumpSound = nullptr;
	JumpVFX = nullptr;
	LandVFX = nullptr;
	LandForceFeedback = nullptr;
}

void APawPlayerHider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		CheckLightExposure();
	}

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
	}

	// Start async loading of jump assets
	LoadJumpAssetsAsync();

	// Bind collision event for breaking objects
	if (IsValid(GetCapsuleComponent()))
	{
		GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &APawPlayerHider::OnHit);
	}

	// Start lit damage timer immediately (always running)
	if (HasAuthority())
	{
		StartHealthEffectTimer();
	}

	// Create HUD for player-controlled pawns
	if (IsLocallyControlled())
	{
		Client_CreateHUD();
	}
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

void APawPlayerHider::CheckLightExposure()
{
	if (!HasAuthority())
	{
		return;
	}

	bool bWasInLight = bIsInLight;
	bool bWasSpotLighted = bIsSpotLighted;

	// Query the Light Detection Subsystem for current light exposure
	if (UPawLightDetectionSubsystem* LightDetectionSubsystem = GetWorld()->GetSubsystem<UPawLightDetectionSubsystem>();
		IsValid(LightDetectionSubsystem))
	{
		FLightExposureResult LightResult = LightDetectionSubsystem->GetLightExposureState(GetActorLocation(), this);
		bIsInLight = LightResult.bIsInLight;
		// Note: bIsSpotLighted is controlled by Seeker players, not environment lights
		// LightResult.bIsSpotLighted is always false from subsystem
	}
	else
	{
		// Fallback: reset light states if subsystem not available
		bIsInLight = false;
		// bIsSpotLighted is not modified here - controlled by Seeker players
		UE_LOG(LogTemp, Error, TEXT("PawLightDetectionSubsystem not available for %s"), *GetName());
	}


	// Handle invisibility 
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

	// Notify if light state changed
	if (bWasInLight != bIsInLight || bWasSpotLighted != bIsSpotLighted)
	{
		OnLightExposureChanged(bCurrentlyLit);
	}
}

void APawPlayerHider::SetInLight(bool bInLight)
{
	if (HasAuthority() && bIsInLight != bInLight)
	{
		bIsInLight = bInLight;
		OnLightExposureChanged(bInLight);

		if (bInLight && bIsInvisible)
		{
			DeactivateInvisibility();
		}
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

void APawPlayerHider::UpdateStealthVisuals()
{
	// Early validation
	if (!IsValid(GetMesh()))
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateStealthVisuals: Mesh is not valid for %s"), *GetName());
		return;
	}

	// Handle visible state
	if (!bIsInvisible)
	{
		RestoreOriginalMaterials();
		if (!OpacityParameterName.IsNone())
		{
			GetMesh()->SetScalarParameterValueOnMaterials(OpacityParameterName, 1.0f);
		}
		return;
	}

	// Handle invisible state
	ApplyInvisibilityMaterials();
	if (!OpacityParameterName.IsNone())
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
	if (IsLocallyControlled() && IsValid(HUD))
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

	UE_LOG(LogTemp, Log, TEXT("ServerConvertToSeeker: Successfully converted %s to seeker ghost"), *GetActorLabel());

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

void APawPlayerHider::MulticastUpdateStealthVisuals_Implementation()
{
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
	UpdateStealthVisuals();
	OnInvisibilityChanged(bIsInvisible);
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
	if (!HasAuthority() || !IsAlive())
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
	if (!IsValid(GetMesh()) || !GetMesh()->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid mesh, skipping material restore for %s"), *GetName());
		return;
	}
	if (GetMesh()->GetBodyInstance() && !GetMesh()->GetBodyInstance()->IsValidBodyInstance())
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid physics body, skipping material restore"));
		return;
	}

	const int32 MaterialCount = GetMesh()->GetNumMaterials();
	const bool bViewerIsSeeker = IsViewerOnSeekerTeam();

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		if (MaterialIndex == 0)
		{
			// Slot 0 (body): Always use invisible material when invisible
			GetMesh()->SetMaterial(MaterialIndex, InvisibleMaterial);
		}
		else if (MaterialIndex == 1)
		{
			// Slot 1 (facial): Only invisible for Seekers, visible for Hiders
			if (bViewerIsSeeker)
			{
				GetMesh()->SetMaterial(MaterialIndex, InvisibleMaterial);
			}
			else
			{
				// Keep original facial material for Hiders
				if (MaterialIndex < CachedBaseMaterials.Num() && IsValid(CachedBaseMaterials[MaterialIndex]))
				{
					GetMesh()->SetMaterial(MaterialIndex, CachedBaseMaterials[MaterialIndex]);
				}
			}
		}
		else
		{
			// Other slots: Use invisible material
			GetMesh()->SetMaterial(MaterialIndex, InvisibleMaterial);
		}
	}
}

void APawPlayerHider::RestoreOriginalMaterials()
{
	const int32 MaterialCount = GetMesh()->GetNumMaterials();

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount && MaterialIndex < CachedBaseMaterials.Num(); ++
	     MaterialIndex)
	{
		if (IsValid(CachedBaseMaterials[MaterialIndex]))
		{
			GetMesh()->SetMaterial(MaterialIndex, CachedBaseMaterials[MaterialIndex]);
		}
	}
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

void APawPlayerHider::LoadJumpAssetsAsync()
{
	TArray<FSoftObjectPath> AssetsToLoad;

	if (!JumpSoundAsset.IsNull()) AssetsToLoad.Add(JumpSoundAsset.ToSoftObjectPath());
	if (!LandSoundAsset.IsNull()) AssetsToLoad.Add(LandSoundAsset.ToSoftObjectPath());
	if (!JumpVFXAsset.IsNull()) AssetsToLoad.Add(JumpVFXAsset.ToSoftObjectPath());
	if (!LandVFXAsset.IsNull()) AssetsToLoad.Add(LandVFXAsset.ToSoftObjectPath());
	if (!LandForceFeedbackAsset.IsNull()) AssetsToLoad.Add(LandForceFeedbackAsset.ToSoftObjectPath());

	if (AssetsToLoad.Num() > 0)
	{
		JumpAssetsHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			AssetsToLoad,
			FStreamableDelegate::CreateUObject(this, &APawPlayerHider::OnJumpAssetsLoaded)
		);
	}
}

void APawPlayerHider::OnJumpAssetsLoaded()
{
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
