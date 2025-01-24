// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawBubbleBase.h"
#include "Components/PointLightComponent.h"
#include "PawBubbleLight.generated.h"

UCLASS()
class PAW_API APawBubbleLight : public APawBubbleBase
{
	GENERATED_BODY()

public:
	APawBubbleLight();

protected:
	virtual void BeginPlay() override;

	virtual void Break_Implementation() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> LightBulbMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UPointLightComponent> PointLight;
	

public:
	virtual void Tick(float DeltaTime) override;
};
