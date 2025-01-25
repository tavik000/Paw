// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectile_Bubble.h"

#include "GameFramework/ProjectileMovementComponent.h"


// Sets default values
APawProjectile_Bubble::APawProjectile_Bubble()
{
	ProjectileMovement->bShouldBounce = false;
}

void APawProjectile_Bubble::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawProjectile_Bubble::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

