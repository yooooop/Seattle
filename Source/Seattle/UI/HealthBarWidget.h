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

protected:
    virtual void NativeConstruct() override;

    /** Progress bar representing health (Bind in UMG or via widget tree) */
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* HealthBar;

    /** Icon image slot */
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* IconImage;
};
