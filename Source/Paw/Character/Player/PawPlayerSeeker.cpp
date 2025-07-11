// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawPlayerSeeker.h"
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
	
}

void APawPlayerSeeker::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	// Notify all hiders about the new seeker after possession is complete
	if (HasAuthority())
	{
		if (AGameStateBase* GameState = GetWorld()->GetGameState(); IsValid(GameState))
		{
			UFunction* MulticastFunc = GameState->FindFunction(FName("Multicast_NotifyPlayerConvertedToSeeker"));
			if (MulticastFunc)
			{
				GameState->ProcessEvent(MulticastFunc, nullptr);
				UE_LOG(LogTemp, Log, TEXT("PawPlayerSeeker: Notified all hiders via Game State in PossessedBy"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("PawPlayerSeeker: Multicast function not found in Game State"));
			}
		}
	}
}

void APawPlayerSeeker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawPlayerSeeker::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

