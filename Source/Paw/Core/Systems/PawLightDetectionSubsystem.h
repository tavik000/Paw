// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "PawLightDetectionSubsystem.generated.h"

class UPointLightComponent;
class AActor;

USTRUCT(BlueprintType)
struct PAW_API FLightExposureResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Light Detection")
	bool bIsInLight = false;

	UPROPERTY(BlueprintReadOnly, Category = "Light Detection") 
	bool bIsSpotLighted = false;

	FLightExposureResult()
	{
		bIsInLight = false;
		bIsSpotLighted = false;
	}

	FLightExposureResult(bool InLight, bool SpotLighted)
		: bIsInLight(InLight), bIsSpotLighted(SpotLighted)
	{
	}
};

UCLASS()
class PAW_API UPawLightDetectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Subsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Bubble Light Management
	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void RegisterBubbleLight(AActor* LightActor, UPointLightComponent* LightComponent);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void UnregisterBubbleLight(AActor* LightActor);

	// Light Exposure Query
	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	FLightExposureResult GetLightExposureState(const FVector& Location, AActor* IgnoreActor = nullptr) const;

	// SunLight Management
	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	AActor* GetSunLightActor() const { return CachedSunLightActor; }

	// Utility Functions
	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	int32 GetRegisteredBubbleLightCount() const { return RegisteredBubbleLights.Num(); }

protected:
	// Cached SunLight Actor (Directional Light with "SunLight" tag)
	UPROPERTY(BlueprintReadOnly, Category = "Light Detection")
	TObjectPtr<AActor> CachedSunLightActor;

	// Registered Bubble Lights (Actor -> PointLightComponent mapping)
	UPROPERTY()
	TMap<TObjectPtr<AActor>, TObjectPtr<UPointLightComponent>> RegisteredBubbleLights;

private:
	// Internal Functions
	void FindAndCacheSunLight();
	bool CheckBubbleLightExposure(const FVector& Location, AActor* IgnoreActor) const;
	bool CheckDirectionalLightExposure(const FVector& Location, AActor* IgnoreActor) const;
};