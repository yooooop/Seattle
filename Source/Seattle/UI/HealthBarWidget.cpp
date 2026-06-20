#include "HealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"

void UHealthBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Ensure progress bar exists; if not, it's ok because BindWidgetOptional is used
    if (HealthBar)
    {
        HealthBar->SetPercent(1.0f);
    }
}

void UHealthBarWidget::SetHealthPercent(float InPercent)
{
    const float Clamped = FMath::Clamp(InPercent, 0.f, 1.f);
    if (HealthBar)
    {
        HealthBar->SetPercent(Clamped);
    }
}
