// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawBubbleObjectCapture.h"
#include "PawBubbleHiderCapture.generated.h"

class APawCharacter;

UCLASS()
class PAW_API APawBubbleHiderCapture : public APawBubbleObjectCapture
{
	GENERATED_BODY()

public:
	APawBubbleHiderCapture();

protected:
	virtual void BeginPlay() override;

	virtual void Break_Implementation() override;

public:
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void CaptureHider(APawCharacter *Hider);

	UFUNCTION(BlueprintCallable)
	void ReleaseHider();

private:
	TWeakObjectPtr<APawCharacter> CapturedHider;
};
