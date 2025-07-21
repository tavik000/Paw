// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawActorPoolSubsystem.h"
#include "Paw/Weapon/Projectile/PawProjectile_Bubble.h"

DEFINE_LOG_CATEGORY_STATIC(LogPawActorPool, Log, All);

void UPawActorPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogPawActorPool, Log, TEXT("PawActorPoolSubsystem Initialized"));
}

void UPawActorPoolSubsystem::Deinitialize()
{
	for (auto& PoolPair : ActorPools)
	{
		for (AActor* Actor : PoolPair.Value)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}
	ActorPools.Empty();
	PooledActors.Empty();
	Super::Deinitialize();
	UE_LOG(LogPawActorPool, Log, TEXT("PawActorPoolSubsystem Deinitialized"));
}

bool UPawActorPoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

AActor* UPawActorPoolSubsystem::TrySpawnPooledActor(UClass* ActorClass, const FVector& Location,
                                                    const FRotator& Rotation,
                                                    const FActorSpawnParameters& SpawnParameters)
{
	if (!IsValid(ActorClass))
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("TrySpawnPooledActor: Invalid ActorClass"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("TrySpawnPooledActor: Invalid World"));
		return nullptr;
	}

	if (ActorPools.Num() == 0)
	{
		InitializeActorPools();
	}

	if (IsActorClassPooled(ActorClass))
	{
		if (AActor* PooledActor = GetActorFromPool(ActorClass))
		{
			ActivatePooledActor(PooledActor, Location, Rotation, SpawnParameters);
			UE_LOG(LogPawActorPool, Warning, TEXT(" Spawned from pool: %s"), *ActorClass->GetName());
			return PooledActor;
		}
		UE_LOG(LogPawActorPool, Log, TEXT("Pool empty for class: %s, falling back to spawn new actor."),
		       *ActorClass->GetName());
	}

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParameters);
	UE_LOG(LogPawActorPool, Log, TEXT("Spawned new actor: %s"), *ActorClass->GetName());
	return NewActor;
}

void UPawActorPoolSubsystem::ReturnToPool(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	if (PooledActors.Contains(Actor))
	{
		ReturnActorToPool(Actor);
		UE_LOG(LogPawActorPool, Log, TEXT("Returned actor to pool: %s"), *Actor->GetName());
	}
	else
	{
		// check authority
		if (Actor->GetLocalRole() != ROLE_Authority)
		{
			UE_LOG(LogPawActorPool, Error, TEXT("Not Authority, cannot return actor to pool: %s"), *Actor->GetName());
			return;
		}
		Actor->Destroy();
		UE_LOG(LogPawActorPool, Warning, TEXT("Actor not in pool, destroyed: %s"), *Actor->GetName());
	}
}

bool UPawActorPoolSubsystem::IsActorPooled(AActor* Actor) const
{
	return PooledActors.Contains(Actor);
}

void UPawActorPoolSubsystem::InitializeActorPools()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->HasBegunPlay())
	{
		return;
	}

	// If no config provided, fallback to hardcoded projectile class
	if (ActorPoolConfig.IsEmpty())
	{
		UClass* ProjectileClass = APawProjectile_Bubble::StaticClass();
		ActorPoolConfig.Add(TSoftClassPtr<AActor>(ProjectileClass));
		UE_LOG(LogPawActorPool, Log, TEXT("No pool config found, using default projectile class"));
	}

	// Initialize pools from configuration
	for (const TSoftClassPtr<AActor>& ClassPtr : ActorPoolConfig)
	{
		if (UClass* ActorClass = ClassPtr.LoadSynchronous())
		{
			if (!IsActorClassPooled(ActorClass))
			{
				UE_LOG(LogPawActorPool, Log, TEXT("Initialized pool for actor class: %s"), *ActorClass->GetName());
				TArray<TObjectPtr<AActor>>& Pool = ActorPools.Add(ActorClass);
				Pool.Reserve(DefaultPoolSize);
				for (int32 i = 0; i < DefaultPoolSize; ++i)
				{
					AActor* NewActor = World->SpawnActor<AActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator);
					if (IsValid(NewActor))
					{
						DeactivatePooledActor(NewActor);
						PooledActors.Add(NewActor);
						Pool.Add(NewActor);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogPawActorPool, Warning, TEXT("Failed to load actor class from config: %s"), *ClassPtr.ToString());
		}
	}
	UE_LOG(LogPawActorPool, Log, TEXT("Actor pools initialized"));
}

bool UPawActorPoolSubsystem::IsActorClassPooled(UClass* ActorClass) const
{
	if (!ActorClass)
	{
		return false;
	}
	for (const auto& PoolPair : ActorPools)
	{
		if (ActorClass->IsChildOf(PoolPair.Key))
		{
			return true;
		}
	}
	return false;
}

AActor* UPawActorPoolSubsystem::GetActorFromPool(UClass* ActorClass)
{
	for (auto& PoolPair : ActorPools)
	{
		if (ActorClass->IsChildOf(PoolPair.Key))
		{
			TArray<TObjectPtr<AActor>>& Pool = PoolPair.Value;
			if (Pool.Num() > 0)
			{
				AActor* PooledActor = Pool.Pop();
				return PooledActor;
			}
		}
	}
	return nullptr;
}

void UPawActorPoolSubsystem::ReturnActorToPool(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}
	UClass* ActorClass = Actor->GetClass();
	for (auto& PoolPair : ActorPools)
	{
		if (ActorClass->IsChildOf(PoolPair.Key))
		{
			DeactivatePooledActor(Actor);
			PoolPair.Value.Add(Actor);
			return;
		}
	}
}

void UPawActorPoolSubsystem::ActivatePooledActor(AActor* Actor, const FVector& Location, const FRotator& Rotation,
                                                 const FActorSpawnParameters& SpawnParameters)
{
	if (!IsValid(Actor))
	{
		return;
	}
	Actor->SetActorLocation(Location);
	Actor->SetActorRotation(Rotation);
	if (SpawnParameters.Owner)
	{
		Actor->SetOwner(SpawnParameters.Owner);
	}
	if (SpawnParameters.Instigator)
	{
		Actor->SetInstigator(SpawnParameters.Instigator);
	}
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
	Actor->SetActorTickEnabled(true);

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		RootPrimitive->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	Actor->Reset();
}

void UPawActorPoolSubsystem::DeactivatePooledActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		RootPrimitive->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		RootPrimitive->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	Actor->SetOwner(nullptr);
	Actor->SetInstigator(nullptr);
}
