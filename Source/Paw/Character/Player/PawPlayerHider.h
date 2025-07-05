#pragma once

#include "CoreMinimal.h"
#include "PawTPPlayer.h"
#include "../../Core/Interfaces/ITeamableInterface.h"
#include "../../Core/Enums/ETeamId.h"
#include "Engine/TimerHandle.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "PawPlayerHider.generated.h"

UCLASS()
class PAW_API APawPlayerHider : public APawTPPlayer, public ITeamableInterface
{
	GENERATED_BODY()

public:
	APawPlayerHider();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Team System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Team")
	ETeamId TeamId;

	// Health System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Health")
	float Health;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Health")
	bool bIsDead;

	// Light Detection System
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Light Detection")
	bool bIsInLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Detection")
	float LightDetectionRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Detection")
	float LightIntensityThreshold;

	// Stealth System
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Stealth")
	bool bIsInvisible;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
	float InvisibilityDuration;

	UPROPERTY(BlueprintReadWrite, Category = "Stealth")
	FTimerHandle InvisibilityTimerHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
	float StealthOpacity;

	// Movement System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RunSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float CrouchSpeed;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Movement")
	bool bIsRunning;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Movement")
	bool bIsCrouching;

	// Sound System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float NoiseLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	float NoiseRadius;

	UPROPERTY(BlueprintReadWrite, Category = "Sound")
	FTimerHandle NoiseTimerHandle;

	// Effects and Materials
	UPROPERTY(BlueprintReadWrite, Category = "Effects")
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	// Multiplayer
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	bool bIsLocalPlayer;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	int32 PlayerIndex;

	// Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LightDetectionPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DetectionMesh;

public:
	// Team Interface Implementation
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Team")
	ETeamId GetTeamId() const;
	virtual ETeamId GetTeamId_Implementation() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Team")
	void SetTeamId(ETeamId NewTeamId);
	virtual void SetTeamId_Implementation(ETeamId NewTeamId);

	// Health System Functions
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeHealthDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float HealAmount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Respawn();

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthChanged(float NewHealth, float NewMaxHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDeath();

	// Light Detection Functions
	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void CheckLightExposure();

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void SetInLight(bool bInLight);

	UFUNCTION(BlueprintImplementableEvent, Category = "Light Detection")
	void OnLightExposureChanged(bool bInLight);

	// Stealth Functions
	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void ActivateInvisibility();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void DeactivateInvisibility();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void UpdateStealthVisuals();

	UFUNCTION(BlueprintImplementableEvent, Category = "Stealth")
	void OnInvisibilityChanged(bool bInvisible);

	// Movement Functions
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void StartRunning();

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void StopRunning();

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void ToggleCrouch();

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void UpdateMovementSpeed();

	// Sound Functions
	UFUNCTION(BlueprintCallable, Category = "Sound")
	void GenerateNoise(float Intensity);

	UFUNCTION(BlueprintCallable, Category = "Sound")
	void ResetNoise();

	// RPC Functions
	UFUNCTION(Server, Reliable, Category = "Multiplayer")
	void ServerSetTeamId(ETeamId NewTeamId);

	UFUNCTION(Server, Reliable, Category = "Multiplayer")
	void ServerTakeHealthDamage(float DamageAmount);

	UFUNCTION(NetMulticast, Reliable, Category = "Multiplayer")
	void MulticastOnDeath();

	UFUNCTION(Client, Reliable, Category = "Multiplayer")
	void ClientUpdateHealth(float NewHealth);

	// Utility Functions
	UFUNCTION(BlueprintCallable, Category = "Utility")
	bool IsAlive() const { return !bIsDead && Health > 0; }

	UFUNCTION(BlueprintCallable, Category = "Utility")
	float GetHealthPercentage() const { return MaxHealth > 0 ? Health / MaxHealth : 0.0f; }

private:
	// Timer Functions
	void OnInvisibilityTimeout();
	void OnNoiseTimeout();

	// Internal Functions
	void InitializeComponents();
	void InitializeMaterials();
};
