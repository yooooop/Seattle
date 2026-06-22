#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StaminaBarWidget.generated.h"

class UProgressBar;

/** Simple stamina bar widget with a progress bar that can be updated */
UCLASS()
class SEATTLE_API UStaminaBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    /** Update the stamina bar percent (0-1) */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetStaminaPercent(float InPercent);

    /** Mark this widget as representing an opponent so it can flip fill direction */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetIsOpponent(bool bOpponent);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
    bool bIsOpponent = false;

protected:
    /** Progress bar bound to the Blueprint widget */
    UPROPERTY(meta = (BindWidget))
    UProgressBar* StaminaBar;
};
