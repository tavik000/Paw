// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "PawActorPoolSubsystem.generated.h"

/**
 * 
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
	TMap<TObjectPtr<UClass>, TArray<TObjectPtr<AActor>>> ActorPools;

	TSet<TObjectPtr<AActor>> PooledActors;

	// TODO
	UPROPERTY(Config)
	TArray<TSoftClassPtr<AActor>> ActorPoolConfig;

private: // Internal Helper Methods
	void InitializeActorPools();
	bool IsActorClassPooled(UClass* ActorClass) const;
	AActor* GetActorFromPool(UClass* ActorClass);
	void ReturnActorToPool(AActor* Actor);
	void ActivatePooledActor(AActor* Actor, const FVector& Location, const FRotator& Rotation,
	                         const FActorSpawnParameters& SpawnParameters);
	void DeactivatePooledActor(AActor* Actor);

private: // Cached State
	static constexpr int32 DefaultPoolSize = 50; // Temp value
};
