#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "../Enum/ETeamId.h"
#include "ITeamableInterface.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class UTeamableInterface : public UInterface
{
    GENERATED_BODY()
};

class PAW_API ITeamableInterface
{
    GENERATED_BODY()

public:
    virtual ETeamId GetTeamId() const = 0;
    virtual void SetTeamId(ETeamId NewTeamId) = 0;
};