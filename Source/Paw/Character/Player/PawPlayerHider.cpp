#include "PawPlayerHider.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "../../Core/System/PawLightDetectionSubsystem.h"

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
	LitDamageAmount = 10.0f;
	LitDamageInterval = 0.1f;

	// Light Detection System
	bIsInLight = false;
	bIsSpotLighted = false;

	// Stealth System
	bIsInvisible = false;
	StealthOpacity = 0.5f;
	OpacityParameterName = FName("Opacity");

	// Capture System
	IsCaptured = false;

	// Multiplayer Properties
	bIsLocalPlayer = false;
	PlayerIndex = -1;

	// UI System
	HUD = nullptr;
}

void APawPlayerHider::BeginPlay()
{
	Super::BeginPlay();

	InitializeMaterials();

	// Start lit damage timer immediately (always running)
	if (HasAuthority())
	{
		StartLitDamage();
	}

	// Create HUD for player-controlled pawns
	if (IsLocallyControlled())
	{
		Client_CreateHUD();
	}
}

void APawPlayerHider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		CheckLightExposure();
	}
}

void APawPlayerHider::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void APawPlayerHider::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APawPlayerHider, Health);
	DOREPLIFETIME(APawPlayerHider, bIsDead);
	DOREPLIFETIME(APawPlayerHider, bIsInLight);
	DOREPLIFETIME(APawPlayerHider, bIsSpotLighted);
	DOREPLIFETIME(APawPlayerHider, bIsInvisible);
	DOREPLIFETIME(APawPlayerHider, IsCaptured);
	DOREPLIFETIME(APawPlayerHider, bIsLocalPlayer);
	DOREPLIFETIME(APawPlayerHider, PlayerIndex);
}

// ================================================================
// Health System
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
		OnDeath.Broadcast();
		MulticastOnDeath();
	}
}

void APawPlayerHider::Respawn()
{
	if (HasAuthority())
	{
		bIsDead = false;
		Health = MaxHealth;
		OnHealthChanged(Health, MaxHealth);
		OnHpChanged.Broadcast(GetHealthPercentage());
	}
}

// ================================================================
// Light Detection System
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
	bool bCanBecomeInvisible = !bCurrentlyLit && !IsCaptured;

	// Automatically become invisible if in shadows and not captured
	if (bCanBecomeInvisible && !bIsInvisible)
	{
		ActivateInvisibility();
	}
	// Become visible if lit or captured
	else if (bIsInvisible && (bCurrentlyLit || IsCaptured))
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
// Stealth System
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
	if (!IsValid(GetMesh()))
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateStealthVisuals: Mesh is not valid for %s"), *GetName());
		return;
	}

	const int32 MaterialCount = GetMesh()->GetNumMaterials();
	
	if (bIsInvisible)
	{
		if (IsValid(InvisibleMaterial))
		{
			// Apply invisible material to all material slots
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				GetMesh()->SetMaterial(MaterialIndex, InvisibleMaterial);
			}
		}
	}
	else
	{
		// Restore original materials
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount && MaterialIndex < CachedBaseMaterials.Num(); ++MaterialIndex)
		{
			if (IsValid(CachedBaseMaterials[MaterialIndex]))
			{
				GetMesh()->SetMaterial(MaterialIndex, CachedBaseMaterials[MaterialIndex]);
			}
		}
	}

	// Set opacity parameter if parameter name is configured
	if (!OpacityParameterName.IsNone())
	{
		const float OpacityValue = bIsInvisible ? GetOpacityForViewingTeam() : 1.0f;
		GetMesh()->SetScalarParameterValueOnMaterials(OpacityParameterName, OpacityValue);
	}
}

void APawPlayerHider::MulticastUpdateStealthVisuals_Implementation()
{
	UpdateStealthVisuals();
}

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

float APawPlayerHider::GetOpacityForViewingTeam() const
{
	// Get the local player's team to determine what opacity they should see
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		// Check all local player controllers to find the one viewing this pawn
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APlayerController* PC = Iterator->Get(); IsValid(PC) && PC->IsLocalController())
			{
				if (APawn* LocalPawn = PC->GetPawn(); IsValid(LocalPawn))
				{
					// Check if local player implements team interface
					if (LocalPawn->GetClass()->ImplementsInterface(UTeamableInterface::StaticClass()))
					{
						// Use Execute_ wrapper for Blueprint-compatible interface calls
						ETeamId LocalTeam = ITeamableInterface::Execute_GetTeamId(LocalPawn);
						
						// If local player is on Seeker team, they should see full invisibility (0.0f)
						if (LocalTeam == ETeamId::Seeker)
						{
							return 0.0f;
						}
						// If local player is on Hider team (same team), they should see partial opacity
						if (LocalTeam == ETeamId::Hider)
						{
							return StealthOpacity; // Default 0.5f
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("GetOpacityForViewingTeam: Local pawn %s does not implement ITeamableInterface"), *LocalPawn->GetName());
					}
				}
			}
		}
	}
	
	// Fallback: use default stealth opacity
	return StealthOpacity;
}

