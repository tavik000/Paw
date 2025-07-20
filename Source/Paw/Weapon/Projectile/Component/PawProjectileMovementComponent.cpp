// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/WorldSettings.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "EngineUtils.h"
#include "Paw/Weapon/Projectile/PawProjectileBase.h"

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

	// Initialize sliding hysteresis system
	LastSlidingStateChangeTime = 0.0f;
	bPreviousSlidingState = false;
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
		// Queue movement update instead of skipping tick entirely
		AddMovementToQueue(DeltaTime);
		// UE_LOG(LogTemp, Verbose, TEXT("Queued movement update for %s (DeltaTime: %.4f, Queue size: %d, bIsSliding: %d)"),
		//        *GetNameSafe(UpdatedComponent->GetOwner()), DeltaTime, QueuedUpdates.Num(), bIsSliding);

		// During sliding with zero friction, process queue more aggressively to reduce lag
		if (bIsSliding && FMath::IsNearlyZero(Friction) && QueuedUpdates.Num() >= 2)
		{
			// UE_LOG(LogTemp, Warning, TEXT("Processing queue early during zero-friction sliding to reduce lag"));
			ProcessQueuedMovements();
		}
		return;
	}

	// Process any queued movement updates first
	ProcessQueuedMovements();
	
	// Clean up expired collision cooldowns periodically
	static float LastCooldownCleanupTime = 0.0f;
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (CurrentTime - LastCooldownCleanupTime > 1.0f) // Clean up every second
	{
		CleanupExpiredCollisionCooldowns();
		LastCooldownCleanupTime = CurrentTime;
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
		ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // Seeker

		if (!AsyncSweepDelegate.IsBound())
		{
			AsyncSweepDelegate.BindUObject(this, &ThisClass::HandleMovementAsyncSweepResult);
		}
		FVector StartLocation = ActorLocation;
		FVector EndLocation = StartLocation + MoveDelta;
		MovementAsyncSweepData.SweepCount += AsyncSweepByObjectType(ActorOwner, EAsyncTraceType::Single,
		                                                            StartLocation, EndLocation,
		                                                            NewRotation,
		                                                            ObjectQueryParams, QueryParams,
		                                                            &AsyncSweepDelegate);
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
	// UE_LOG(LogTemp, Warning, TEXT(" HandleDeflection called for %s, SubTickTimeRemaining: %.3f"),
	//        *GetNameSafe(UpdatedComponent->GetOwner()), SubTickTimeRemaining);
	const FVector Normal = ConstrainNormalToPlane(Hit.Normal);

	// Multiple hits within very short time period?
	const bool bMultiHit = (PreviousHitTime < 1.f && Hit.Time <= UE_KINDA_SMALL_NUMBER);

	// if velocity still into wall (after HandleBlockingHit() had a chance to adjust), slide along wall
	constexpr float DotTolerance = 0.05f; // Increased from 0.01f to reduce sliding sensitivity
	constexpr float MinSlidingVelocity = 50.0f; // Minimum velocity to consider sliding

	// UE_LOG(LogTemp, Warning, TEXT("HandleDeflection: bMultiHit: %d, PreviousHitNormal: %s, Normal: %s, VelocityNormal: %s, Dot: %.3f, VelMag: %.1f, bIsSliding: %d, Friction: %.3f"),
	//        bMultiHit, *PreviousHitNormal.ToString(), *Normal.ToString(),
	//        *Velocity.GetSafeNormal().ToString(), (Velocity.GetSafeNormal() | Normal), Velocity.Size(), bIsSliding, Friction);

	const bool bIsGroundSurface = FMath::Abs(Normal.Z) > 0.7f;

	// If the previous hit normal is not valid, or if the current hit normal is not parallel to the previous hit normal,
	const bool bShouldResumeSliding = !bIsSliding && bMultiHit && FVector::Coincident(PreviousHitNormal, Normal) &&
		bIsGroundSurface;
	const bool bVelocityParallelToSurface = (Velocity.GetSafeNormal() | Normal) <= DotTolerance && Velocity.Size() >=
		MinSlidingVelocity;
	bool bNewSlidingState = bShouldResumeSliding || bVelocityParallelToSurface;

	// Apply hysteresis to prevent rapid sliding state changes
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float TimeSinceLastChange = CurrentTime - LastSlidingStateChangeTime;

	if (bNewSlidingState != bPreviousSlidingState)
	{
		// Only change state if enough time has passed since last change
		if (TimeSinceLastChange >= SlidingHysteresisTime)
		{
			bIsSliding = bNewSlidingState;
			LastSlidingStateChangeTime = CurrentTime;
			bPreviousSlidingState = bNewSlidingState;
			// UE_LOG(LogTemp, Warning, TEXT("Sliding state changed from %d to %d (hysteresis applied)"), 
			//        !bNewSlidingState, bNewSlidingState);
		}
		else
		{
			// Keep previous state due to hysteresis
			bIsSliding = bPreviousSlidingState;
			// UE_LOG(LogTemp, Verbose, TEXT("Sliding state change suppressed by hysteresis (%.3fs since last change)"), 
			//        TimeSinceLastChange);
		}
	}
	else
	{
		// State hasn't changed, just update current state
		bIsSliding = bNewSlidingState;
	}

	// UE_LOG(LogTemp, Warning, TEXT("Set bIsSliding to %d for %s, PreviousHitNormal: %s, Normal: %s, VelocityNormal: %s, Dot: %.3f"),
	//        bIsSliding, *GetNameSafe(UpdatedComponent->GetOwner()), *PreviousHitNormal.ToString(),
	//        *Normal.ToString(), *Velocity.GetSafeNormal().ToString(), (Velocity.GetSafeNormal() | Normal));

	if (bIsSliding)
	{
		const float FrameTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
		// UE_LOG(LogTemp, Warning, TEXT("SLIDING: Projectile %s: (Role: %d) Velocity: %s, Normal: %s, Friction: %.3f, FrameTime: %.3fms"),
		//        *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
		//        *Velocity.ToString(), *Normal.ToString(), Friction, FrameTime * 1000.0f);

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
			//adjust to move along new wall with improved Z velocity preservation
			const FVector OriginalVelocity = Velocity;
			Velocity = ComputeSlideVector(Velocity, 1.f, Normal, Hit);
			
			// Preserve more Z velocity when sliding, especially for projectile-projectile collisions
			const bool bIsHorizontalSurface = FMath::Abs(Normal.Z) > 0.7f; // Ground/ceiling surface
			if (bIsHorizontalSurface && FMath::Abs(OriginalVelocity.Z) > 0.1f)
			{
				// For horizontal surfaces, preserve some vertical momentum to prevent complete flattening
				const float ZPreservationFactor = FMath::IsNearlyZero(Friction) ? 0.9f : 0.5f;
				Velocity.Z = FMath::Lerp(Velocity.Z, OriginalVelocity.Z, ZPreservationFactor);
			}
			
			// UE_LOG(LogTemp, Warning,
			//        TEXT("Slide vector: Original %s -> Final %s (Z preservation applied: %d)"),
			//        *OriginalVelocity.ToString(), *Velocity.ToString(), bIsHorizontalSurface);
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
		ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // Seeker

		const FVector StartLocation = ActorOwner->GetActorLocation();
		const FVector EndLocation = StartLocation + MoveDelta;
		const FQuat NewRotation = ActorOwner->GetActorQuat() * MovementAsyncSweepData.RelativeQuat;

		// UE_LOG(LogTemp, Warning, TEXT(" Bounce Sweep for %s, Start %s, End %s, MoveDistance %.3f"),
		//        *GetNameSafe(ActorOwner), *StartLocation.ToString(), *EndLocation.ToString(),
		//        BounceAsyncData.MoveDistance);

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

	// Fast path for zero-friction sliding - skip async sweeps for better performance
	if (FMath::IsNearlyZero(Friction))
	{
		const float StartTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
		// UE_LOG(LogTemp, Warning, TEXT("SLIDING FAST PATH: Zero-friction sliding for %s at %.3fms"), 
		//        *GetNameSafe(ActorOwner), StartTime * 1000.0f);

		// Direct position update without async sweep
		FVector MoveDelta = Velocity * SubTickTimeRemaining;
		FTransform NewTransform = ActorOwner->GetActorTransform();
		NewTransform.SetLocation(NewTransform.GetLocation() + MoveDelta);
		ActorOwner->SetActorTransform(NewTransform);

		// Apply zero-friction sliding physics immediately
		const FVector HorizontalVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
		const FVector VerticalAcceleration = FVector(0.0f, 0.0f, GetGravityZ() * SubTickTimeRemaining);
		Velocity = HorizontalVelocity + FVector(0.0f, 0.0f, Velocity.Z) + VerticalAcceleration;
		Velocity = ConstrainDirectionToPlane(Velocity);

		const float EndTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
		const float ProcessingTime = (EndTime - StartTime) * 1000.0f;
		// UE_LOG(LogTemp, Warning, TEXT("FAST PATH COMPLETE: Processing time %.3fms, new velocity %s"), 
		//        ProcessingTime, *Velocity.ToString());

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

	// Standard async sweep path for non-zero friction
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
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1); // Seeker

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

	// Clear any queued movement updates when stopping simulation
	ClearMovementQueue();

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
	// UE_LOG(LogTemp, Warning,
	//        TEXT(" AsyncSweepByObjectType: %s, Start: %s, End: %s, Rot: %s, ObjectQueryParams: %d, Params: %s"),
	//        *Actor->GetName(), *Start.ToString(), *End.ToString(), *Rot.Rotator().ToString(),
	//        ObjectQueryParams.ObjectTypesToQuery, *Params.ToString());
	TotalSweepCount++;
	return TotalSweepCount;
}

void UPawProjectileMovementComponent::HandleMovementAsyncSweepResult(const FTraceHandle& TraceHandle, FTraceDatum& Data)
{
	// UE_LOG(LogTemp, Warning, TEXT(" HandleAsyncSweepResult HitCount: %d"), Data.OutHits.Num());
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
	// UE_LOG(LogTemp, Warning, TEXT(" HandleAsyncSweepCompleted: HitCount: %d, HitMinDistance: %f, Direction: %s, HitActor: %s"),
	//        MovementAsyncSweepData.HitCount, MovementAsyncSweepData.HitMinDistance, *MovementAsyncSweepData.Direction.ToString(),
	//        *GetNameSafe(MovementAsyncSweepData.HitResult.GetActor()));
	AActor* ActorOwner = UpdatedComponent ? UpdatedComponent->GetOwner() : NULL;
	// If we hit a trigger that destroyed us, abort.
	if (!CheckStillInWorld() || !IsValid(ActorOwner) || HasStoppedSimulation())
	{
		return;
	}

	auto& AsyncData = MovementAsyncSweepData;
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
		// UE_LOG(LogTemp, Warning, TEXT("Set bIsSliding to false, no hits found."));

		// Only calculate new velocity if events didn't change it during the movement update.
		if (Velocity == OldVelocity)
		{
			Velocity = ComputeVelocity(Velocity, CurrentTimeTick);
		}

		ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
		UpdateComponentVelocity();

		// Logging
		// UE_LOG(LogTemp, Warning,
		//        TEXT("Move Projectile %s: (Role: %d) Hit at %.3f, Location: %s, Velocity: %s, IsSliding: %d"),
		//        *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole(), AsyncData.HitMinDistance,
		//        *NewLocation.ToString(), *Velocity.ToString(), bIsSliding);
	}
	else
	{
		// UE_LOG(LogTemp, Warning, TEXT("AsyncData.HitCount: %d, HitMinDistance: %f, Direction: %s"),
		//        AsyncData.HitCount, AsyncData.HitMinDistance, *AsyncData.Direction.ToString());
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
			// UE_LOG(LogTemp, Warning, TEXT("Velocity: %s, OldVelocity: %s, HitTime: %.3f, CurrentTimeTick: %.3f"),
			//        *Velocity.ToString(), *OldVelocity.ToString(), HitTime, CurrentTimeTick);
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
		// UE_LOG(LogTemp, Warning, TEXT("OldNormal: %s, HitNormal: %s, ConstrainNormalToPlane(Hit.Normal): %s"),
		//        *AsyncData.OldHitNormal.ToString(), *Hit.Normal.ToString(),
		//        *ConstrainDirectionToPlane(Hit.Normal).ToString());
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
	// UE_LOG(LogTemp, Warning, TEXT(" HandleSlidingAsyncSweepCompleted: HitCount: %d, HitMinDistance: %f, Direction: %s, HitActor: %s"),
	//        SlidingAsyncData.HitCount, SlidingAsyncData.HitMinDistance, *SlidingAsyncData.Direction.ToString(),
	//        *GetNameSafe(SlidingAsyncData.HitResult.GetActor()));
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

		UE_LOG(LogTemp, Warning,
		       TEXT(" SlidingAsyncSweepCompleted but not stopped simulation, Hit: %s, Time: %.3f, RemainingTime: %.3f"),
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

		// Handle sliding physics based on friction value
		if (FMath::IsNearlyZero(Friction))
		{
			// Zero friction - preserve horizontal sliding velocity, only apply gravity to vertical component
			const FVector HorizontalVelocity = FVector(Velocity.X, Velocity.Y, 0.0f);
			const FVector VerticalAcceleration = FVector(0.0f, 0.0f, GetGravityZ() * SubTickTimeRemaining);

			// Maintain horizontal sliding momentum, only apply gravity to vertical
			Velocity = HorizontalVelocity + FVector(0.0f, 0.0f, Velocity.Z) + VerticalAcceleration;
			Velocity = ConstrainDirectionToPlane(Velocity);

			// UE_LOG(LogTemp, Warning,
			//        TEXT("Projectile %s: (Role: %d) Zero-friction sliding, preserved horizontal velocity: %s"),
			//        *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
			//        *Velocity.ToString());
		}
		else
		{
			// Non-zero friction - use complex physics calculation
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
				// UE_LOG(LogProjectileMovement, Warning,
				//        TEXT("Projectile %s: (Role: %d) Sliding along surface with friction %.3f, new velocity: %s"),
				//        *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
				//        Friction, *Velocity.ToString());
			}
			else
			{
				Velocity = PostTickVelocity;
				// UE_LOG(LogTemp, Warning,
				//        TEXT("Projectile %s: (Role: %d) Sliding along surface with friction %.3f (no surface force), new velocity: %s"),
				//        *GetNameSafe(UpdatedComponent->GetOwner()), (int32)UpdatedComponent->GetOwner()->GetLocalRole(),
				//        Friction, *Velocity.ToString());
			}
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
	// UE_LOG(LogTemp, Warning, TEXT(" HandleBounceAsyncSweepResult HitCount: %d"), Data.OutHits.Num());
	auto& AsyncData = BounceAsyncData;
	for (FHitResult Hit : Data.OutHits)
	{
		if (AsyncData.OldHitNormal == ConstrainDirectionToPlane(Hit.Normal))
		{
			// UE_LOG(LogTemp, Warning, TEXT(" Bounce Sweep skipping hit with same normal: %s"), *Hit.Normal.ToString());
			continue;
		}
		if (AsyncData.HitMinDistance > Hit.Distance)
		{
			// UE_LOG(LogTemp, Warning, TEXT(" Updating Bounce HitMinDistance: %.3f -> %.3f"),
			//        AsyncData.HitMinDistance, Hit.Distance);
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
	// UE_LOG(LogTemp, Warning, TEXT(" HandleBounceAsyncSweepCompleted: HitCount: %d, HitMinDistance: %f, Direction: %s, HitActor: %s"),
	//        BounceAsyncData.HitCount, BounceAsyncData.HitMinDistance, *BounceAsyncData.Direction.ToString(),
	//        *GetNameSafe(BounceAsyncData.HitResult.GetActor()));
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

		// UE_LOG(LogTemp, Warning, TEXT(" Bounce Projectile %s: (Role: %d) No hits, safe movement to %s"),
		//        *GetNameSafe(ActorOwner), (int32)ActorOwner->GetLocalRole(), *NewLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Hit during bounce sweep: HitCount: %d, HitMinDistance: %f, Direction: %s"),
		       BounceData.HitCount, BounceData.HitMinDistance, *BounceData.Direction.ToString());


		// Check if we hit another projectile - don't trigger corner bounce for projectile-projectile collisions
		AActor* HitActor = BounceData.HitResult.GetActor();
		const bool bHitProjectile = HitActor && HitActor->IsA<APawProjectileBase>();
		
		// Detect corner bounce (immediate hit during bounce movement) - but exclude projectile collisions
		if (constexpr float CornerDetectionDistance = 5.0f; BounceData.HitMinDistance < CornerDetectionDistance && !bHitProjectile)
		{
			// Check for infinite bounce protection
			const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

			if (const bool bRecentCornerBounce = (CurrentTime - LastCornerBounceTime) < CornerBounceTimeoutWindow)
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
				UE_LOG(LogTemp, Warning,
				       TEXT(
					       "Projectile %s exceeded max consecutive corner bounces (%d), stopping simulation to prevent infinite loop"
				       ),
				       *GetNameSafe(ActorOwner), MaxConsecutiveCornerBounces);
				StopSimulating(BounceData.HitResult);
				return;
			}

			// Corner bounce detected - use intelligent escape logic
			UE_LOG(LogTemp, Warning, TEXT("Corner bounce detected (distance: %.3f, bounce #%d) "),
			       BounceData.HitMinDistance, ConsecutiveCornerBounces);

			auto& AsyncData = MovementAsyncSweepData;
			NewTransform = ActorOwner->GetActorTransform();
			NewLocation = NewTransform.GetLocation() - AsyncData.Direction * AsyncData.HitMinDistance;
			NewRotation = NewTransform.GetRotation() * AsyncData.RelativeQuat;
			NewTransform.SetLocation(NewLocation);
			NewTransform.SetRotation(NewRotation);

			ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
			UpdateComponentVelocity();
		}
		else if (bHitProjectile)
		{
			// Check collision cooldown to prevent immediate re-collision
			if (IsProjectileCollisionOnCooldown(HitActor))
			{
				UE_LOG(LogTemp, Warning, TEXT("Projectile-projectile collision on cooldown, ignoring collision with %s"), 
				       *GetNameSafe(HitActor));
				
				// Treat as no collision - continue movement without applying collision physics
				ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
				UpdateComponentVelocity();
				return;
			}
			
			// Enhanced logging for projectile-projectile collision
			UE_LOG(LogTemp, Warning, TEXT("=== PROJECTILE-PROJECTILE COLLISION ==="));
			UE_LOG(LogTemp, Warning, TEXT("This projectile: %s, Velocity: %s"), 
			       *GetNameSafe(ActorOwner), *Velocity.ToString());
			UE_LOG(LogTemp, Warning, TEXT("Other projectile: %s"), *GetNameSafe(HitActor));
			UE_LOG(LogTemp, Warning, TEXT("Collision distance: %.6f, Normal: %s"), 
			       BounceData.HitMinDistance, *BounceData.HitResult.Normal.ToString());
			
			// Projectile-projectile collision - apply momentum exchange physics
			UE_LOG(LogTemp, Warning, TEXT("Applying momentum exchange physics..."));
			
			// Move to collision point
			NewLocation = ActorOwner->GetActorLocation() + BounceData.Direction * BounceData.HitMinDistance;
			NewTransform.SetLocation(NewLocation);
			ActorOwner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
			
			// Apply momentum exchange between projectiles
			if (APawProjectileBase* OtherProjectile = Cast<APawProjectileBase>(HitActor))
			{
				if (UPawProjectileMovementComponent* OtherMovement = OtherProjectile->GetProjectileMovement())
				{
					// Simple elastic collision physics - exchange velocities
					const FVector ThisVelocity = Velocity;
					const FVector OtherVelocity = OtherMovement->Velocity;
					
					// Apply collision normal for realistic bounce direction
					const FVector CollisionNormal = BounceData.HitResult.Normal;
					
					// Calculate relative velocity
					const FVector RelativeVelocity = ThisVelocity - OtherVelocity;
					const float VelAlongNormal = FVector::DotProduct(RelativeVelocity, CollisionNormal);
					
					// Don't resolve if velocities are separating
					if (VelAlongNormal > 0)
					{
						// Assuming equal mass, apply simple elastic collision
						const float Restitution = 0.8f; // Slight energy loss
						const FVector VelocityChange = -(1 + Restitution) * VelAlongNormal * CollisionNormal;
						
						Velocity = ThisVelocity + VelocityChange;
						OtherMovement->Velocity = OtherVelocity - VelocityChange;
						
						// Ensure both projectiles maintain minimum velocities
						Velocity = LimitVelocity(Velocity);
						OtherMovement->Velocity = OtherMovement->LimitVelocity(OtherMovement->Velocity);
						
						UE_LOG(LogTemp, Warning, TEXT("Momentum exchange successful:"));
						UE_LOG(LogTemp, Warning, TEXT("  This projectile: %s -> %s"), 
						       *ThisVelocity.ToString(), *Velocity.ToString());
						UE_LOG(LogTemp, Warning, TEXT("  Other projectile: %s -> %s"), 
						       *OtherVelocity.ToString(), *OtherMovement->Velocity.ToString());
						UE_LOG(LogTemp, Warning, TEXT("  Relative velocity along normal: %.3f"), VelAlongNormal);
						UE_LOG(LogTemp, Warning, TEXT("  Restitution applied: %.3f"), Restitution);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("Projectiles already separating (VelAlongNormal: %.3f), no momentum exchange needed"), VelAlongNormal);
					}
					
					// CRITICAL: Physically separate overlapping projectiles to prevent infinite collision loops
					if (BounceData.HitMinDistance <= 0.001f) // Overlapping or extremely close
					{
						const FVector BounceCollisionNormal = BounceData.HitResult.Normal;
						const float MinSeparationDistance = 10.0f; // Minimum separation in Unreal units
						
						// Calculate separation positions
						const FVector ThisNewLocation = ActorOwner->GetActorLocation() - BounceCollisionNormal * (MinSeparationDistance * 0.5f);
						const FVector OtherNewLocation = OtherProjectile->GetActorLocation() + BounceCollisionNormal * (MinSeparationDistance * 0.5f);
						
						// Apply separation
						ActorOwner->SetActorLocation(ThisNewLocation, false, nullptr, ETeleportType::TeleportPhysics);
						OtherProjectile->SetActorLocation(OtherNewLocation, false, nullptr, ETeleportType::TeleportPhysics);
						
						UE_LOG(LogTemp, Warning, TEXT("Applied physical separation:"));
						UE_LOG(LogTemp, Warning, TEXT("  This projectile moved to: %s"), *ThisNewLocation.ToString());
						UE_LOG(LogTemp, Warning, TEXT("  Other projectile moved to: %s"), *OtherNewLocation.ToString());
						UE_LOG(LogTemp, Warning, TEXT("  Separation distance: %.3f"), MinSeparationDistance);
					}
					
					// Add collision cooldown to prevent immediate re-collision between these same projectiles
					AddProjectileCollisionCooldown(OtherProjectile);
					UE_LOG(LogTemp, Log, TEXT("Added collision cooldown between projectiles %s and %s"), 
					       *GetNameSafe(ActorOwner), *GetNameSafe(OtherProjectile));
				}
			}
			
			// Reset corner bounce counter since this was a projectile collision
			ConsecutiveCornerBounces = 0;
			UpdateComponentVelocity();
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
				UE_LOG(LogTemp, Warning,
				       TEXT("Normal bounce detected (distance: %.3f, bounce #%d), applying bounce physics"),
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
	return (MovementAsyncSweepData.SweepCount <= 0 && SlidingAsyncData.SweepCount <= 0 && BounceAsyncData.SweepCount <=
		0);
}

void UPawProjectileMovementComponent::AddMovementToQueue(float DeltaTime)
{
	if (!UpdatedComponent)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	// Clean up old entries before adding new ones
	QueuedUpdates.RemoveAll([CurrentTime](const FQueuedMovementUpdate& Update)
	{
		return (CurrentTime - Update.TimeStamp) > MaxQueueTime;
	});

	// Enforce queue size limit
	if (QueuedUpdates.Num() >= MaxQueuedUpdates)
	{
		// Remove oldest entry to make room
		QueuedUpdates.RemoveAt(0);
		UE_LOG(LogTemp, Verbose, TEXT("Queue overflow, removed oldest movement update"));
	}

	// Add new movement update to queue
	FQueuedMovementUpdate NewUpdate(DeltaTime, Velocity, CurrentTime);
	QueuedUpdates.Add(NewUpdate);

	UE_LOG(LogTemp, Verbose, TEXT("Added movement to queue: DeltaTime=%.4f, Velocity=%s, QueueSize=%d"),
	       DeltaTime, *Velocity.ToString(), QueuedUpdates.Num());
}

void UPawProjectileMovementComponent::ProcessQueuedMovements()
{
	if (QueuedUpdates.Num() == 0)
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("Processing %d queued movement updates"), QueuedUpdates.Num());

	// Accumulate all queued delta times and apply movement
	float AccumulatedDeltaTime = 0.0f;
	FVector AccumulatedVelocityChange = FVector::ZeroVector;

	for (const FQueuedMovementUpdate& Update : QueuedUpdates)
	{
		AccumulatedDeltaTime += Update.DeltaTime;

		// Apply physics updates (gravity, acceleration) for each queued frame
		const FVector VelocityChange = ComputeAcceleration(Update.StartVelocity, Update.DeltaTime) * Update.DeltaTime;
		AccumulatedVelocityChange += VelocityChange;
	}

	// Apply accumulated velocity changes
	Velocity += AccumulatedVelocityChange;
	Velocity = LimitVelocity(Velocity);

	UE_LOG(LogTemp, Verbose, TEXT("Processed queued movements: AccumulatedDeltaTime=%.4f, FinalVelocity=%s"),
	       AccumulatedDeltaTime, *Velocity.ToString());

	// Clear the queue after processing
	ClearMovementQueue();
}

void UPawProjectileMovementComponent::ClearMovementQueue()
{
	if (QueuedUpdates.Num() > 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Clearing movement queue (%d entries)"), QueuedUpdates.Num());
		QueuedUpdates.Empty();
	}
}

void UPawProjectileMovementComponent::AddProjectileCollisionCooldown(AActor* OtherProjectile)
{
	if (!IsValid(OtherProjectile))
	{
		return;
	}
	
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	const float CurrentTime = World->GetTimeSeconds();
	const float CooldownEndTime = CurrentTime + ProjectileCollisionCooldownTime;
	
	// Clean up expired cooldowns before adding new ones
	CleanupExpiredCollisionCooldowns();
	
	// Check if this projectile already has a cooldown entry
	for (FProjectileCollisionCooldown& Cooldown : ProjectileCollisionCooldowns)
	{
		if (Cooldown.OtherProjectile.Get() == OtherProjectile)
		{
			// Update existing cooldown
			Cooldown.CooldownEndTime = CooldownEndTime;
			UE_LOG(LogTemp, Verbose, TEXT("Updated collision cooldown for projectile %s, ends at %.3f"), 
			       *GetNameSafe(OtherProjectile), CooldownEndTime);
			return;
		}
	}
	
	// Enforce memory limits
	if (ProjectileCollisionCooldowns.Num() >= MaxCollisionCooldowns)
	{
		// Remove oldest cooldown to make room
		ProjectileCollisionCooldowns.RemoveAt(0);
		UE_LOG(LogTemp, Verbose, TEXT("Collision cooldown overflow, removed oldest entry"));
	}
	
	// Add new cooldown entry
	ProjectileCollisionCooldowns.Add(FProjectileCollisionCooldown(OtherProjectile, CooldownEndTime));
	UE_LOG(LogTemp, Verbose, TEXT("Added collision cooldown for projectile %s, ends at %.3f (array size: %d)"), 
	       *GetNameSafe(OtherProjectile), CooldownEndTime, ProjectileCollisionCooldowns.Num());
}

bool UPawProjectileMovementComponent::IsProjectileCollisionOnCooldown(AActor* OtherProjectile) const
{
	if (!IsValid(OtherProjectile))
	{
		return false;
	}
	
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}
	
	const float CurrentTime = World->GetTimeSeconds();
	
	// Check if this projectile has an active cooldown
	for (const FProjectileCollisionCooldown& Cooldown : ProjectileCollisionCooldowns)
	{
		if (Cooldown.OtherProjectile.Get() == OtherProjectile)
		{
			const bool bOnCooldown = CurrentTime < Cooldown.CooldownEndTime;
			if (bOnCooldown)
			{
				const float TimeRemaining = Cooldown.CooldownEndTime - CurrentTime;
				UE_LOG(LogTemp, Verbose, TEXT("Projectile %s collision on cooldown, %.3f seconds remaining"), 
				       *GetNameSafe(OtherProjectile), TimeRemaining);
			}
			return bOnCooldown;
		}
	}
	
	return false;
}

void UPawProjectileMovementComponent::CleanupExpiredCollisionCooldowns()
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	const float CurrentTime = World->GetTimeSeconds();
	const int32 InitialCount = ProjectileCollisionCooldowns.Num();
	
	// Remove expired cooldowns
	ProjectileCollisionCooldowns.RemoveAll([CurrentTime](const FProjectileCollisionCooldown& Cooldown)
	{
		const bool bExpired = CurrentTime >= Cooldown.CooldownEndTime || !Cooldown.OtherProjectile.IsValid();
		return bExpired;
	});
	
	const int32 RemovedCount = InitialCount - ProjectileCollisionCooldowns.Num();
	if (RemovedCount > 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Cleaned up %d expired collision cooldowns (remaining: %d)"), 
		       RemovedCount, ProjectileCollisionCooldowns.Num());
	}
}
