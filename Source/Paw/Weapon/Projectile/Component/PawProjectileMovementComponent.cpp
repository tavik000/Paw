// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PawProjectileMovementComponent)

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);
DEFINE_LOG_CATEGORY_STATIC(LogProjectileMovement, Log, All);

DEFINE_LOG_CATEGORY_STATIC(LogProjectileMovementInterpolation, Log, All);

const float UPawProjectileMovementComponent::MIN_TICK_TIME = 1e-6f;

// Sets default values for this component's properties
UPawProjectileMovementComponent::UPawProjectileMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUpdateOnlyIfRendered = false;
	bInitialVelocityInLocalSpace = true;
	bForceSubStepping = false;
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

	if (Velocity.SizeSquared() > 0.f)
	{
		// InitialSpeed > 0 overrides initial velocity magnitude.
		if (InitialSpeed > 0.f)
		{
			Velocity = Velocity.GetSafeNormal() * InitialSpeed;
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
		}

		UpdateComponentVelocity();

		if (UpdatedPrimitive && UpdatedPrimitive->IsSimulatingPhysics())
		{
			UpdatedPrimitive->SetPhysicsLinearVelocity(Velocity);
		}
	}
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
	const bool bUseScopedInterpolatedMove = bInterpolationUseScopedMovement && bSimulationEnabled;
	const FScopedMovementUpdate ScopedInterpolatedMove(GetInterpolatedComponent(),
	                                                   bUseScopedInterpolatedMove
		                                                   ? EScopedUpdate::DeferredUpdates
		                                                   : EScopedUpdate::ImmediateUpdates);

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
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(UpdatedComponent) || !bSimulationEnabled)
	{
		return;
	}

	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!ActorOwner || !CheckStillInWorld())
	{
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	if (!IsAllAsyncSweepingCompleted())
	{
		UE_LOG(LogTemp, Warning, TEXT("Skip Tick, Async sweeps are not completed for %s"),
		       *GetNameSafe(UpdatedComponent->GetOwner()));
		return;
	}

	

	FVector::FReal VelocityTolerance = 0.0;
	switch (ActorOwner->GetReplicatedMovement().VelocityQuantizationLevel)
	{
	case EVectorQuantization::RoundWholeNumber: VelocityTolerance = 1.0;
		break;
	case EVectorQuantization::RoundOneDecimal: VelocityTolerance = 0.1;
		break;
	case EVectorQuantization::RoundTwoDecimals: VelocityTolerance = 0.01;
		break;
	default: VelocityTolerance = UE_KINDA_SMALL_NUMBER;
		break;
	}

	float RemainingTime = DeltaTime;
	NumImpacts = 0;
	NumBounces = 0;
	int32 LoopCount = 0;
	int32 Iterations = 0;
	FHitResult Hit(1.f);

	QUICK_SCOPE_CYCLE_COUNTER(STAT_ProjectileMovementComponent_PerformMovement);
	const FScopedMovementUpdate ScopedProjectileUpdate(bSimulationUseScopedMovement ? UpdatedComponent : nullptr,
	                                                   EScopedUpdate::DeferredUpdates);

	// while (bSimulationEnabled && RemainingTime >= MIN_TICK_TIME && (Iterations < MaxSimulationIterations) &&
	// 	IsValid(ActorOwner) && !HasStoppedSimulation())
	// {
	// 	LoopCount++;
	// 	Iterations++;

	// subdivide long ticks to more closely follow parabolic trajectory
	// const float InitialTimeRemaining = RemainingTime;
	CurrentTimeTick = ShouldUseSubStepping()
		                  ? GetSimulationTimeStep(RemainingTime, Iterations)
		                  : RemainingTime;
	RemainingTime -= CurrentTimeTick;

	// Logging
	// UE_LOG(LogProjectileMovement, Log, TEXT("Projectile %s: (Role: %d, Iteration %d, step %.3f, [%.3f / %.3f] cur/total) sim (Pos %s, Vel %s)"),
	// 	*GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole(), LoopCount, TimeTick, FMath::Max(0.f, DeltaTime - InitialTimeRemaining), DeltaTime,
	// 	*UpdatedComponent->GetComponentLocation().ToString(), *Velocity.ToString());

	// Initial move state
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
		// If we can bounce, we are allowed to move out of penetrations, so use SafeMoveUpdatedComponent which does that automatically.
		// SafeMoveUpdatedComponent(MoveDelta, NewRotation, bSweepCollision, Hit);

		FVector ActorLocation = ActorOwner->GetActorLocation();
		FQuat ActorQuat = ActorOwner->GetActorQuat();
		auto& AsyncData = MovementAsyncSweepData;
		AsyncData.Reset();
		AsyncData.MoveDistance = MoveDelta.Length();
		AsyncData.HitMinDistance = AsyncData.MoveDistance;
		AsyncData.Direction = MoveDelta.GetSafeNormal();
		AsyncData.RelativeQuat = ActorQuat.Inverse() * NewRotation;
		AsyncData.MoveDelta = MoveDelta;

		if (FMath::IsNearlyZero(AsyncData.MoveDistance))
		{
			return;
		}
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(ActorOwner);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

		if (!AsyncSweepDelegate.IsBound())
		{
			AsyncSweepDelegate.BindUObject(this, &ThisClass::HandleMovementAsyncSweepResult);
		}
		FVector StartLocation = ActorLocation;
		FVector EndLocation = StartLocation + MoveDelta;
		MovementAsyncSweepData.SweepCount += AsyncSweepByObjectType(ActorOwner, EAsyncTraceType::Single,
		                                                    StartLocation, EndLocation,
		                                                    NewRotation,
		                                                    ObjectQueryParams, QueryParams, &AsyncSweepDelegate);
	}
	else
	{
		// If we can't bounce, then we shouldn't adjust if initially penetrating, because that should be a blocking hit that causes a hit event and stop simulation.
		TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags,
		                                                   MoveComponentFlags |
		                                                   MOVECOMP_NeverIgnoreBlockingOverlaps);
		MoveUpdatedComponent(MoveDelta, NewRotation, bSweepCollision, &Hit);
	}
	// }
}


