#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "SeattleGameInstance.generated.h"

class APlayerController;

UCLASS()
class SEATTLE_API USeattleGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    /** Called by UI / HUD when a local player presses Start - will create session if first, otherwise join */
    UFUNCTION(BlueprintCallable, Category = "Session")
    void StartOrFindSession(APlayerController* RequestingPC);

protected:
    // Session delegates
    void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
    void OnFindSessionsComplete(bool bWasSuccessful);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    // Helpers
    void CreateGameSession(APlayerController* RequestingPC);
    void FindGameSessions();
    void JoinFoundSession(int32 Index = 0);

    // Online subsystem pointers
    IOnlineSessionPtr SessionInterface;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    // Delegate handles
    FOnCreateSessionCompleteDelegate CreateCompleteDelegate;
    FDelegateHandle CreateCompleteDelegateHandle;

    FOnFindSessionsCompleteDelegate FindCompleteDelegate;
    FDelegateHandle FindCompleteDelegateHandle;

    FOnJoinSessionCompleteDelegate JoinCompleteDelegate;
    FDelegateHandle JoinCompleteDelegateHandle;

    // Cached requesting controller
    TWeakObjectPtr<APlayerController> PendingRequestingPC;
};
