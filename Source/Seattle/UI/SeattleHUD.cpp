#include "SeattleHUD.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.h"
#include "EndScreenWidget.h"
#include "HealthBarWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "SeattlePlayerController.h"
#include "SeattleGameMode.h"

ASeattleHUD::ASeattleHUD()
{
}

void ASeattleHUD::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (MainMenuWidgetClass)
    {
        if (PC)
        {
            MainMenuWidget = CreateWidget<UMainMenuWidget>(PC, MainMenuWidgetClass);
        }
        else
        {
            MainMenuWidget = CreateWidget<UMainMenuWidget>(GetWorld(), MainMenuWidgetClass);
        }

        if (MainMenuWidget)
        {
            MainMenuWidget->AddToViewport(10);
            MainMenuWidget->OnStartPressed.AddDynamic(this, &ASeattleHUD::HideMainMenu);
            // set input to UI only and show mouse cursor while main menu is visible
            if (PC)
            {
                PC->bShowMouseCursor = true;
                FInputModeUIOnly InputMode;
                PC->SetInputMode(InputMode);
                //PC->SetPause(true);
            }
            ShowMainMenu();
            UE_LOG(LogTemp, Warning, TEXT("IsInViewport = %d"), MainMenuWidget->IsInViewport());
            UE_LOG(LogTemp, Warning, TEXT("Visibility = %d"), (int32)MainMenuWidget->GetVisibility());
        }
    }

    // create health bars but don't add yet
    if (HealthBarWidgetClass)
    {
        if (PC)
        {
            PlayerHealthWidget = CreateWidget<UHealthBarWidget>(PC, HealthBarWidgetClass);
            OpponentHealthWidget = CreateWidget<UHealthBarWidget>(PC, HealthBarWidgetClass);
        }
        else
        {
            PlayerHealthWidget = CreateWidget<UHealthBarWidget>(GetWorld(), HealthBarWidgetClass);
            OpponentHealthWidget = CreateWidget<UHealthBarWidget>(GetWorld(), HealthBarWidgetClass);
        }
    }

    if (EndScreenWidgetClass)
    {
        if (PC)
        {
            EndScreenWidget = CreateWidget<UEndScreenWidget>(PC, EndScreenWidgetClass);
        }
        else
        {
            EndScreenWidget = CreateWidget<UEndScreenWidget>(GetWorld(), EndScreenWidgetClass);
        }
    }
}

void ASeattleHUD::ShowMainMenu()
{
    if (MainMenuWidget)
    {
        MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
        MainMenuWidget->SetRenderOpacity(1.f);
        MainMenuWidget->SetDesiredSizeInViewport(FVector2D(1920, 1080));
    }
}

void ASeattleHUD::HideMainMenu()
{
    if (MainMenuWidget)
    {
        MainMenuWidget->RemoveFromParent();
    }

    // restore game input and hide cursor
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
    {
        PC->bShowMouseCursor = false;
        FInputModeGameOnly GameInput;
        PC->SetInputMode(GameInput);
        PC->SetPause(false);

        // If this is the local player, request server to start the match for all players
        if (PC->IsLocalController())
        {
            if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
            {
                SPC->Server_RequestStartGame();
            }
        }
    }

    ShowInGameUI();
}

void ASeattleHUD::ShowEndScreen(const FText& WinnerText)
{
    if (EndScreenWidget)
    {
        EndScreenWidget->AddToViewport(20);
        EndScreenWidget->SetVisibility(ESlateVisibility::Visible);
        EndScreenWidget->SetWinnerText(WinnerText);
    }
}

void ASeattleHUD::HideEndScreen()
{
    if (EndScreenWidget)
    {
        EndScreenWidget->RemoveFromParent();
    }
}

void ASeattleHUD::ShowInGameUI()
{
    if (PlayerHealthWidget)
    {
        PlayerHealthWidget->AddToViewport(5);
        // place bottom-left via anchors/blueprint; C++ can't set exact slot easily without Slate
    }
    if (OpponentHealthWidget)
    {
        OpponentHealthWidget->AddToViewport(5);
    }
}

void ASeattleHUD::UpdatePlayerHealth(float Percent)
{
    if (PlayerHealthWidget)
    {
        PlayerHealthWidget->SetHealthPercent(Percent);
    }
}

void ASeattleHUD::UpdateOpponentHealth(float Percent)
{
    if (OpponentHealthWidget)
    {
        OpponentHealthWidget->SetHealthPercent(Percent);
    }
}
