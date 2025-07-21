// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PawActorPoolSettings.generated.h"

USTRUCT()
struct PAW_API FActorPoolConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(DisplayName="Actor Class"))
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, meta=(DisplayName="Pool Size", ClampMin=1, ClampMax=500))
	int32 PoolSize = 50;

	FActorPoolConfig()
	{
		PoolSize = 50;
	}

	FActorPoolConfig(TSoftClassPtr<AActor> InActorClass, int32 InPoolSize = 50)
		: ActorClass(InActorClass), PoolSize(InPoolSize)
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

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName("Game"); }
	virtual FName GetSectionName() const override { return FName("Actor Pool Settings"); }
};