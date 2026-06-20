#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartPressed);

UCLASS()
class SEATTLE_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category="UI")
    FOnStartPressed OnStartPressed;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* StartButton;

    UFUNCTION()
    void HandleStartClicked();
};
