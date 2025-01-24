// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubbleBase.h"


APawBubbleBase::APawBubbleBase()
{
	PrimaryActorTick.bCanEverTick = true;
	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	BubbleMesh->SetupAttachment(RootComponent);
}

void APawBubbleBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawBubbleBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

