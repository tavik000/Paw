// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawPlayerSeeker.h"

#include "Components/SpotLightComponent.h"
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

void APawPlayerSeeker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawPlayerSeeker::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

