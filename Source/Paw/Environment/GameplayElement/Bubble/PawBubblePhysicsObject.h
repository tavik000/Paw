// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawBubbleBase.h"
#include "PawBubblePhysicsObject.generated.h"

UCLASS()
class PAW_API APawBubblePhysicsObject : public APawBubbleBase
{
	GENERATED_BODY()

public:
	APawBubblePhysicsObject();

protected:
	virtual void BeginPlay() override;

	virtual void Break_Implementation() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> PhysicsObjectMesh;
	
public:
	virtual void Tick(float DeltaTime) override;
};
