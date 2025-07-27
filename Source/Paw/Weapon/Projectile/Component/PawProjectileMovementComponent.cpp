// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Paw/Weapon/Projectile/PawProjectileBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawProjectileMovementComponent)

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);
DEFINE_LOG_CATEGORY_STATIC(LogPawProjectileMovement, Log, All);

const float UPawProjectileMovementComponent::MIN_TICK_TIME = 1e-6f;

// Sets default values for this component's properties
UPawProjectileMovementComponent::UPawProjectileMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUpdateOnlyIfRendered = false;
	bInitialVelocityInLocalSpace = true;
	bSimulationEnabled = true;
	bSweepCollision = true;
	bInterpMovement = false;
	bInterpRotation = false;
	bInterpolationComplete = true;
	bSimulationUseScopedMovement = false;
	bInterpolationUseScopedMovement = true;
	InterpLocationTime = 0.100f;
	InterpRotationTime = 0.050f;
	InterpLocationMaxLagDistance = 300.0f;
	InterpLocationSnapToTargetDistance = 500.0f;
	bThrottleInterpolation = false;
	ThrottleInterpolationThresholdNotRenderedShortTime = 0.20f;
	ThrottleInterpolationThresholdNotRenderedLongTime = 1.0f;
	ThrottleInterpolationFramesSinceInterp = 0;
	ThrottleInterpolationSkipFramesRecent = 1;
	ThrottleInterpolationSkipFramesNotRecent = 2;

	Velocity = FVector(1.f, 0.f, 0.f);

	ProjectileGravityScale = 1.f;

	Bounciness = 0.6f;
	Friction = 0.2f;
	BounceVelocityStopSimulatingThreshold = 5.f;
	MinFrictionFraction = 0.0f;

	HomingAccelerationMagnitude = 0.f;

	bWantsInitializeComponent = true;
	bComponentShouldUpdatePhysicsVolume = false;

	MaxSimulationTimeStep = 0.05f;
	MaxSimulationIterations = 4;
	BounceAdditionalIterations = 1;

	bBounceAngleAffectsFriction = false;
	bIsSliding = false;
	PreviousHitTime = 1.f;
	PreviousHitNormal = FVector::UpVector;

	// Initialize anti-infinite-bounce protection
	ConsecutiveCornerBounces = 0;
	LastCornerBounceTime = 0.0f;
	TotalBounceCount = 0;
}


void UPawProjectileMovementComponent::PostLoad()
{
	Super::PostLoad();

	const FPackageFileVersion LinkerUEVer = GetLinkerUEVersion();

	if (LinkerUEVer < VER_UE4_REFACTOR_PROJECTILE_MOVEMENT)
	{
		// Old code used to treat Bounciness as Friction as well.
		Friction = FMath::Clamp(1.f - Bounciness, 0.f, 1.f);

		// Old projectiles probably don't want to use this behavior by default.
		bInitialVelocityInLocalSpace = false;
	}
}


void UPawProjectileMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	AActor* Actor = GetOwner();
	if (!Actor)
	{
		return;
	}
	if (!Actor->HasAuthority())
	{
		return;
	}

	if (Velocity.SizeSquared() > 0.f)
	{
		// InitialSpeed > 0 overrides initial velocity magnitude.
		if (InitialSpeed > 0.f)
		{
			Velocity = Velocity.GetSafeNormal() * InitialSpeed;
			UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  Verbose - Applied InitialSpeed, new velocity: %s, Actor: %s, IsAuthority : %d"),
				   *Velocity.ToString(), *GetOwner()->GetName(), GetOwner()->HasAuthority());
		}

		if (bInitialVelocityInLocalSpace)
		{
			SetVelocityInLocalSpace(Velocity);
		}

		if (bRotationFollowsVelocity)
		{
			if (UpdatedComponent)
			{
				FRotator DesiredRotation = Velocity.Rotation();
				if (bRotationRemainsVertical)
				{
					DesiredRotation.Pitch = 0.0f;
					DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
					DesiredRotation.Roll = 0.0f;
				}

				UpdatedComponent->SetWorldRotation(DesiredRotation);
			}
			else
			{
				UE_LOG(LogPawProjectileMovement, Warning, TEXT("  - Cannot set rotation, UpdatedComponent is null"));
			}
		}

		UpdateComponentVelocity();

		if (UpdatedPrimitive && UpdatedPrimitive->IsSimulatingPhysics())
		{
			UpdatedPrimitive->SetPhysicsLinearVelocity(Velocity);
		}
	}
	else
	{
		UE_LOG(LogPawProjectileMovement, Warning, TEXT("  - Velocity is zero, skipping initialization"));
	}

	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("PawProjectileMovementComponent: InitializeComponent completed"));
}


void UPawProjectileMovementComponent::UpdateTickRegistration()
{
	if (bAutoUpdateTickRegistration)
	{
		if (!bInterpolationComplete)
		{
			SetComponentTickEnabled(true);
		}
		else
		{
			Super::UpdateTickRegistration();
		}
	}
}

void UPawProjectileMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
                                                    FActorComponentTickFunction* ThisTickFunction)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ProjectileMovementComponent_TickComponent);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ProjectileMovement);

	// Can avoid moving the interpolated object's children until the end of the entire simulation frame.
	// This only makes sense if simulation is also enabled, which would move the UpdatedComponent and move the attached InterpolatedComponent (and children) again.

	// Handle interpolation validation and early returns
	if (!TickInterpolationValidation(DeltaTime))
	{
		UE_LOG(LogPawProjectileMovement, Warning,
		       TEXT(" ProjectileMovementComponent: TickInterpolationValidation failed, skipping tick."));
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(UpdatedComponent) || !bSimulationEnabled)
	{
		UE_LOG(LogPawProjectileMovement, Warning,
		       TEXT(" ProjectileMovementComponent: UpdatedComponent is invalid or simulation is disabled."));
		return;
	}

	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!ActorOwner || !CheckStillInWorld())
	{
		UE_LOG(LogPawProjectileMovement, Warning, TEXT(" ProjectileMovementComponent: ActorOwner is invalid or not in world."));
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	// Run the main physics simulation
	TickPhysicsSimulation(DeltaTime, ActorOwner);
}

