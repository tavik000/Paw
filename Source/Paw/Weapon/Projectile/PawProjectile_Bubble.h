// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawProjectileBase.h"
#include "Paw/Environment/GameplayElement/Common/Interface/PawCollideBreakableInterface.h"
#include "PawProjectile_Bubble.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class APawBubbleHiderCapture;

UCLASS()
class PAW_API APawProjectile_Bubble : public APawProjectileBase, public IPawCollideBreakableInterface
{
	GENERATED_BODY()

public:
	APawProjectile_Bubble();
	virtual void Tick(float DeltaTime) override;
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                   FVector NormalImpulse, const FHitResult& Hit) override;

	UFUNCTION(BlueprintNativeEvent)
	void SpawnMasterField(FVector SpawnLocation, FRotator SpawnRotation);

	// IPawPoolableInterface
	virtual void OnActivateFromPool(UPawActorPool* InActorPool, const FVector& Location, const FRotator& Rotation, const FActorSpawnParameters& SpawnParameters) override;
	virtual void OnDeactivateFromPool() override;

protected:
	virtual void BeginPlay() override;
	virtual void Break_Implementation() override;
	virtual bool CanBeBreakByHider_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UNiagaraSystem> BreakEffectAsset;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraComponent> BreakEffect;

	UPROPERTY(EditAnywhere)
	float BreakEffectScale = 1.0f;

	UPROPERTY(EditDefaultsOnly)
	float BubbleLifeTime = 5.0f;

	UPROPERTY(EditAnywhere)
	USoundBase* BreakSound;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<APawBubbleHiderCapture> BubbleHiderCaptureClass;

	UPROPERTY(EditDefaultsOnly)
	float BubbleHeightOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly)
	float PushForce = 30.0f;

protected:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpawnBreakEffect();

private:
	void SelfBreak();
	void OnBreakEffectLoaded();
	void LoadBreakEffect();

private:
	FTimerHandle LifeCycleTimerHandle;
};
