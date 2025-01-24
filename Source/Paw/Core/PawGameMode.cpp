// Copyright Epic Games, Inc. All Rights Reserved.

#include "PawGameMode.h"
#include "PawCharacter.h"
#include "UObject/ConstructorHelpers.h"

APawGameMode::APawGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