void UPawProjectileMovementComponent::TickPhysicsSimulation(float DeltaTime, AActor* ActorOwner)
{
	FVector::FReal VelocityTolerance;
	switch (ActorOwner->GetReplicatedMovement().VelocityQuantizationLevel)
	{
	case EVectorQuantization::RoundWholeNumber:
		VelocityTolerance = 1.0;
		break;
	case EVectorQuantization::RoundOneDecimal:
		VelocityTolerance = 0.1;
		break;
	case EVectorQuantization::RoundTwoDecimals:
		VelocityTolerance = 0.01;
		break;
	default:
		VelocityTolerance = UE_KINDA_SMALL_NUMBER;
		break;
	}

	UE_LOG(LogPawProjectileMovement, Verbose,
	       TEXT(" ProjectileMovementComponent: TickPhysicsSimulation called with DeltaTime: %f, ActorOwner: %s"),
	       DeltaTime, *ActorOwner->GetName());

	FHitResult Hit(1.f);

	CurrentTimeTick = DeltaTime;

	Hit.Time = 1.f;
	OldVelocity = Velocity;
	const FVector MoveDelta = ComputeMoveDelta(OldVelocity, CurrentTimeTick);
	FQuat NewRotation = (bRotationFollowsVelocity && !OldVelocity.IsNearlyZero(VelocityTolerance))
		                    ? OldVelocity.ToOrientationQuat()
		                    : UpdatedComponent->GetComponentQuat();

	if (bRotationFollowsVelocity && bRotationRemainsVertical)
	{
		FRotator DesiredRotation = NewRotation.Rotator();
		DesiredRotation.Pitch = 0.0f;
		DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
		DesiredRotation.Roll = 0.0f;
		NewRotation = DesiredRotation.Quaternion();
	}

	// Move the component
	if (bShouldBounce)
	{
		FVector ActorLocation = ActorOwner->GetActorLocation();
		FQuat ActorQuat = ActorOwner->GetActorQuat();

		float MoveDistance = MoveDelta.Length();
		if (FMath::IsNearlyZero(MoveDistance))
		{
			return;
		}

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(ActorOwner);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // Seeker
		ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2); // Hider

		FVector StartLocation = ActorLocation;
		FVector EndLocation = StartLocation + MoveDelta;

		// Use bounce async sweep
		StartAsyncSweep(EAsyncSweepType::Movement, ActorOwner, EAsyncTraceType::Single,
		                StartLocation, EndLocation, NewRotation,
		                ObjectQueryParams, QueryParams, MoveDistance, MoveDelta.GetSafeNormal(),
		                ActorQuat.Inverse() * NewRotation, MoveDelta);
	}
	else
	{
		// If we can't bounce, then we shouldn't adjust if initially penetrating, because that should be a blocking hit that causes a hit event and stop simulation.
		TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags,
		                                                   MoveComponentFlags |
		                                                   MOVECOMP_NeverIgnoreBlockingOverlaps);
		MoveUpdatedComponent(MoveDelta, NewRotation, bSweepCollision, &Hit);
	}
}

bool UPawProjectileMovementComponent::TickInterpolationValidation(float DeltaTime)
{
	// Still need to finish interpolating after we've stopped simulating, so do that first.
	if (!bInterpolationComplete)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ProjectileMovementComponent_TickInterpolation);
		TickInterpolation(DeltaTime);
	}

	// Consume PendingForce and reset to zero.
	// At this point, any calls to AddForce() will apply to the next frame.
	PendingForceThisUpdate = PendingForce;
	ClearPendingForce();

	// skip if don't want component updated when not rendered or updated component can't move
	if (HasStoppedSimulation() || ShouldSkipUpdate(DeltaTime))
	{
		return false;
	}

	return true;
}

// ============================================================================
// Physics & Collision System
// ============================================================================

bool UPawProjectileMovementComponent::HandleBouncing(const FHitResult& Hit, const float& SubTickTimeRemaining)
{
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : nullptr;
	// If we hit a trigger that destroyed us, abort.
	if (!CheckStillInWorld() || !IsValid(ActorOwner) || HasStoppedSimulation())
	{
		return false;
	}

	// Use async sweep for bounce movement instead of direct transform
	// If it is bouncing, the velocity was modified from ComputeBounceResult;
	FVector MoveDelta = Velocity * SubTickTimeRemaining;
	float MoveDistance = MoveDelta.Length();

	if (FMath::IsNearlyZero(MoveDistance))
	{
		return false;
	}

	if (IsValid(PrimitiveComponent.Get()))
	{
		UE_LOG(LogPawProjectileMovement, Verbose, TEXT("Broadcasting OnComponentHit for %s, HitActor: %s, HitComponent: %s"),
		       *GetNameSafe(PrimitiveComponent.Get()), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()));
		PrimitiveComponent->OnComponentHit.Broadcast(
			PrimitiveComponent.Get(), Hit.GetActor(), Hit.GetComponent(),
			Hit.ImpactNormal, Hit);
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(ActorOwner);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // Seeker
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel2); // Hider
	
	const FVector StartLocation = ActorOwner->GetActorLocation();
	const FVector EndLocation = StartLocation + MoveDelta;
	const FQuat NewRotation = ActorOwner->GetActorQuat() * MovementAsyncSweepData.RelativeQuat;

	// Use unified async system for bounce sweeps
	StartAsyncSweep(EAsyncSweepType::Bounce, ActorOwner, EAsyncTraceType::Single,
	                StartLocation, EndLocation, NewRotation,
	                ObjectQueryParams, QueryParams, MoveDistance, MoveDelta.GetSafeNormal(),
	                MovementAsyncSweepData.RelativeQuat, MoveDelta,
	                SubTickTimeRemaining, ConstrainNormalToPlane(Hit.Normal));
	return true;
}

bool UPawProjectileMovementComponent::HandleDeflection(FHitResult& Hit, float& SubTickTimeRemaining)
{
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);

	// Multiple hits within very short time period?
	const bool bMultiHit = (PreviousHitTime < 1.f && Hit.Time <= UE_KINDA_SMALL_NUMBER);

	// if velocity still into wall (after HandleBlockingHit() had a chance to adjust), slide along wall
	constexpr float DotTolerance = 0.02f;


	const bool bIsGroundSurface = FMath::Abs(Normal.Z) > 0.7f;

	// If the previous hit normal is not valid, or if the current hit normal is not parallel to the previous hit normal,
	const bool bVelocityParallelToSurface = (Velocity.GetSafeNormal() | Normal) <= DotTolerance;
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  Velocity GetSafeNormal: %s, Normal: %s, Dot: %f, bIsGroundSurface: %d"),
	       *Velocity.GetSafeNormal().ToString(), *Normal.ToString(),
	       (Velocity.GetSafeNormal() | Normal), bIsGroundSurface);
	bool bNewSlidingState = FVector::Coincident(PreviousHitNormal, Normal) &&
		bIsGroundSurface && bVelocityParallelToSurface;

	UE_LOG(LogPawProjectileMovement, Verbose,
	       TEXT(
		       " Projectile %s: HandleDeflection called, SameHitNormal: %d, VelocityParallelToSurface: %d, bIsGroundSurface: %d, HitActor : %s, bNewSlidingState: %d, current bIsSliding: %d"
	       ),
	       *GetNameSafe(UpdatedComponent->GetOwner()),
	       FVector::Coincident(PreviousHitNormal, Normal),
	       bVelocityParallelToSurface,
	       bIsGroundSurface,
	       *GetNameSafe(Hit.GetActor()),
	       bNewSlidingState, bIsSliding);

	if (bNewSlidingState != bIsSliding)
	{
		bIsSliding = bNewSlidingState;
		UE_LOG(LogPawProjectileMovement, Verbose, TEXT("Sliding state changed from %d to %d"),
		       !bNewSlidingState, bNewSlidingState);
	}


	if (bIsSliding)
	{
		// Velocity is now parallel to the impact surface.
		if (SubTickTimeRemaining > UE_KINDA_SMALL_NUMBER)
		{
			if (!HandleSliding(Hit, SubTickTimeRemaining))
			{
				// Log: Sliding handler failed, stopping simulation
				UE_LOG(LogPawProjectileMovement, Warning, TEXT("Projectile %s: Stopping simulation after sliding failure"),
				       *GetNameSafe(UpdatedComponent->GetOwner()));
				return false;
			}
		}
	}
	else
	{
		// If we are not sliding, we are bouncing.
		if (!HandleBouncing(Hit, SubTickTimeRemaining))
		{
			UE_LOG(LogPawProjectileMovement, Warning, TEXT("Projectile %s: Stopping simulation after bouncing failure"),
			       *GetNameSafe(UpdatedComponent->GetOwner()));
			return false;
		}
	}

	return true;
}


