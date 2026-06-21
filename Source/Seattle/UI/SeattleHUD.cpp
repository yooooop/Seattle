#include "SeattleHUD.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.h"
#include "EndScreenWidget.h"
#include "HealthBarWidget.h"
#include "GeneralInGameUI.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Variant_Combat/CombatCharacter.h"
#include "SeattleAI.h"
#include "Variant_Combat/AI/CombatEnemy.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "SeattleCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "SeattlePlayerController.h"
#include "SeattleGameMode.h"

ASeattleHUD::ASeattleHUD()
{
}

void ASeattleHUD::StartEndSequence(AActor* DownedActor)
{
    if (!DownedActor || bEndSequenceActive)
    {
        return;
    }

    // find the scene capture actor in the level (prefer tagged actor 'EndScreenCapture')
    ASceneCapture2D* FoundCapture = nullptr;

    for (TActorIterator<ASceneCapture2D> It(GetWorld()); It; ++It)
    {
        if (It->GetActorLabel() == TEXT("EndScreenCaptureCamera"))
        {
            FoundCapture = *It;
            break;
        }
    }

    if (!FoundCapture)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartEndSequence: No SceneCapture2D found in level"));
        return;
    }

    EndSceneCaptureActor = FoundCapture;
    EndSceneCaptureComp = FoundCapture->GetCaptureComponent2D();

    // position the capture directly above the downed actor
    FVector Origin;
    FVector Extent;
    DownedActor->GetActorBounds(false, Origin, Extent);
    const float TopZ = Origin.Z + Extent.Z;

    EndSeqStartLocation = FVector(Origin.X, Origin.Y, TopZ + EndSeqStartHeight);
    EndSeqTargetLocation = FVector(Origin.X, Origin.Y, TopZ + EndSeqEndHeight);

    FoundCapture->SetActorLocation(EndSeqStartLocation);
    if (EndSceneCaptureComp)
    {
        EndSceneCaptureComp->ProjectionType = ECameraProjectionMode::Perspective;
        EndSceneCaptureComp->FOVAngle = EndSeqStartFOV;
    }

    // look at the actor's origin
    FRotator LookRot = (Origin - EndSeqStartLocation).Rotation();
    FoundCapture->SetActorRotation(LookRot);

    // store starting rotation and look target for spin/rotation during the sequence
    EndSeqInitialRotation = FoundCapture->GetActorRotation();
    EndSeqLookTarget = Origin;

    bEndSequenceActive = true;
    EndSeqElapsed = 0.f;

    // ensure end screen UI is visible
    ShowEndScreen(FText::FromString(DownedActor->GetName()));

    // start tick timer for the sequence (call UpdateEndSequence every frame-ish)
    GetWorldTimerManager().SetTimer(EndSequenceTimerHandle, [this]() {
        if (GetWorld())
        {
            UpdateEndSequence(GetWorld()->GetDeltaSeconds());
        }
    }, 0.1f, true);
}

void ASeattleHUD::UpdateEndSequence(float DeltaTime)
{
    if (!bEndSequenceActive || !EndSceneCaptureActor)
    {
        return;
    }

    EndSeqElapsed += DeltaTime;
    const float Alpha = FMath::Clamp(EndSeqElapsed / EndSeqDuration, 0.f, 1.f);
    // keep capture actor at the start location but interpolate FOV to create zoom-out effect
    if (EndSceneCaptureComp)
    {
        const float NewFOV = FMath::Lerp(EndSeqStartFOV, EndSeqEndFOV, Alpha);
        EndSceneCaptureComp->FOVAngle = NewFOV;
    }

    // optionally move the actor slightly upward for cinematic effect (small lerp)
    const FVector NewLoc = FMath::Lerp(EndSeqStartLocation, EndSeqTargetLocation, Alpha * 0.2f);
    EndSceneCaptureActor->SetActorLocation(NewLoc);

    // Compute rotation to look at the knocked-down actor's origin (stored earlier)
    FRotator NewRot = (EndSeqLookTarget - NewLoc).Rotation();

    // Apply a slow spin around the Z axis while preserving the look direction pitch/roll
    const float SpinYaw = EndSeqElapsed * EndSeqSpinRate;
    NewRot.Yaw += SpinYaw;
    EndSceneCaptureActor->SetActorRotation(NewRot);

    if (Alpha >= 1.f)
    {
        bEndSequenceActive = false;
        GetWorldTimerManager().ClearTimer(EndSequenceTimerHandle);

        // Start fade to black on the local player's camera, then return to main menu
        if (!bEndSequenceFading)
        {
            bEndSequenceFading = true;
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (PC->PlayerCameraManager)
                {
                    PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, EndSeqFadeDuration, FLinearColor::Black, true, true);
                }
            }

            // Schedule finalization after fade completes
            GetWorldTimerManager().SetTimer(EndSequenceTimerHandle, [this]() {
                OnEndSequenceFinished();
            }, EndSeqFadeDuration, false);
        }
    }
}

