// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawGameplayElementBase.h"


APawGameplayElementBase::APawGameplayElementBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APawGameplayElementBase::BeginPlay()
{
	Super::BeginPlay();
}

void APawGameplayElementBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
