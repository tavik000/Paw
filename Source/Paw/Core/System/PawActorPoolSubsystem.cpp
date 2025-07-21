// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawActorPoolSubsystem.h"

#include "Engine/AssetManager.h"
#include "Interface/IPawPoolableInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogPawActorPool, Log, All);

void UPawActorPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogPawActorPool, Log, TEXT("PawActorPoolSubsystem Initialized"));
}

void UPawActorPoolSubsystem::Deinitialize()
{
	// Cancel any pending async loading
	for (auto& Handle : StreamableHandles)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}
	StreamableHandles.Empty();

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
	PendingPoolConfigs.Empty();
	ClassToPoolCache.Empty();
	PriorityLoadingQueues.Empty();
	
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
			UE_LOG(LogPawActorPool, Log, TEXT("Spawned pooled actor: %s at location: %s, rotation: %s"),
			       *PooledActor->GetName(), *Location.ToString(), *Rotation.ToString());
			return PooledActor;
		}
		UE_LOG(LogPawActorPool, Log, TEXT("Pool empty for class: %s, falling back to spawn new actor."),
		       *ActorClass->GetName());
	}
	UE_LOG(LogTemp, Warning, TEXT("Class is not pooled, Fallback spawning new actor of class: %s"), *ActorClass->GetName());

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParameters);
	return NewActor;
}

void UPawActorPoolSubsystem::ReturnToPool(AActor* Actor)
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

	// Try to return to pool, destroy if not poolable
	UClass* ActorClass = Actor->GetClass();
	UClass* PoolClass = FindPoolClassForActorClass(ActorClass);
	if (PoolClass)
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("Returning actor %s (class: %s) to pool (pool class: %s)"), *Actor->GetName(), *ActorClass->GetName(), *PoolClass->GetName());
		ReturnActorToPool(Actor);
	}
	else
	{
		Actor->Destroy();
		UE_LOG(LogPawActorPool, Warning, TEXT("Actor not poolable, destroyed: %s (class: %s). Available pools:"), *Actor->GetName(), *ActorClass->GetName());
		for (const auto& PoolPair : ActorPools)
		{
			UE_LOG(LogPawActorPool, Warning, TEXT("  - Pool class: %s"), *PoolPair.Key->GetName());
		}
	}
}

bool UPawActorPoolSubsystem::IsActorPooled(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}
	
	// Check if actor's class is poolable
	return IsActorClassPooled(Actor->GetClass());
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

	// Group pool configs by priority for optimized loading
	PriorityLoadingQueues.Empty();
	for (const FActorPoolConfig& PoolConfig : Settings->ActorPools)
	{
		if (!PoolConfig.ActorClass.IsNull())
		{
			PriorityLoadingQueues.FindOrAdd(PoolConfig.LoadingPriority).Add(PoolConfig);
		}
	}

	if (PriorityLoadingQueues.Num() == 0)
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("No valid actor classes found in pool configuration"));
		return;
	}

	// Start priority-based async loading (Critical first, then High, Normal, Low)
	UE_LOG(LogPawActorPool, Log, TEXT("Starting priority-based async loading for actor pools"));
	LoadNextPriorityBatch();
}

