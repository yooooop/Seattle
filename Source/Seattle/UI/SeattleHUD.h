#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SeattleHUD.generated.h"

class UMainMenuWidget;
class UEndScreenWidget;
class UHealthBarWidget;
class UGeneralInGameUI;

UCLASS()
class SEATTLE_API ASeattleHUD : public AHUD
{
    GENERATED_BODY()

public:
    ASeattleHUD();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="UI")
    void ShowMainMenu();

    UFUNCTION(BlueprintCallable, Category="UI")
    void HideMainMenu();

    UFUNCTION(BlueprintCallable, Category="UI")
    void ShowEndScreen(const FText& WinnerText);

    UFUNCTION(BlueprintCallable, Category="UI")
    void HideEndScreen();

    UFUNCTION(BlueprintCallable, Category="UI")
    void ShowInGameUI();

    /** Handler for player health changes (bound to player delegate) */
    UFUNCTION()
    void OnPlayerHealthChanged(float NewHealth, float HealthDelta);

    UFUNCTION(BlueprintCallable, Category="UI")
    void UpdatePlayerHealth(float Percent);

    UFUNCTION(BlueprintCallable, Category="UI")
    void UpdateOpponentHealth(float Percent);


    void StartEndSequence(AActor* DownedActor);

protected:
    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UUserWidget> MainMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UUserWidget> EndScreenWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UUserWidget> HealthBarWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UUserWidget> GeneralInGameWidgetClass;

private:
    UPROPERTY()
    UMainMenuWidget* MainMenuWidget;

    UPROPERTY()
    UEndScreenWidget* EndScreenWidget;

    UPROPERTY()
    UHealthBarWidget* PlayerHealthWidget;


    UPROPERTY()
    UHealthBarWidget* OpponentHealthWidget;

    /** Cached player pawn pointer */
    UPROPERTY()
    APawn* CachedPlayerPawn = nullptr;

    /** Cached opponent pawn pointer (first non-player we bind to) */
    UPROPERTY()
    APawn* CachedOpponentPawn = nullptr;

    /** Handler for opponent health changes (bound to opponent delegate) */
    UFUNCTION()
    void OnOpponentHealthChanged(float NewHealth, float HealthDelta);


    UPROPERTY()
    UGeneralInGameUI* GeneralInGameWidget;

    // (polling removed) 

    // End screen capture sequence
    UPROPERTY()
    AActor* EndSceneCaptureActor = nullptr;

    UPROPERTY()
    class USceneCaptureComponent2D* EndSceneCaptureComp = nullptr;

    // Zoom animation
    bool bEndSequenceActive = false;
    FVector EndSeqStartLocation;
    FVector EndSeqTargetLocation;
    float EndSeqDuration = 3.0f;
    float EndSeqElapsed = 0.0f;
    void UpdateEndSequence(float DeltaTime);
    /** Where the capture should look at during the end sequence (world space) */
    FVector EndSeqLookTarget = FVector::ZeroVector;

    /** Initial rotation of the capture actor when the sequence starts */
    FRotator EndSeqInitialRotation = FRotator::ZeroRotator;

    /** Spin rate in degrees per second applied around the Z axis while zooming */
    UPROPERTY(EditDefaultsOnly, Category = "EndSequence")
    float EndSeqSpinRate = 20.f;

    /** Fade duration after camera finishes zoom/rotation */
    UPROPERTY(EditDefaultsOnly, Category = "EndSequence")
    float EndSeqFadeDuration = 1.0f;

    /** Whether we have started the fade-out portion of the end sequence */
    bool bEndSequenceFading = false;

    FTimerHandle EndSequenceTimerHandle;

    // End sequence tuning (editable in defaults)
    UPROPERTY(EditDefaultsOnly, Category = "EndSequence")
    float EndSeqStartHeight = 200.f;

    UPROPERTY(EditDefaultsOnly, Category = "EndSequence")
    float EndSeqEndHeight = 800.f;

    UPROPERTY(EditDefaultsOnly, Category = "EndSequence")
    float EndSeqStartFOV = 30.f;

    UPROPERTY(EditDefaultsOnly, Category = "EndSequence")
    float EndSeqEndFOV = 90.f;
    bool bEndScreenShown = false;

    /** Called after fade-out completes to return to main menu */
    void OnEndSequenceFinished();
};
