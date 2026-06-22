#include "SeattleHUD.h"
#include "Blueprint/UserWidget.h"
#include "Seattle.h"
#include "MainMenuWidget.h"
#include "EndScreenWidget.h"
#include "HealthBarWidget.h"
#include "StaminaBarWidget.h"
#include "GeneralInGameUI.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Variant_Combat/CombatCharacter.h"
#include "SeattleAI.h"
#include "Variant_Combat/AI/CombatEnemy.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "SeattleCharacter.h"
#include "StaminaComponent.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "SeattlePlayerController.h"
#include "SeattleGameMode.h"

ASeattleHUD::ASeattleHUD()
{
}

void ASeattleHUD::OnOpponentStaminaChanged(float CurrentStamina, float MaxStamina)
{
    if (!CachedOpponentPawn)
    {
        return;
    }

    const float Percent = (MaxStamina > 0.f) ? CurrentStamina / MaxStamina : 0.f;
    UpdateOpponentStamina(Percent);
}

void ASeattleHUD::UpdateOpponentStamina(float Percent)
{
    if (GeneralInGameWidget)
    {
        if (UWidget* Found = GeneralInGameWidget->GetWidgetFromName(TEXT("OpponentStaminaBar")))
        {
            if (UStaminaBarWidget* SB = Cast<UStaminaBarWidget>(Found))
            {
                SB->SetStaminaPercent(Percent);
                return;
            }
        }
        // fallback to direct function
        GeneralInGameWidget->SetOpponentStaminaPercent(Percent);
        return;
    }

    // no general widget: attempt to update standalone opponent widget
    if (OpponentHealthWidget)
    {
        // nothing to do here - health widget doesn't show stamina
    }
}

void ASeattleHUD::HideMainMenu_SuppressStart()
{
    if (MainMenuWidget)
    {
        MainMenuWidget->RemoveFromParent();
    }

    // restore game input and hide cursor
    if (APlayerController* PC = GetOwningPlayerController())
    {
        PC->bShowMouseCursor = false;
        FInputModeGameOnly GameInput;
        PC->SetInputMode(GameInput);
        PC->SetPause(false);
    }

    ShowInGameUI();
}

void ASeattleHUD::StartSlideOverlay(float Duration)
{
    if (!SlideOverlayWidget || SlideOverlayPhase != ESlideOverlayPhase::Idle)
    {
        return;
    }

    SlideOverlayPhase = ESlideOverlayPhase::FadingIn;
    SlideOverlayElapsed = 0.f;
    SlideOverlayWidget->SetVisibility(ESlateVisibility::Visible);
    SlideOverlayWidget->SetRenderOpacity(0.f);

    // we'll hold the overlay for Duration seconds at full opacity
    // store Duration in DamageHoldTime temporarily by reusing SlideOverlayElapsed? Simpler: store in local timer via lambda
    const float HoldDuration = Duration;

    GetWorldTimerManager().SetTimer(SlideOverlayTimerHandle, [this, HoldDuration]() {
        // tick every frame-ish
        if (GetWorld())
        {
            UpdateSlideOverlay(GetWorld()->GetDeltaSeconds());
        }
    }, 0.033f, true);

    // Save hold duration in SlideOverlayElapsed negative to indicate target? Simpler: store as a property via SlideOverlayElapsedWhilstHeld
    // We'll store HoldDuration in DamageHoldTime temporarily (not ideal) but add a local field instead. For now, reuse DamageHoldTime as generic hold time is ok.
    DamageHoldTime = HoldDuration;
}

