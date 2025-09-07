// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.


#include "PawCharacterBase.h"


APawCharacterBase::APawCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	ACharacter::SetReplicateMovement(true);
	
	// Initialize team to None by default
	TeamId = ETeamId::None;
}

void APawCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void APawCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APawCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(APawCharacterBase, TeamId);
}

// ITeamableInterface Implementation
ETeamId APawCharacterBase::GetTeamId() const
{
	return TeamId;
}

void APawCharacterBase::SetTeamId(ETeamId NewTeamId)
{
	TeamId = NewTeamId;
}


