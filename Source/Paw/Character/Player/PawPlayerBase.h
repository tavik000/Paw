// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawBattleCharacter.h"
#include "PawPlayerBase.generated.h"

UCLASS()
class PAW_API APawPlayerBase : public APawBattleCharacter
{
	GENERATED_BODY()

public:
	APawPlayerBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
