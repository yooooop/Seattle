#include "GeneralInGameUI.h"
#include "HealthBarWidget.h"
#include "StaminaBarWidget.h"

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
        OpponentHealthBar->SetIsOpponent(true);
    }
    if (PlayerStaminaBar)
    {
        PlayerStaminaBar->SetStaminaPercent(1.0f);
    }
    if (OpponentStaminaBar)
    {
        OpponentStaminaBar->SetStaminaPercent(1.0f);
        OpponentStaminaBar->SetIsOpponent(true);
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

void UGeneralInGameUI::SetPlayerStaminaPercent(float Percent)
{
    if (PlayerStaminaBar)
    {
        PlayerStaminaBar->SetStaminaPercent(Percent);
    }
}

void UGeneralInGameUI::SetOpponentStaminaPercent(float Percent)
{
    if (OpponentStaminaBar)
    {
        OpponentStaminaBar->SetStaminaPercent(Percent);
    }
}
