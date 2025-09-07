// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#include "PawLightDetectionSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/CapsuleComponent.h"
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

	// Pre-allocate memory for performance optimization
	CachedTestPoints.Reserve(5); // 5 test points for capsule detection
	CachedHiders.Reserve(8); // Expect up to 8 hiders in typical gameplay

	StartUnifiedLightDetectionTimer();

	UE_LOG(LogTemp, Log, TEXT("PawLightDetectionSubsystem initialized"));
}

void UPawLightDetectionSubsystem::Deinitialize()
{
	// Stop unified light detection timer
	StopUnifiedLightDetectionTimer();

	// Clear all registered lights, seekers, and hiders
	RegisteredBubbleLights.Empty();
	RegisteredSeekers.Empty();
	RegisteredHiders.Empty();
	CachedSunLightActor = nullptr;
	SunLightState = ESunLightState::NotSearched;

	// Clear performance optimization caches
	CachedHiders.Empty();
	CachedTestPoints.Empty();

	UE_LOG(LogTemp, Log, TEXT("PawLightDetectionSubsystem deinitialized"));

	Super::Deinitialize();
}

bool UPawLightDetectionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	UWorld* World = Cast<UWorld>(Outer);
	TEnumAsByte<EWorldType::Type> type = World->WorldType;
	if (type != EWorldType::Editor && type != EWorldType::EditorPreview && type != EWorldType::None)
	{
		return World->GetNetMode() < NM_Client;
	}
	return false;
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

			// Stop unified light detection timer if no more seekers or hiders
			if (RegisteredSeekers.Num() == 0 && RegisteredHiders.Num() == 0)
			{
				StopUnifiedLightDetectionTimer();
			}
		}
	}
}

void UPawLightDetectionSubsystem::RegisterHider(AActor* HiderActor)
{
	if (IsValid(HiderActor))
	{
		RegisteredHiders.Add(HiderActor);
		UE_LOG(LogTemp, Log, TEXT("Registered hider: %s"), *HiderActor->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to register hider - invalid actor"));
	}
}

void UPawLightDetectionSubsystem::UnregisterHider(AActor* HiderActor)
{
	if (IsValid(HiderActor))
	{
		if (RegisteredHiders.Remove(HiderActor) > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Unregistered hider: %s"), *HiderActor->GetName());

			// Stop unified light detection timer if no more hiders
			if (RegisteredHiders.Num() == 0)
			{
				StopUnifiedLightDetectionTimer();
			}
		}
	}
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
	TRACE_CPUPROFILER_EVENT_SCOPE(PawLightDetectionSubsystem_CheckBubbleLightExposure);
	// Early exit if no bubble lights registered
	if (RegisteredBubbleLights.Num() == 0)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	// Check each registered bubble light
	for (const auto& LightPair : RegisteredBubbleLights)
	{
		AActor* LightActor = LightPair.Key;
		UPointLightComponent* LightComponent = LightPair.Value;

		// Lightweight validation
		if (!LightActor || !LightComponent)
		{
			continue;
		}

		// Use squared distance for cheaper comparison
		FVector LightLocation = LightActor->GetActorLocation();
		float DistanceSquared = FVector::DistSquared(Location, LightLocation);
		float AttenuationRadius = LightComponent->AttenuationRadius;
		float AttenuationRadiusSquared = AttenuationRadius * AttenuationRadius;

		// Check if within light radius (squared comparison)
		if (DistanceSquared <= AttenuationRadiusSquared)
		{
			// Create fresh collision params for this trace (avoids API issues)
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(IgnoreActor);
			QueryParams.AddIgnoredActor(LightActor); // Ignore the light itself

			FHitResult HitResult;
			// Line trace to check if path to light is clear
			bool bHit = World->
				LineTraceSingleByChannel(HitResult, Location, LightLocation, ECC_Visibility, QueryParams);

			// If no obstruction, we're lit by this bubble light
			if (!bHit)
			{
				return true;
			}
		}
	}

	return false;
}

bool UPawLightDetectionSubsystem::CheckDirectionalLightExposure(const FVector& Location, AActor* IgnoreActor) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PawLightDetectionSubsystem_CheckDirectionalLightExposure);
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

void UPawLightDetectionSubsystem::StartUnifiedLightDetectionTimer()
{
	if (UnifiedLightDetectionTimerHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Unified light detection timer already running"));
		return;
	}
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		// Use configurable tick rate for light detection
		World->GetTimerManager().SetTimer(UnifiedLightDetectionTimerHandle, this,
		                                  &UPawLightDetectionSubsystem::OnUnifiedLightDetectionTick,
		                                  LightDetectionTickRate, true);
		UE_LOG(LogTemp, Log, TEXT("Started unified light detection timer with rate: %f"), LightDetectionTickRate);
	}
}