bool UPawProjectileMovementComponent::HandleSliding(FHitResult& Hit, float& SubTickTimeRemaining)
{
	if (HasStoppedSimulation())
	{
		return false;
	}
	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return false;
	}


	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);
	const FVector OriginalVelocity = Velocity;

	// Handle the case from drop to slide.
	if (FMath::Abs(OriginalVelocity.Z) > 0.1f)
	{
		UE_LOG(LogPawProjectileMovement, Verbose, TEXT(" Projectile %s: Sliding from drop, OriginalVelocity: %s, Normal: %s"),
		       *GetNameSafe(ActorOwner), *OriginalVelocity.ToString(), *Normal.ToString());
		Velocity = ComputeSlideVector(Velocity, 1.f, Normal, Hit);


		// For horizontal surfaces, preserve some vertical momentum to prevent complete flattening
		const float ZPreservationFactor = FMath::IsNearlyZero(Friction) ? 0.9f : 0.5f;
		Velocity.Z = FMath::Lerp(Velocity.Z, OriginalVelocity.Z, ZPreservationFactor);
	}

	// Check min velocity.
	if (IsVelocityUnderSimulationThreshold())
	{
		// Log: Stopping simulation due to velocity below threshold after sliding
		UE_LOG(LogPawProjectileMovement, Verbose,
		       TEXT("Projectile %s: Stopping simulation due to low velocity after sliding"),
		       *GetNameSafe(UpdatedComponent->GetOwner()));
		StopSimulating(Hit);
		return false;
	}

	const FVector OldHitNormal = ConstrainDirectionToPlane(Hit.Normal);

	// Direct position update without async sweep
	FVector MoveDelta = Velocity * SubTickTimeRemaining;
	FTransform NewTransform = ActorOwner->GetActorTransform();
	NewTransform.SetLocation(NewTransform.GetLocation() + MoveDelta);
	ActorOwner->SetActorTransform(NewTransform);

	const FVector HorizontalVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
	const FVector VerticalAcceleration = FVector(0.0f, 0.0f, GetGravityZ() * SubTickTimeRemaining);


	Velocity = HorizontalVelocity + FVector(0.0f, 0.0f, Velocity.Z) + VerticalAcceleration;


	Velocity = ConstrainDirectionToPlane(Velocity);


	// Check min velocity
	if (IsVelocityUnderSimulationThreshold())
	{
		StopSimulating(Hit);
		return false;
	}

	SubTickTimeRemaining = 0.f;
	UpdateComponentVelocity();
	return true;
}


// ============================================================================
// Movement & Velocity System
// ============================================================================

void UPawProjectileMovementComponent::SetVelocityInLocalSpace(FVector NewVelocity)
{
	if (UpdatedComponent)
	{
		Velocity = UpdatedComponent->GetComponentToWorld().TransformVectorNoScale(NewVelocity);
	}
}


FVector UPawProjectileMovementComponent::ComputeVelocity(FVector InitialVelocity, float DeltaTime) const
{
	// v = v0 + a*t
	const FVector Acceleration = ComputeAcceleration(InitialVelocity, DeltaTime);
	FVector NewVelocity = InitialVelocity + (Acceleration * DeltaTime);

	return LimitVelocity(NewVelocity);
}


FVector UPawProjectileMovementComponent::LimitVelocity(FVector NewVelocity) const
{
	const float CurrentMaxSpeed = GetMaxSpeed();
	if (CurrentMaxSpeed > 0.f)
	{
		NewVelocity = NewVelocity.GetClampedToMaxSize(CurrentMaxSpeed);
	}

	return ConstrainDirectionToPlane(NewVelocity);
}

FVector UPawProjectileMovementComponent::ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const
{
	// Velocity Verlet integration (http://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet)
	// The addition of p0 is done outside this method, we are just computing the delta.
	// p = p0 + v0*t + 1/2*a*t^2

	// We use ComputeVelocity() here to infer the acceleration, to make it easier to apply custom velocities.
	// p = p0 + v0*t + 1/2*((v1-v0)/t)*t^2
	// p = p0 + v0*t + 1/2*((v1-v0))*t

	const FVector NewVelocity = ComputeVelocity(InVelocity, DeltaTime);
	const FVector Delta = (InVelocity * DeltaTime) + (NewVelocity - InVelocity) * (0.5f * DeltaTime);
	return Delta;
}

FVector UPawProjectileMovementComponent::ComputeAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	FVector Acceleration(FVector::ZeroVector);

	Acceleration.Z += GetGravityZ();

	Acceleration += PendingForceThisUpdate;

	if (bIsHomingProjectile && HomingTargetComponent.IsValid())
	{
		Acceleration += ComputeHomingAcceleration(InVelocity, DeltaTime);
	}

	return Acceleration;
}

// Allow the projectile to track towards its homing target.
FVector UPawProjectileMovementComponent::ComputeHomingAcceleration(const FVector& InVelocity, float DeltaTime) const
{
	FVector HomingAcceleration = ((HomingTargetComponent->GetComponentLocation() - UpdatedComponent->
		GetComponentLocation()).GetSafeNormal() * HomingAccelerationMagnitude);
	return HomingAcceleration;
}


void UPawProjectileMovementComponent::AddForce(FVector Force)
{
	PendingForce += Force;
}

void UPawProjectileMovementComponent::ClearPendingForce(bool bClearImmediateForce)
{
	PendingForce = FVector::ZeroVector;
	if (bClearImmediateForce)
	{
		PendingForceThisUpdate = FVector::ZeroVector;
	}
}

float UPawProjectileMovementComponent::GetGravityZ() const
{
	// TODO: apply buoyancy if in water
	return ShouldApplyGravity() ? Super::GetGravityZ() * ProjectileGravityScale : 0.f;
}


