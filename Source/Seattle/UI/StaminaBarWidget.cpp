#include "StaminaBarWidget.h"
#include "Components/ProgressBar.h"

void UStaminaBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (StaminaBar)
    {
        // Initialize to full stamina
        StaminaBar->SetPercent(1.0f);
    }
}

void UStaminaBarWidget::SetStaminaPercent(float InPercent)
{
    if (StaminaBar)
    {
        StaminaBar->SetPercent(FMath::Clamp(InPercent, 0.0f, 1.0f));
    }
}
