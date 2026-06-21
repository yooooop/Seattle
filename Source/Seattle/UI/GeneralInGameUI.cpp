#include "GeneralInGameUI.h"
#include "HealthBarWidget.h"

void UGeneralInGameUI::NativeConstruct()
{
    Super::NativeConstruct();

    if (PlayerHealthBar)
    {
        PlayerHealthBar->SetHealthPercent(1.0f);
    }
    if (OpponentHealthBar)
    {
        OpponentHealthBar->SetHealthPercent(1.0f);
    }
}

void UGeneralInGameUI::SetPlayerHealthPercent(float Percent)
{
    if (PlayerHealthBar)
    {
        PlayerHealthBar->SetHealthPercent(Percent);
    }
}

void UGeneralInGameUI::SetOpponentHealthPercent(float Percent)
{
    if (OpponentHealthBar)
    {
        OpponentHealthBar->SetHealthPercent(Percent);
    }
}
