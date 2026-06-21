#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GeneralInGameUI.generated.h"

class UHealthBarWidget;

UCLASS()
class SEATTLE_API UGeneralInGameUI : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category="UI")
    void SetPlayerHealthPercent(float Percent);

    UFUNCTION(BlueprintCallable, Category="UI")
    void SetOpponentHealthPercent(float Percent);

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    UHealthBarWidget* PlayerHealthBar;

    UPROPERTY(meta = (BindWidgetOptional))
    UHealthBarWidget* OpponentHealthBar;
};