void UPawLightDetectionSubsystem::StopUnifiedLightDetectionTimer()
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(UnifiedLightDetectionTimerHandle);
		UE_LOG(LogTemp, Log, TEXT("Stopped unified light detection timer"));
	}
}

void UPawLightDetectionSubsystem::OnUnifiedLightDetectionTick()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PawLightDetectionSubsystem_OnUnifiedLightDetectionTick);
	if (UWorld* World = GetWorld(); !IsValid(World))
	{
		return;
	}

	// Refresh hider cache if needed (much cheaper than GetAllActorsOfClass every tick)
	RefreshHiderCache();

	// For each cached hider, check all light types
	for (int32 i = CachedHiders.Num() - 1; i >= 0; --i)
	{
		APawPlayerHider* Hider = CachedHiders[i].Get();
		if (!Hider)
		{
			// Remove stale weak pointer
			CachedHiders.RemoveAtSwap(i);
			continue;
		}

		// Early exit for dead or captured hiders (no light processing needed)
		if (!Hider->IsAlive() || Hider->IsCaptured())
		{
			Hider->SetInLight(true);
			continue;
		}

		// Check all light types separately for modularity
		bool bDirectionalLit = CheckDirectionalLightsForHider(Hider);
		bool bBubbleLit = CheckBubbleLightsForHider(Hider);
		bool bSpotlightLit = false;

		// Check spotlights separately (only if seekers exist)
		if (RegisteredSeekers.Num() > 0)
		{
			bSpotlightLit = CheckSpotlightsForHider(Hider);
		}

		// Combine directional and bubble light results
		bool bNewIsInLight = bDirectionalLit || bBubbleLit || bSpotlightLit;


		if (bNewIsInLight != Hider->IsInLight())
		{
			Hider->SetInLight(bNewIsInLight);
		}
	}
}

bool UPawLightDetectionSubsystem::CheckDirectionalLightsForHider(APawPlayerHider* Hider)
{
	if (!IsValid(Hider))
	{
		return false;
	}

	// Check directional light exposure only and return result
	return CheckDirectionalLightExposure(Hider->GetActorLocation(), Hider);
}

bool UPawLightDetectionSubsystem::CheckBubbleLightsForHider(APawPlayerHider* Hider)
{
	if (!IsValid(Hider))
	{
		return false;
	}

	// Check bubble light exposure only and return result
	return CheckBubbleLightExposure(Hider->GetActorLocation(), Hider);
}

bool UPawLightDetectionSubsystem::CheckSpotlightsForHider(APawPlayerHider* Hider)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PawLightDetectionSubsystem_CheckSpotlightsForHider);

	if (!IsValid(Hider) || RegisteredSeekers.Num() == 0)
	{
		return false;
	}

	const bool bWasSpotLighted = Hider->IsSpotLighted();
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

		// Check if hider capsule is in this seeker's spotlight cone
		if (IsHiderCapsuleInSpotlightCone(Hider, SeekerActor, SpotLight))
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
	return bIsSpotLighted;
}

bool UPawLightDetectionSubsystem::IsPointInSpotlightCone(const FVector& Point, AActor* SeekerActor,
                                                         USpotLightComponent* SpotLight) const
{
	if (!SeekerActor || !SpotLight)
	{
		return false;
	}

	// Get spotlight properties
	FVector SpotLightLocation = SpotLight->GetComponentLocation();
	FVector SpotLightDirection = SpotLight->GetForwardVector();
	float OuterConeAngle = SpotLight->OuterConeAngle;
	float AttenuationRadius = SpotLight->AttenuationRadius;

	// Calculate squared distance to point (cheaper than Distance)
	FVector ToPoint = Point - SpotLightLocation;
	float DistanceSquared = ToPoint.SizeSquared();

	// Calculate effective detection range based on spotlight attenuation radius
	float EffectiveDetectionRange = AttenuationRadius * SpotlightDetectionFactor;
	float EffectiveDetectionRangeSquared = EffectiveDetectionRange * EffectiveDetectionRange;

	if (DistanceSquared > EffectiveDetectionRangeSquared)
	{
		return false;
	}

	// Normalize only after distance check passes (avoid unnecessary sqrt)
	ToPoint.Normalize();

	// Check if within cone angle (use multiplier for more forgiving detection)
	float DotProduct = FVector::DotProduct(SpotLightDirection, ToPoint);

	// Use cosine comparison instead of angle calculation (cheaper)
	float EffectiveConeAngle = OuterConeAngle * SpotlightConeAngleMultiplier;
	float CosConeAngle = FMath::Cos(FMath::DegreesToRadians(EffectiveConeAngle));

	if (DotProduct < CosConeAngle)
	{
		return false;
	}

	// Check for obstruction (pass the hider as well to allow hitting them)
	return !IsObstructedForHiderDetection(SpotLightLocation, Point, SeekerActor);
}