void UPawActorPoolSubsystem::LoadNextPriorityBatch()
{
	// Find the highest priority batch that needs loading
	EPoolLoadingPriority CurrentPriority = EPoolLoadingPriority::Critical;
	TArray<FActorPoolConfig>* CurrentBatch = nullptr;
	
	// Check priorities in order: Critical, High, Normal, Low
	for (int32 Priority = 0; Priority <= 3; ++Priority)
	{
		EPoolLoadingPriority PriorityLevel = static_cast<EPoolLoadingPriority>(Priority);
		if (TArray<FActorPoolConfig>* Batch = PriorityLoadingQueues.Find(PriorityLevel))
		{
			if (Batch->Num() > 0)
			{
				CurrentPriority = PriorityLevel;
				CurrentBatch = Batch;
				break;
			}
		}
	}

	if (!CurrentBatch || CurrentBatch->Num() == 0)
	{
		UE_LOG(LogPawActorPool, Log, TEXT("All priority batches loaded, actor pool initialization complete"));
		return;
	}

	// Collect asset paths for current priority batch
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FActorPoolConfig& PoolConfig : *CurrentBatch)
	{
		if (!PoolConfig.ActorClass.IsNull())
		{
			AssetsToLoad.Add(PoolConfig.ActorClass.ToSoftObjectPath());
		}
	}

	if (AssetsToLoad.Num() == 0)
	{
		// No valid assets in this batch, try next priority
		PriorityLoadingQueues.Remove(CurrentPriority);
		LoadNextPriorityBatch();
		return;
	}

	// Start async loading for current priority batch with priority hints
	const TCHAR* PriorityName = CurrentPriority == EPoolLoadingPriority::Critical ? TEXT("Critical") :
								CurrentPriority == EPoolLoadingPriority::High ? TEXT("High") :
								CurrentPriority == EPoolLoadingPriority::Normal ? TEXT("Normal") : TEXT("Low");
	
	UE_LOG(LogPawActorPool, Log, TEXT("Loading %s priority batch: %d assets"), PriorityName, AssetsToLoad.Num());
	
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	
	// Create streamable request with priority hints
	FStreamableDelegate LoadDelegate = FStreamableDelegate::CreateLambda([this, CurrentPriority]()
	{
		OnPriorityBatchLoaded(CurrentPriority);
	});
	
	TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
		AssetsToLoad,
		LoadDelegate,
		CurrentPriority == EPoolLoadingPriority::Critical ? FStreamableManager::AsyncLoadHighPriority : FStreamableManager::DefaultAsyncLoadPriority
	);
	
	if (Handle.IsValid())
	{
		StreamableHandles.Add(Handle);
	}
}

void UPawActorPoolSubsystem::OnPriorityBatchLoaded(EPoolLoadingPriority CompletedPriority)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogPawActorPool, Warning, TEXT("World is not valid when priority batch loaded"));
		return;
	}

	// Get the completed batch configs
	if (TArray<FActorPoolConfig>* CompletedBatch = PriorityLoadingQueues.Find(CompletedPriority))
	{
		const TCHAR* PriorityName = CompletedPriority == EPoolLoadingPriority::Critical ? TEXT("Critical") :
									CompletedPriority == EPoolLoadingPriority::High ? TEXT("High") :
									CompletedPriority == EPoolLoadingPriority::Normal ? TEXT("Normal") : TEXT("Low");
		
		UE_LOG(LogPawActorPool, Log, TEXT("%s priority batch loaded, creating pools"), PriorityName);
		
		// Create pools for this batch
		CreatePoolsFromConfigs(*CompletedBatch);
		
		// Remove completed batch from queues
		PriorityLoadingQueues.Remove(CompletedPriority);
	}

	// Load next priority batch
	LoadNextPriorityBatch();
}

void UPawActorPoolSubsystem::CreatePoolsFromConfigs(const TArray<FActorPoolConfig>& PoolConfigs)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// Create pools for each loaded asset with memory-efficient weak references
	for (const FActorPoolConfig& PoolConfig : PoolConfigs)
	{
		if (UClass* ActorClass = PoolConfig.ActorClass.Get())
		{
			if (!IsActorClassPooled(ActorClass))
			{
				const int32 PoolSize = FMath::Max(1, PoolConfig.PoolSize);
				UE_LOG(LogPawActorPool, Log, TEXT("Creating pool for actor class: %s (Size: %d)"), *ActorClass->GetName(), PoolSize);
				
				TArray<TObjectPtr<AActor>>& Pool = ActorPools.Add(ActorClass);
				Pool.Reserve(PoolSize);
				
				// Batch spawn actors for better performance
				TArray<AActor*> SpawnedActors;
				SpawnedActors.Reserve(PoolSize);
				
				for (int32 i = 0; i < PoolSize; ++i)
				{
					AActor* NewActor = World->SpawnActor(ActorClass, &FVector::ZeroVector, &FRotator::ZeroRotator);
					if (IsValid(NewActor))
					{
						SpawnedActors.Add(NewActor);
					}
				}
				
				// Batch deactivate all spawned actors
				for (AActor* Actor : SpawnedActors)
				{
					DeactivatePooledActor(Actor);
					Pool.Add(Actor);
				}
				
				UE_LOG(LogPawActorPool, Log, TEXT("Pool created successfully: %s with %d actors"), *ActorClass->GetName(), SpawnedActors.Num());
				
				// Initialize pool statistics
				PoolStatistics.Add(ActorClass, FPoolStats(SpawnedActors.Num()));
			}
			
			// Release strong reference to loaded class to reduce memory pressure
			// The class will remain loaded as long as the actors exist
		}
		else
		{
			UE_LOG(LogPawActorPool, Warning, TEXT("Failed to get loaded actor class: %s"), *PoolConfig.ActorClass.ToString());
		}
	}

	// Clear any poisoned cache entries that may have been created before pools were ready
	ClassToPoolCache.Empty();
	UE_LOG(LogPawActorPool, Log, TEXT("Cache cleared after pool creation to remove any poisoned entries"));
}

