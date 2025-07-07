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
	LitDamageAmount = 0.5f;
	ShadowHealAmount = 0.25f;
	HealthEffectInterval = 0.03f;

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
		StartHealthEffectTimer();
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

// ================================================================
// UI Helper Functions
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
// Stealth Helper Functions
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

// ================================================================
// Health Effects System
// ================================================================
void APawPlayerHider::StartHealthEffectTimer()
{
	if (HasAuthority() && IsAlive() && HealthEffectInterval > 0)
	{
		// Start repeating timer for health effects (damage and healing)
		GetWorldTimerManager().SetTimer(HealthEffectTimerHandle, this, &APawPlayerHider::OnHealthEffectTick,
		                                HealthEffectInterval, true, 0.0f);
	}
}

void APawPlayerHider::StopHealthEffects()
{
	if (HasAuthority())
	{
		GetWorldTimerManager().ClearTimer(HealthEffectTimerHandle);
	}
}

void APawPlayerHider::OnHealthEffectTick()
{
	if (!HasAuthority() || !IsAlive())
	{
		return;
	}

	bool bCurrentlyLit = bIsInLight || bIsSpotLighted;

	if (bCurrentlyLit)
	{
		// Apply lit damage
		TakeHealthDamage(LitDamageAmount);
	}
	else if (!IsCaptured)
	{
		// Apply shadow healing
		Heal(ShadowHealAmount);
	}
	// Do nothing if in shadow but captured
}
