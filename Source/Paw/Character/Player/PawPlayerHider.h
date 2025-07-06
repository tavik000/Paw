#pragma once

#include "CoreMinimal.h"
#include "PawTPPlayer.h"
#include "Engine/TimerHandle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"
#include "PawPlayerHider.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathDelegate);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHpChangedDelegate, float, HpPercentage);

UCLASS()
class PAW_API APawPlayerHider : public APawTPPlayer
{
	GENERATED_BODY()

public:
	APawPlayerHider();

protected:
	// Core Overrides
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
	float LitDamageInterval;

	UPROPERTY(BlueprintReadWrite, Category = "Health")
	FTimerHandle LitDamageTimerHandle;

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

	UPROPERTY(BlueprintReadWrite, Category = "Stealth")
	TArray<TObjectPtr<UMaterialInterface>> CachedBaseMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
	TObjectPtr<UMaterialInterface> InvisibleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
	FName OpacityParameterName;

	// === Capture System ===
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Capture")
	bool IsCaptured;

	// === Multiplayer Properties ===
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	bool bIsLocalPlayer;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Multiplayer")
	int32 PlayerIndex;

	// === UI System ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TObjectPtr<UUserWidget> HUD;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;


public:
	// === Health System ===
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

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthChanged(float NewHealth, float NewMaxHealth);

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeathDelegate OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHpChangedDelegate OnHpChanged;

	// === Light Detection System ===
	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void CheckLightExposure();

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void SetInLight(bool bInLight);

	UFUNCTION(BlueprintImplementableEvent, Category = "Light Detection")
	void OnLightExposureChanged(bool bInLight);

	// === Stealth System ===
	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void ActivateInvisibility();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void DeactivateInvisibility();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	void UpdateStealthVisuals();

	UFUNCTION(BlueprintCallable, Category = "Stealth")
	float GetOpacityForViewingTeam() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stealth")
	void OnInvisibilityChanged(bool bInvisible);

	// === UI System ===
	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* GetHUDSafe() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool HasValidHUD() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool IsEventDispatcherReady() const;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void TriggerHpChangedManually();

	// === RPC Functions ===
	UFUNCTION(Server, Reliable, Category = "Multiplayer")
	void ServerSetTeamId(ETeamId NewTeamId);

	UFUNCTION(Server, Reliable, Category = "Health")
	void ServerTakeHealthDamage(float DamageAmount);

	UFUNCTION(Server, Reliable, Category = "Capture")
	void ServerSetCaptured(bool NewIsCaptured);

	UFUNCTION(NetMulticast, Reliable, Category = "Stealth")
	void MulticastUpdateStealthVisuals();

	UFUNCTION(NetMulticast, Reliable, Category = "Health")
	void MulticastOnDeath();

	UFUNCTION(Client, Reliable, Category = "Health")
	void ClientUpdateHealth(float NewHealth);

	UFUNCTION(Client, Reliable, Category = "UI")
	void Client_CreateHUD();

	// === RepNotify Functions ===
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_IsInvisible();

private:
	// === Internal System Functions ===
	void InitializeMaterials();
	void StartLitDamage();
	void StopLitDamage();
	void OnLitDamageTimeout();
};
