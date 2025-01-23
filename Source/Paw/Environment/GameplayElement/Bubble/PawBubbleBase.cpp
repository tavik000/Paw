// Fill out your copyright notice in the Description page of Project Settings.


#include "PawBubbleBase.h"


APawBubbleBase::APawBubbleBase()
{
	PrimaryActorTick.bCanEverTick = true;
	BubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BubbleMesh"));
	BubbleMesh->SetupAttachment(RootComponent);
}

void APawBubbleBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawBubbleBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

