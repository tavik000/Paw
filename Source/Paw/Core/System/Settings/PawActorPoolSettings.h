// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PawActorPoolSettings.generated.h"

UENUM()
enum class EPoolLoadingPriority : uint8
{
	Critical = 0 UMETA(DisplayName="Critical (Load First)"),
	High = 1 UMETA(DisplayName="High Priority"),
	Normal = 2 UMETA(DisplayName="Normal Priority"),
	Low = 3 UMETA(DisplayName="Low Priority")
};

USTRUCT()
struct PAW_API FActorPoolConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(DisplayName="Actor Class"))
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, meta=(DisplayName="Pool Size", ClampMin=1, ClampMax=500))
	int32 PoolSize = 50;

	UPROPERTY(EditAnywhere, meta=(DisplayName="Loading Priority"))
	EPoolLoadingPriority LoadingPriority = EPoolLoadingPriority::Normal;

	FActorPoolConfig()
	{
		PoolSize = 50;
		LoadingPriority = EPoolLoadingPriority::Normal;
	}

	FActorPoolConfig(TSoftClassPtr<AActor> InActorClass, int32 InPoolSize = 50, EPoolLoadingPriority InLoadingPriority = EPoolLoadingPriority::Normal)
		: ActorClass(InActorClass), PoolSize(InPoolSize), LoadingPriority(InLoadingPriority)
	{
	}
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Actor Pool Settings"))
class PAW_API UPawActorPoolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPawActorPoolSettings();

	UPROPERTY(Config, EditAnywhere, Category="Actor Pools", meta=(DisplayName="Actor Pool Configuration"))
	TArray<FActorPoolConfig> ActorPools;

	// Dynamic Scaling Settings
	UPROPERTY(Config, EditAnywhere, Category="Dynamic Scaling", meta=(DisplayName="Enable Dynamic Scaling"))
	bool bEnableDynamicScaling = true;

	UPROPERTY(Config, EditAnywhere, Category="Dynamic Scaling", meta=(DisplayName="Minimum Pool Size", ClampMin=1, ClampMax=100))
	int32 MinPoolSize = 10;

	UPROPERTY(Config, EditAnywhere, Category="Dynamic Scaling", meta=(DisplayName="Maximum Pool Size", ClampMin=10, ClampMax=1000))
	int32 MaxPoolSize = 200;

	UPROPERTY(Config, EditAnywhere, Category="Dynamic Scaling", meta=(DisplayName="Growth Factor", ClampMin=0.1, ClampMax=2.0))
	float GrowthFactor = 0.25f;

	UPROPERTY(Config, EditAnywhere, Category="Dynamic Scaling", meta=(DisplayName="Shrink Factor", ClampMin=0.05, ClampMax=0.5))
	float ShrinkFactor = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category="Dynamic Scaling", meta=(DisplayName="Usage Threshold for Shrinking", ClampMin=0.1, ClampMax=0.9))
	float ShrinkUsageThreshold = 0.2f;

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName("Game"); }
	virtual FName GetSectionName() const override { return FName("Actor Pool Settings"); }
};