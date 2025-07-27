// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawActorPool.h"

#include <Paw/Core/System/Interface/IPawPoolableInterface.h>

DEFINE_LOG_CATEGORY_STATIC(LogPawActorPool, Log, All);

void UPawActorPool::InitializePool(TSubclassOf<AActor> InActorClass, int32 InPrewarmCount)
{
	if (!IsValid(InActorClass))
	{
		UE_LOG(LogPawActorPool, Error, TEXT("UPawActorPool::InitializePool - Invalid Actor Class"));
		return;
	}

	ActorClass = InActorClass;
	PrewarmCount = InPrewarmCount;
	PooledActors.Empty();
	PrewarmPool();
}

void UPawActorPool::PrewarmPool()
{
	if (!IsValid(ActorClass) || PrewarmCount <= 0)
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("UPawActorPool::PrewarmPool - Invalid Actor Class or Prewarm Count"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogPawActorPool, Error, TEXT("UPawActorPool::PrewarmPool - Invalid World"));
		return;
	}
	for (int32 i = 0; i < PrewarmCount; ++i)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* NewActor = World->SpawnActor<AActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator,
		                                             SpawnParams);
		if (IsValid(NewActor))
		{
			DeactivatePooledActor(NewActor);
			PushActor(NewActor);
			UE_LOG(LogPawActorPool, Log, TEXT("Prewarmed actor %s (%d/%d)"),
			       *NewActor->GetName(), i + 1, PrewarmCount);
		}
		else
		{
			UE_LOG(LogPawActorPool, Error, TEXT("UPawActorPool::PrewarmPool - Failed to spawn actor %s"),
			       *ActorClass->GetName());
		}
	}
}

AActor* UPawActorPool::TrySpawnPooledActor(const FVector& Location, const FRotator& Rotation,
                                           const FActorSpawnParameters& SpawnParameters)
{
	if (!IsValid(ActorClass) || PrewarmCount <= 0)
	{
		UE_LOG(LogPawActorPool, Warning,
		       TEXT("UPawActorPool::TrySpawnPooledActor - Invalid Actor Class or Prewarm Count"));
		return nullptr;
	}


	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("TrySpawnPooledActor: Invalid World"));
		return nullptr;
	}

	if (AActor* PooledActor = PopActor())
	{
		ActivatePooledActor(PooledActor, Location, Rotation, SpawnParameters);
		UE_LOG(LogPawActorPool, Log, TEXT("Spawned pooled actor: %s at location: %s, rotation: %s"),
		       *PooledActor->GetName(), *Location.ToString(), *Rotation.ToString());
		return PooledActor;
	}

	PoolMisses++;
	UE_LOG(LogPawActorPool, Warning,
	       TEXT("Pool empty for class: %s, falling back to spawn new actor. Pool Misses: %d, Prewarm Count: %d"),
	       *ActorClass->GetName(), PoolMisses, PrewarmCount);

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParameters);
	if (IsValid(NewActor))
	{
		ActivatePooledActor(NewActor, Location, Rotation, SpawnParameters);
	}
	return NewActor;
}

void UPawActorPool::ReturnToPool(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	// Check authority for network safety
	if (Actor->GetLocalRole() != ROLE_Authority)
	{
		UE_LOG(LogPawActorPool, Error, TEXT("Not Authority, cannot return actor to pool: %s"), *Actor->GetName());
		return;
	}

	DeactivatePooledActor(Actor);
	PushActor(Actor);
}

void UPawActorPool::ActivatePooledActor(AActor* Actor, const FVector& Location, const FRotator& Rotation,
                                        const FActorSpawnParameters& SpawnParameters)
{
	if (!IsValid(Actor))
	{
		return;
	}

	// Cache root component lookup to avoid repeated virtual calls
	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent());

	// Batch actor transform and ownership changes
	Actor->SetActorLocationAndRotation(Location, Rotation);
	Actor->SetOwner(SpawnParameters.Owner);
	Actor->SetInstigator(SpawnParameters.Instigator);

	// Batch actor state changes
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
	Actor->SetActorTickEnabled(true);

	// Reset physics state if primitive component exists
	if (RootPrimitive)
	{
		RootPrimitive->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	Actor->Reset();

	// Call poolable interface if implemented
	if (IPawPoolableInterface* PoolableInterface = Cast<IPawPoolableInterface>(Actor))
	{
		PoolableInterface->OnActivateFromPool(this, Location, Rotation, SpawnParameters);
	}
}

void UPawActorPool::DeactivatePooledActor(AActor* Actor)
{
	// Call poolable interface first to allow cleanup before state changes
	if (IPawPoolableInterface* PoolableInterface = Cast<IPawPoolableInterface>(Actor))
	{
		PoolableInterface->OnDeactivateFromPool();
	}

	// Cache root component lookup to avoid repeated virtual calls
	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent());

	// Batch actor state changes
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	// Reset physics state if primitive component exists
	if (RootPrimitive)
	{
		RootPrimitive->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	// Clear ownership references
	Actor->SetOwner(nullptr);
	Actor->SetInstigator(nullptr);
}

void UPawActorPool::PushActor(AActor* Actor)
{
	PooledActors.Add(Actor);
}

AActor* UPawActorPool::PopActor()
{
	while (IsEmpty() == false)
	{
		if (AActor* Actor = PooledActors.Pop(); IsValid(Actor))
		{
			return Actor;
		}
	}
	return nullptr;
}
