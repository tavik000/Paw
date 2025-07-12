// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#include "PawLightDetectionSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Paw/Character/Player/PawPlayerHider.h"
#include "TimerManager.h"

void UPawLightDetectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// SunLight will be found lazily when first needed (handles timing issues)
	SunLightState = ESunLightState::NotSearched;

	UE_LOG(LogTemp, Log, TEXT("PawLightDetectionSubsystem initialized"));
}

void UPawLightDetectionSubsystem::Deinitialize()
{
	// Stop spotlight detection timer
	StopSpotlightDetectionTimer();

	// Clear all registered lights and seekers
	RegisteredBubbleLights.Empty();
	RegisteredSeekers.Empty();
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

void UPawLightDetectionSubsystem::RegisterSeeker(AActor* SeekerActor, USpotLightComponent* SpotLightComponent)
{
	if (IsValid(SeekerActor) && IsValid(SpotLightComponent))
	{
		RegisteredSeekers.Add(SeekerActor, SpotLightComponent);
		UE_LOG(LogTemp, Log, TEXT("Registered seeker: %s"), *SeekerActor->GetName());

		// Start spotlight detection timer if this is the first seeker
		if (RegisteredSeekers.Num() == 1)
		{
			StartSpotlightDetectionTimer();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to register seeker - invalid actor or spotlight component"));
	}
}

void UPawLightDetectionSubsystem::UnregisterSeeker(AActor* SeekerActor)
{
	if (IsValid(SeekerActor))
	{
		if (RegisteredSeekers.Remove(SeekerActor) > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Unregistered seeker: %s"), *SeekerActor->GetName());

			// Stop spotlight detection timer if no more seekers
			if (RegisteredSeekers.Num() == 0)
			{
				StopSpotlightDetectionTimer();
			}
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

void UPawLightDetectionSubsystem::StartSpotlightDetectionTimer()
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		// 0.1s tick rate same as hider light checking
		World->GetTimerManager().SetTimer(SpotlightDetectionTimerHandle, this, 
			&UPawLightDetectionSubsystem::OnSpotlightDetectionTick, 0.1f, true);
		UE_LOG(LogTemp, Log, TEXT("Started spotlight detection timer"));
	}
}

void UPawLightDetectionSubsystem::StopSpotlightDetectionTimer()
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(SpotlightDetectionTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("Stopped spotlight detection timer"));
	}
}

void UPawLightDetectionSubsystem::OnSpotlightDetectionTick()
{
	if (UWorld* World = GetWorld(); !IsValid(World))
	{
		return;
	}

	// Get all hider players in the world
	TArray<AActor*> AllHiders;
	UGameplayStatics::GetAllActorsOfClass(this, APawPlayerHider::StaticClass(), AllHiders);

	// For each hider, check if they're in any seeker's spotlight
	for (AActor* HiderActor : AllHiders)
	{
		APawPlayerHider* Hider = Cast<APawPlayerHider>(HiderActor);
		if (!IsValid(Hider))
		{
			continue;
		}

		bool bWasSpotLighted = Hider->IsSpotLighted();
		bool bIsSpotLighted = false;

		// Check against all registered seekers
		for (const auto& SeekerPair : RegisteredSeekers)
		{
			AActor* SeekerActor = SeekerPair.Key;
			USpotLightComponent* SpotLight = SeekerPair.Value;

			if (!IsValid(SeekerActor) || !IsValid(SpotLight))
			{
				continue;
			}

			// Check if hider is in this seeker's spotlight cone
			if (IsHiderInSpotlightCone(Hider->GetActorLocation(), SeekerActor, SpotLight))
			{
				bIsSpotLighted = true;
				break; // Found at least one spotlight, no need to check others
			}
		}

		// Update hider's spotlight state if changed
		if (bIsSpotLighted != bWasSpotLighted)
		{
			Hider->SetSpotLighted(bIsSpotLighted);
		}
	}
}

bool UPawLightDetectionSubsystem::IsHiderInSpotlightCone(const FVector& HiderLocation, AActor* SeekerActor, USpotLightComponent* SpotLight) const
{
	if (!IsValid(SeekerActor) || !IsValid(SpotLight))
	{
		return false;
	}

	// Get spotlight properties
	FVector SpotLightLocation = SpotLight->GetComponentLocation();
	FVector SpotLightDirection = SpotLight->GetForwardVector();
	float AttenuationRadius = SpotLight->AttenuationRadius;
	float InnerConeAngle = SpotLight->InnerConeAngle;
	float OuterConeAngle = SpotLight->OuterConeAngle;

	// Calculate distance to hider
	FVector ToHider = HiderLocation - SpotLightLocation;
	float Distance = ToHider.Size();

	// Check if within attenuation radius
	if (Distance > AttenuationRadius)
	{
		return false;
	}

	// Check if within cone angle (use outer cone angle for detection)
	ToHider.Normalize();
	float DotProduct = FVector::DotProduct(SpotLightDirection, ToHider);
	float AngleToHider = FMath::RadiansToDegrees(FMath::Acos(DotProduct));

	if (AngleToHider > OuterConeAngle)
	{
		return false;
	}

	// Perform line trace to check for obstruction
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		FVector Start = SpotLightLocation;
		FVector End = HiderLocation;

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(SeekerActor); // Ignore the seeker
		
		// Line trace to check if path to hider is clear
		bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

		// If we hit something, check if it's the hider or something else
		if (bHit)
		{
			// If we hit the hider, they're visible
			if (HitResult.GetActor() == nullptr || HitResult.GetActor()->GetRootComponent()->GetOwner() != SeekerActor)
			{
				// Hit something else (obstruction), hider is not visible
				return false;
			}
		}
	}

	return true;
}
