// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawProjectileBase.h"
#include "PawProjectile_Bubble.generated.h"

class APawBubbleHiderCapture;

UCLASS()
class PAW_API APawProjectile_Bubble : public APawProjectileBase
{
	GENERATED_BODY()

public:
	APawProjectile_Bubble();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> BubbleMesh;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;


	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<APawBubbleHiderCapture> BubbleHiderCaptureClass;

	UPROPERTY(EditDefaultsOnly)
	float BubbleHeightOffset = 20.0f;
	
};
