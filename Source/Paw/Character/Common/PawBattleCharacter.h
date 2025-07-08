// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawCharacterBase.h"
#include "PawBattleCharacter.generated.h"

class UPawCharacterWeaponComponent;

UCLASS()
class PAW_API APawBattleCharacter : public APawCharacterBase
{
	GENERATED_BODY()

public:
	APawBattleCharacter();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UPawCharacterWeaponComponent* CharacterWeaponComponent;


};
