﻿// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawBubbleBase.h"
#include "PawBubbleObjectCapture.generated.h"

UCLASS()
class PAW_API APawBubbleObjectCapture : public APawBubbleBase
{
	GENERATED_BODY()

public:
	APawBubbleObjectCapture();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
};
