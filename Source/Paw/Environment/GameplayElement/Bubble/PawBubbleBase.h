// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Paw/Environment/GameplayElement/PawGameplayElementBase.h"
#include "PawBubbleBase.generated.h"

UCLASS()
class PAW_API APawBubbleBase : public APawGameplayElementBase
{
	GENERATED_BODY()

public:
	APawBubbleBase();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> BubbleMesh;
	
public:
	virtual void Tick(float DeltaTime) override;
};
