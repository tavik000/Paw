// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawFPSPlayer.h"
#include "PawPlayerSeeker.generated.h"

class USpotLightComponent;
class UPawFlashLightComponent;

UCLASS()
class PAW_API APawPlayerSeeker : public APawFPSPlayer
{
	GENERATED_BODY()

public:
	APawPlayerSeeker();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UPawFlashLightComponent> FlashLightComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USpotLightComponent> SpotLightComponent;
};
