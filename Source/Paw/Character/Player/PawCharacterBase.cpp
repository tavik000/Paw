// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawCharacterBase.h"


APawCharacterBase::APawCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APawCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


