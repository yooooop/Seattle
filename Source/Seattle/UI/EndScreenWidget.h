#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EndScreenWidget.generated.h"

class UTextBlock;

UCLASS()
class SEATTLE_API UEndScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetWinnerText(const FText& InText);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* WinnerText;
};
