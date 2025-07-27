// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectileBase.h"

#include "Component/PawProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Misc/MapErrors.h"
#include "Paw/Core/System/PawActorPoolSubsystem.h"


APawProjectileBase::APawProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	AActor::SetReplicateMovement(false);

	// Use a sphere as a simple collision representation
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollisionComp"));
	CollisionComp->InitSphereRadius(5.0f);
	CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");

	// Players can't walk on it
	CollisionComp->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
	CollisionComp->CanCharacterStepUpOn = ECB_No;

	// Set as root component
	RootComponent = CollisionComp;

	// Create a mesh component to represent the projectile visually
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionProfileName(TEXT("NoCollision"));
	ProjectileMesh->SetIsReplicated(true);
	ProjectileMesh->SetVisibility(true);


	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = CreateDefaultSubobject<UPawProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->PrimitiveComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->SetIsReplicated(true);

	// Network interpolation settings for smooth multiplayer movement
	ProjectileMovement->bInterpMovement = true;
	ProjectileMovement->bInterpRotation = true;
	ProjectileMovement->SetInterpolatedComponent(ProjectileMesh);

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

void APawProjectileBase::PostNetReceiveLocationAndRotation()
{
	Super::PostNetReceiveLocationAndRotation();
	if (ProjectileMovement && GetLocalRole() == ROLE_SimulatedProxy)
	{
		ProjectileMovement->MoveInterpolationTarget(GetActorLocation(), GetActorRotation());
	}
}

void APawProjectileBase::ReturnToPoolOrDestroy()
{
	if (!HasAuthority())
	{
		return;
	}
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		UPawActorPoolSubsystem* ActorPoolSubsystem = World->GetSubsystem<UPawActorPoolSubsystem>();
		if (IsValid(ActorPoolSubsystem))
		{
			ActorPoolSubsystem->ReturnToPool(this);
		}
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

void APawProjectileBase::OnPoolActivate(const FVector& Location, const FRotator& Rotation,
	const FActorSpawnParameters& SpawnParameters)
{
	UE_LOG(LogTemp, Warning, TEXT("PawProjectileBase: OnPoolActivate called for %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("  - Location: %s"), *Location.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  - Rotation: %s"), *Rotation.ToString());
	
	// Recalculate projectile velocity based on rotation
	if (UPawProjectileMovementComponent* MovementComp = GetProjectileMovement())
	{
		// Ensure the movement component has the correct UpdatedComponent
		if (CollisionComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Setting UpdatedComponent to CollisionComp: %s"), *CollisionComp->GetName());
			MovementComp->SetUpdatedComponent(CollisionComp);
		}
		
		// Calculate velocity based on spawn rotation, not actor forward (which might be wrong for pooled actors)
		FVector InitialVelocity = Rotation.Vector() * MovementComp->InitialSpeed;
		UE_LOG(LogTemp, Warning, TEXT("  - Calculated initial velocity: %s"), *InitialVelocity.ToString());
		
		MovementComp->StartSimulating(InitialVelocity);
		UE_LOG(LogTemp, Warning, TEXT("  - StartSimulating called"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  - No movement component found!"));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("PawProjectileBase: OnPoolActivate completed"));
}

void APawProjectileBase::OnPoolDeactivate()
{
	if (UPawProjectileMovementComponent* MovementComp = GetProjectileMovement())
	{
		UE_LOG(LogTemp, Warning, TEXT(" Deactivating pooled projectile: %s"), *GetName());
		MovementComp->StopSimulating(FHitResult());
		
		MovementComp->UpdateComponentVelocity();
	}
}
