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

void APawBubbleLight::Break()
{
	Super::Break();
	ServerDestroy();
}

void APawBubbleLight::ServerDestroy_Implementation()
{
	Destroy();
}

bool APawBubbleLight::ServerDestroy_Validate()
{
	return HasAuthority();
}

void APawBubbleLight::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

