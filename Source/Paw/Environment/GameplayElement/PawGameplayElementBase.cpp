// Fill out your copyright notice in the Description page of Project Settings.


#include "PawGameplayElementBase.h"


// Sets default values
APawGameplayElementBase::APawGameplayElementBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void APawGameplayElementBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APawGameplayElementBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