void UPawProjectileMovementComponent::StopSimulating(const FHitResult& HitResult)
{
	Velocity = FVector::ZeroVector;
	PendingForce = FVector::ZeroVector;
	PendingForceThisUpdate = FVector::ZeroVector;

	bSimulationEnabled = false;
	UpdateComponentVelocity();
	SetUpdatedComponent(nullptr);
	OnProjectileStop.Broadcast(HitResult);

	// Reset the operation state
	ResetAsyncSweepState(EAsyncSweepType::Movement);
	ResetAsyncSweepState(EAsyncSweepType::Bounce);

	UE_LOG(LogPawProjectileMovement, Verbose, TEXT(" Projectile Stop Simulation"));
}

void UPawProjectileMovementComponent::StartSimulating(const FVector& InitialVelocity)
{
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("PawProjectileMovementComponent: StartSimulating called, Actor: %s"),
	       *GetOwner()->GetName());
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  - InitialVelocity: %s"), *InitialVelocity.ToString());
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  - UpdatedComponent before: %s"), UpdatedComponent ? TEXT("Valid") : TEXT("Null"));
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  - Component tick enabled before: %s"),
	       IsComponentTickEnabled() ? TEXT("Yes") : TEXT("No"));

	// Enable simulation and set velocity
	bSimulationEnabled = true;
	Velocity = InitialVelocity;
	UpdateComponentVelocity();

	// Enable component ticking
	SetComponentTickEnabled(true);

	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  - Final velocity: %s"), *Velocity.ToString());
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  - bSimulationEnabled: %s"), bSimulationEnabled ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  - Component tick enabled after: %s"),
	       IsComponentTickEnabled() ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("  - UpdatedComponent after: %s"), UpdatedComponent ? TEXT("Valid") : TEXT("Null"));
	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("PawProjectileMovementComponent: StartSimulating completed"));
}


UPawProjectileMovementComponent::EHandleBlockingHitResult UPawProjectileMovementComponent::HandleBlockingHit(
	const FHitResult& Hit, float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining)
{
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	if (!CheckStillInWorld() || !IsValid(ActorOwner))
	{
		// Error: Projectile invalid or outside world bounds
		UE_LOG(LogPawProjectileMovement, Error, TEXT("Projectile %s is not valid or not in world!"), *GetNameSafe(ActorOwner));
		return EHandleBlockingHitResult::Abort;
	}

	HandleImpact(Hit, TimeTick, MoveDelta);

	if (!IsValid(ActorOwner) || HasStoppedSimulation())
	{
		// Error: Projectile became invalid during impact handling
		UE_LOG(LogPawProjectileMovement, Error, TEXT("Projectile %s is no longer valid or has stopped simulation!"),
		       *GetNameSafe(ActorOwner));
		return EHandleBlockingHitResult::Abort;
	}

	if (Hit.bStartPenetrating)
	{
		// Penetrating hit detected, continuing with deflection
	}

	SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
	return EHandleBlockingHitResult::Deflect;
}

FVector UPawProjectileMovementComponent::ComputeBounceResult(const FHitResult& Hit, float TimeSlice,
                                                             const FVector& MoveDelta)
{
	FVector TempVelocity = Velocity;
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);
	const float VDotNormal = (TempVelocity | Normal);

	// Only if velocity is opposed by normal or parallel
	if (VDotNormal <= 0.f)
	{
		// Project velocity onto normal in reflected direction.
		const FVector ProjectedNormal = Normal * -VDotNormal;

		// Point velocity in direction parallel to surface
		TempVelocity += ProjectedNormal;

		// Only tangential velocity should be affected by friction.
		const float ScaledFriction = (bBounceAngleAffectsFriction || bIsSliding)
			                             ? FMath::Clamp(-VDotNormal / TempVelocity.Size(), MinFrictionFraction, 1.f) *
			                             Friction
			                             : Friction;
		TempVelocity *= FMath::Clamp(1.f - ScaledFriction, 0.f, 1.f);

		// Coefficient of restitution only applies perpendicular to impact.
		TempVelocity += (ProjectedNormal * FMath::Max(Bounciness, 0.f));

		// Bounciness could cause us to exceed max speed.
		TempVelocity = LimitVelocity(TempVelocity);
	}

	UE_LOG(LogPawProjectileMovement, Verbose, TEXT("compute bounce result, Before bounce velocity: %s, After bounce velocity: %s"),
	       *Velocity.ToString(), *TempVelocity.ToString());

	return TempVelocity;
}

void UPawProjectileMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	bool bStopSimulating = false;

	if (bShouldBounce)
	{
		const FVector _OldVelocity = Velocity;
		Velocity = ComputeBounceResult(Hit, TimeSlice, MoveDelta);


		// Trigger bounce events
		OnProjectileBounce.Broadcast(Hit, _OldVelocity);

		// Event may modify velocity or threshold, so check velocity threshold now.
		Velocity = LimitVelocity(Velocity);


		if (IsVelocityUnderSimulationThreshold())
		{
			bStopSimulating = true;
		}
	}
	else
	{
		bStopSimulating = true;
	}


	if (bStopSimulating)
	{
		StopSimulating(Hit);
	}
}

bool UPawProjectileMovementComponent::CheckStillInWorld()
{
	if (!UpdatedComponent)
	{
		return false;
	}

	const UWorld* MyWorld = GetWorld();
	if (!MyWorld)
	{
		return false;
	}

	// check the variations of KillZ
	AWorldSettings* WorldSettings = MyWorld->GetWorldSettings(true);
	if (!WorldSettings->AreWorldBoundsChecksEnabled())
	{
		return true;
	}
	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return false;
	}
	if (ActorOwner->GetActorLocation().Z < WorldSettings->KillZ)
	{
		UDamageType const* DmgType = WorldSettings->KillZDamageType
			                             ? WorldSettings->KillZDamageType->GetDefaultObject<UDamageType>()
			                             : GetDefault<UDamageType>();
		ActorOwner->FellOutOfWorld(*DmgType);
		return false;
	}
	// Check if box has poked outside the world
	if (UpdatedComponent && UpdatedComponent->IsRegistered())
	{
		const FBox& Box = UpdatedComponent->Bounds.GetBox();
		if (Box.Min.X < -HALF_WORLD_MAX || Box.Max.X > HALF_WORLD_MAX ||
			Box.Min.Y < -HALF_WORLD_MAX || Box.Max.Y > HALF_WORLD_MAX ||
			Box.Min.Z < -HALF_WORLD_MAX || Box.Max.Z > HALF_WORLD_MAX)
		{
			UE_LOG(LogPawProjectileMovement, Warning, TEXT("%s is outside the world bounds!"), *ActorOwner->GetName());
			ActorOwner->OutsideWorldBounds();
			// not safe to use physics or collision at this point
			ActorOwner->SetActorEnableCollision(false);
			FHitResult Hit(1.f);
			StopSimulating(Hit);
			return false;
		}
	}
	return true;
}

// ============================================================================
// Network Interpolation System
// ============================================================================

