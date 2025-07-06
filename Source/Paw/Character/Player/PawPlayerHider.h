#pragma once

#include "CoreMinimal.h"
#include "PawTPPlayer.h"
#include "../../Core/Interfaces/ITeamableInterface.h"
#include "../../Core/Enums/ETeamId.h"
#include "Engine/TimerHandle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"
#include "PawPlayerHider.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHpChangedDelegate, float, HpPercentage);

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

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Light Detection")
	bool bIsSpotLighted;

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

	// Lit Damage System
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Detection")
	float LitDamageAmount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Detection")
	float LitDamageInterval;

	UPROPERTY(BlueprintReadWrite, Category = "Light Detection")
	FTimerHandle LitDamageTimerHandle;

	// Effects and Materials
	UPROPERTY(BlueprintReadWrite, Category = "Effects")
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	// Capture System
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Capture")
	bool IsCaptured;

	// Multiplayer
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	bool bIsLocalPlayer;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	int32 PlayerIndex;

	// UI System (Client-side only, not replicated)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TObjectPtr<UUserWidget> HUD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;


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

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathDelegate OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHpChangedDelegate OnHpChanged;

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
	void UpdateMovementSpeed();

	// RPC Functions
	UFUNCTION(Server, Reliable, Category = "Multiplayer")
	void ServerSetTeamId(ETeamId NewTeamId);

	UFUNCTION(Server, Reliable, Category = "Multiplayer")
	void ServerTakeHealthDamage(float DamageAmount);

	UFUNCTION(Server, Reliable, Category = "Capture")
	void ServerSetCaptured(bool NewIsCaptured);

	UFUNCTION(NetMulticast, Reliable, Category = "Multiplayer")
	void MulticastOnDeath();

	UFUNCTION(Client, Reliable, Category = "Multiplayer")
	void ClientUpdateHealth(float NewHealth);

	UFUNCTION(Client, Reliable, Category = "UI")
	void Client_CreateHUD();

	// Utility Functions
	UFUNCTION(BlueprintCallable, Category = "Utility")
	bool IsAlive() const { return !bIsDead && Health > 0; }

	UFUNCTION(BlueprintCallable, Category = "Utility")
	float GetHealthPercentage() const { return MaxHealth > 0 ? Health / MaxHealth : 0.0f; }

	// HUD Access Functions (Client-side only)
	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* GetHUDSafe() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool HasValidHUD() const;

	// Event Dispatcher Helper Functions
	UFUNCTION(BlueprintCallable, Category = "Events")
	bool IsEventDispatcherReady() const;

	UFUNCTION(BlueprintCallable, Category = "Events")
	void TriggerHpChangedManually();

private:
	// Timer Functions
	void OnInvisibilityTimeout();
	void OnLitDamageTimeout();

	// Internal Functions
	void InitializeMaterials();
	void StartLitDamage();
	void StopLitDamage();
};
