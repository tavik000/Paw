#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "../Enums/ETeamId.h"
#include "ITeamableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UTeamableInterface : public UInterface
{
	GENERATED_BODY()
};

class PAWGAME_API ITeamableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Team")
	ETeamId GetTeamId() const;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Team")
	void SetTeamId(ETeamId NewTeamId);
};
