// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawBubbleBase.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

APawBubbleBase::APawBubbleBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	AActor::SetReplicateMovement(true);
	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	SetRootComponent(BubbleMesh);
	BubbleMesh->SetupAttachment(RootComponent);
	BubbleMesh->SetIsReplicated(true);
	MovementPathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("MovementPathSpline"));
	MovementPathSpline->SetupAttachment(SceneComponent);
	MovementPathSpline->SetWorldLocation(GetActorLocation());
}

void APawBubbleBase::BeginPlay()
{
	Super::BeginPlay();
	Activate();
	LoadBreakEffect();
}

void APawBubbleBase::Activate()
{
	IsActivated = true;
}

void APawBubbleBase::Deactivate()
{
	IsActivated = false;
}

void APawBubbleBase::Break_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}
	IPawCollideBreakableInterface::Break_Implementation();
	Deactivate();
	MulticastSpawnBreakEffect();
}


void APawBubbleBase::LoadBreakEffect()
{
	UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(BreakEffectAsset.ToSoftObjectPath(),
	                                                             FStreamableDelegate::CreateUObject(
		                                                             this, &APawBubbleBase::OnBreakEffectLoaded));
}

void APawBubbleBase::MulticastSpawnBreakEffect_Implementation()
{
	BreakEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BreakEffectAsset.Get(), GetActorLocation(),
	                                                             GetActorRotation(),
	                                                             FVector::One() * BreakEffectScale, true, true, ENCPoolMethod::AutoRelease,
	                                                             true);
}

void APawBubbleBase::OnBreakEffectLoaded()
{
}

void APawBubbleBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority())
	{
		return;
	}

	if (!IsValid(MovementPathSpline))
	{
		return;
	}

	if (!IsActivated)
	{
		return;
	}

	if (MovementType == EPawSplineMovementType::None)
	{
		return;
	}

	float SplineLength = MovementPathSpline->GetSplineLength();
	if (SplineLength <= 0.0f)
	{
		return;
	}

	switch (MovementType)
	{
	case EPawSplineMovementType::None:
		break;
	case EPawSplineMovementType::Loop:
		CurrentDistance += MovementSpeed * DeltaTime;
		if (CurrentDistance >= SplineLength)
		{
			CurrentDistance = 0.0f;
		}
		break;
	case EPawSplineMovementType::BackAndForth:
		if (MovementDirection == EPawSplineMovementDirection::Forward)
		{
			CurrentDistance += MovementSpeed * DeltaTime;
			if (CurrentDistance >= SplineLength)
			{
				CurrentDistance = SplineLength;
				MovementDirection = EPawSplineMovementDirection::Backward;
			}
		}
		else
		{
			CurrentDistance -= MovementSpeed * DeltaTime;
			if (CurrentDistance <= 0.0f)
			{
				CurrentDistance = 0.0f;
				MovementDirection = EPawSplineMovementDirection::Forward;
			}
		}
		break;
	}

	if (IsValid(MovementPathSpline))
	{
		FVector NewLocation = MovementPathSpline->GetLocationAtDistanceAlongSpline(
			CurrentDistance, ESplineCoordinateSpace::World);
		SetActorLocation(NewLocation);
	}
}
