// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawPlayerSeeker.h"

#include "Blueprint/UserWidget.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Paw/Character/Common/Component/PawFlashLightComponent.h"
#include "Paw/Core/Enum/ETeamId.h"


APawPlayerSeeker::APawPlayerSeeker()
{
	FlashLightComponent = CreateDefaultSubobject<UPawFlashLightComponent>(TEXT("FlashLightComponent"));
	FlashLightComponent->SetupAttachment(GetArmMesh());
	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLightComponent"));
	SpotLightComponent->SetupAttachment(FlashLightComponent);

	TeamId = ETeamId::Seeker;
}

void APawPlayerSeeker::BeginPlay()
{
	Super::BeginPlay();
	
	// Create HUD for player-controlled pawns
	if (IsLocallyControlled())
	{
		Client_CreateHUD();
	}
}

void APawPlayerSeeker::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void APawPlayerSeeker::SetupHUDWidget(APlayerController* PC)
{
	HUD = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
	if (!IsValid(HUD))
	{
		return;
	}

	// Add widget to viewport
	HUD->AddToViewport();
	HUD->SetVisibility(ESlateVisibility::Visible);

	// Configure child widgets
	ConfigureHealthBar();
}

void APawPlayerSeeker::ConfigureHealthBar()
{
	if (!IsValid(HUD))
	{
		return;
	}

	// Call Blueprint Custom Event to set target Pawn for HealthBar
	UFunction* DeleteHpBarFunction = HUD->GetClass()->FindFunctionByName(TEXT("DeleteHpBar"));
	if (!IsValid(DeleteHpBarFunction))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("APawPlayerSeeker::ConfigureHealthBar - DeleteHpBar function not found in HUD widget class"));
		return;
	}

	HUD->ProcessEvent(DeleteHpBarFunction, nullptr);
}

void APawPlayerSeeker::Client_CreateHUD_Implementation()
{
	// Early validation - only create HUD for player-controlled pawns
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!IsValid(PC))
	{
		return;
	}

	// Early validation - check if HUD widget class is configured
	if (!IsValid(HUDWidgetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("HUDWidgetClass is not set for %s"), *GetName());
		return;
	}

	// Setup HUD widget and configure child widgets
	SetupHUDWidget(PC);
}

void APawPlayerSeeker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawPlayerSeeker::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}
