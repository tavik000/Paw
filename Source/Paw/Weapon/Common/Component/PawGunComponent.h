// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawWeaponComponent.h"
#include "PawGunComponent.generated.h"

class APawFPSPlayer;
/**
 * For FPS Player Only
 */
UCLASS()
class PAW_API UPawGunComponent : public UPawWeaponComponent
{
	GENERATED_BODY()

	UPawGunComponent();

public:
	virtual bool AttachWeapon(APawBattleCharacter* TargetCharacter) override;

	/** Make the weapon Fire a Projectile */
	UFUNCTION(BlueprintCallable, Category="Weapon")
	void Fire();

	UFUNCTION(Server, Reliable)
	void ServerSpawnProjectile(FVector SpawnLocation, FRotator SpawnRotation);

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class APawProjectileBase> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	USoundBase* FireSound;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	UAnimMontage* FireAnimation;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FVector MuzzleOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	float FireClosetDistance = 22.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	float FacingDownPitch = -20.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	float TooCloseAdjustOffset = -20.0f;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputMappingContext* FireMappingContext;

	/** Fire Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputAction* FireAction;

protected:
	UFUNCTION()
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintReadOnly)
	APawFPSPlayer* FPSPlayer;

	UPROPERTY(EditAnywhere)
	float FireCoolDown = 3.0f;

private:
	bool IsCoolDown = false;

	FTimerHandle FireCoolDownTimerHandle;

	void ResetCoolDown();
};
