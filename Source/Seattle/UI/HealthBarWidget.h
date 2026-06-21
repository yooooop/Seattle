#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarWidget.generated.h"

class UProgressBar;
class UImage;

/** Simple health bar widget exposing a SetHealth function and an icon slot. */
UCLASS()
class SEATTLE_API UHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Set health percent (0..1) */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetHealthPercent(float InPercent);

    /** Mark this widget as representing an opponent (red) or player (green) */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetIsOpponent(bool bOpponent);

    /** Whether this health bar represents an opponent */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
    bool bIsOpponent = false;

protected:
    virtual void NativeConstruct() override;

    /** Progress bar representing health (Bind in UMG or via widget tree) */
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* HealthBar;

    /** Icon image slot */
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* IconImage;

    /** Optional left-side icon (for player) */
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* LeftIconImage;

    /** Optional right-side icon (for opponent) */
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* RightIconImage;
};
