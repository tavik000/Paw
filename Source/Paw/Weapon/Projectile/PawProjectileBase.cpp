// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectileBase.h"

#include "Component/PawProjectileMovementComponent.h"
#include "Components/SphereComponent.h"


APawProjectileBase::APawProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	AActor::SetReplicateMovement(true);

	// Use a sphere as a simple collision representation
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollisionComp"));
	CollisionComp->InitSphereRadius(5.0f);
	CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");

	// Players can't walk on it
	CollisionComp->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
	CollisionComp->CanCharacterStepUpOn = ECB_No;

	// Set as root component
	RootComponent = CollisionComp;

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = CreateDefaultSubobject<UPawProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->SetIsReplicated(true);

	// Network interpolation settings for smooth multiplayer movement
	ProjectileMovement->bInterpMovement = true;
	ProjectileMovement->bInterpRotation = true;

	// Die after 3 seconds by default
	InitialLifeSpan = 3.0f;


	CollisionComp->SetIsReplicated(true);
}

void APawProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	// Bind hit delegate now that reflection data is available
	CollisionComp->OnComponentHit.AddDynamic(this, &APawProjectileBase::OnHit);

	// Disable movement on clients
	if (!HasAuthority())
	{
		ProjectileMovement->Deactivate();
	}
}

void APawProjectileBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawProjectileBase::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                               FVector NormalImpulse, const FHitResult& Hit)
{
	// Only add impulse and destroy projectile if we hit a physics
	if ((OtherActor != nullptr) && (OtherActor != this) && (OtherComp != nullptr) && OtherComp->IsSimulatingPhysics())
	{
		OtherComp->AddImpulseAtLocation(GetVelocity() * 100.0f, GetActorLocation());

		Destroy();
	}
}