bool UPawProjectileMovementComponent::HandleDeflection(FHitResult& Hit, float& SubTickTimeRemaining)
{
	UE_LOG(LogTemp, Warning, TEXT(" HandleDeflection called for %s, SubTickTimeRemaining: %.3f"),
	       *GetNameSafe(UpdatedComponent->GetOwner()), SubTickTimeRemaining);
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);

	// Multiple hits within very short time period?
	const bool bMultiHit = (PreviousHitTime < 1.f && Hit.Time <= UE_KINDA_SMALL_NUMBER);

	// if velocity still into wall (after HandleBlockingHit() had a chance to adjust), slide along wall
	constexpr float DotTolerance = 0.01f;

	UE_LOG(LogTemp, Warning, TEXT("bMultiHit: %d, PreviousHitNormal: %s, Normal: %s, VelocityNormal: %s, Dot: %.3f, bIsSliding: %d"),
	       bMultiHit, *PreviousHitNormal.ToString(), *Normal.ToString(),
	       *Velocity.GetSafeNormal().ToString(), (Velocity.GetSafeNormal() | Normal), bIsSliding);
	
	const bool bIsGroundSurface = FMath::Abs(Normal.Z) > 0.7f;

	// If the previous hit normal is not valid, or if the current hit normal is not parallel to the previous hit normal,
	const bool bShouldResumeSliding = !bIsSliding && bMultiHit && FVector::Coincident(PreviousHitNormal, Normal) && bIsGroundSurface;
	const bool bVelocityParallelToSurface = (Velocity.GetSafeNormal() | Normal) <= DotTolerance;
	bIsSliding = bShouldResumeSliding || bVelocityParallelToSurface;

	UE_LOG(LogTemp, Warning, TEXT("Set bIsSliding to %d for %s, PreviousHitNormal: %s, Normal: %s, VelocityNormal: %s, Dot: %.3f"),
	       bIsSliding, *GetNameSafe(UpdatedComponent->GetOwner()), *PreviousHitNormal.ToString(),
	       *Normal.ToString(), *Velocity.GetSafeNormal().ToString(), (Velocity.GetSafeNormal() | Normal));

	if (bIsSliding)
	{
		UE_LOG(LogTemp, Warning, TEXT("Sliding Projectile %s: (Role: %d) Sliding along surface, Velocity: %s, Hit Normal: %s"),
		       *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
		       *Velocity.ToString(), *Normal.ToString());
		// Disable multihit testing for now. but it might cause issues with sliding along corners.
		// for angle < 80 degrees, slide along wall. Cos 80 degrees = 0.173648f
		if (bMultiHit && (PreviousHitNormal | Normal) < 0.173648f)
		{
			//90 degree or less corner, so use cross product for direction

			// FVector NewDir = (Normal ^ PreviousHitNormal);
			// NewDir = NewDir.GetSafeNormal();
			// Velocity = Velocity.ProjectOnToNormal(NewDir);
			// if ((OldVelocity | Velocity) < 0.f)
			// {
			// 	Velocity *= -1.f;
			// }
			// Velocity = ConstrainDirectionToPlane(Velocity);
			// UE_LOG(LogTemp, Warning, TEXT("PreviousHitNormal %s, Normal %s, MultiHit %d, Dot %f"),
			// *PreviousHitNormal.ToString(), *Normal.ToString(), bMultiHit, (PreviousHitNormal | Normal));
			// UE_LOG(LogTemp, Warning, TEXT("MultiHit Projectile %s: (Role: %d) Sliding along corner, new velocity: %s"),
			// 	*GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(), *Velocity.ToString());
		}
		else
		{
			//adjust to move along new wall
			Velocity = ComputeSlideVector(Velocity, 1.f, Normal, Hit);
			UE_LOG(LogTemp, Warning,
			       TEXT("No MultiHit Projectile %s: (Role: %d) Sliding along surface, new velocity: %s"),
			       *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
			       *Velocity.ToString());
		}

		// Check min velocity.
		if (IsVelocityUnderSimulationThreshold())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(" Projectile %s: (Role: %d) Stopping simulation due to low velocity after sliding."),
			       *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole());
			StopSimulating(Hit);
			return false;
		}

		// Velocity is now parallel to the impact surface.
		if (SubTickTimeRemaining > UE_KINDA_SMALL_NUMBER)
		{
			if (!HandleSliding(Hit, SubTickTimeRemaining))
			{
				UE_LOG(LogTemp, Warning, TEXT(" Projectile %s: (Role: %d) Stopping simulation after sliding."),
				       *GetNameSafe(UpdatedComponent->GetOwner()),
				       (int32)UpdatedComponent->GetOwner()->GetLocalRole());
				return false;
			}
		}
	}
	else
	{
		// If we are not sliding, we are bouncing.
		
		AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
		// If we hit a trigger that destroyed us, abort.
		if (!CheckStillInWorld() || !IsValid(ActorOwner) || HasStoppedSimulation())
		{
			return false;
		}

		// Use async sweep for bounce movement instead of direct transform
		FVector MoveDelta = Velocity * SubTickTimeRemaining;
		BounceAsyncData.Reset();
		BounceAsyncData.SubTickTimeRemaining = SubTickTimeRemaining;
		BounceAsyncData.Direction = MoveDelta.GetSafeNormal();
		BounceAsyncData.MoveDistance = MoveDelta.Length();
		BounceAsyncData.HitMinDistance = BounceAsyncData.MoveDistance;
		BounceAsyncData.RelativeQuat = MovementAsyncSweepData.RelativeQuat;
		BounceAsyncData.OldHitNormal = ConstrainNormalToPlane(Hit.Normal);

		if (FMath::IsNearlyZero(BounceAsyncData.MoveDistance))
		{
			return true; // No movement needed
		}

		if (!AsyncBounceDelegate.IsBound())
		{
			AsyncBounceDelegate.BindUObject(this, &ThisClass::HandleBounceAsyncSweepResult);
		}

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(ActorOwner);
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

		const FVector StartLocation = ActorOwner->GetActorLocation();
		const FVector EndLocation = StartLocation + MoveDelta;
		const FQuat NewRotation = ActorOwner->GetActorQuat() * MovementAsyncSweepData.RelativeQuat;

		UE_LOG(LogTemp, Warning, TEXT(" Bounce Sweep for %s, Start %s, End %s, MoveDistance %.3f"),
		       *GetNameSafe(ActorOwner), *StartLocation.ToString(), *EndLocation.ToString(),
		       BounceAsyncData.MoveDistance);

		BounceAsyncData.SweepCount += AsyncSweepByObjectType(ActorOwner, EAsyncTraceType::Single,
		                                                     StartLocation, EndLocation,
		                                                     NewRotation,
		                                                     ObjectQueryParams, QueryParams, &AsyncBounceDelegate);
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
	FHitResult InitialHit(Hit);
	const FVector OldHitNormal = ConstrainDirectionToPlane(Hit.Normal);

	// Velocity is now parallel to the impact surface.
	// Perform the move now, before adding gravity/accel again, so we don't just keep hitting the surface.
	// SafeMoveUpdatedComponent(Velocity * SubTickTimeRemaining, UpdatedComponent->GetComponentQuat(), bSweepCollision,
	//                          Hit);


	// Async Slide Sweep
	SlidingAsyncData.Reset();
	SlidingAsyncData.SubTickTimeRemaining = SubTickTimeRemaining;
	SlidingAsyncData.InitialHit = InitialHit;
	SlidingAsyncData.OldHitNormal = OldHitNormal;

	FVector MoveDelta = Velocity * SubTickTimeRemaining;
	SlidingAsyncData.Direction = MoveDelta.GetSafeNormal();
	SlidingAsyncData.MoveDistance = MoveDelta.Length();
	SlidingAsyncData.HitMinDistance = SlidingAsyncData.MoveDistance;

	if (!AsyncSlidingDelegate.IsBound())
	{
		AsyncSlidingDelegate.BindUObject(this, &ThisClass::HandleSlidingAsyncSweepResult);
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(ActorOwner);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	const FVector StartLocation = ActorOwner->GetActorLocation();
	const FVector EndLocation = StartLocation + MoveDelta;

	UE_LOG(LogTemp, Warning, TEXT("Sliding Seeep for %s, Start %s, End %s, MoveDistance %.3f"),
	       *GetNameSafe(ActorOwner), *StartLocation.ToString(), *EndLocation.ToString(),
	       SlidingAsyncData.MoveDistance);
	SlidingAsyncData.SweepCount += AsyncSweepByObjectType(ActorOwner, EAsyncTraceType::Single,
	                                                      StartLocation, EndLocation,
	                                                      UpdatedComponent->GetComponentQuat(),
	                                                      ObjectQueryParams, QueryParams, &AsyncSlidingDelegate);

	return true;
}


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
	// UE_LOG(LogTemp, Warning, TEXT(" Projectile %s stopped simulating at location %s with velocity %s"),
	// 	*GetNameSafe(UpdatedComponent->GetOwner()), *UpdatedComponent->GetComponentLocation().ToString(), *Velocity.ToString());
	Velocity = FVector::ZeroVector;
	PendingForce = FVector::ZeroVector;
	PendingForceThisUpdate = FVector::ZeroVector;
	UpdateComponentVelocity();
	SetUpdatedComponent(NULL);
	OnProjectileStop.Broadcast(HitResult);
}


