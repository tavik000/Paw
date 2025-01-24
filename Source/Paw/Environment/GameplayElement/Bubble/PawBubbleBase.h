// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Paw/Environment/GameplayElement/PawGameplayElementBase.h"
#include "Paw/Environment/GameplayElement/Common/Interface/PawCollideBreakableInterface.h"
#include "PawBubbleBase.generated.h"

UENUM(BlueprintType)
enum class EPawSplineMovementType: uint8
{
	None,
	Loop,
	BackAndForth
};

UENUM(BlueprintType)
enum class EPawSplineMovementDirection: uint8
{
	Forward,
	Backward
};

UCLASS()
class PAW_API APawBubbleBase : public APawGameplayElementBase
                               , public IPawCollideBreakableInterface
{
	GENERATED_BODY()

public:
	APawBubbleBase();

protected:
	virtual void BeginPlay() override;

	void Activate();
	void Deactivate();


	virtual void Break_Implementation() override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> BubbleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USplineComponent> MovementPathSpline;

	UPROPERTY(EditAnywhere, Category = "Movement")
	EPawSplineMovementType MovementType = EPawSplineMovementType::None;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MovementSpeed = 0.0f;

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	bool IsActivated = false;

private:
	float CurrentDistance = 0.0f;
	EPawSplineMovementDirection MovementDirection = EPawSplineMovementDirection::Forward;
};
