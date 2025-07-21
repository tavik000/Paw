// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/WorldSubsystem.h"
#include "Settings/PawActorPoolSettings.h"
#include "PawActorPoolSubsystem.generated.h"

/**
 * High-performance circular buffer for pool storage
 * Optimized for cache performance and O(1) operations
 */
template<typename T, int32 Capacity>
class TCircularPool
{
public:
	TCircularPool()
	{
		static_assert(Capacity > 0, "Pool capacity must be greater than 0");
		Pool.SetNumUninitialized(Capacity);
		Clear();
	}

	FORCEINLINE bool IsEmpty() const { return Count == 0; }
	FORCEINLINE bool IsFull() const { return Count == Capacity; }
	FORCEINLINE int32 Num() const { return Count; }
	FORCEINLINE int32 GetCapacity() const { return Capacity; }
	FORCEINLINE float GetUsageRatio() const { return Capacity > 0 ? float(Count) / float(Capacity) : 0.0f; }

	// O(1) operations for optimal performance
	FORCEINLINE bool Push(const T& Item)
	{
		if (IsFull())
		{
			return false;
		}
		
		Pool[Tail] = Item;
		Tail = (Tail + 1) % Capacity;
		++Count;
		return true;
	}

	FORCEINLINE bool Pop(T& OutItem)
	{
		if (IsEmpty())
		{
			return false;
		}
		
		OutItem = Pool[Head];
		Head = (Head + 1) % Capacity;
		--Count;
		return true;
	}

	FORCEINLINE T* PopPtr()
	{
		if (IsEmpty())
		{
			return nullptr;
		}
		
		T* Result = &Pool[Head];
		Head = (Head + 1) % Capacity;
		--Count;
		return Result;
	}

	void Clear()
	{
		Head = 0;
		Tail = 0;
		Count = 0;
	}

	// Reserve capacity for efficiency (pre-allocates fixed size)
	void Reserve(int32 NewCapacity)
	{
		// Fixed capacity - no dynamic allocation for performance
		checkf(NewCapacity <= Capacity, TEXT("Cannot reserve more than fixed capacity"));
	}

private:
	// Cache-friendly contiguous storage
	TArray<T> Pool;
	int32 Head = 0;
	int32 Tail = 0;
	int32 Count = 0;
};

/**
 * Pool storage types for different usage patterns
 */
UENUM()
enum class EPoolStorageType : uint8
{
	Dynamic,     // TArray - for variable size pools with dynamic scaling
	Circular,    // TCircularPool - for high-frequency fixed-size pools
	Auto         // Automatically select based on usage patterns
};

/**
 * Cache-aligned pool container for high-performance projectile pooling
 */
struct alignas(64) FOptimizedPoolContainer
{
	// High-frequency circular pool for projectiles (cache-aligned)
	TCircularPool<TObjectPtr<AActor>, 128> ProjectilePool;
	
	// Pool type tracking
	TMap<TObjectPtr<UClass>, EPoolStorageType> PoolTypes;
	
	FOptimizedPoolContainer()
	{
		// Pre-clear projectile pool
		ProjectilePool.Clear();
	}
};

/**
 * High-performance Actor Pool Subsystem
 * Supports both dynamic arrays and circular buffers for optimal performance
 */
UCLASS(Config=Game)
class PAW_API UPawActorPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public: // Constructor & Public Engine Overrides
	//~ USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	//~ End USubsystem Interface
	

public: // Blueprint-Callable API

	UFUNCTION(BlueprintCallable, Category = "Actor Pool")
	void ReturnToPool(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Actor Pool")
	bool IsActorPooled(AActor* Actor) const;

public: // C++ Public Helpers

	AActor* TrySpawnPooledActor(UClass* ActorClass, const FVector& Location, const FRotator& Rotation,
	                            const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());

	template <class T>
	T* TrySpawnPooledActor(UClass* ActorClass, const FVector& Location, const FRotator& Rotation,
	                       const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters())
	{
		if (ActorClass)
		{
			return Cast<T>(TrySpawnPooledActor(ActorClass, Location, Rotation, SpawnParameters));
		}
		return nullptr;
	}

protected: // Properties
	// Traditional dynamic pools
	TMap<TObjectPtr<UClass>, TArray<TObjectPtr<AActor>>> DynamicPools;
	
	// High-performance optimized pools
	FOptimizedPoolContainer OptimizedPools;

private: // Internal Helper Methods
	void InitializeActorPools();
	void LoadNextPriorityBatch();
	void OnPriorityBatchLoaded(EPoolLoadingPriority CompletedPriority);
	void CreatePoolsFromConfigs(const TArray<FActorPoolConfig>& PoolConfigs);
	UClass* FindPoolClassForActorClass(UClass* ActorClass) const;
	bool IsActorClassPooled(UClass* ActorClass) const;
	AActor* GetActorFromPool(UClass* ActorClass);
	void ReturnActorToPool(AActor* Actor);
	void ActivatePooledActor(AActor* Actor, const FVector& Location, const FRotator& Rotation,
	                         const FActorSpawnParameters& SpawnParameters);
	void DeactivatePooledActor(AActor* Actor);

	// Dynamic Scaling Methods
	void UpdatePoolStatistics(UClass* PoolClass, bool bActorTaken);
	bool ShouldExpandPool(UClass* PoolClass) const;
	bool ShouldShrinkPool(UClass* PoolClass) const;
	void ExpandPool(UClass* PoolClass, int32 AdditionalActors);
	void ShrinkPool(UClass* PoolClass, int32 ActorsToRemove);

	// Memory Optimization Methods
	EPoolStorageType DetermineOptimalPoolType(UClass* ActorClass, int32 PoolSize) const;
	bool IsHighFrequencyPool(UClass* ActorClass) const;
	TCircularPool<TObjectPtr<AActor>, 128>* GetCircularPool(UClass* ActorClass);
	TArray<TObjectPtr<AActor>>* GetDynamicPool(UClass* ActorClass);
	AActor* GetActorFromOptimizedPool(UClass* ActorClass);
	void ReturnActorToOptimizedPool(AActor* Actor, UClass* PoolClass);

private: // Cached State
	TArray<TSharedPtr<FStreamableHandle>> StreamableHandles;
	TArray<FActorPoolConfig> PendingPoolConfigs;
	mutable TMap<TObjectPtr<UClass>, TObjectPtr<UClass>> ClassToPoolCache;
	TMap<EPoolLoadingPriority, TArray<FActorPoolConfig>> PriorityLoadingQueues;

	// Pool Usage Statistics
	struct FPoolStats
	{
		int32 PeakUsage = 0;           // Maximum actors taken from pool simultaneously
		int32 CurrentUsage = 0;        // Current actors in use (out of pool)
		int32 TotalRequests = 0;       // Total number of actor requests
		int32 PoolMisses = 0;          // Times pool was empty and had to fallback spawn
		float LastShrinkTime = 0.0f;   // Last time pool was considered for shrinking
		int32 OriginalSize = 0;        // Initial pool size for reference
		
		FPoolStats() = default;
		FPoolStats(int32 InitialSize) : OriginalSize(InitialSize) {}
	};
	
	TMap<TObjectPtr<UClass>, FPoolStats> PoolStatistics;
};
