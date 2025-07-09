// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawBubbleObjectCapture.h"
#include "Engine/Engine.h"
#include "PawBubbleHiderCapture.generated.h"

class APawPlayerHider;

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

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerCaptureHider(APawPlayerHider *Hider);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerReleaseHider();
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetHiderFloatingEnable(APawPlayerHider* Hider, bool bEnable);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAttachHiderToBubble(APawPlayerHider* Hider);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDetachHiderFromBubble(APawPlayerHider* Hider);

private:

	UFUNCTION()
	void OnCapturedHiderDestroy(AActor* DestroyedActor);
	
	UPROPERTY(Replicated)
	TWeakObjectPtr<APawPlayerHider> CapturedHider;
	
	// Store original movement mode to restore later
	TEnumAsByte<EMovementMode> OriginalMovementMode;
};
