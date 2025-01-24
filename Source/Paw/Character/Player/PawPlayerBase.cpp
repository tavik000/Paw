// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawPlayerBase.h"


APawPlayerBase::APawPlayerBase()
{
}

void APawPlayerBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawPlayerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawPlayerBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

