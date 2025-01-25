// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawFlashLightComponent.h"


UPawFlashLightComponent::UPawFlashLightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UPawFlashLightComponent::BeginPlay()
{
	Super::BeginPlay();
}


void UPawFlashLightComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