UClass* UPawActorPoolSubsystem::FindPoolClassForActorClass(UClass* ActorClass) const
{
	if (!ActorClass)
	{
		return nullptr;
	}

	// Check cache first for O(1) lookup
	if (const TObjectPtr<UClass>* CachedResult = ClassToPoolCache.Find(ActorClass))
	{
		return CachedResult->Get();
	}

	// Cache miss - perform class equality and inheritance check, then cache the result
	for (const auto& PoolPair : ActorPools)
	{
		if (ActorClass == PoolPair.Key || ActorClass->IsChildOf(PoolPair.Key))
		{
			ClassToPoolCache.Add(ActorClass, PoolPair.Key);
			return PoolPair.Key;
		}
	}

	// No pool found - cache null result to avoid future searches
	ClassToPoolCache.Add(ActorClass, nullptr);
	return nullptr;
}

bool UPawActorPoolSubsystem::IsActorClassPooled(UClass* ActorClass) const
{
	return FindPoolClassForActorClass(ActorClass) != nullptr;
}

AActor* UPawActorPoolSubsystem::GetActorFromPool(UClass* ActorClass)
{
	UClass* PoolClass = FindPoolClassForActorClass(ActorClass);
	if (!PoolClass)
	{
		return nullptr;
	}

	if (TArray<TObjectPtr<AActor>>* Pool = ActorPools.Find(PoolClass))
	{
		// Update statistics
		UpdatePoolStatistics(PoolClass, true);
		
		if (Pool->Num() > 0)
		{
			return Pool->Pop();
		}
		// Pool is empty - record miss and check if we should expand
		if (FPoolStats* Stats = PoolStatistics.Find(PoolClass))
		{
			Stats->PoolMisses++;
			UE_LOG(LogPawActorPool, Warning, TEXT("Pool miss for class: %s (Total misses: %d)"), *PoolClass->GetName(), Stats->PoolMisses);
		}
			
		// Check if we should expand the pool
		if (ShouldExpandPool(PoolClass))
		{
			const UPawActorPoolSettings* Settings = GetDefault<UPawActorPoolSettings>();
			if (Settings && Settings->bEnableDynamicScaling)
			{
				int32 CurrentSize = Pool->Num();
				int32 NewActors = FMath::Max(1, FMath::RoundToInt(CurrentSize * Settings->GrowthFactor));
				NewActors = FMath::Min(NewActors, Settings->MaxPoolSize - CurrentSize);
					
				if (NewActors > 0)
				{
					ExpandPool(PoolClass, NewActors);
					UE_LOG(LogPawActorPool, Log, TEXT("Expanded pool for class: %s by %d actors"), *PoolClass->GetName(), NewActors);
						
					// Try to get actor from newly expanded pool
					if (Pool->Num() > 0)
					{
						return Pool->Pop();
					}
				}
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
	UClass* PoolClass = FindPoolClassForActorClass(ActorClass);
	if (!PoolClass)
	{
		return;
	}

	if (TArray<TObjectPtr<AActor>>* Pool = ActorPools.Find(PoolClass))
	{
		DeactivatePooledActor(Actor);
		Pool->Add(Actor);
		
		// Update statistics
		UpdatePoolStatistics(PoolClass, false);
		
		// Check if we should shrink the pool
		if (ShouldShrinkPool(PoolClass))
		{
			const UPawActorPoolSettings* Settings = GetDefault<UPawActorPoolSettings>();
			if (Settings && Settings->bEnableDynamicScaling)
			{
				int32 CurrentSize = Pool->Num();
				int32 ActorsToRemove = FMath::RoundToInt(CurrentSize * Settings->ShrinkFactor);
				ActorsToRemove = FMath::Min(ActorsToRemove, CurrentSize - Settings->MinPoolSize);
				
				if (ActorsToRemove > 0)
				{
					ShrinkPool(PoolClass, ActorsToRemove);
					UE_LOG(LogPawActorPool, Log, TEXT("Shrunk pool for class: %s by %d actors"), *PoolClass->GetName(), ActorsToRemove);
				}
			}
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
		PoolableInterface->OnPoolActivate(Location, Rotation, SpawnParameters);
	}
}

void UPawActorPoolSubsystem::DeactivatePooledActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	// Call poolable interface first to allow cleanup before state changes
	if (IPawPoolableInterface* PoolableInterface = Cast<IPawPoolableInterface>(Actor))
	{
		PoolableInterface->OnPoolDeactivate();
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

void UPawActorPoolSubsystem::UpdatePoolStatistics(UClass* PoolClass, bool bActorTaken)
{
	if (FPoolStats* Stats = PoolStatistics.Find(PoolClass))
	{
		Stats->TotalRequests++;
		
		if (bActorTaken)
		{
			Stats->CurrentUsage++;
			Stats->PeakUsage = FMath::Max(Stats->PeakUsage, Stats->CurrentUsage);
		}
		else
		{
			Stats->CurrentUsage = FMath::Max(0, Stats->CurrentUsage - 1);
		}
	}
}

bool UPawActorPoolSubsystem::ShouldExpandPool(UClass* PoolClass) const
{
	const UPawActorPoolSettings* Settings = GetDefault<UPawActorPoolSettings>();
	if (!Settings || !Settings->bEnableDynamicScaling)
	{
		return false;
	}

	if (const TArray<TObjectPtr<AActor>>* Pool = ActorPools.Find(PoolClass))
	{
		// Don't expand if already at max size
		if (Pool->Num() >= Settings->MaxPoolSize)
		{
			return false;
		}

		// Expand if we've had multiple recent misses
		if (const FPoolStats* Stats = PoolStatistics.Find(PoolClass))
		{
			return Stats->PoolMisses > 0 && Stats->CurrentUsage > Pool->Num() * 0.8f;
		}
	}
	
	return false;
}

bool UPawActorPoolSubsystem::ShouldShrinkPool(UClass* PoolClass) const
{
	const UPawActorPoolSettings* Settings = GetDefault<UPawActorPoolSettings>();
	if (!Settings || !Settings->bEnableDynamicScaling)
	{
		return false;
	}

	if (const TArray<TObjectPtr<AActor>>* Pool = ActorPools.Find(PoolClass))
	{
		// Don't shrink if already at min size
		if (Pool->Num() <= Settings->MinPoolSize)
		{
			return false;
		}

		// Only consider shrinking if usage is consistently low
		if (const FPoolStats* Stats = PoolStatistics.Find(PoolClass))
		{
			float UsageRatio = Pool->Num() > 0 ? float(Stats->CurrentUsage) / float(Pool->Num()) : 0.0f;
			float TimeSinceLastShrink = GetWorld()->GetTimeSeconds() - Stats->LastShrinkTime;
			
			return UsageRatio < Settings->ShrinkUsageThreshold && TimeSinceLastShrink > 30.0f; // 30 seconds cooldown
		}
	}
	
	return false;
}

void UPawActorPoolSubsystem::ExpandPool(UClass* PoolClass, int32 AdditionalActors)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || AdditionalActors <= 0)
	{
		return;
	}

	if (TArray<TObjectPtr<AActor>>* Pool = ActorPools.Find(PoolClass))
	{
		Pool->Reserve(Pool->Num() + AdditionalActors);
		
		for (int32 i = 0; i < AdditionalActors; ++i)
		{
			AActor* NewActor = World->SpawnActor(PoolClass, &FVector::ZeroVector, &FRotator::ZeroRotator);
			if (IsValid(NewActor))
			{
				DeactivatePooledActor(NewActor);
				Pool->Add(NewActor);
			}
		}
	}
}

void UPawActorPoolSubsystem::ShrinkPool(UClass* PoolClass, int32 ActorsToRemove)
{
	if (ActorsToRemove <= 0)
	{
		return;
	}

	if (TArray<TObjectPtr<AActor>>* Pool = ActorPools.Find(PoolClass))
	{
		int32 ActorsRemoved = 0;
		while (ActorsRemoved < ActorsToRemove && Pool->Num() > 0)
		{
			if (AActor* Actor = Pool->Pop())
			{
				Actor->Destroy();
				ActorsRemoved++;
			}
		}
		
		// Update shrink time
		if (FPoolStats* Stats = PoolStatistics.Find(PoolClass))
		{
			Stats->LastShrinkTime = GetWorld()->GetTimeSeconds();
		}
	}
}
