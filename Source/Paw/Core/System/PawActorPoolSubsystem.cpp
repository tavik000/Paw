// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawActorPoolSubsystem.h"

#include "Engine/AssetManager.h"
#include "Interface/IPawPoolableInterface.h"
#include "Paw/Weapon/Projectile/PawProjectile_Bubble.h"

DEFINE_LOG_CATEGORY_STATIC(LogPawActorPool, Log, All);

void UPawActorPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogPawActorPool, Log, TEXT("PawActorPoolSubsystem Initialized"));
}

void UPawActorPoolSubsystem::Deinitialize()
{
	// Cancel any pending async loading
	if (StreamableHandle.IsValid())
	{
		StreamableHandle->CancelHandle();
		StreamableHandle.Reset();
	}

	// Clean up pools
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
	PendingPoolConfigs.Empty();
	
	Super::Deinitialize();
	UE_LOG(LogPawActorPool, Log, TEXT("PawActorPoolSubsystem Deinitialized"));
}

void UPawActorPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (ActorPools.Num() == 0)
	{
		InitializeActorPools();
	}
}

bool UPawActorPoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (!IsValid(World))
	{
		return false;
	}

	return Super::ShouldCreateSubsystem(Outer) && World->GetNetMode() != NM_Client;
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

	if (IsActorClassPooled(ActorClass))
	{
		if (AActor* PooledActor = GetActorFromPool(ActorClass))
		{
			ActivatePooledActor(PooledActor, Location, Rotation, SpawnParameters);
			return PooledActor;
		}
		UE_LOG(LogPawActorPool, Log, TEXT("Pool empty for class: %s, falling back to spawn new actor."),
		       *ActorClass->GetName());
	}

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParameters);
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
	if (!IsValid(World))
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("World is not valid, cannot initialize actor pools"));
		return;
	}

	// Get pool configuration from project settings
	const UPawActorPoolSettings* Settings = GetDefault<UPawActorPoolSettings>();
	if (!IsValid(Settings))
	{
		UE_LOG(LogPawActorPool, Error, TEXT("Unable to get PawActorPoolSettings, cannot initialize pools"));
		return;
	}

	// Store pending pool configs for use in callback
	PendingPoolConfigs = Settings->ActorPools;

	// Collect all asset paths for async loading
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FActorPoolConfig& PoolConfig : PendingPoolConfigs)
	{
		if (!PoolConfig.ActorClass.IsNull())
		{
			AssetsToLoad.Add(PoolConfig.ActorClass.ToSoftObjectPath());
		}
	}

	if (AssetsToLoad.Num() == 0)
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("No valid actor classes found in pool configuration"));
		return;
	}

	// Start async loading
	UE_LOG(LogPawActorPool, Log, TEXT("Starting async load of %d actor classes for pools"), AssetsToLoad.Num());
	
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	StreamableHandle = StreamableManager.RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateUObject(this, &UPawActorPoolSubsystem::OnAssetsLoaded)
	);
}

void UPawActorPoolSubsystem::OnAssetsLoaded()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("World is not valid when assets loaded, cannot create pools"));
		return;
	}

	UE_LOG(LogPawActorPool, Log, TEXT("Assets loaded, creating actor pools"));

	// Create pools for each loaded asset
	for (const FActorPoolConfig& PoolConfig : PendingPoolConfigs)
	{
		if (UClass* ActorClass = PoolConfig.ActorClass.Get())
		{
			if (!IsActorClassPooled(ActorClass))
			{
				const int32 PoolSize = FMath::Max(1, PoolConfig.PoolSize); // Ensure valid pool size
				UE_LOG(LogPawActorPool, Log, TEXT("Creating pool for actor class: %s (Size: %d)"), *ActorClass->GetName(), PoolSize);
				
				TArray<TObjectPtr<AActor>>& Pool = ActorPools.Add(ActorClass);
				Pool.Reserve(PoolSize);
				for (int32 i = 0; i < PoolSize; ++i)
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
			UE_LOG(LogPawActorPool, Warning, TEXT("Failed to get loaded actor class: %s"), *PoolConfig.ActorClass.ToString());
		}
	}

	// Clear pending configs and streamable handle
	PendingPoolConfigs.Empty();
	StreamableHandle.Reset();

	UE_LOG(LogPawActorPool, Log, TEXT("Actor pools created successfully via async loading"));
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

	// Call poolable interface if implemented
	if (IPawPoolableInterface* PoolableInterface = Cast<IPawPoolableInterface>(Actor))
	{
		PoolableInterface->OnPoolActivate(Location, Rotation, SpawnParameters);
	}
}

void UPawActorPoolSubsystem::DeactivatePooledActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	// Call poolable interface if implemented
	if (IPawPoolableInterface* PoolableInterface = Cast<IPawPoolableInterface>(Actor))
	{
		PoolableInterface->OnPoolDeactivate();
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
