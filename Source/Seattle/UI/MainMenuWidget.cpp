#include "MainMenuWidget.h"
#include "Components/Button.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (StartButton)
    {
        StartButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleStartClicked);
    }
}

void UMainMenuWidget::HandleStartClicked()
{
    OnStartPressed.Broadcast();
}
