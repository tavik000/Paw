// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubblePhysicsObject.h"


APawBubblePhysicsObject::APawBubblePhysicsObject()
{
	PrimaryActorTick.bCanEverTick = true;
	PhysicsObjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhysicsObjectMesh"));
	PhysicsObjectMesh->SetupAttachment(RootComponent);
}

void APawBubblePhysicsObject::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawBubblePhysicsObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

