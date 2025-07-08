#pragma once

#include "CoreMinimal.h"
#include "PawTPPlayer.h"
#include "Engine/TimerHandle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Blueprint/UserWidget.h"
#include "Engine/StreamableManager.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "PawPlayerHider.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHpChangedDelegate, float, HpPercentage);

UCLASS()
class PAW_API APawPlayerHider : public APawTPPlayer
{
	GENERATED_BODY()

public: // Constructor & Core Engine Overrides
	APawPlayerHider();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void Jump() override;

public: // Blueprint Callable API
	// Health
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeHealthDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float HealAmount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Respawn();

	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsAlive() const { return !bIsDead && Health > 0; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealthPercentage() const { return MaxHealth > 0 ? Health / MaxHealth : 0.0f; }

	// Light Detection
	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void CheckLightExposure();

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void SetInLight(bool bInLight);

	// Stealth
	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void ActivateInvisibility();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void DeactivateInvisibility();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void UpdateStealthVisuals();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	float GetOpacityForViewerTeam() const;

	// UI
	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* GetHUDSafe() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool HasValidHUD() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool IsEventDispatcherReady() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void TriggerHpChangedManually();

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	bool IsInLight() const { return bIsInLight; }

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	bool IsInvisible() const { return bIsInvisible; }

	UFUNCTION(BlueprintCallable, Category = "Capture")
	bool IsCaptured() const { return bIsCaptured; }

	UFUNCTION(BlueprintCallable, Category = "Multiplayer")
	bool IsLocalPlayer() const { return bIsLocalPlayer; }

public: // C++ Public Helper
	float GetDefaultGravityScale() const { return DefaultGravityScale; }

public: // Blueprint Events & Delegates
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathDelegate OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHpChangedDelegate OnHpChanged;

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthChanged(float NewHealth, float NewMaxHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Light Detection")
	void OnLightExposureChanged(bool bInLight);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stealth")
	void OnInvisibilityChanged(bool bInvisible);

protected: // Engine Overrides
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanMove() override;
	virtual bool CanJump() override;

protected: // Properties (State & Configuration)
	// === Health System ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_Health, Category = "Health")
	float Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Health")
	bool bIsDead;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float LitDamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float ShadowHealAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float CaptureDamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HealthEffectInterval;

	// === Light Detection System ===
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Light Detection")
	bool bIsInLight;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Light Detection")
	bool bIsSpotLighted;

	// === Stealth System ===
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_IsInvisible, Category = "Stealth")
	bool bIsInvisible;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
	float StealthOpacity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
	TObjectPtr<UMaterialInterface> InvisibleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
	FName OpacityParameterName;

	// === Capture System ===
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Capture")
	bool bIsCaptured;

	// === Multiplayer Properties ===
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	bool bIsLocalPlayer;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	int32 PlayerIndex;

	// === UI System ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	// === Jump System ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float HangTimeGravityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float JumpVFXVelocityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float LandVFXVelocityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float LandingVolumeDivisor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float HangTimeVelocityZMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float HangTimeVelocityZMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float AirControlInputTolerance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float JumpSFXVolumeMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump System")
	float JumpSFXVolumeMax;

	UPROPERTY(EditAnywhere, Category = "Jump System")
	TSoftObjectPtr<USoundBase> JumpSoundAsset;

	UPROPERTY(EditAnywhere, Category = "Jump System")
	TSoftObjectPtr<USoundBase> LandSoundAsset;

	UPROPERTY(EditAnywhere, Category = "Jump System")
	TSoftObjectPtr<UNiagaraSystem> JumpVFXAsset;

	UPROPERTY(EditAnywhere, Category = "Jump System")
	TSoftObjectPtr<UNiagaraSystem> LandVFXAsset;

	UPROPERTY(EditAnywhere, Category = "Jump System")
	TSoftObjectPtr<UForceFeedbackEffect> LandForceFeedbackAsset;

protected: // Networking (RPCs & RepNotifies)
	// RPCs
	UFUNCTION(Server, Reliable, Category = "Multiplayer")
	void ServerSetTeamId(ETeamId NewTeamId);

	UFUNCTION(Server, Reliable, Category = "Health")
	void ServerTakeHealthDamage(float DamageAmount);

	UFUNCTION(Server, Reliable, Category = "Capture")
	void ServerSetCaptured(bool NewIsCaptured);

	UFUNCTION(Server, Reliable, Category = "Movement")
	void ServerRequestCancelHorizontalVelocity();

	UFUNCTION(NetMulticast, Reliable, Category = "Stealth")
	void MulticastUpdateStealthVisuals();

	UFUNCTION(NetMulticast, Reliable, Category = "Health")
	void MulticastOnDeath();

	UFUNCTION(Client, Reliable, Category = "Health")
	void ClientUpdateHealth(float NewHealth);

	UFUNCTION(Client, Reliable, Category = "UI")
	void Client_CreateHUD();

	UFUNCTION(NetMulticast, Reliable, Category = "Jump System")
	void MulticastPlayJumpEffects();

	UFUNCTION(NetMulticast, Reliable, Category = "Jump System")
	void MulticastPlayLandEffects();

	// RepNotifies
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_IsInvisible();

private: // Internal Helper Functions
	// === Internal System Functions ===
	void InitializeMaterials();
	void StartHealthEffectTimer();
	void StopHealthEffects();
	void OnHealthEffectTick();

	// === Stealth Helper Functions ===
	bool IsViewerOnSeekerTeam() const;
	void ApplyInvisibilityMaterials();
	void RestoreOriginalMaterials();

	// === UI Helper Functions ===
	void SetupHUDWidget(APlayerController* PC);
	void ConfigureCrosshair();
	void ConfigureHealthBar();

	// === Jump System Helper Functions ===
	void LoadJumpAssetsAsync();
	void OnJumpAssetsLoaded();
	void SpawnJumpVFX();
	void SpawnLandVFX();
	void PlayJumpSound();
	void PlayLandSound(float Volume);
	void PlayLandForceFeedback();

private: // Internal State & Cached Data
	// === Health System ===
	FTimerHandle HealthEffectTimerHandle;

	// === Stealth System ===
	TArray<TObjectPtr<UMaterialInterface>> CachedBaseMaterials;

	// === UI System ===
	TObjectPtr<UUserWidget> HUD;

	// === Jump System ===
	// Cached loaded assets
	TObjectPtr<USoundBase> JumpSound;
	TObjectPtr<USoundBase> LandSound;
	TObjectPtr<UNiagaraSystem> JumpVFX;
	TObjectPtr<UNiagaraSystem> LandVFX;
	TObjectPtr<UForceFeedbackEffect> LandForceFeedback;
	TSharedPtr<FStreamableHandle> JumpAssetsHandle;
	float DefaultGravityScale;
};
