// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PawFlashLightComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PAW_API UPawFlashLightComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UPawFlashLightComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	
};
