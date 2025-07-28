// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawWeaponComponent.h"
#include "PawGunComponent.generated.h"

class UPawActorPool;
class APawFPSPlayer;
/**
 * For FPS Player Only
 */
UCLASS()
class PAW_API UPawGunComponent : public UPawWeaponComponent
{
	GENERATED_BODY()

public:
	UPawGunComponent();
	virtual bool AttachWeapon(APawBattleCharacter* TargetCharacter) override;

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void Fire();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class APawProjectileBase> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFX")
	USoundBase* FireSound;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFX")
	USoundBase* DryFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	UAnimMontage* FireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	float MuzzleOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	float FireClosetDistance = 22.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	float FacingDownPitch = -20.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	float TooCloseAdjustOffset = -20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputMappingContext* FireMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputAction* FireAction;

	UPROPERTY(BlueprintReadOnly)
	APawFPSPlayer* FPSPlayer;

	UPROPERTY(EditAnywhere)
	float FireCoolDown = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Actor Pool")
	TObjectPtr<UPawActorPool> ProjectilePool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Pool", meta = (ClampMin = "1"))
	int32 PrewarmCount = 3;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSpawnProjectile(FVector SpawnLocation, FRotator SpawnRotation);

private:
	void ResetCoolDown();

private:
	bool IsCoolDown = false;
	FTimerHandle FireCoolDownTimerHandle;
};
