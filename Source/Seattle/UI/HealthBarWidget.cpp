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

void UHealthBarWidget::SetIsOpponent(bool bOpponent)
{
    bIsOpponent = bOpponent;

    // Set bar color: green for player, red for opponent
    if (HealthBar)
    {
        const FLinearColor BarColor = bIsOpponent ? FLinearColor(1.f, 0.f, 0.f, 1.f) : FLinearColor(0.f, 1.f, 0.f, 1.f);
        HealthBar->SetFillColorAndOpacity(BarColor);
    }

    // Show/hide left/right icons depending on role
    if (LeftIconImage)
    {
        LeftIconImage->SetVisibility(bIsOpponent ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
    if (RightIconImage)
    {
        RightIconImage->SetVisibility(bIsOpponent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}