void ASeattleHUD::UpdateSlideOverlay(float DeltaTime)
{
    if (!SlideOverlayWidget)
    {
        return;
    }

    SlideOverlayElapsed += DeltaTime;
    switch (SlideOverlayPhase)
    {
    case ESlideOverlayPhase::FadingIn:
    {
        const float Alpha = FMath::Clamp(SlideOverlayElapsed / 0.08f, 0.f, 1.f);
        SlideOverlayWidget->SetRenderOpacity(Alpha);
        if (Alpha >= 1.f)
        {
            SlideOverlayPhase = ESlideOverlayPhase::Holding;
            SlideOverlayElapsed = 0.f;
        }
        break;
    }
    case ESlideOverlayPhase::Holding:
        if (SlideOverlayElapsed >= DamageHoldTime)
        {
            SlideOverlayPhase = ESlideOverlayPhase::FadingOut;
            SlideOverlayElapsed = 0.f;
        }
        break;
    case ESlideOverlayPhase::FadingOut:
    {
        const float Alpha = 1.f - FMath::Clamp(SlideOverlayElapsed / 0.25f, 0.f, 1.f);
        SlideOverlayWidget->SetRenderOpacity(Alpha);
        if (Alpha <= 0.f)
        {
            SlideOverlayPhase = ESlideOverlayPhase::Idle;
            SlideOverlayElapsed = 0.f;
            SlideOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
            GetWorldTimerManager().ClearTimer(SlideOverlayTimerHandle);
        }
        break;
    }
    default:
        break;
    }
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

void ASeattleHUD::StartDamageOverlay()
{
    if (!DamageOverlayWidget || DamageOverlayPhase != EDamageOverlayPhase::Idle)
    {
        return;
    }

    DamageOverlayPhase = EDamageOverlayPhase::FadingIn;
    DamageOverlayElapsed = 0.f;
    DamageOverlayWidget->SetVisibility(ESlateVisibility::Visible);
    DamageOverlayWidget->SetRenderOpacity(0.f);

    // Tick the overlay quickly to animate fade in/out
    GetWorldTimerManager().SetTimer(DamageOverlayTimerHandle, [this]() {
        if (GetWorld())
        {
            UpdateDamageOverlay(GetWorld()->GetDeltaSeconds());
        }
    }, 0.033f, true);
}

void ASeattleHUD::UpdateDamageOverlay(float DeltaTime)
{
    if (!DamageOverlayWidget)
    {
        return;
    }

    DamageOverlayElapsed += DeltaTime;
    switch (DamageOverlayPhase)
    {
    case EDamageOverlayPhase::FadingIn:
    {
        const float Alpha = FMath::Clamp(DamageOverlayElapsed / DamageFadeInTime, 0.f, 1.f);
        DamageOverlayWidget->SetRenderOpacity(Alpha);
        if (Alpha >= 1.f)
        {
            // move to hold
            DamageOverlayPhase = EDamageOverlayPhase::Holding;
            DamageOverlayElapsed = 0.f;
        }
        break;
    }
    case EDamageOverlayPhase::Holding:
        if (DamageOverlayElapsed >= DamageHoldTime)
        {
            DamageOverlayPhase = EDamageOverlayPhase::FadingOut;
            DamageOverlayElapsed = 0.f;
        }
        break;
    case EDamageOverlayPhase::FadingOut:
    {
        const float Alpha = 1.f - FMath::Clamp(DamageOverlayElapsed / DamageFadeOutTime, 0.f, 1.f);
        DamageOverlayWidget->SetRenderOpacity(Alpha);
        if (Alpha <= 0.f)
        {
            DamageOverlayPhase = EDamageOverlayPhase::Idle;
            DamageOverlayElapsed = 0.f;
            DamageOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
            GetWorldTimerManager().ClearTimer(DamageOverlayTimerHandle);
        }
        break;
    }
    default:
        break;
    }
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
    // Hide end UI and in-game UI, clear overlays
    HideEndScreen();
    if (GeneralInGameWidget)
    {
        GeneralInGameWidget->RemoveFromParent();
    }
    if (DamageOverlayWidget)
    {
        DamageOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
        DamageOverlayWidget->SetRenderOpacity(0.f);
    }
    if (SlideOverlayWidget)
    {
        SlideOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
        SlideOverlayWidget->SetRenderOpacity(0.f);
    }

    // Show main menu UI again (ensure it is added back to viewport)
    ShowMainMenu();

    // Request server to reset match state (so all actors/variables are restored)
    if (APlayerController* PC = GetOwningPlayerController())
    {
        if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
        {
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [HUD] Requesting server match reset from client %s"), *GetNameSafe(PC));
            SPC->Server_RequestMatchReset();
        }
    }

    // Clear any lingering timers
    GetWorldTimerManager().ClearTimer(EndSequenceTimerHandle);
    bEndSequenceFading = false;
    bEndScreenShown = false;
}

void ASeattleHUD::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetOwningPlayerController();
    // Fallback: if HUD isn't owned yet, try the first local player controller (works on clients)
    if (!PC)
    {
        PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    }
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

    // create damage overlay widget (hidden) for this local HUD
    if (DamageOverlayClass)
    {
        if (PC)
        {
            DamageOverlayWidget = CreateWidget<UUserWidget>(PC, DamageOverlayClass);
        }
        else
        {
            DamageOverlayWidget = CreateWidget<UUserWidget>(GetWorld(), DamageOverlayClass);
        }

        if (DamageOverlayWidget)
        {
            DamageOverlayWidget->AddToViewport(50);
            DamageOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
            DamageOverlayWidget->SetRenderOpacity(0.f);
            UE_LOG(LogSeattle, Log, TEXT("ASeattleHUD::BeginPlay - Created DamageOverlayWidget for %s"), *GetNameSafe(PC));
        }
        else
        {
            UE_LOG(LogSeattle, Warning, TEXT("ASeattleHUD::BeginPlay - Failed to create DamageOverlayWidget for %s"), *GetNameSafe(PC));
        }
    }

    // create slide overlay widget (hidden) for this local HUD
    if (SlideOverlayClass)
    {
        if (PC)
        {
            SlideOverlayWidget = CreateWidget<UUserWidget>(PC, SlideOverlayClass);
        }
        else
        {
            SlideOverlayWidget = CreateWidget<UUserWidget>(GetWorld(), SlideOverlayClass);
        }

        if (SlideOverlayWidget)
        {
            SlideOverlayWidget->AddToViewport(51);
            SlideOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
            SlideOverlayWidget->SetRenderOpacity(0.f);
            UE_LOG(LogSeattle, Log, TEXT("ASeattleHUD::BeginPlay - Created SlideOverlayWidget for %s"), *GetNameSafe(PC));
        }
        else
        {
            UE_LOG(LogSeattle, Warning, TEXT("ASeattleHUD::BeginPlay - Failed to create SlideOverlayWidget for %s"), *GetNameSafe(PC));
        }
    }
}

