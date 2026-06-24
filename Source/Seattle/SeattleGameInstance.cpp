#include "SeattleGameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void USeattleGameInstance::Init()
{
    Super::Init();

    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS)
    {
        SessionInterface = OSS->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            CreateCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &USeattleGameInstance::OnCreateSessionComplete);
            FindCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &USeattleGameInstance::OnFindSessionsComplete);
            JoinCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &USeattleGameInstance::OnJoinSessionComplete);

            UE_LOG(LogTemp, Warning, TEXT("USeattleGameInstance::Init - OnlineSubsystem found"));

        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("USeattleGameInstance::Init - No OnlineSubsystem found"));
    }
}

void USeattleGameInstance::StartOrFindSession(APlayerController* RequestingPC)
{
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("StartOrFindSession: SessionInterface not valid"));
        return;
    }

    // Cache requesting PC
    PendingRequestingPC = RequestingPC;

    // If we're already hosting a session, do nothing
    FNamedOnlineSession* Existing = SessionInterface->GetNamedSession(NAME_GameSession);
    if (Existing != nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("StartOrFindSession: Already have session"));
        return;
    }

    // Try to find sessions first - if none found, create one
    FindGameSessions();
}

void USeattleGameInstance::CreateGameSession(APlayerController* RequestingPC)
{
    if (!SessionInterface.IsValid()) return;

    FOnlineSessionSettings Settings;
    Settings.NumPublicConnections = 2;
    Settings.bIsLANMatch = true; // use LAN for local/testing; change to false for online subsystems
    Settings.bShouldAdvertise = true;
    Settings.bUsesPresence = true;

    CreateCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateCompleteDelegate);

    const ULocalPlayer* LocalPlayer = RequestingPC ? RequestingPC->GetLocalPlayer() : nullptr;
    const int32 LocalUserNum = LocalPlayer ? LocalPlayer->GetControllerId() : 0;
    if (!SessionInterface->CreateSession(LocalUserNum, NAME_GameSession, Settings))
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateGameSession failed to start"));
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteDelegateHandle);
    }
}

void USeattleGameInstance::FindGameSessions()
{
    if (!SessionInterface.IsValid()) return;

    SessionSearch = MakeShareable(new FOnlineSessionSearch());
    SessionSearch->bIsLanQuery = true;
    SessionSearch->MaxSearchResults = 20;

    FindCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindCompleteDelegate);

    const ULocalPlayer* LocalPlayer = PendingRequestingPC.IsValid() ? PendingRequestingPC.Get()->GetLocalPlayer() : nullptr;
    const int32 LocalUserNum = LocalPlayer ? LocalPlayer->GetControllerId() : 0;
    if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
    {
        UE_LOG(LogTemp, Warning, TEXT("FindSessions failed to start"));
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteDelegateHandle);
    }
}

void USeattleGameInstance::JoinFoundSession(int32 Index)
{
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid()) return;
    if (SessionSearch->SearchResults.Num() <= Index) return;

    JoinCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinCompleteDelegate);
    const ULocalPlayer* LocalPlayer = PendingRequestingPC.IsValid() ? PendingRequestingPC.Get()->GetLocalPlayer() : nullptr;
    const int32 LocalUserNum = LocalPlayer ? LocalPlayer->GetControllerId() : 0;
    SessionInterface->JoinSession(LocalUserNum, NAME_GameSession, SessionSearch->SearchResults[Index]);
}

void USeattleGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteDelegateHandle);

    UE_LOG(LogTemp, Log, TEXT("OnCreateSessionComplete: %s success=%d"), *SessionName.ToString(), bWasSuccessful);

    if (bWasSuccessful && PendingRequestingPC.IsValid())
    {
        // Travel as listen server
        UWorld* World = GetWorld();
        if (World)
        {
            FString MapName = World->GetMapName();
            // Remove PIE prefix if present
            MapName.RemoveFromStart(World->StreamingLevelsPrefix);
            FString Cmd = FString::Printf(TEXT("open %s?listen"), *MapName);
            PendingRequestingPC->ConsoleCommand(Cmd);
        }
    }
}

void USeattleGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
    SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteDelegateHandle);
    UE_LOG(LogTemp, Log, TEXT("OnFindSessionsComplete: success=%d results=%d"), bWasSuccessful, SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0);

    if (bWasSuccessful && SessionSearch.IsValid() && SessionSearch->SearchResults.Num() > 0)
    {
        // join first available session
        JoinFoundSession(0);
    }
    else
    {
        // no sessions found: create one (become host)
        CreateGameSession(PendingRequestingPC.Get());
    }
}

void USeattleGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteDelegateHandle);
    UE_LOG(LogTemp, Log, TEXT("OnJoinSessionComplete: %s result=%d"), *SessionName.ToString(), (int)Result);

    if (PendingRequestingPC.IsValid())
    {
        FString ConnectInfo;
        if (SessionInterface->GetResolvedConnectString(SessionName, ConnectInfo))
        {
            PendingRequestingPC->ClientTravel(ConnectInfo, TRAVEL_Absolute);
        }
    }
}
