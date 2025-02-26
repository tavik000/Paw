﻿// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"

#include "PawGameplayElementBase.generated.h"

UCLASS()
class PAW_API APawGameplayElementBase : public AActor
{
	GENERATED_BODY()

public:
	APawGameplayElementBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
};