void ASeattleHUD::ShowMainMenu()
{
    if (MainMenuWidget)
    {
        // Ensure the main menu is added to the viewport (it may have been removed earlier)
        if (!MainMenuWidget->IsInViewport())
        {
            MainMenuWidget->AddToViewport(10);
        }
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
    if (APlayerController* PC = GetOwningPlayerController())
    {
        PC->bShowMouseCursor = false;
        FInputModeGameOnly GameInput;
        PC->SetInputMode(GameInput);
        PC->SetPause(false);

        // If this is the local player, either request the server to start the match (client) or start locally (listen-server)
        if (PC->IsLocalController())
        {
            if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
            {
                if (PC->HasAuthority())
                {
                    // We're the listen-server; invoke the server-side handler directly to start the match
                    SPC->HandleStartGameRequest();
                }
                else
                {
                    // We're a remote client; request the server to start
                    SPC->Server_RequestStartGame();
                }
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
    APlayerController* LocalPC = GetOwningPlayerController();
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
            // Ensure we don't double-bind when ShowInGameUI is called multiple times
            SC->OnHealthChanged.RemoveAll(this);
            SC->OnHealthChanged.AddUObject(this, &ASeattleHUD::OnPlayerHealthChanged);
            // initialize UI with current health
            UpdatePlayerHealth(SC->GetHealthPercent());

            // bind to stamina changes if component exists
            if (UStaminaComponent* StaminaComp = SC->GetStaminaComponent())
            {
                // Remove any previous dynamic binding then add to avoid ensure() failures in editor builds
                StaminaComp->OnStaminaChanged.RemoveDynamic(this, &ASeattleHUD::OnPlayerStaminaChanged);
                StaminaComp->OnStaminaChanged.AddDynamic(this, &ASeattleHUD::OnPlayerStaminaChanged);
                // initialize UI with current stamina (guarded)
                UpdatePlayerStamina(StaminaComp->GetStaminaPercent());
            }
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
            // bind to stamina changes on AI if component exists
            if (UStaminaComponent* StaminaComp = AI->GetStaminaComponent())
            {
                StaminaComp->OnStaminaChanged.AddDynamic(this, &ASeattleHUD::OnOpponentStaminaChanged);
                UpdateOpponentStamina(StaminaComp->GetStaminaPercent());
            }

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

        // show damage overlay and play camera shake when damaged (HealthDelta > 0)
        if (HealthDelta > 0.f)
        {
            StartDamageOverlay();

            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (PC->PlayerCameraManager && SC->HitCameraShakeClass)
                {
                    PC->PlayerCameraManager->StartCameraShake(SC->HitCameraShakeClass);
                }
            }
        }

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

        // CombatCharacter's OnHealthChanged uses positive delta for damage as well
        if (HealthDelta > 0.f)
        {
            StartDamageOverlay();
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
            {
                if (PC->PlayerCameraManager && CC->HitCameraShakeClass)
                {
                    PC->PlayerCameraManager->StartCameraShake(CC->HitCameraShakeClass);
                }
            }
        }

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

void ASeattleHUD::UpdatePlayerStamina(float Percent)
{
    // Use the general in-game UI method if available
    if (GeneralInGameWidget)
    {
        GeneralInGameWidget->SetPlayerStaminaPercent(Percent);
    }
}

void ASeattleHUD::OnPlayerStaminaChanged(float CurrentStamina, float MaxStamina)
{
    const float Percent = (MaxStamina > 0.0f) ? (CurrentStamina / MaxStamina) : 0.0f;
    UpdatePlayerStamina(Percent);
}