void UPawProjectileMovementComponent::SetInterpolatedComponent(USceneComponent* Component)
{
	if (Component == GetInterpolatedComponent())
	{
		return;
	}

	if (Component)
	{
		if (!ensureMsgf(Component != UpdatedComponent,
		                TEXT(
			                "ProjectileMovement interpolated component should not be the same as the simulated component."
		                )))
		{
			return;
		}

		ResetInterpolation();
		InterpolatedComponentPtr = Component;
		InterpInitialLocationOffset = Component->GetRelativeLocation();
		InterpInitialRotationOffset = Component->GetRelativeRotation().Quaternion();
		// We start at the "completed" location, wait for MoveInterpolationTarget() to actually mark it dirty.
		bInterpolationComplete = true;

		// Space out interpolation skipping to avoid objects spawned on single frame from always updating in sync
		ThrottleInterpolationFramesSinceInterp = ThrottleInterpolationSkipFramesRecent > 0
			                                         ? FMath::RandRange(0, ThrottleInterpolationSkipFramesRecent)
			                                         : 0;
	}
	else
	{
		ResetInterpolation();
		InterpolatedComponentPtr = nullptr;
		InterpInitialLocationOffset = FVector::ZeroVector;
		InterpInitialRotationOffset = FQuat::Identity;
		bInterpolationComplete = true;
		// Disabling interpolation should stop our ticking if we are done simulating and just trying to finish interpolation.
		if (bAutoUpdateTickRegistration && (UpdatedComponent == nullptr))
		{
			UpdateTickRegistration();
		}
	}
}

USceneComponent* UPawProjectileMovementComponent::GetInterpolatedComponent() const
{
	return InterpolatedComponentPtr.Get();
}

void UPawProjectileMovementComponent::MoveInterpolationTarget(const FVector& NewLocation, const FRotator& NewRotation)
{
	if (!UpdatedComponent)
	{
		return;
	}

	bool bHandledMovement = false;
	if (bInterpMovement)
	{
		if (USceneComponent* InterpComponent = GetInterpolatedComponent())
		{
			// Avoid moving the child, it will interpolate later
			const FRotator InterpRelativeRotation = InterpComponent->GetRelativeRotation();
			FScopedPreventAttachedComponentMove ScopedChildNoMove(InterpComponent);

			// Update interp offset
			const FVector OldLocation = UpdatedComponent->GetComponentLocation();
			const FVector NewToOldVector = (OldLocation - NewLocation);
			InterpLocationOffset += NewToOldVector;

			// Enforce distance limits
			if (NewToOldVector.SizeSquared() > FMath::Square(InterpLocationSnapToTargetDistance))
			{
				InterpLocationOffset = FVector::ZeroVector;
			}
			else if (InterpLocationOffset.SizeSquared() > FMath::Square(InterpLocationMaxLagDistance))
			{
				InterpLocationOffset = InterpLocationMaxLagDistance * InterpLocationOffset.GetSafeNormal();
			}

			// Handle rotation
			if (bInterpRotation)
			{
				const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
				InterpRotationOffset = (NewRotation.Quaternion().Inverse() * OldRotation) * InterpRotationOffset;
			}
			else
			{
				// If not interpolating rotation, we should allow the component to rotate.
				// The absolute flag will get restored by the scoped move.
				InterpComponent->SetUsingAbsoluteRotation(false);
				InterpComponent->SetRelativeRotation_Direct(InterpRelativeRotation);
				InterpRotationOffset = FQuat::Identity;
			}

			// Move the root
			UpdatedComponent->SetRelativeLocationAndRotation(NewLocation, NewRotation);
			bHandledMovement = true;
			bInterpolationComplete = false;
		}
		else
		{
			ResetInterpolation();
			bInterpolationComplete = true;
		}
	}

	if (!bHandledMovement)
	{
		UpdatedComponent->SetRelativeLocationAndRotation(NewLocation, NewRotation);
	}
}

void UPawProjectileMovementComponent::ResetInterpolation()
{
	if (USceneComponent* InterpComponent = GetInterpolatedComponent())
	{
		// Snap to original (non-interpolated) offset, we may be forcibly stopping interpolation and need to have it stay at the correct location.
		InterpComponent->SetRelativeLocationAndRotation(InterpInitialLocationOffset, InterpInitialRotationOffset);
	}

	InterpLocationOffset = FVector::ZeroVector;
	InterpRotationOffset = FQuat::Identity;
	bInterpolationComplete = true;

	ThrottleInterpolationFramesSinceInterp = 0;
}

void UPawProjectileMovementComponent::TickInterpolation(float DeltaTime)
{
	if (!bInterpolationComplete)
	{
		if (bInterpMovement)
		{
			// Smooth location. Interp faster when stopping.
			const float ActualInterpLocationTime = Velocity.IsZero() ? 0.5f * InterpLocationTime : InterpLocationTime;
			if (DeltaTime < ActualInterpLocationTime)
			{
				// Slowly decay translation offset (lagged exponential smoothing)
				InterpLocationOffset = (InterpLocationOffset * (1.f - DeltaTime / ActualInterpLocationTime));
			}
			else
			{
				InterpLocationOffset = FVector::ZeroVector;
			}

			// Smooth rotation
			if (DeltaTime < InterpRotationTime && bInterpRotation)
			{
				// Slowly decay rotation offset
				InterpRotationOffset = FQuat::FastLerp(InterpRotationOffset, FQuat::Identity,
				                                       DeltaTime / InterpRotationTime).GetNormalized();
			}
			else
			{
				InterpRotationOffset = FQuat::Identity;
			}

			// Test for reaching the end
			if (InterpLocationOffset.IsNearlyZero(1e-2f) && InterpRotationOffset.Equals(FQuat::Identity, 1e-5f))
			{
				InterpLocationOffset = FVector::ZeroVector;
				InterpRotationOffset = FQuat::Identity;
				bInterpolationComplete = true;
			}

			if (USceneComponent* InterpComponent = GetInterpolatedComponent())
			{
				const bool bShouldThrottleNow = UpdateThrottleInterpolation(DeltaTime, InterpComponent);
				if (bShouldThrottleNow)
				{
					// Skip applying transform to InterpolatedComponent.
					// Don't say we're done interpolating if we haven't applied the result yet, we need it to update next frame.
					bInterpolationComplete = false;
				}
				else
				{
					ThrottleInterpolationFramesSinceInterp = 0;

					// Apply interpolation result
					if (UpdatedComponent)
					{
						const FVector NewRelTranslation = UpdatedComponent->GetComponentToWorld().
						                                                    InverseTransformVectorNoScale(
							                                                    InterpLocationOffset) +
							InterpInitialLocationOffset;
						if (bInterpRotation)
						{
							const FQuat NewRelRotation = InterpRotationOffset * InterpInitialRotationOffset;
							InterpComponent->SetRelativeLocationAndRotation(NewRelTranslation, NewRelRotation);
						}
						else
						{
							InterpComponent->SetRelativeLocation(NewRelTranslation);
						}
					}
				}
			}
		}
		else
		{
			ResetInterpolation();
			bInterpolationComplete = true;
		}

		// Might be done interpolating and want to disable tick
		if (bInterpolationComplete && bAutoUpdateTickRegistration && (UpdatedComponent == nullptr))
		{
			UpdateTickRegistration();
		}
	}
}


