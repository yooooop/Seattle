#include "EndScreenWidget.h"
#include "Components/TextBlock.h"

void UEndScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UEndScreenWidget::SetWinnerText(const FText& InText)
{
    if (WinnerText)
    {
        WinnerText->SetText(InText);
    }
}
