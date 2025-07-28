// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "UObject/Object.h"
#include "PawActorPool.generated.h"

/**
 * 
 */
UCLASS()
class PAW_API UPawActorPool : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Actor Pool")
	void InitializePool(TSubclassOf<AActor> InActorClass, int32 InPrewarmCount = 5);

	UFUNCTION(BlueprintCallable, Category = "Actor Pool")
	void ReturnToPool(AActor* Actor);

public:
	AActor* TrySpawnPooledActor(const FVector& Location, const FRotator& Rotation,
	                            const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters());

	FORCEINLINE bool IsEmpty() const
	{
		return PooledActors.Num() == 0;
	}

	FORCEINLINE int32 GetSize() const
	{
		return PooledActors.Num();
	}

	FORCEINLINE void PushActor(AActor* Actor);

	AActor* PopActor();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Pool")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Pool", meta = (ClampMin = "1"))
	int32 PrewarmCount = 5;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> PooledActors;

private:
	void PrewarmPool();
	void ActivatePooledActor(AActor* Actor, const FVector& Location, const FRotator& Rotation,
	                         const FActorSpawnParameters& SpawnParameters);
	void DeactivatePooledActor(AActor* Actor);

private:
	// Stats
	int32 PoolMisses = 0;
};
