// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/World.h"
#include "PawLightDetectionSubsystem.generated.h"

class UPointLightComponent;
class USpotLightComponent;
class AActor;

UENUM()
enum class ESunLightState : uint8
{
	NotSearched, // Haven't looked yet
	Found, // Found and cached
	NotExists // Searched but doesn't exist in this level
};

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
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void RegisterBubbleLight(AActor* LightActor, UPointLightComponent* LightComponent);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void UnregisterBubbleLight(AActor* LightActor);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void RegisterSeeker(AActor* SeekerActor, USpotLightComponent* SpotLightComponent);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	void UnregisterSeeker(AActor* SeekerActor);

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	FLightExposureResult GetLightExposureState(const FVector& Location, AActor* IgnoreActor = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	AActor* GetSunLightActor() const { return CachedSunLightActor; }

	UFUNCTION(BlueprintCallable, Category = "Light Detection")
	int32 GetRegisteredBubbleLightCount() const { return RegisteredBubbleLights.Num(); }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Light Detection")
	mutable TObjectPtr<AActor> CachedSunLightActor;

	UPROPERTY()
	TMap<TObjectPtr<AActor>, TObjectPtr<UPointLightComponent>> RegisteredBubbleLights;

	UPROPERTY()
	TMap<TObjectPtr<AActor>, TObjectPtr<USpotLightComponent>> RegisteredSeekers;

private:
	void FindAndCacheSunLight() const;
	bool CheckBubbleLightExposure(const FVector& Location, AActor* IgnoreActor) const;
	bool CheckDirectionalLightExposure(const FVector& Location, AActor* IgnoreActor) const;
	void StartSpotlightDetectionTimer();
	void StopSpotlightDetectionTimer();
	void OnSpotlightDetectionTick();
	bool IsHiderInSpotlightCone(const FVector& HiderLocation, AActor* SeekerActor, USpotLightComponent* SpotLight) const;

private:
	mutable ESunLightState SunLightState = ESunLightState::NotSearched;
	FTimerHandle SpotlightDetectionTimerHandle;
};
