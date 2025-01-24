// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

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
