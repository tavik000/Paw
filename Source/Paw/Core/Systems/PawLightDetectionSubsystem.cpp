// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#include "PawLightDetectionSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"

void UPawLightDetectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// SunLight will be found lazily when first needed (handles timing issues)
	SunLightState = ESunLightState::NotSearched;

	UE_LOG(LogTemp, Log, TEXT("PawLightDetectionSubsystem initialized"));
}

void UPawLightDetectionSubsystem::Deinitialize()
{
	// Clear all registered lights
	RegisteredBubbleLights.Empty();
	CachedSunLightActor = nullptr;
	SunLightState = ESunLightState::NotSearched;

	UE_LOG(LogTemp, Log, TEXT("PawLightDetectionSubsystem deinitialized"));

	Super::Deinitialize();
}

void UPawLightDetectionSubsystem::RegisterBubbleLight(AActor* LightActor, UPointLightComponent* LightComponent)
{
	if (IsValid(LightActor) && IsValid(LightComponent))
	{
		RegisteredBubbleLights.Add(LightActor, LightComponent);
		UE_LOG(LogTemp, Log, TEXT("Registered bubble light: %s"), *LightActor->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to register bubble light - invalid actor or component"));
	}
}

void UPawLightDetectionSubsystem::UnregisterBubbleLight(AActor* LightActor)
{
	if (IsValid(LightActor))
	{
		if (RegisteredBubbleLights.Remove(LightActor) > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Unregistered bubble light: %s"), *LightActor->GetName());
		}
	}
}

FLightExposureResult UPawLightDetectionSubsystem::GetLightExposureState(
	const FVector& Location, AActor* IgnoreActor) const
{
	bool bBubbleLightLit = CheckBubbleLightExposure(Location, IgnoreActor);
	bool bDirectionalLightLit = CheckDirectionalLightExposure(Location, IgnoreActor);

	// Combine bubble lights and directional light for bIsInLight
	bool bIsInLight = bBubbleLightLit || bDirectionalLightLit;

	// bIsSpotLighted is controlled by Seeker players, not environment lights
	bool bIsSpotLighted = false;

	return FLightExposureResult(bIsInLight, bIsSpotLighted);
}

void UPawLightDetectionSubsystem::FindAndCacheSunLight() const
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClassWithTag(World, ADirectionalLight::StaticClass(), FName("SunLight"),
		                                             FoundActors);

		if (FoundActors.Num() > 0)
		{
			CachedSunLightActor = FoundActors[0];
			SunLightState = ESunLightState::Found;
			UE_LOG(LogTemp, Log, TEXT("LightDetectionSubsystem: Found SunLight actor: %s"),
			       *CachedSunLightActor->GetName());
		}
		else
		{
			SunLightState = ESunLightState::NotExists;
			UE_LOG(LogTemp, Log,
			       TEXT("LightDetectionSubsystem: No DirectionalLight with 'SunLight' tag found in level"));
		}
	}
}

bool UPawLightDetectionSubsystem::CheckBubbleLightExposure(const FVector& Location, AActor* IgnoreActor) const
{
	// Check each registered bubble light
	for (const auto& LightPair : RegisteredBubbleLights)
	{
		AActor* LightActor = LightPair.Key;
		UPointLightComponent* LightComponent = LightPair.Value;

		if (!IsValid(LightActor) || !IsValid(LightComponent))
		{
			continue;
		}

		// Calculate distance to light
		float Distance = FVector::Dist(Location, LightActor->GetActorLocation());
		float AttenuationRadius = LightComponent->AttenuationRadius;

		// Check if within light radius
		if (Distance <= AttenuationRadius)
		{
			// Perform line trace to check for occlusion
			if (UWorld* World = GetWorld(); IsValid(World))
			{
				FVector Start = Location;
				FVector End = LightActor->GetActorLocation();

				FHitResult HitResult;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(IgnoreActor);
				QueryParams.AddIgnoredActor(LightActor); // Ignore the light itself

				// Line trace to check if path to light is clear
				bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

				// If no obstruction, we're lit by this bubble light
				if (!bHit)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool UPawLightDetectionSubsystem::CheckDirectionalLightExposure(const FVector& Location, AActor* IgnoreActor) const
{
	// Lazy load: search for SunLight if not attempted yet
	if (SunLightState == ESunLightState::NotSearched)
	{
		FindAndCacheSunLight();
	}

	// Only check directional light if one exists in this level
	if (SunLightState == ESunLightState::Found && IsValid(CachedSunLightActor))
	{
		if (UWorld* World = GetWorld(); IsValid(World))
		{
			FVector Start = Location;
			// Get the forward vector from SunLight actor (light direction)
			FVector LightDirection = CachedSunLightActor->GetActorForwardVector();
			// Trace in the opposite direction of light (towards light source)
			FVector End = Start - (LightDirection * 1000.0f);

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(IgnoreActor);

			// Line trace to check if we're in shadow from directional light
			bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

			// If no hit (clear path to light), we're lit by directional light
			return !bHit;
		}
	}

	// No directional light exposure if not found or doesn't exist
	return false;
}