bool UPawProjectileMovementComponent::UpdateThrottleInterpolation(float DeltaTime, USceneComponent* InterpComponent)
{
	bool bIsThrottlingThisFrame = false;

	if (bThrottleInterpolation && bInterpMovement && InterpComponent)
	{
		const int32 ThrottleFrames = ComputeThrottleInterpolationMaxFrames(DeltaTime, InterpComponent);
		if (ThrottleFrames > 0)
		{
			ThrottleInterpolationFramesSinceInterp += 1;
			if (ThrottleInterpolationFramesSinceInterp <= ThrottleFrames)
			{
				bIsThrottlingThisFrame = true;
			}
		}
	}

	// Detect transition from throttled to not throttled.
	if (!bIsThrottlingThisFrame && ThrottleInterpolationFramesSinceInterp > 0)
	{
		// Reset counter, not throttling this frame.
		ThrottleInterpolationFramesSinceInterp = 0;

		// Hook for custom reset logic
		ResetThrottleInterpolation(DeltaTime);
	}

	return bIsThrottlingThisFrame;
}

int32 UPawProjectileMovementComponent::ComputeThrottleInterpolationMaxFrames(
	float DeltaTime, USceneComponent* InterpComponent)
{
	int32 ThrottleFrames = 0;
	if (AActor* ActorOwner = InterpComponent->GetOwner())
	{
		// Not recently rendered?
		if (!ActorOwner->WasRecentlyRendered(ThrottleInterpolationThresholdNotRenderedShortTime))
		{
			// Not rendered even a long time ago?
			if (!ActorOwner->WasRecentlyRendered(ThrottleInterpolationThresholdNotRenderedLongTime))
			{
				ThrottleFrames = ThrottleInterpolationSkipFramesNotRecent;
			}
			else
			{
				ThrottleFrames = ThrottleInterpolationSkipFramesRecent;
			}
		}
	}

	return ThrottleFrames;
}

void UPawProjectileMovementComponent::ResetThrottleInterpolation(float DeltaTime)
{
	ThrottleInterpolationFramesSinceInterp = 0;
}

// ============================================================================
// Async Sweep System
// ============================================================================

int UPawProjectileMovementComponent::AsyncSweepByObjectType(const AActor* Actor, EAsyncTraceType InTraceType,
                                                            const FVector& Start, const FVector& End, const FQuat& Rot,
                                                            const FCollisionObjectQueryParams& ObjectQueryParams,
                                                            const FCollisionQueryParams& Params,
                                                            const FTraceDelegate* InDelegate, uint32 UserData)
{
	if (!IsValid(Actor))
	{
		return 0;
	}
	UWorld* World = Actor->GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return 0;
	}
	UPrimitiveComponent* Prim = Actor->FindComponentByClass<UPrimitiveComponent>();
	if (!Prim)
	{
		return 0;
	}

	int TotalSweepCount = 0;

	FCollisionShape CollisionShape = Prim->GetCollisionShape();

	World->AsyncSweepByObjectType(
		InTraceType, Start, End, Rot, ObjectQueryParams, CollisionShape, Params,
		InDelegate, UserData);
	TotalSweepCount++;
	return TotalSweepCount;
}

// ============================================================================
// Unified Async Sweep Handlers (New System)
// ============================================================================

bool UPawProjectileMovementComponent::StartAsyncSweep(EAsyncSweepType SweepType, AActor* ActorOwner,
                                                      EAsyncTraceType TraceType,
                                                      const FVector& Start, const FVector& End,
                                                      const FQuat& Rotation,
                                                      const FCollisionObjectQueryParams& ObjectQueryParams,
                                                      const FCollisionQueryParams& QueryParams,
                                                      float MoveDistance,
                                                      const FVector& Direction, const FQuat& RelativeQuat,
                                                      const FVector& MoveDelta, float SubTickTimeRemaining,
                                                      const FVector& OldHitNormal)
{
	// Validate state before starting
	if (!ValidateAsyncSweepState())
	{
		HandleAsyncSweepError(TEXT("Invalid state for async sweep operation"), SweepType);
		return false;
	}

	if (!IsValid(ActorOwner))
	{
		HandleAsyncSweepError(TEXT("Invalid ActorOwner"), SweepType);
		return false;
	}


	// Reset and configure unified data with provided parameters
	switch (SweepType)
	{
	case EAsyncSweepType::Movement:
		MovementAsyncSweepData.Reset();
		MovementAsyncSweepData.SweepType = SweepType;
		MovementAsyncSweepData.Direction = Direction;
		MovementAsyncSweepData.MoveDistance = MoveDistance;
		MovementAsyncSweepData.HitMinDistance = MoveDistance;
		MovementAsyncSweepData.MoveDelta = MoveDelta;
		MovementAsyncSweepData.RelativeQuat = RelativeQuat;
		MovementAsyncSweepData.SubTickTimeRemaining = SubTickTimeRemaining;
		MovementAsyncSweepData.OldHitNormal = OldHitNormal;
		break;
	case EAsyncSweepType::Bounce:
		BounceAsyncSweepData.Reset();
		BounceAsyncSweepData.SweepType = SweepType;
		BounceAsyncSweepData.Direction = Direction;
		BounceAsyncSweepData.MoveDistance = MoveDistance;
		BounceAsyncSweepData.HitMinDistance = MoveDistance;
		BounceAsyncSweepData.MoveDelta = MoveDelta;
		BounceAsyncSweepData.RelativeQuat = RelativeQuat;
		BounceAsyncSweepData.SubTickTimeRemaining = SubTickTimeRemaining;
		BounceAsyncSweepData.OldHitNormal = OldHitNormal;
		break;
	}

	if (!AsyncMovementSweepDelegate.IsBound())
	{
		AsyncMovementSweepDelegate.BindUObject(this, &ThisClass::HandleMovementSweepResult);
	}
	if (!AsyncBounceSweepDelegate.IsBound())
	{
		AsyncBounceSweepDelegate.BindUObject(this, &ThisClass::HandleBounceSweepResult);
	}


	int SweepCount = 0;
	// Start the async sweep
	switch (SweepType)
	{
	case EAsyncSweepType::Movement:
		UE_LOG(LogPawProjectileMovement, Verbose, TEXT(" Starting async movement sweep for %s"),
		       *GetNameSafe(ActorOwner));
		SweepCount = AsyncSweepByObjectType(ActorOwner, TraceType,
		                                    Start, End, Rotation,
		                                    ObjectQueryParams, QueryParams,
		                                    &AsyncMovementSweepDelegate);
		MovementAsyncSweepData.SweepCount += SweepCount;
		break;
	case EAsyncSweepType::Bounce:
		UE_LOG(LogPawProjectileMovement, Verbose, TEXT(" Starting async bounce sweep for %s"),
		       *GetNameSafe(ActorOwner));
		SweepCount = AsyncSweepByObjectType(ActorOwner, TraceType,
		                                    Start, End, Rotation,
		                                    ObjectQueryParams, QueryParams,
		                                    &AsyncBounceSweepDelegate);
		BounceAsyncSweepData.SweepCount += SweepCount;
		break;
	}

	if (SweepCount <= 0)
	{
		HandleAsyncSweepError(TEXT("Failed to start async sweep operation"), SweepType);
		return false;
	}
	return true;
}


