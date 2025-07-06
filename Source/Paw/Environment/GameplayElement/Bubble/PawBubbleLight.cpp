// Fill out your copyright notice in the Description page of Project Settings.


#include "PawBubbleLight.h"

#include "Paw/Core/Systems/PawLightDetectionSubsystem.h"


APawBubbleLight::APawBubbleLight()
{
	PrimaryActorTick.bCanEverTick = true;
	LightBulbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LightBulbMesh"));
	LightBulbMesh->SetupAttachment(BubbleMesh);
	PointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PointLight"));
	PointLight->SetupAttachment(LightBulbMesh);
}

void APawBubbleLight::BeginPlay()
{
	Super::BeginPlay();

	// register to LightDetectionSubsystem
	if (HasAuthority())
	{
		if (UPawLightDetectionSubsystem* LightDetectionSubsystem = GetWorld()->GetSubsystem<
			UPawLightDetectionSubsystem>(); IsValid(LightDetectionSubsystem))
		{
			{
				LightDetectionSubsystem->RegisterBubbleLight(this, PointLight);
			}
		}
	}
}

void APawBubbleLight::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (HasAuthority())
	{
		if (UPawLightDetectionSubsystem* LightDetectionSubsystem = GetWorld()->GetSubsystem<
			UPawLightDetectionSubsystem>(); IsValid(LightDetectionSubsystem))
		{
			LightDetectionSubsystem->UnregisterBubbleLight(this);
		}
	}
}


void APawBubbleLight::Break_Implementation()
{
	Super::Break_Implementation();
	if (HasAuthority())
	{
		OnBubbleLightBreak.Broadcast();
		Destroy();
	}
}

void APawBubbleLight::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