UPawProjectileMovementComponent::EHandleBlockingHitResult UPawProjectileMovementComponent::HandleBlockingHit(
	const FHitResult& Hit, float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining)
{
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	if (!CheckStillInWorld() || !IsValid(ActorOwner))
	{
		UE_LOG(LogTemp, Warning, TEXT(" Projectile %s is not valid or not in world!"), *GetNameSafe(ActorOwner));
		return EHandleBlockingHitResult::Abort;
	}

	HandleImpact(Hit, TimeTick, MoveDelta);

	if (!IsValid(ActorOwner) || HasStoppedSimulation())
	{
		UE_LOG(LogTemp, Warning, TEXT(" Projectile %s is no longer valid or has stopped simulation!"),
		       *GetNameSafe(ActorOwner));
		return EHandleBlockingHitResult::Abort;
	}

	if (Hit.bStartPenetrating)
	{
		// UE_LOG(LogTemp, Warning, TEXT(" Projectile %s hit penetrating surface!"), *GetNameSafe(ActorOwner));
		// return EHandleBlockingHitResult::Abort;
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

		// UE_LOG(LogTemp, Warning, TEXT("After bounce: Velocity %s, OldVelocity %s, Hit Normal %s, Bounciness %f, Friction %f"),
		// 	*Velocity.ToString(), *OldVelocity.ToString(), *Hit.Normal.ToString(), Bounciness, Friction);

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
	else if (UpdatedComponent && UpdatedComponent->IsRegistered())
	{
		const FBox& Box = UpdatedComponent->Bounds.GetBox();
		if (Box.Min.X < -HALF_WORLD_MAX || Box.Max.X > HALF_WORLD_MAX ||
			Box.Min.Y < -HALF_WORLD_MAX || Box.Max.Y > HALF_WORLD_MAX ||
			Box.Min.Z < -HALF_WORLD_MAX || Box.Max.Z > HALF_WORLD_MAX)
		{
			UE_LOG(LogProjectileMovement, Warning, TEXT("%s is outside the world bounds!"), *ActorOwner->GetName());
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


bool UPawProjectileMovementComponent::ShouldUseSubStepping() const
{
	return bForceSubStepping || (GetGravityZ() != 0.f) || (bIsHomingProjectile && HomingTargetComponent.IsValid());
}


float UPawProjectileMovementComponent::GetSimulationTimeStep(float RemainingTime, int32 Iterations) const
{
	if (RemainingTime > MaxSimulationTimeStep)
	{
		if (Iterations < MaxSimulationIterations)
		{
			// Subdivide moves to be no longer than MaxSimulationTimeStep seconds
			RemainingTime = FMath::Min(MaxSimulationTimeStep, RemainingTime * 0.5f);
		}
		else
		{
			// If this is the last iteration, just use all the remaining time. This is better than cutting things short, as the simulation won't move far enough otherwise.
			// Print a throttled warning.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (const UWorld* const World = GetWorld())
			{
				// Don't report during long hitches, we're more concerned about normal behavior just to make sure we have reasonable simulation settings.
				if (World->DeltaTimeSeconds < 0.20f)
				{
					static uint32 s_WarningCount = 0;
					if ((s_WarningCount++ < 100) || (GFrameCounter & 15) == 0)
					{
						UE_LOG(LogProjectileMovement, Warning,
						       TEXT(
							       "GetSimulationTimeStep() - Max iterations %d hit while remaining time %.6f > MaxSimulationTimeStep (%.3f) for '%s'"
						       ), MaxSimulationIterations, RemainingTime, MaxSimulationTimeStep,
						       *GetPathNameSafe(UpdatedComponent));
					}
				}
			}
#endif
		}
	}

	// no less than MIN_TICK_TIME (to avoid potential divide-by-zero during simulation).
	return FMath::Max(MIN_TICK_TIME, RemainingTime);
}

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
					// UE_LOG(LogProjectileMovementInterpolation, Verbose, TEXT("--- Skip  Interpolation (%d frames : %s)"), ThrottleInterpolationFramesSinceInterp, *GetPathNameSafe(InterpComponent));
					// Skip applying transform to InterpolatedComponent.
					// Don't say we're done interpolating if we haven't applied the result yet, we need it to update next frame.
					bInterpolationComplete = false;
				}
				else
				{
					ThrottleInterpolationFramesSinceInterp = 0;
					// UE_LOG(LogProjectileMovementInterpolation, Verbose, TEXT("+++ Apply Interpolation (%d frames : %s)"), ThrottleInterpolationFramesSinceInterp, *GetPathNameSafe(InterpComponent));

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

int UPawProjectileMovementComponent::AsyncSweepByObjectType(AActor* Actor, EAsyncTraceType InTraceType,
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
	UE_LOG(LogTemp, Warning,
	       TEXT(" AsyncSweepByObjectType: %s, Start: %s, End: %s, Rot: %s, ObjectQueryParams: %d, Params: %s"),
	       *Actor->GetName(), *Start.ToString(), *End.ToString(), *Rot.Rotator().ToString(),
	       ObjectQueryParams.ObjectTypesToQuery, *Params.ToString());
	TotalSweepCount++;
	return TotalSweepCount;
}

void UPawProjectileMovementComponent::HandleMovementAsyncSweepResult(const FTraceHandle& TraceHandle, FTraceDatum& Data)
{
	UE_LOG(LogTemp, Warning, TEXT(" HandleAsyncSweepResult HitCount: %d"), Data.OutHits.Num());
	auto& AsyncData = MovementAsyncSweepData;
	for (FHitResult Hit : Data.OutHits)
	{
		if (AsyncData.HitMinDistance > Hit.Distance)
		{
			AsyncData.HitMinDistance = Hit.Distance;
			AsyncData.HitCount++;
			AsyncData.HitImpactNormal = Hit.ImpactNormal;
			AsyncData.HitResult = Hit;
		}
	}

	AsyncData.SweepCount--;
	if (AsyncData.SweepCount <= 0)
	{
		HandleMovementAsyncSweepCompleted();
	}
}

void UPawProjectileMovementComponent::HandleMovementAsyncSweepCompleted()
{
	UE_LOG(LogTemp, Warning, TEXT(" HandleAsyncSweepCompleted: HitCount: %d, HitMinDistance: %f, Direction: %s, HitActor: %s"),
	       MovementAsyncSweepData.HitCount, MovementAsyncSweepData.HitMinDistance, *MovementAsyncSweepData.Direction.ToString(),
	       *GetNameSafe(MovementAsyncSweepData.HitResult.GetActor()));
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	// If we hit a trigger that destroyed us, abort.
	if (!CheckStillInWorld() || !IsValid(ActorOwner) || HasStoppedSimulation())
	{
		return;
	}

	auto& AsyncData = MovementAsyncSweepData;
	double RemainDistance = AsyncData.MoveDistance - AsyncData.HitMinDistance;
	auto NewTransform = ActorOwner->GetActorTransform();
	auto NewLocation = NewTransform.GetLocation() + AsyncData.Direction * AsyncData.HitMinDistance;
	auto NewRotation = NewTransform.GetRotation() * AsyncData.RelativeQuat;
	NewTransform.SetLocation(NewLocation);
	NewTransform.SetRotation(NewRotation);

	// Handle Sweep Hit Result
	if (AsyncData.HitCount == 0)
	{
		PreviousHitTime = 1.f;
		bIsSliding = false;
		UE_LOG(LogTemp, Warning, TEXT("Set bIsSliding to false, no hits found."));

		// Only calculate new velocity if events didn't change it during the movement update.
		if (Velocity == OldVelocity)
		{
			Velocity = ComputeVelocity(Velocity, CurrentTimeTick);
		}

		ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		UpdateComponentVelocity();

		// Logging
		UE_LOG(LogTemp, Warning,
		       TEXT("Move Projectile %s: (Role: %d) Hit at %.3f, Location: %s, Velocity: %s, IsSliding: %d"),
		       *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole(), AsyncData.HitMinDistance,
		       *NewLocation.ToString(), *Velocity.ToString(), bIsSliding);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AsyncData.HitCount: %d, HitMinDistance: %f, Direction: %s"),
		       AsyncData.HitCount, AsyncData.HitMinDistance, *AsyncData.Direction.ToString());
		// Only calculate new velocity if events didn't change it during the movement update.
		FHitResult& Hit = AsyncData.HitResult;
		const float& HitTime = AsyncData.HitResult.Time;
		const FVector& MoveDelta = AsyncData.MoveDelta;
		if (Velocity == OldVelocity)
		{
			// re-calculate end velocity for partial time
			Velocity = (HitTime > UE_KINDA_SMALL_NUMBER)
				           ? ComputeVelocity(OldVelocity, CurrentTimeTick * HitTime)
				           : OldVelocity;
			UE_LOG(LogTemp, Warning, TEXT("Velocity: %s, OldVelocity: %s, HitTime: %.3f, CurrentTimeTick: %.3f"),
			       *Velocity.ToString(), *OldVelocity.ToString(), HitTime, CurrentTimeTick);
		}

		// Handle blocking hit
		NumImpacts++;
		float SubTickTimeRemaining = CurrentTimeTick * (1.f - HitTime);
		const EHandleBlockingHitResult HandleBlockingResult = HandleBlockingHit(
			Hit, CurrentTimeTick, MoveDelta, SubTickTimeRemaining);

		if (HandleBlockingResult == EHandleBlockingHitResult::Abort || HasStoppedSimulation())
		{
			UE_LOG(LogTemp, Warning, TEXT("Abort Projectile %s: (Role: %d) Stopped simulation after async sweep hit."),
			       *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole());
			return;
		}
		if (HandleBlockingResult == EHandleBlockingHitResult::Deflect)
		{
			NumBounces++;
			if (!HandleDeflection(Hit, SubTickTimeRemaining))
			{
				UE_LOG(LogTemp, Warning, TEXT(" Projectile %s: (Role: %d) Deflection failed, stopping simulation."),
				       *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole());
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

void UPawProjectileMovementComponent::HandleSlidingAsyncSweepResult(const FTraceHandle& TraceHandle, FTraceDatum& Data)
{
	UE_LOG(LogTemp, Warning, TEXT(" HandleSlidingAsyncSweepResult HitCount: %d"), Data.OutHits.Num());
	auto& AsyncData = SlidingAsyncData;
	for (FHitResult Hit : Data.OutHits)
	{
		UE_LOG(LogTemp, Warning, TEXT("OldNormal: %s, HitNormal: %s, ConstrainNormalToPlane(Hit.Normal): %s"),
		       *AsyncData.OldHitNormal.ToString(), *Hit.Normal.ToString(),
		       *ConstrainDirectionToPlane(Hit.Normal).ToString());
		if (AsyncData.OldHitNormal == ConstrainDirectionToPlane(Hit.Normal))
		{
			UE_LOG(LogTemp, Warning, TEXT(" Skipping hit with same normal: %s"), *Hit.Normal.ToString());
			continue;
		}
		if (AsyncData.HitMinDistance > Hit.Distance)
		{
			UE_LOG(LogTemp, Warning, TEXT(" Updating HitMinDistance: %.3f -> %.3f"),
			       AsyncData.HitMinDistance, Hit.Distance);
			AsyncData.HitMinDistance = Hit.Distance;
			AsyncData.HitCount++;
			AsyncData.HitResult = Hit;
		}
	}

	AsyncData.SweepCount--;
	if (AsyncData.SweepCount <= 0)
	{
		HandleSlidingAsyncSweepCompleted();
	}
}

void UPawProjectileMovementComponent::HandleSlidingAsyncSweepCompleted()
{
	UE_LOG(LogTemp, Warning, TEXT(" HandleSlidingAsyncSweepCompleted: HitCount: %d, HitMinDistance: %f, Direction: %s, HitActor: %s"),
	       SlidingAsyncData.HitCount, SlidingAsyncData.HitMinDistance, *SlidingAsyncData.Direction.ToString(),
	       *GetNameSafe(SlidingAsyncData.HitResult.GetActor()));
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	// If we hit a trigger that destroyed us, abort.
	if (!CheckStillInWorld() || !IsValid(ActorOwner) || HasStoppedSimulation())
	{
		return;
	}

	// A second hit can deflect the velocity (through the normal bounce code), for the next iteration.
	auto& SlideAsyncData = SlidingAsyncData;
	FHitResult& Hit = SlidingAsyncData.HitResult;
	float& SubTickTimeRemaining = SlideAsyncData.SubTickTimeRemaining;
	const FVector& OldHitNormal = SlideAsyncData.OldHitNormal;
	if (SlideAsyncData.HitCount > 0)
	{
		const float TimeTick = SubTickTimeRemaining;
		SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);

		if (HandleBlockingHit(Hit, TimeTick, Velocity * TimeTick, SubTickTimeRemaining) ==
			EHandleBlockingHitResult::Abort ||
			HasStoppedSimulation())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("SlidingAsyncSweepCompleted: Projectile %s: (Role: %d) Stopped simulation after sliding hit."),
			       *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole());
			StopSimulating(Hit);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT(" SlidingAsyncSweepCompleted but not stopped simulation, Hit: %s, Time: %.3f, RemainingTime: %.3f"),
		       *Hit.ToString(), Hit.Time, SubTickTimeRemaining);
	}
	else
	{

		auto& MovementSweepAsyncData = MovementAsyncSweepData;
		auto NewTransform = ActorOwner->GetActorTransform();
		auto NewLocation = NewTransform.GetLocation() + SlideAsyncData.Direction * SlideAsyncData.HitMinDistance;
		auto NewRotation = NewTransform.GetRotation() * MovementSweepAsyncData.RelativeQuat;
		NewTransform.SetLocation(NewLocation);
		NewTransform.SetRotation(NewRotation);
		ActorOwner->SetActorTransform(NewTransform);
		
		// Find velocity after elapsed time
		const FVector PostTickVelocity = ComputeVelocity(Velocity, SubTickTimeRemaining);

		// If pointing back into surface, apply friction and acceleration.
		const FVector Force = (PostTickVelocity - Velocity);
		const float ForceDotN = (Force | OldHitNormal);
		if (ForceDotN < 0.f)
		{
			const FVector ProjectedForce = FVector::VectorPlaneProject(Force, OldHitNormal);
			const FVector NewVelocity = Velocity + ProjectedForce;

			const FVector FrictionForce = -NewVelocity.GetSafeNormal() * FMath::Min(
				-ForceDotN * Friction, NewVelocity.Size());
			Velocity = ConstrainDirectionToPlane(NewVelocity + FrictionForce);
			UE_LOG(LogProjectileMovement, Warning,
			       TEXT("Projectile %s: (Role: %d) Sliding along surface with friction, new velocity: %s"),
			       *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
			       *Velocity.ToString());
		}
		else
		{
			Velocity = PostTickVelocity;
			UE_LOG(LogTemp, Warning,
			       TEXT(" Projectile %s: (Role: %d) Sliding along surface without friction, new velocity: %s"),
			       *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
			       *Velocity.ToString());
		}

		// Check min velocity
		if (IsVelocityUnderSimulationThreshold())
		{
			StopSimulating(Hit);
			return;
		}

		SubTickTimeRemaining = 0.f;
		
		UpdateComponentVelocity();
	}
}

void UPawProjectileMovementComponent::HandleBounceAsyncSweepResult(const FTraceHandle& TraceHandle, FTraceDatum& Data)
{
	UE_LOG(LogTemp, Warning, TEXT(" HandleBounceAsyncSweepResult HitCount: %d"), Data.OutHits.Num());
	auto& AsyncData = BounceAsyncData;
	for (FHitResult Hit : Data.OutHits)
	{
		if (AsyncData.OldHitNormal == ConstrainDirectionToPlane(Hit.Normal))
		{
			UE_LOG(LogTemp, Warning, TEXT(" Bounce Sweep skipping hit with same normal: %s"), *Hit.Normal.ToString());
			continue;
		}
		if (AsyncData.HitMinDistance > Hit.Distance)
		{
			UE_LOG(LogTemp, Warning, TEXT(" Updating Bounce HitMinDistance: %.3f -> %.3f"),
			       AsyncData.HitMinDistance, Hit.Distance);
			AsyncData.HitMinDistance = Hit.Distance;
			AsyncData.HitCount++;
			AsyncData.HitResult = Hit;
		}
	}

	AsyncData.SweepCount--;
	if (AsyncData.SweepCount <= 0)
	{
		HandleBounceAsyncSweepCompleted();
	}
}

void UPawProjectileMovementComponent::HandleBounceAsyncSweepCompleted()
{
	UE_LOG(LogTemp, Warning, TEXT(" HandleBounceAsyncSweepCompleted: HitCount: %d, HitMinDistance: %f, Direction: %s, HitActor: %s"),
	       BounceAsyncData.HitCount, BounceAsyncData.HitMinDistance, *BounceAsyncData.Direction.ToString(),
	       *GetNameSafe(BounceAsyncData.HitResult.GetActor()));
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	// If we hit a trigger that destroyed us, abort.
	if (!CheckStillInWorld() || !IsValid(ActorOwner) || HasStoppedSimulation())
	{
		return;
	}

	auto& BounceData = BounceAsyncData;
	auto NewTransform = ActorOwner->GetActorTransform();
	auto NewLocation = NewTransform.GetLocation() + BounceData.Direction * BounceData.HitMinDistance;
	auto NewRotation = NewTransform.GetRotation() * BounceData.RelativeQuat;
	NewTransform.SetLocation(NewLocation);
	NewTransform.SetRotation(NewRotation);

	if (BounceData.HitCount == 0)
	{
		// No hits during bounce movement, safe to move
		ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		UpdateComponentVelocity();
		
		// Reset consecutive corner bounce counter on successful movement
		ConsecutiveCornerBounces = 0;
		
		UE_LOG(LogTemp, Warning, TEXT(" Bounce Projectile %s: (Role: %d) No hits, safe movement to %s"),
		       *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole(), *NewLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Hit during bounce sweep: HitCount: %d, HitMinDistance: %f, Direction: %s"),
		       BounceData.HitCount, BounceData.HitMinDistance, *BounceData.Direction.ToString());
		
		// Detect corner bounce (immediate hit during bounce movement)
		if (constexpr float CornerDetectionDistance = 5.0f; BounceData.HitMinDistance < CornerDetectionDistance)
		{
			// Check for infinite bounce protection
			const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const bool bRecentCornerBounce = (CurrentTime - LastCornerBounceTime) < CornerBounceTimeoutWindow;
			
			if (bRecentCornerBounce)
			{
				ConsecutiveCornerBounces++;
			}
			else
			{
				// Reset counter if enough time has passed
				ConsecutiveCornerBounces = 1;
			}
			
			LastCornerBounceTime = CurrentTime;
			
			// Check if we've exceeded the maximum consecutive bounces
			if (ConsecutiveCornerBounces > MaxConsecutiveCornerBounces)
			{
				UE_LOG(LogTemp, Warning, TEXT("Projectile %s exceeded max consecutive corner bounces (%d), stopping simulation to prevent infinite loop"), 
				       *GetNameSafe(ActorOwner), MaxConsecutiveCornerBounces);
				StopSimulating(BounceData.HitResult);
				return;
			}
			
			// Corner bounce detected - use intelligent escape logic
			UE_LOG(LogTemp, Warning, TEXT("Corner bounce detected (distance: %.3f, bounce #%d), attempting intelligent escape"), 
			       BounceData.HitMinDistance, ConsecutiveCornerBounces);
			
			// Try to find a clear escape direction
			FVector EscapeDirection;
			float EscapeDistance;
			if (FindCornerEscapeDirection(NewLocation, EscapeDirection, EscapeDistance))
			{
				// Successfully found escape direction
				NewLocation += EscapeDirection * EscapeDistance;
				NewTransform.SetLocation(NewLocation);
				ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
				
				// Adjust velocity to point away from collision
				Velocity = EscapeDirection.GetSafeNormal() * Velocity.Size() * Bounciness;
				
				// Apply friction for energy loss
				Velocity *= (1.0f - Friction);
				
				UpdateComponentVelocity();
				UE_LOG(LogTemp, Warning, TEXT("Corner escape successful: direction=%s, distance=%.3f, new velocity=%s"), 
				       *EscapeDirection.ToString(), EscapeDistance, *Velocity.ToString());
			}
			else
			{
				// Fallback: use original logic with larger distance
				UE_LOG(LogTemp, Warning, TEXT("No clear escape direction found, using fallback escape"));
				
				// Reverse velocity direction with bounciness factor
				Velocity = -Velocity * Bounciness;
				Velocity *= (1.0f - Friction);
				
				// Move away with larger distance
				FVector AwayDirection = -BounceData.Direction.GetSafeNormal();
				if (AwayDirection.IsNearlyZero())
				{
					AwayDirection = FVector::UpVector; // Default to upward escape
				}
				
				NewLocation += AwayDirection * 15.0f; // Increased from 3.0f
				NewTransform.SetLocation(NewLocation);
				ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
				
				UpdateComponentVelocity();
				UE_LOG(LogTemp, Warning, TEXT("Fallback corner escape applied, moved away by: %s"), 
				       *(AwayDirection * 15.0f).ToString());
			}
			
			// Check if velocity is still above simulation threshold
			if (IsVelocityUnderSimulationThreshold())
			{
				UE_LOG(LogTemp, Warning, TEXT("Corner bounce velocity too low, stopping simulation"));
				StopSimulating(BounceData.HitResult);
				return;
			}
		}
		else
		{
			// Check if this is a normal bounce (reasonable distance) vs far hit that should stop
			constexpr float MaxNormalBounceDistance = 50.0f;
			
			if (BounceData.HitMinDistance < MaxNormalBounceDistance)
			{
				// Check bounce limits to prevent infinite bouncing
				TotalBounceCount++;
				if (TotalBounceCount > MaxTotalBounces)
				{
					UE_LOG(LogTemp, Warning, TEXT("Projectile %s exceeded max total bounces (%d), stopping simulation"), 
					       *GetNameSafe(ActorOwner), MaxTotalBounces);
					StopSimulating(BounceData.HitResult);
					return;
				}
				
				// Normal bounce case - apply proper bounce physics
				UE_LOG(LogTemp, Warning, TEXT("Normal bounce detected (distance: %.3f, bounce #%d), applying bounce physics"), 
				       BounceData.HitMinDistance, TotalBounceCount);
				
				// Move to the hit location
				NewLocation = ActorOwner->GetActorLocation() + BounceData.Direction * BounceData.HitMinDistance;
				NewTransform.SetLocation(NewLocation);
				ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
				
				// Apply bounce physics using the existing ComputeBounceResult method
				const FVector MoveDelta = BounceData.Direction * BounceData.HitMinDistance;
				Velocity = ComputeBounceResult(BounceData.HitResult, CurrentTimeTick, MoveDelta);
				
				// Reset corner bounce counter on normal bounce
				ConsecutiveCornerBounces = 0;
				
				UpdateComponentVelocity();
				
				// Check if velocity is still above simulation threshold after bounce
				if (IsVelocityUnderSimulationThreshold())
				{
					UE_LOG(LogTemp, Warning, TEXT("Normal bounce velocity too low, stopping simulation"));
					StopSimulating(BounceData.HitResult);
					return;
				}
				
				UE_LOG(LogTemp, Warning, TEXT("Normal bounce applied, new velocity: %s, total bounces: %d"), 
				       *Velocity.ToString(), TotalBounceCount);
			}
			else
			{
				// Far hit - likely missed intended target, stop simulation
				UE_LOG(LogTemp, Warning, TEXT("Far hit during bounce movement (distance: %.3f), stopping simulation"), 
				       BounceData.HitMinDistance);
				StopSimulating(BounceData.HitResult);
			}
		}
	}
}

bool UPawProjectileMovementComponent::IsAllAsyncSweepingCompleted() const
{
	return (MovementAsyncSweepData.SweepCount <= 0 && SlidingAsyncData.SweepCount <= 0 && BounceAsyncData.SweepCount <= 0);
}

bool UPawProjectileMovementComponent::FindCornerEscapeDirection(const FVector& CurrentLocation, FVector& OutEscapeDirection, float& OutEscapeDistance)
{
	if (!UpdatedComponent)
	{
		return false;
	}

	AActor* ActorOwner = UpdatedComponent->GetOwner();
	if (!IsValid(ActorOwner))
	{
		return false;
	}

	// Set up collision query parameters - exclude other projectiles
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(ActorOwner);
	QueryParams.bTraceComplex = false;
	
	// Add all other projectiles to ignore list
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const float ProjectileRadius = 5.0f; // Match projectile collision radius
	
	// Test escape directions in order of preference
	TArray<FVector> TestDirections = {
		FVector::UpVector,                    // Straight up (preferred)
		FVector(0, 0, 0.7f).GetSafeNormal(), // Slight upward angle
		FVector(1, 0, 0.5f).GetSafeNormal(), // Forward-up diagonal
		FVector(-1, 0, 0.5f).GetSafeNormal(),// Backward-up diagonal
		FVector(0, 1, 0.5f).GetSafeNormal(), // Right-up diagonal
		FVector(0, -1, 0.5f).GetSafeNormal(),// Left-up diagonal
		FVector::ForwardVector,               // Forward
		FVector::BackwardVector,              // Backward
		FVector::RightVector,                 // Right
		FVector::LeftVector                   // Left
	};

	// Test progressively larger escape distances
	TArray<float> TestDistances = { 15.0f, 25.0f, 35.0f, 50.0f };

	for (float TestDistance : TestDistances)
	{
		for (const FVector& Direction : TestDirections)
		{
			FVector TestLocation = CurrentLocation + Direction * TestDistance;
			
			// Check if this location is clear
			bool bLocationClear = !World->OverlapAnyTestByChannel(
				TestLocation,
				FQuat::Identity,
				ECC_WorldStatic,
				FCollisionShape::MakeSphere(ProjectileRadius),
				QueryParams
			);

			if (bLocationClear)
			{
				OutEscapeDirection = Direction;
				OutEscapeDistance = TestDistance;
				UE_LOG(LogTemp, Log, TEXT("Found clear escape direction: %s at distance %.3f"), 
				       *Direction.ToString(), TestDistance);
				return true;
			}
		}
	}

	// No clear escape direction found
	UE_LOG(LogTemp, Warning, TEXT("Failed to find clear escape direction from %s"), *CurrentLocation.ToString());
	return false;
}
