// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IPawPoolableInterface.generated.h"

class UPawActorPool;

UINTERFACE(MinimalAPI)
class UPawPoolableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that can be managed by the actor pool system.
 * Provides callbacks for when actors are activated from or returned to the pool.
 */
class PAW_API IPawPoolableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called when an actor is retrieved from the pool and activated for gameplay.
	 * Use this to start timers, initialize state, and prepare for active use.
	 * @param Location The world location to spawn at
	 * @param Rotation The world rotation to spawn with
	 * @param SpawnParameters Additional spawn parameters
	 */
	virtual void OnActivateFromPool(UPawActorPool* InActorPool, const FVector& Location, const FRotator& Rotation, const FActorSpawnParameters& SpawnParameters) = 0;

	/**
	 * Called when an actor is being returned to the pool and deactivated.
	 * Use this to clear timers, reset state, and prepare for pool storage.
	 */
	virtual void OnDeactivateFromPool() = 0;
};