void UPawProjectileMovementComponent::UpdateSweepDataHit(FAsyncSweepData* SweepData, const FHitResult& Hit)
{
	if (SweepData->HitMinDistance > Hit.Distance)
	{
		SweepData->HitMinDistance = Hit.Distance;
		SweepData->HitCount++;
		SweepData->HitResult = Hit;

		// Store type-specific hit data using optimized approach
		if (SweepData->IsMovementType())
		{
			SweepData->HitImpactNormal = Hit.ImpactNormal;
		}
	}
}

void UPawProjectileMovementComponent::HandleMovementSweepResult(const FTraceHandle& TraceHandle, FTraceDatum& Data)
{
	if (!ValidateAsyncSweepState())
	{
		return;
	}

	// Process hits based on sweep type
	for (const FHitResult& Hit : Data.OutHits)
	{
		// Movement type accepts all hits (no additional filtering needed)
		UpdateSweepDataHit(&MovementAsyncSweepData, Hit);
	}

	MovementAsyncSweepData.SweepCount--;
	if (MovementAsyncSweepData.SweepCount <= 0)
	{
		HandleMovementSweepCompleted();
	}
}

void UPawProjectileMovementComponent::HandleBounceSweepResult(const FTraceHandle& TraceHandle, FTraceDatum& Data)
{
	if (!ValidateAsyncSweepState())
	{
		return;
	}

	// For sliding and bounce types, skip hits with same normal as previous hit
	for (const FHitResult& Hit : Data.OutHits)
	{
		if (BounceAsyncSweepData.OldHitNormal == ConstrainDirectionToPlane(Hit.Normal))
		{
			continue;
		}

		UpdateSweepDataHit(&BounceAsyncSweepData, Hit);
	}

	BounceAsyncSweepData.SweepCount--;
	if (BounceAsyncSweepData.SweepCount <= 0)
	{
		HandleBounceSweepCompleted();
	}
}

void UPawProjectileMovementComponent::HandleMovementSweepCompleted()
{
	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return;
	}
	auto NewTransform = ActorOwner->GetActorTransform();
	auto NewLocation = NewTransform.GetLocation() + MovementAsyncSweepData.Direction * MovementAsyncSweepData.
		HitMinDistance;
	const auto NewRotation = NewTransform.GetRotation() * MovementAsyncSweepData.RelativeQuat;
	NewTransform.SetLocation(NewLocation);
	NewTransform.SetRotation(NewRotation);

	if (!MovementAsyncSweepData.HasHits())
	{
		// No hits - free movement
		PreviousHitTime = 1.f;
		bIsSliding = false;

		if (Velocity == OldVelocity)
		{
			Velocity = ComputeVelocity(Velocity, CurrentTimeTick);
		}

		ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogPawProjectileMovement, Verbose,
		       TEXT(" Projectile %s: No hits during async sweep, free movement, Set bIsSliding to false"),
		       *GetNameSafe(ActorOwner));
		UpdateComponentVelocity();
	}
	else
	{
		// Hit detected - handle collision
		// Extract all Movement sweep data into local variables BEFORE calling HandleDeflection
		// to prevent race condition when HandleDeflection starts new async sweeps
		FHitResult Hit = MovementAsyncSweepData.HitResult; // Copy, not reference
		const float HitTime = Hit.Time;
		const FVector MoveDelta = MovementAsyncSweepData.MoveDelta; // Copy, not reference
		const FQuat MovementRelativeQuat = MovementAsyncSweepData.RelativeQuat; // Store for deflection

		// Handle Hit Hider
		if (Hit.GetActor()->ActorHasTag(FName("Hider")))
		{
			if (IsValid(PrimitiveComponent.Get()))
			{
				UE_LOG(LogPawProjectileMovement, Verbose,
				       TEXT("Hider Detected, Broadcasting OnComponentHit for %s, HitActor: %s, HitComponent: %s"),
				       *GetNameSafe(PrimitiveComponent.Get()), *GetNameSafe(Hit.GetActor()),
				       *GetNameSafe(Hit.GetComponent()));
				PrimitiveComponent->OnComponentHit.Broadcast(
					PrimitiveComponent.Get(), Hit.GetActor(), Hit.GetComponent(),
					Hit.ImpactNormal, Hit);
			}
			
			StopSimulating(Hit);
			return;
		}

		if (Velocity == OldVelocity)
		{
			// re-calculate end velocity for partial time
			Velocity = (HitTime > UE_KINDA_SMALL_NUMBER)
				           ? ComputeVelocity(OldVelocity, CurrentTimeTick * HitTime)
				           : OldVelocity;
		}

		// Handle blocking hit
		float SubTickTimeRemaining = CurrentTimeTick * (1.f - HitTime);
		UE_LOG(LogPawProjectileMovement, Verbose,
		       TEXT(
			       " Projectile %s: Hit detected during async sweep, handling hit, HitActor: %s, HitTime: %f, SubTickTimeRemaining: %f, HasAuthority: %s"
		       ),
		       *GetNameSafe(ActorOwner), *GetNameSafe(Hit.GetActor()), HitTime, SubTickTimeRemaining,
		       ActorOwner->HasAuthority() ? TEXT("Yes") : TEXT("No"));
		const EHandleBlockingHitResult HandleBlockingResult = HandleBlockingHit(
			Hit, CurrentTimeTick, MoveDelta, SubTickTimeRemaining);

		if (HandleBlockingResult == EHandleBlockingHitResult::Abort || HasStoppedSimulation())
		{
			UE_LOG(LogPawProjectileMovement, Warning, TEXT("Projectile %s: Stopped simulation after async sweep hit"),
			       *GetNameSafe(ActorOwner));
			return;
		}
		if (HandleBlockingResult == EHandleBlockingHitResult::Deflect)
		{
			// Temporarily store the Movement RelativeQuat for HandleDeflection to use
			// This prevents race condition when HandleDeflection starts Bounce/Sliding sweeps
			MovementAsyncSweepData.RelativeQuat = MovementRelativeQuat;

			if (!HandleDeflection(Hit, SubTickTimeRemaining))
			{
				// Error: Deflection handling failed, stopping simulation
				UE_LOG(LogPawProjectileMovement, Error, TEXT("Projectile %s: Deflection failed, stopping simulation"),
				       *GetNameSafe(ActorOwner));
				StopSimulating(Hit);
				return;
			}

			PreviousHitTime = Hit.Time;
			PreviousHitNormal = ConstrainNormalToPlane(Hit.Normal);
		}
		else if (HandleBlockingResult == EHandleBlockingHitResult::AdvanceNextSubstep)
		{
			// Reset deflection logic to ignore this hit
			PreviousHitTime = 1.f;
		}
		else
		{
			checkNoEntry();
		}
	}
}

