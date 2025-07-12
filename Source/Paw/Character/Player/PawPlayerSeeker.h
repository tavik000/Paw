// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "PawFPSPlayer.h"
#include "PawPlayerSeeker.generated.h"

class USpotLightComponent;
class UPawFlashLightComponent;
class UPawLightDetectionSubsystem;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	
	UFUNCTION(Client, Reliable, Category = "UI")
	void Client_CreateHUD();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UPawFlashLightComponent> FlashLightComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USpotLightComponent> SpotLightComponent;
	
	// === UI System ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

private:
	// === UI Helper Functions ===
	void SetupHUDWidget(APlayerController* PC);
	
	void ConfigureHealthBar();

	// === Spotlight Detection ===
	void RegisterWithLightDetectionSubsystem();
	void UnregisterFromLightDetectionSubsystem();
	
	// === UI System ===
	TObjectPtr<UUserWidget> HUD;

	// === Spotlight Detection ===
	TObjectPtr<UPawLightDetectionSubsystem> LightDetectionSubsystem;
};
