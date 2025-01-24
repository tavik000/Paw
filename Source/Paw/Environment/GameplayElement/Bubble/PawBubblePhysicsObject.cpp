// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubblePhysicsObject.h"


APawBubblePhysicsObject::APawBubblePhysicsObject()
{
	PrimaryActorTick.bCanEverTick = true;
	PhysicsObjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhysicsObjectMesh"));
	PhysicsObjectMesh->SetupAttachment(RootComponent);
	PhysicsObjectMesh->SetRelativeLocation(FVector::ZeroVector);
	PhysicsObjectMesh->SetRelativeRotation(FRotator::ZeroRotator);
	PhysicsObjectMesh->SetSimulatePhysics(false);
	PhysicsObjectMesh->SetIsReplicated(true);
}

void APawBubblePhysicsObject::BeginPlay()
{
	Super::BeginPlay();
}

void APawBubblePhysicsObject::Break_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}
	Super::Break_Implementation();
	BubbleMesh->SetVisibility(false);
	BubbleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PhysicsObjectMesh->SetSimulatePhysics(true);
}


void APawBubblePhysicsObject::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
