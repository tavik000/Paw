// ©2025 Key. All rights reserved. "Project Paw" and all related assets, trademarks, and materials are the intellectual property of Key. Unauthorized reproduction, distribution, or use of any content from this game is strictly prohibited.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "PawMultiplayerSessionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FServerCreateDelegate, bool, WasSuccessful);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FServerJoinDelegate, bool, WasSuccessful);

/**
 * 
 */
UCLASS()
class PAW_API UPawMultiplayerSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UPawMultiplayerSessionSubsystem();
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;

	UFUNCTION(BlueprintCallable)
	void CreateServer(FString ServerName);
	UFUNCTION(BlueprintCallable)
	void FindServer(FString ServerName);

	IOnlineSessionPtr SessionInterface;

public:
	UPROPERTY(BlueprintAssignable)
	FServerCreateDelegate ServerCreateDel;
	UPROPERTY(BlueprintAssignable)
	FServerJoinDelegate ServerJoinDel;

protected:
	UPROPERTY(BlueprintReadWrite)
	FString GameMapPath;

private:
	void OnCreateSessionComplete(FName ToCreateSessionName, bool WasSuccessful);
	void OnDestroySessionComplete(FName ToDestroySessionName, bool WasSuccessful);
	void OnFindSessionsComplete(bool WasSuccessful);
	void OnJoinSessionComplete(FName ToJoinSessionName, EOnJoinSessionCompleteResult::Type Result);

private:
	bool ShouldCreateServerAfterDestroy = false;
	FString DestroyServerName;
	FString ServerNameToFind;
	FName MySessionName;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
};