void ASeattleHUD::OnEndSequenceFinished()
{
    // Hide end UI and return to main menu locally
    HideEndScreen();

    // Show main menu UI again
    ShowMainMenu();

    // Clear any lingering timers
    GetWorldTimerManager().ClearTimer(EndSequenceTimerHandle);
    bEndSequenceFading = false;
    bEndScreenShown = false;
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
            MainMenuWidget->AddToViewport(10);
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

        // configure roles
        if (PlayerHealthWidget)
        {
            PlayerHealthWidget->SetIsOpponent(false);
        }
        if (OpponentHealthWidget)
        {
            OpponentHealthWidget->SetIsOpponent(true);
        }
    }

    // create general in-game UI widget (contains the two health bars)
    if (GeneralInGameWidgetClass)
    {
        if (PC)
        {
            GeneralInGameWidget = CreateWidget<UGeneralInGameUI>(PC, GeneralInGameWidgetClass);
        }
        else
        {
            GeneralInGameWidget = CreateWidget<UGeneralInGameUI>(GetWorld(), GeneralInGameWidgetClass);
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
    // prefer the general in-game UI if available
    if (GeneralInGameWidget)
    {
        GeneralInGameWidget->AddToViewport(5);
        // If the general widget contains named health bars, configure their role
        if (UWidget* Found = GeneralInGameWidget->GetWidgetFromName(TEXT("PlayerHealthBar")))
        {
            if (UHealthBarWidget* HB = Cast<UHealthBarWidget>(Found))
            {
                HB->SetIsOpponent(false);
            }
        }
        if (UWidget* FoundOpp = GeneralInGameWidget->GetWidgetFromName(TEXT("OpponentHealthBar")))
        {
            if (UHealthBarWidget* HB2 = Cast<UHealthBarWidget>(FoundOpp))
            {
                HB2->SetIsOpponent(true);
            }
        }
    }
    else
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

    // start polling for health values to keep HUD updated
    // Subscribe to player health changes instead of polling
    // Prefer binding to the local player's possessed pawn or the shared character pawn (handles shared-pawn/co-op setups reliably)
    APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    APawn* PlayerPawn = nullptr;
    if (LocalPC)
    {
        // first prefer the pawn the local controller actually possesses
        PlayerPawn = LocalPC->GetPawn();

        // if this controller is a SeattlePlayerController and doesn't possess a pawn, try the replicated shared pawn
        if (!PlayerPawn)
        {
            if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(LocalPC))
            {
                PlayerPawn = SPC->GetSharedCharacterPawn();
            }
        }
    }

    // fallback to engine helper
    if (!PlayerPawn)
    {
        PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    }

    if (PlayerPawn)
    {
        CachedPlayerPawn = PlayerPawn;

        if (ASeattleCharacter* SC = Cast<ASeattleCharacter>(PlayerPawn))
        {
            // bind using C++ multicast delegate
            SC->OnHealthChanged.AddUObject(this, &ASeattleHUD::OnPlayerHealthChanged);
            // initialize UI with current health
            UpdatePlayerHealth(SC->GetHealthPercent());
        }
        else if (auto* CC = Cast<class ACombatCharacter>(PlayerPawn))
        {
            CC->OnHealthChanged.AddUObject(this, &ASeattleHUD::OnPlayerHealthChanged);
            UpdatePlayerHealth(CC->GetHealthPercent());
        }
    }

    // find an opponent actor (AI or combat enemy) and subscribe to its health changes
    // Preference: ASeattleAI, then ACombatCharacter/CombatEnemy
    if (!CachedOpponentPawn)
    {
        for (TActorIterator<ASeattleAI> It(GetWorld()); It; ++It)
        {
            ASeattleAI* AI = *It;
            if (!AI || AI == CachedPlayerPawn)
            {
                continue;
            }

            // bind dynamic delegate
            AI->OnHealthChanged.AddDynamic(this, &ASeattleHUD::OnOpponentHealthChanged);
            CachedOpponentPawn = AI;
            UpdateOpponentHealth(AI->GetHealthPercent());
            break;
        }
    }

    if (!CachedOpponentPawn)
    {
        for (TActorIterator<ACombatCharacter> It(GetWorld()); It; ++It)
        {
            ACombatCharacter* CC = *It;
            if (!CC || CC == CachedPlayerPawn)
            {
                continue;
            }

            CC->OnHealthChanged.AddUObject(this, &ASeattleHUD::OnOpponentHealthChanged);
            CachedOpponentPawn = CC;
            UpdateOpponentHealth(CC->GetHealthPercent());
            break;
        }
    }
}

