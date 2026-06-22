#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "MainMenuWidget.h"
#include "Engine/Engine.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!StartButton)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMainMenuWidget::NativeConstruct - StartButton is null"));
        return;
    }

    // Verify the handler exists as a UFunction with compatible signature before binding
    UFunction* HandlerFunc = GetClass()->FindFunctionByName(FName("HandleStartClicked"));
    if (!HandlerFunc)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMainMenuWidget::NativeConstruct - HandleStartClicked UFunction not found on %s"), *GetNameSafe(this));
        return;
    }

    // Ensure the handler takes no parameters (OnClicked expects void())
    if (HandlerFunc->NumParms != 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMainMenuWidget::NativeConstruct - HandleStartClicked has unexpected parameters; binding skipped"));
        return;
    }

    // Bind safely using AddUniqueDynamic to avoid duplicate bindings
    StartButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::HandleStartClicked);
}

void UMainMenuWidget::HandleStartClicked()
{
    OnStartPressed.Broadcast();
}
