// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "../../Core/Interface/ITeamableInterface.h"
#include "../../Core/Enum/ETeamId.h"
#include "Net/UnrealNetwork.h"
#include "PawCharacterBase.generated.h"

UCLASS()
class PAW_API APawCharacterBase : public ACharacter, public ITeamableInterface
{
	GENERATED_BODY()

public:
	APawCharacterBase();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Team")
	ETeamId GetTeamId() const;
	virtual ETeamId GetTeamId_Implementation() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Team")
	void SetTeamId(ETeamId NewTeamId);
	virtual void SetTeamId_Implementation(ETeamId NewTeamId);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Team")
	ETeamId TeamId;
};