void UPawProjectileMovementComponent::HandleBounceSweepCompleted()
{
	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return;
	}
	const auto& SweepData = BounceAsyncSweepData;
	auto NewTransform = ActorOwner->GetActorTransform();
	auto NewLocation = NewTransform.GetLocation() + SweepData.Direction * SweepData.HitMinDistance;
	const auto NewRotation = NewTransform.GetRotation() * SweepData.RelativeQuat;
	NewTransform.SetLocation(NewLocation);
	NewTransform.SetRotation(NewRotation);

	const AActor* HitActor = SweepData.HitResult.GetActor();

	if (const bool bHitProjectile = HitActor && HitActor->IsA<APawProjectileBase>(); !SweepData.HasHits() ||
		bHitProjectile)
	{
		// No hits during bounce movement, or hit another projectile - safe to move
		ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		UpdateComponentVelocity();
		ConsecutiveCornerBounces = 0; // Reset on successful movement
	}
	else
	{
		// Hit detected - check for corner bounce or normal bounce

		if (IsValid(PrimitiveComponent.Get()))
		{
			
			FHitResult Hit = SweepData.HitResult;
			UE_LOG(LogPawProjectileMovement, Verbose, TEXT("Bounce Broadcasting OnComponentHit for %s, HitActor: %s, HitComponent: %s"),
				   *GetNameSafe(PrimitiveComponent.Get()), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()));
			PrimitiveComponent->OnComponentHit.Broadcast(
				PrimitiveComponent.Get(), Hit.GetActor(), Hit.GetComponent(),
				Hit.ImpactNormal, Hit);
			
			// Handle Hit Hider
			if (Hit.GetActor()->ActorHasTag(FName("Hider")))
			{
				StopSimulating(Hit);
				return;
			}
		}


		constexpr float CornerDetectionDistance = 5.0f;

		if (const bool bCornerBounce = SweepData.HitMinDistance < CornerDetectionDistance && !bHitProjectile)
		{
			// Handle corner bounce with infinite loop protection
			const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

			if (const bool bRecentCornerBounce = (CurrentTime - LastCornerBounceTime) < CornerBounceTimeoutWindow)
			{
				ConsecutiveCornerBounces++;
			}
			else
			{
				ConsecutiveCornerBounces = 1;
			}

			LastCornerBounceTime = CurrentTime;

			if (ConsecutiveCornerBounces > MaxConsecutiveCornerBounces)
			{
				UE_LOG(LogPawProjectileMovement, Warning,
				       TEXT("Projectile %s exceeded max consecutive corner bounces (%d), stopping simulation"),
				       *GetNameSafe(ActorOwner), MaxConsecutiveCornerBounces);
				StopSimulating(SweepData.HitResult);
				return;
			}

			// Reverse direction for corner bounce
			NewLocation = ActorOwner->GetActorLocation() - SweepData.Direction * SweepData.HitMinDistance;
			NewTransform.SetLocation(NewLocation);
			ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
			UpdateComponentVelocity();
		}
		else
		{
			// Normal bounce handling
			constexpr float MaxNormalBounceDistance = 50.0f;

			if (SweepData.HitMinDistance < MaxNormalBounceDistance)
			{
				// Check total bounce limits
				TotalBounceCount++;
				if (TotalBounceCount > MaxTotalBounces)
				{
					UE_LOG(LogPawProjectileMovement, Warning, TEXT("Projectile %s exceeded max total bounces (%d), stopping simulation"),
					       *GetNameSafe(ActorOwner), MaxTotalBounces);
					StopSimulating(SweepData.HitResult);
					return;
				}


				const FVector MoveDelta = SweepData.Direction * SweepData.HitMinDistance;
				Velocity = ComputeBounceResult(SweepData.HitResult, CurrentTimeTick, MoveDelta);
				ConsecutiveCornerBounces = 0;
				UpdateComponentVelocity();

				// Apply bounce physics
				NewLocation = ActorOwner->GetActorLocation() + SweepData.Direction * SweepData.HitMinDistance;
				NewTransform.SetLocation(NewLocation);
				ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);

				if (IsVelocityUnderSimulationThreshold())
				{
					UE_LOG(LogPawProjectileMovement, Warning, TEXT("Bounce velocity too low, stopping simulation"));
					StopSimulating(SweepData.HitResult);
				}
			}
			else
			{
				// Far hit - stop simulation
				UE_LOG(LogPawProjectileMovement, Warning, TEXT("Far hit during bounce movement (distance: %.3f), stopping simulation"),
				       SweepData.HitMinDistance);
				StopSimulating(SweepData.HitResult);
			}
		}
	}
}

bool UPawProjectileMovementComponent::ValidateAsyncSweepState() const
{
	// Check if component is in valid state for async operations
	if (!IsValid(UpdatedComponent))
	{
		return false;
	}

	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return false;
	}

	// Check if world is valid for async sweeps
	const UWorld* World = ActorOwner->GetWorld();
	if (!IsValid(World) || !World->IsGameWorld())
	{
		UE_LOG(LogPawProjectileMovement, Warning, TEXT("	ProjectileMovementComponent's world is not valid or not a game world!"));
		return false;
	}

	// Check for simulation state
	if (HasStoppedSimulation())
	{
		UE_LOG(LogPawProjectileMovement, Warning, TEXT(" ProjectileMovementComponent has stopped simulation!"));
		return false;
	}


	return true;
}

void UPawProjectileMovementComponent::HandleAsyncSweepError(const FString& ErrorMessage, EAsyncSweepType SweepType)
{
	// Log the error with context
	const TCHAR* SweepTypeStr = TEXT("Unknown");
	switch (SweepType)
	{
	case EAsyncSweepType::Movement:
		SweepTypeStr = TEXT("Movement");
		break;
	case EAsyncSweepType::Bounce:
		SweepTypeStr = TEXT("Bounce");
		break;
	}

	UE_LOG(LogPawProjectileMovement, Error, TEXT("Async Sweep Error [%s]: %s"), SweepTypeStr, *ErrorMessage);


	// Try to recover by stopping simulation if needed
	if (!CheckStillInWorld())
	{
		const FHitResult DummyHit;
		StopSimulating(DummyHit);
	}
}

void UPawProjectileMovementComponent::ResetAsyncSweepState(EAsyncSweepType SweepType)
{
	switch (SweepType)
	{
	case EAsyncSweepType::Movement:
		MovementAsyncSweepData.Reset();
		break;
	case EAsyncSweepType::Bounce:
		BounceAsyncSweepData.Reset();
		break;
	}

	// Clear any pending delegates to prevent stale callbacks
	if (AsyncMovementSweepDelegate.IsBound())
	{
		AsyncMovementSweepDelegate.Unbind();
	}
	if (AsyncBounceSweepDelegate.IsBound())
	{
		AsyncBounceSweepDelegate.Unbind();
	}
}


bool UPawProjectileMovementComponent::IsAllAsyncSweepingCompleted() const
{
	return MovementAsyncSweepData.SweepCount <= 0 && BounceAsyncSweepData.SweepCount <= 0;
}
