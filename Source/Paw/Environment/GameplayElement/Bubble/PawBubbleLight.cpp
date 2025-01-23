// Fill out your copyright notice in the Description page of Project Settings.


#include "PawBubbleLight.h"


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
	
}

void APawBubbleLight::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