void ASeattleHUD::OnPlayerHealthChanged(float NewHealth, float HealthDelta)
{
    if (!CachedPlayerPawn)
    {
        return;
    }

    if (ASeattleCharacter* SC = Cast<ASeattleCharacter>(CachedPlayerPawn))
    {
        const float Percent = SC->GetHealthPercent();
        UpdatePlayerHealth(Percent);

        if (NewHealth <= 0.f && !bEndScreenShown)
        {
            bEndScreenShown = true;
            StartEndSequence(SC);

            // Notify server to stop AI and notify other clients to start end sequence
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
                {
                    if (SPC->IsLocalController())
                    {
                        SPC->Server_NotifyEndGame();
                    }
                }
            }
        }
    }

    else if (auto* CC = Cast<class ACombatCharacter>(CachedPlayerPawn))
    {
        const float Percent = CC->GetHealthPercent();
        UpdatePlayerHealth(Percent);

        if (NewHealth <= 0.f && !bEndScreenShown)
        {
            bEndScreenShown = true;
            StartEndSequence(CC);

            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
                {
                    if (SPC->IsLocalController())
                    {
                        SPC->Server_NotifyEndGame();
                    }
                }
            }
        }
    }
}

void ASeattleHUD::OnOpponentHealthChanged(float NewHealth, float HealthDelta)
{
    if (!CachedOpponentPawn)
    {
        return;
    }

    if (ASeattleAI* AI = Cast<ASeattleAI>(CachedOpponentPawn))
    {
        const float Percent = AI->GetHealthPercent();
        UpdateOpponentHealth(Percent);

        if (NewHealth <= 0.f && !bEndScreenShown)
        {
            bEndScreenShown = true;
            StartEndSequence(AI);

            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
                {
                    if (SPC->IsLocalController())
                    {
                        SPC->Server_NotifyEndGame();
                    }
                }
            }
        }
    }
    else if (ACombatCharacter* CC = Cast<ACombatCharacter>(CachedOpponentPawn))
    {
        const float Percent = CC->GetHealthPercent();
        UpdateOpponentHealth(Percent);

        if (NewHealth <= 0.f && !bEndScreenShown)
        {
            bEndScreenShown = true;
            StartEndSequence(CC);

            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
                {
                    if (SPC->IsLocalController())
                    {
                        SPC->Server_NotifyEndGame();
                    }
                }
            }
        }
    }
}

// Polling removed; HUD now subscribes to health change delegates.

void ASeattleHUD::UpdatePlayerHealth(float Percent)
{
    // If the general in-game UI widget contains named health bar widgets, prefer those
    if (GeneralInGameWidget)
    {
        if (UWidget* Found = GeneralInGameWidget->GetWidgetFromName(TEXT("PlayerHealthBar")))
        {
            if (UHealthBarWidget* HB = Cast<UHealthBarWidget>(Found))
            {
                HB->SetHealthPercent(Percent);
                return;
            }
        }
    }

    if (PlayerHealthWidget)
    {
        PlayerHealthWidget->SetHealthPercent(Percent);
    }
}

void ASeattleHUD::UpdateOpponentHealth(float Percent)
{
    if (GeneralInGameWidget)
    {
        if (UWidget* Found = GeneralInGameWidget->GetWidgetFromName(TEXT("OpponentHealthBar")))
        {
            if (UHealthBarWidget* HB = Cast<UHealthBarWidget>(Found))
            {
                HB->SetHealthPercent(Percent);
                return;
            }
        }
    }

    if (OpponentHealthWidget)
    {
        OpponentHealthWidget->SetHealthPercent(Percent);
    }
}
