#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SeattleHUD.generated.h"

class UMainMenuWidget;
class UEndScreenWidget;
class UHealthBarWidget;

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

    UFUNCTION(BlueprintCallable, Category="UI")
    void UpdatePlayerHealth(float Percent);

    UFUNCTION(BlueprintCallable, Category="UI")
    void UpdateOpponentHealth(float Percent);

protected:
    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UUserWidget> MainMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UUserWidget> EndScreenWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category="UI")
    TSubclassOf<UUserWidget> HealthBarWidgetClass;

private:
    UPROPERTY()
    UMainMenuWidget* MainMenuWidget;

    UPROPERTY()
    UEndScreenWidget* EndScreenWidget;

    UPROPERTY()
    UHealthBarWidget* PlayerHealthWidget;

    UPROPERTY()
    UHealthBarWidget* OpponentHealthWidget;
};
