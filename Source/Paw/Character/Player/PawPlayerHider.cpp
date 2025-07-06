#include "PawPlayerHider.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Components/Slider.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"

APawPlayerHider::APawPlayerHider()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Initialize default values
	TeamId = ETeamId::Hider;
	Health = 100.0f;
	MaxHealth = 100.0f;
	bIsDead = false;
	bIsInLight = false;
	bIsSpotLighted = false;
	LightDetectionRadius = 500.0f;
	LightIntensityThreshold = 0.5f;
	bIsInvisible = false;
	InvisibilityDuration = 5.0f;
	StealthOpacity = 0.3f;
	WalkSpeed = 300.0f;
	LitDamageAmount = 10.0f;
	LitDamageInterval = 0.1f;
	IsCaptured = false;
	bIsLocalPlayer = false;
	PlayerIndex = -1;

	// Initialize UI
	HUD = nullptr;

	// Initialize cached references
	CachedSunLightActor = nullptr;
}

void APawPlayerHider::BeginPlay()
{
	Super::BeginPlay();

	InitializeMaterials();
	UpdateMovementSpeed();

	// Find and cache SunLight actor
	FindSunLightActor();

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
	// Additional input bindings can be added here
}

void APawPlayerHider::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APawPlayerHider, TeamId);
	DOREPLIFETIME(APawPlayerHider, Health);
	DOREPLIFETIME(APawPlayerHider, bIsDead);
	DOREPLIFETIME(APawPlayerHider, bIsInLight);
	DOREPLIFETIME(APawPlayerHider, bIsSpotLighted);
	DOREPLIFETIME(APawPlayerHider, bIsInvisible);
	DOREPLIFETIME(APawPlayerHider, IsCaptured);
	DOREPLIFETIME(APawPlayerHider, bIsLocalPlayer);
	DOREPLIFETIME(APawPlayerHider, PlayerIndex);
}

// Team Interface Implementation
ETeamId APawPlayerHider::GetTeamId_Implementation() const
{
	return TeamId;
}

void APawPlayerHider::SetTeamId_Implementation(ETeamId NewTeamId)
{
	if (HasAuthority())
	{
		TeamId = NewTeamId;
	}
	else
	{
		ServerSetTeamId(NewTeamId);
	}
}

// Health System
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

// Light Detection
void APawPlayerHider::CheckLightExposure()
{
	if (!HasAuthority())
	{
		return;
	}

	bool bWasInLight = bIsInLight;
	bool bWasSpotLighted = bIsSpotLighted;

	// Reset light states
	bIsInLight = false;
	bIsSpotLighted = false;

	// Bubble Light Detection - replicate Blueprint's GetOverlappingActors logic
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);

	for (AActor* Actor : OverlappingActors)
	{
		// Check if actor has BubbleLight tag or is a PointLightComponent
		if (IsValid(Actor) && (Actor->Tags.Contains(TEXT("BubbleLight")) || Actor->GetClass()->GetName().Contains(
			TEXT("BubbleLight"))))
		{
			// Check for PointLightComponent
			UPointLightComponent* PointLight = Actor->FindComponentByClass<UPointLightComponent>();
			if (IsValid(PointLight))
			{
				// Calculate distance and check against light attenuation radius
				float Distance = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
				float AttenuationRadius = PointLight->AttenuationRadius;

				if (Distance <= AttenuationRadius)
				{
					bIsInLight = true;
					break; // Found a light source, no need to check more
				}
			}
		}
	}

	// Directional Light Detection - use SunLight actor's forward vector
	if (IsValid(CachedSunLightActor))
	{
		if (UWorld* World = GetWorld(); IsValid(World))
		{
			FVector Start = GetActorLocation();
			// Get the forward vector from SunLight actor (light direction)
			FVector LightDirection = CachedSunLightActor->GetActorForwardVector();
			// Trace in the direction of the light (distance: 1000 units)
			FVector End = Start - (LightDirection * 1000.0f);

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);

			// Line trace to check if we're in shadow from directional light
			bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

			// If hit something, we're in shadow; if no hit, we're lit by directional light
			if (!bHit)
			{
				bIsInLight = true;
			}
		}
	}

	// Handle invisibility - check if can be invisible forever in shadows
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

void APawPlayerHider::FindSunLightActor()
{
	// Find DirectionalLight actor with "SunLight" tag
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClassWithTag(World, ADirectionalLight::StaticClass(), FName("SunLight"),
		                                             FoundActors);

		if (FoundActors.Num() > 0)
		{
			CachedSunLightActor = FoundActors[0];
			UE_LOG(LogTemp, Log, TEXT("Found SunLight actor: %s"), *CachedSunLightActor->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No DirectionalLight with 'SunLight' tag found in level"));
		}
	}
}

// Stealth System
void APawPlayerHider::ActivateInvisibility()
{
	if (HasAuthority() && !bIsInvisible)
	{
		bIsInvisible = true;
		// Remove timer - invisibility is permanent while in shadows
		UpdateStealthVisuals();
		OnInvisibilityChanged(true);
	}
}

void APawPlayerHider::DeactivateInvisibility()
{
	if (HasAuthority() && bIsInvisible)
	{
		bIsInvisible = false;
		UpdateStealthVisuals();
		OnInvisibilityChanged(false);
	}
}

void APawPlayerHider::UpdateStealthVisuals()
{
	if (IsValid(DynamicMaterial))
	{
		float OpacityValue = bIsInvisible ? StealthOpacity : 1.0f;
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), OpacityValue);
	}
}

// Movement System
void APawPlayerHider::UpdateMovementSpeed()
{
	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement(); IsValid(MovementComp))
	{
		MovementComp->MaxWalkSpeed = WalkSpeed;
	}
}

// RPC Functions
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

				// Broadcast initial HP changed event
				OnHpChanged.Broadcast(GetHealthPercentage());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("HUDWidgetClass is not set for %s"), *GetName());
		}
	}
}

// HUD Access Functions
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

// Private Functions
void APawPlayerHider::OnInvisibilityTimeout()
{
	DeactivateInvisibility();
}


void APawPlayerHider::InitializeMaterials()
{
	if (IsValid(BaseMaterial) && IsValid(GetMesh()))
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		GetMesh()->SetMaterial(0, DynamicMaterial);
	}
}

// Lit Damage System
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
