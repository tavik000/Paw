// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawPlayerBase.h"
#include "PawFPSPlayer.generated.h"

class UCameraComponent;

UCLASS()
class PAW_API APawFPSPlayer : public APawPlayerBase
{
	GENERATED_BODY()

public:
	APawFPSPlayer();

protected:
	virtual void BeginPlay() override;


	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void Move(const FInputActionValue& Value) override;
	virtual void Look(const FInputActionValue& Value) override;

	// FPS Arms Mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* ArmMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;
	
public:
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	USkeletalMeshComponent* GetArmMesh() const { return ArmMesh; }
	
	UFUNCTION(BlueprintCallable)
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
};