bool UPawLightDetectionSubsystem::IsHiderCapsuleInSpotlightCone(APawPlayerHider* Hider, AActor* SeekerActor,
                                                                USpotLightComponent* SpotLight) const
{
	if (!Hider)
	{
		return false;
	}

	// Get hider's capsule component for bounds
	UCapsuleComponent* CapsuleComp = Hider->GetCapsuleComponent();
	if (!CapsuleComp)
	{
		// Fallback to actor location if no capsule component
		return IsPointInSpotlightCone(Hider->GetActorLocation(), SeekerActor, SpotLight);
	}

	FVector CapsuleLocation = CapsuleComp->GetComponentLocation();
	float CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();
	float CapsuleRadius = CapsuleComp->GetScaledCapsuleRadius();

	// Use cached test points array to avoid memory allocation
	CachedTestPoints.Reset(5);

	// Pre-calculate offsets
	float HeightOffset = CapsuleHalfHeight * 0.8f;
	float RadiusOffset = CapsuleRadius * 0.8f;

	// Build test points (top, center, bottom, left, right)
	CachedTestPoints.Add(CapsuleLocation + FVector(0, 0, HeightOffset)); // Near top
	CachedTestPoints.Add(CapsuleLocation); // Center
	CachedTestPoints.Add(CapsuleLocation - FVector(0, 0, HeightOffset)); // Near bottom
	CachedTestPoints.Add(CapsuleLocation + FVector(RadiusOffset, 0, 0)); // Right side
	CachedTestPoints.Add(CapsuleLocation + FVector(-RadiusOffset, 0, 0)); // Left side

	// If any point is detected, the hider is spotlighted
	for (const FVector& TestPoint : CachedTestPoints)
	{
		if (IsPointInSpotlightCone(TestPoint, SeekerActor, SpotLight))
		{
			return true;
		}
	}

	return false;
}

bool UPawLightDetectionSubsystem::IsObstructedForHiderDetection(const FVector& Start, const FVector& End,
                                                                AActor* SeekerActor) const
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(SeekerActor); // Ignore the seeker

		// Line trace to check if path is clear
		bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

		// If we hit something, check if it's a real obstruction
		if (bHit)
		{
			AActor* HitActor = HitResult.GetActor();
			if (IsValid(HitActor))
			{
				// If we hit a hider player, it's not an obstruction (we want to detect them)
				if (Cast<APawPlayerHider>(HitActor))
				{
					return false; // Not obstructed, we hit our target
				}

				// If we hit something else (walls, objects, etc.), it's an obstruction
				return true;
			}
		}
	}

	return false; // No obstruction found
}

void UPawLightDetectionSubsystem::RefreshHiderCache()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PawLightDetectionSubsystem_RefreshHiderCache);
	// Only refresh cache periodically or when needed (not every tick)
	static int32 CacheRefreshCounter = 0;
	const int32 CacheRefreshInterval = 50; // Refresh every ~5 seconds at 0.1s tick rate

	if (++CacheRefreshCounter >= CacheRefreshInterval)
	{
		CacheRefreshCounter = 0;

		// Get current hiders in world
		TArray<AActor*> AllHiders;

		// TODO: this is heavy
		UGameplayStatics::GetAllActorsOfClass(this, APawPlayerHider::StaticClass(), AllHiders);

		// Clear and rebuild cache
		CachedHiders.Empty(AllHiders.Num());

		for (AActor* HiderActor : AllHiders)
		{
			if (APawPlayerHider* Hider = Cast<APawPlayerHider>(HiderActor))
			{
				CachedHiders.Add(Hider);
			}
		}
	}

	// Always clean up stale weak pointers (lightweight operation)
	for (int32 i = CachedHiders.Num() - 1; i >= 0; --i)
	{
		if (!CachedHiders[i].IsValid())
		{
			CachedHiders.RemoveAtSwap(i);
		}
	}
}

void UPawLightDetectionSubsystem::AddHiderToCache(APawPlayerHider* Hider)
{
	if (Hider)
	{
		CachedHiders.AddUnique(Hider);
	}
}

void UPawLightDetectionSubsystem::RemoveHiderFromCache(APawPlayerHider* Hider)
{
	if (Hider)
	{
		CachedHiders.RemoveSingle(Hider);
	}
}