// ================================================================
// RPC Functions
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
	IsCaptured = NewIsCaptured;
}

void APawPlayerHider::MulticastOnDeath_Implementation()
{
	OnDeath.Broadcast();
}

void APawPlayerHider::ClientUpdateHealth_Implementation(float NewHealth)
{
	Health = NewHealth;
	OnHealthChanged(Health, MaxHealth);
	OnHpChanged.Broadcast(GetHealthPercentage());
}

void APawPlayerHider::Client_CreateHUD_Implementation()
{
	// Only create HUD for player-controlled pawns
	if (APlayerController* PC = Cast<APlayerController>(GetController()); IsValid(PC))
	{
		// Create the HUD widget using configurable class
		if (IsValid(HUDWidgetClass))
		{
			HUD = CreateWidget<UUserWidget>(PC, HUDWidgetClass);

			if (IsValid(HUD))
			{
				// Add widget to viewport
				HUD->AddToViewport();

				// Set widget visibility
				HUD->SetVisibility(ESlateVisibility::Visible);

				// Hide crosshair for hiders (as shown in Blueprint)
				if (UWidget* CrosshairWidget = HUD->GetWidgetFromName(TEXT("IMG_Crosshair")); IsValid(CrosshairWidget))
				{
					CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
				}

				// Find and configure HealthBar child widget
				if (UWidget* HealthBarWidget = HUD->GetWidgetFromName(TEXT("WBP_HealthBar")); IsValid(HealthBarWidget))
				{
					// Call Blueprint Custom Event to set target Pawn for HealthBar
					if (UFunction* SetTargetPawnEvent = HealthBarWidget->GetClass()->FindFunctionByName(TEXT("SetTargetPawn")))
					{
						struct FSetTargetPawnParams
						{
							APawn* TargetPawn;
						};
						
						FSetTargetPawnParams Params;
						Params.TargetPawn = this;
						HealthBarWidget->ProcessEvent(SetTargetPawnEvent, &Params);
						
						// SetTargetPawn event called successfully
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("Client_CreateHUD: SetTargetPawn Custom Event not found on HealthBar widget for %s"), *GetName());
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Client_CreateHUD: HealthBar widget not found in HUD for %s"), *GetName());
				}

				// Broadcast initial HP changed event
				OnHpChanged.Broadcast(GetHealthPercentage());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HUDWidgetClass is not set for %s"), *GetName());
		}
	}
}

// ================================================================
// UI System Functions
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

// Event Dispatcher Helper Functions
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
// Internal System Functions
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
			UE_LOG(LogTemp, Warning, TEXT("InitializeMaterials: Material at slot %d is null for %s"), MaterialIndex, *GetName());
		}
	}

	// Validate invisible material setup
	if (!IsValid(InvisibleMaterial))
	{
		UE_LOG(LogTemp, Error, TEXT("InitializeMaterials: InvisibleMaterial is not set for %s. Stealth effects will not work properly."), *GetName());
	}

	// Materials cached successfully
}

// ================================================================
// Lit Damage System
// ================================================================
void APawPlayerHider::StartLitDamage()
{
	if (HasAuthority() && IsAlive() && LitDamageAmount > 0 && LitDamageInterval > 0)
	{
		// Start repeating timer for lit damage (always running)
		GetWorldTimerManager().SetTimer(LitDamageTimerHandle, this, &APawPlayerHider::OnLitDamageTimeout,
		                                LitDamageInterval, true, 0.0f);
	}
}

void APawPlayerHider::StopLitDamage()
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(LitDamageTimerHandle);
	}
}

void APawPlayerHider::OnLitDamageTimeout()
{
	if (HasAuthority() && IsAlive())
	{
		// Only apply damage if currently lit (in light or spotlight)
		if (bool bCurrentlyLit = bIsInLight || bIsSpotLighted)
		{
			TakeHealthDamage(LitDamageAmount);
		}
	}
}
