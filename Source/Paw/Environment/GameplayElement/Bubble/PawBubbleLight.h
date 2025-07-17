// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PawBubbleBase.h"
#include "Components/PointLightComponent.h"
#include "PawBubbleLight.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBubbleLightBreakSignature);

UCLASS()
class PAW_API APawBubbleLight : public APawBubbleBase
{
	GENERATED_BODY()

public:
	APawBubbleLight();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, BlueprintCallable)
	float GetLightAttenuationRadius() const { return PointLight->AttenuationRadius; }

public:
	UPROPERTY(BlueprintAssignable)
	FOnBubbleLightBreakSignature OnBubbleLightBreak;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Break_Implementation() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> LightBulbMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UPointLightComponent> PointLight;
};
