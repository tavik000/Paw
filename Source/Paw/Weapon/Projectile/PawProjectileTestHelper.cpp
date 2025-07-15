// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#include "PawProjectileTestHelper.h"
#include "PawProjectileBase.h"
#include "PawProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

APawProjectileTestHelper::APawProjectileTestHelper()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create a simple wall for testing
	TestWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TestWall"));
	RootComponent = TestWall;

	// Try to find a cube mesh for the wall
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		TestWall->SetStaticMesh(CubeMeshAsset.Object);
	}

	// Set up collision for perfect wall testing
	TestWall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TestWall->SetCollisionProfileName(TEXT("BlockAll"));
	TestWall->SetNotifyRigidBodyCollision(true);
}

void APawProjectileTestHelper::BeginPlay()
{
	Super::BeginPlay();
}

void APawProjectileTestHelper::SetupBasicWallTest()
{
	// Position this actor to create a simple test scenario
	SetActorLocation(FVector(500, 0, 100));
	SetActorScale3D(FVector(1, 10, 5));

	UE_LOG(LogTemp, Warning, TEXT("Test Helper: Basic wall test setup complete"));
	UE_LOG(LogTemp, Warning, TEXT("Wall Location: %s"), *GetActorLocation().ToString());
	UE_LOG(LogTemp, Warning, TEXT("Wall Scale: %s"), *GetActorScale3D().ToString());
}

void APawProjectileTestHelper::FireTestProjectiles(int32 NumProjectiles, float FireInterval)
{
	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Test Helper: No projectile class set!"));
		return;
	}

	ProjectilesToFire = NumProjectiles;
	
	// Clear any existing timer
	if (GetWorld()->GetTimerManager().IsTimerActive(FireTimerHandle))
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	}

	// Start firing projectiles at intervals
	GetWorld()->GetTimerManager().SetTimer(
		FireTimerHandle,
		this,
		&APawProjectileTestHelper::FireSingleProjectile,
		FireInterval,
		true
	);

	UE_LOG(LogTemp, Warning, TEXT("Test Helper: Starting to fire %d projectiles at %.2f second intervals"), NumProjectiles, FireInterval);
}

void APawProjectileTestHelper::CreatePerfectWall(const FVector& Location, const FVector& Scale)
{
	SetActorLocation(Location);
	SetActorScale3D(Scale);

	// Ensure perfect collision setup
	if (TestWall)
	{
		TestWall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TestWall->SetCollisionResponseToAllChannels(ECR_Block);
		TestWall->SetCollisionProfileName(TEXT("BlockAll"));
		TestWall->SetNotifyRigidBodyCollision(true);
		TestWall->SetGenerateOverlapEvents(false);
	}

	UE_LOG(LogTemp, Warning, TEXT("Test Helper: Perfect wall created at %s with scale %s"), *Location.ToString(), *Scale.ToString());
}

void APawProjectileTestHelper::EnableAllProjectileLogging(bool bEnable)
{
	for (APawProjectileBase* Projectile : ActiveProjectiles)
	{
		if (IsValid(Projectile))
		{
			Projectile->EnableBounceLogging(bEnable);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Test Helper: %s logging for %d active projectiles"), 
		bEnable ? TEXT("Enabled") : TEXT("Disabled"), ActiveProjectiles.Num());
}

void APawProjectileTestHelper::ClearAllProjectiles()
{
	for (APawProjectileBase* Projectile : ActiveProjectiles)
	{
		if (IsValid(Projectile))
		{
			Projectile->Destroy();
		}
	}

	ActiveProjectiles.Empty();

	// Clear the timer if active
	if (GetWorld()->GetTimerManager().IsTimerActive(FireTimerHandle))
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	}

	UE_LOG(LogTemp, Warning, TEXT("Test Helper: Cleared all projectiles and stopped firing"));
}

void APawProjectileTestHelper::FireSingleProjectile()
{
	if (ProjectilesToFire <= 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
		UE_LOG(LogTemp, Warning, TEXT("Test Helper: Finished firing all projectiles"));
		return;
	}

	if (!ProjectileClass || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("Test Helper: Cannot fire projectile - missing class or world"));
		return;
	}

	// Calculate spawn parameters
	const FVector SpawnLocation = GetActorLocation() + (FireDirection * -800.0f) + FireLocation;
	const FRotator SpawnRotation = FireDirection.Rotation();

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APawProjectileBase* NewProjectile = GetWorld()->SpawnActor<APawProjectileBase>(
		ProjectileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (IsValid(NewProjectile))
	{
		// Set projectile velocity
		if (auto* MovementComp = NewProjectile->GetProjectileMovement())
		{
			MovementComp->Velocity = FireDirection * ProjectileSpeed;
			NewProjectile->EnableBounceLogging(true);
		}

		ActiveProjectiles.Add(NewProjectile);
		ProjectilesToFire--;

		UE_LOG(LogTemp, Log, TEXT("Test Helper: Fired projectile #%d from %s towards wall"), 
			ActiveProjectiles.Num(), *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Test Helper: Failed to spawn projectile"));
	}
}