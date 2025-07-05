#pragma once

#include "CoreMinimal.h"
#include "ETeamId.generated.h"

UENUM(BlueprintType)
enum class ETeamId : uint8
{
	None UMETA(DisplayName = "None"),
	Seeker UMETA(DisplayName = "Seeker"),
	Hider UMETA(DisplayName = "Hider")
};
