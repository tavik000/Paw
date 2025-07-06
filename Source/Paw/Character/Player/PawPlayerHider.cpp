
  #include "PawPlayerHider.h"
  #include "Components/StaticMeshComponent.h"
  #include "Components/SceneComponent.h"
  #include "Materials/MaterialInstanceDynamic.h"
  #include "GameFramework/CharacterMovementComponent.h"
  #include "Net/UnrealNetwork.h"
  #include "Engine/Engine.h"

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
      RunSpeed = 600.0f;
      CrouchSpeed = 150.0f;
      bIsRunning = false;
      bIsCrouching = false;
      NoiseLevel = 0.0f;
      NoiseRadius = 800.0f;
      IsCaptured = false;
      bIsLocalPlayer = false;
      PlayerIndex = -1;

      // Initialize UI
      HUD = nullptr;

      // Create components
      LightDetectionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LightDetectionPoint"));
      LightDetectionPoint->SetupAttachment(RootComponent);

      DetectionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DetectionMesh"));
      DetectionMesh->SetupAttachment(LightDetectionPoint);
  }

  void APawPlayerHider::BeginPlay()
  {
      Super::BeginPlay();

      InitializeComponents();
      InitializeMaterials();
      UpdateMovementSpeed();
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
      DOREPLIFETIME(APawPlayerHider, bIsRunning);
      DOREPLIFETIME(APawPlayerHider, bIsCrouching);
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
          HpChanged.Broadcast(GetHealthPercentage());

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
          HpChanged.Broadcast(GetHealthPercentage());
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
          HpChanged.Broadcast(GetHealthPercentage());
      }
  }

  // Light Detection
  void APawPlayerHider::CheckLightExposure()
  {
      // Implementation for light detection logic
      // This would involve checking nearby light sources and determining visibility
      // For now, this is a stub that can be implemented with specific light detection logic
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

  // Stealth System
  void APawPlayerHider::ActivateInvisibility()
  {
      if (HasAuthority() && !bIsInLight && !bIsInvisible)
      {
          bIsInvisible = true;
          GetWorldTimerManager().SetTimer(InvisibilityTimerHandle, this, &APawPlayerHider::OnInvisibilityTimeout, InvisibilityDuration, false);
          UpdateStealthVisuals();
          OnInvisibilityChanged(true);
      }
  }

  void APawPlayerHider::DeactivateInvisibility()
  {
      if (HasAuthority() && bIsInvisible)
      {
          bIsInvisible = false;
          GetWorldTimerManager().ClearTimer(InvisibilityTimerHandle);
          UpdateStealthVisuals();
          OnInvisibilityChanged(false);
      }
  }

  void APawPlayerHider::UpdateStealthVisuals()
  {
      if (DynamicMaterial)
      {
          float OpacityValue = bIsInvisible ? StealthOpacity : 1.0f;
          DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), OpacityValue);
      }
  }

  // Movement System
  void APawPlayerHider::StartRunning()
  {
      if (!bIsRunning)
      {
          bIsRunning = true;
          UpdateMovementSpeed();
          GenerateNoise(1.0f);
      }
  }

  void APawPlayerHider::StopRunning()
  {
      if (bIsRunning)
      {
          bIsRunning = false;
          UpdateMovementSpeed();
          ResetNoise();
      }
  }

  void APawPlayerHider::ToggleCrouch()
  {
      bIsCrouching = !bIsCrouching;
      UpdateMovementSpeed();

      if (bIsCrouching)
      {
          ResetNoise();
      }
  }

  void APawPlayerHider::UpdateMovementSpeed()
  {
      if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
      {
          if (bIsCrouching)
          {
              MovementComp->MaxWalkSpeed = CrouchSpeed;
          }
          else if (bIsRunning)
          {
              MovementComp->MaxWalkSpeed = RunSpeed;
          }
          else
          {
              MovementComp->MaxWalkSpeed = WalkSpeed;
          }
      }
  }

  // Sound System
  void APawPlayerHider::GenerateNoise(float Intensity)
  {
      NoiseLevel = Intensity;
      GetWorldTimerManager().SetTimer(NoiseTimerHandle, this, &APawPlayerHider::OnNoiseTimeout, 1.0f, false);
  }

  void APawPlayerHider::ResetNoise()
  {
      NoiseLevel = 0.0f;
      GetWorldTimerManager().ClearTimer(NoiseTimerHandle);
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
      HpChanged.Broadcast(GetHealthPercentage());
  }

  // Private Functions
  void APawPlayerHider::OnInvisibilityTimeout()
  {
      DeactivateInvisibility();
  }

  void APawPlayerHider::OnNoiseTimeout()
  {
      ResetNoise();
  }

  void APawPlayerHider::InitializeComponents()
  {
      // Initialize component positions and settings
      if (LightDetectionPoint)
      {
          LightDetectionPoint->SetRelativeLocation(FVector(0, 0, 100));
      }
  }

  void APawPlayerHider::InitializeMaterials()
  {
      if (BaseMaterial && GetMesh())
      {
          DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
          GetMesh()->SetMaterial(0, DynamicMaterial);
      }
  }
