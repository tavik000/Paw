// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "PawLightDetectionSubsystem.generated.h"

class APawBubbleLight;
class APawPlayerHider;
class UPointLightComponent;
class USpotLightComponent;
class AActor;
class APawPlayerSeeker;

UENUM()
enum class ESunLightState : uint8
{
	NotSearched, // Haven't looked yet
	Found, // Found and cached
	NotExists // Searched but doesn't exist in this level
};

UCLASS()
class PAW_API UPawLightDetectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void RegisterBubbleLight(APawBubbleLight* Light, UPointLightComponent* LightComponent);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void UnregisterBubbleLight(APawBubbleLight* Light);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void RegisterSeeker(APawPlayerSeeker* Seeker, USpotLightComponent* SpotLightComponent);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void UnregisterSeeker(APawPlayerSeeker* Seeker);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void RegisterHider(APawPlayerHider* Hider);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void UnregisterHider(APawPlayerHider* Hider);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	AActor* GetSunLightActor() const { return CachedSunLightActor; }

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	int32 GetRegisteredSeekerCount() const { return RegisteredSeekers.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	int32 GetRegisteredHiderCount() const { return RegisteredHiders.Num(); }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Light Detection")
	mutable TObjectPtr<AActor> CachedSunLightActor;

	UPROPERTY()
	TMap<TObjectPtr<APawBubbleLight>, TObjectPtr<UPointLightComponent>> RegisteredBubbleLights;

	UPROPERTY()
	TMap<TObjectPtr<APawPlayerSeeker>, TObjectPtr<USpotLightComponent>> RegisteredSeekers;

	UPROPERTY()
	TSet<TObjectPtr<APawPlayerHider>> RegisteredHiders;


	// === Light Detection Configuration ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light Detection", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float LightDetectionTickRate = 0.1f;

	// === Spotlight Detection Configuration ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spotlight Detection")
	float SpotlightDetectionFactor = 0.6785f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spotlight Detection")
	float SpotlightConeAngleMultiplier = 1.1f;

private:
	void FindAndCacheSunLight() const;
	bool CheckBubbleLightExposure(const FVector& Location, AActor* IgnoreActor) const;
	bool CheckDirectionalLightExposure(const FVector& Location, AActor* IgnoreActor) const;
	void StartUnifiedLightDetectionTimer();
	void StopUnifiedLightDetectionTimer();
	void OnUnifiedLightDetectionTick();
	bool CheckDirectionalLightsForHider(APawPlayerHider* Hider);
	bool CheckBubbleLightsForHider(APawPlayerHider* Hider);
	bool CheckSpotlightsForHider(APawPlayerHider* Hider);
	bool IsHiderCapsuleInSpotlightCone(APawPlayerHider* Hider, AActor* SeekerActor, USpotLightComponent* SpotLight);
	bool IsPointInSpotlightCone(const FVector& Point, AActor* SeekerActor, USpotLightComponent* SpotLight);
	bool IsObstructedForHiderDetection(const FVector& Start, const FVector& End, AActor* SeekerActor) const;

private:
	mutable ESunLightState SunLightState = ESunLightState::NotSearched;
	FTimerHandle UnifiedLightDetectionTimerHandle;
	
	// === Performance Optimization Members ===
	mutable TArray<FVector> CachedTestPoints;
	float CachedCosConeAngle = 0.0f;
};
