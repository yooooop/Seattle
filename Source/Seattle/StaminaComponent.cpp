// Copyright Epic Games, Inc. All Rights Reserved.

#include "StaminaComponent.h"
#include "Net/UnrealNetwork.h"
#include "Seattle.h"

UStaminaComponent::UStaminaComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UStaminaComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize stamina to max
    CurrentStamina = MaxStamina;

    // Only run regeneration on server
    if (GetOwnerRole() == ROLE_Authority)
    {
        StartRegenerationTimer();
    }
}

void UStaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UStaminaComponent, CurrentStamina);
}

void UStaminaComponent::OnRep_CurrentStamina()
{
    // Broadcast to listeners (UI, etc.)
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}

bool UStaminaComponent::HasEnoughStamina(float Cost) const
{
    const float ActualCost = (Cost < 0.0f) ? StaminaCostPerAction : Cost;
    return CurrentStamina >= ActualCost;
}

bool UStaminaComponent::ConsumeStamina(float Cost)
{
    // Only server can consume stamina
    if (GetOwnerRole() != ROLE_Authority)
    {
        return false;
    }

    const float ActualCost = (Cost < 0.0f) ? StaminaCostPerAction : Cost;

    if (!HasEnoughStamina(ActualCost))
    {
        return false;
    }

    const float Prev = CurrentStamina;
    CurrentStamina = FMath::Clamp(CurrentStamina - ActualCost, 0.0f, MaxStamina);

    // Stamina consumed successfully

    // Notify clients of stamina change
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);

    // Reset regeneration timer
    OnStaminaConsumed();

    return true;
}

void UStaminaComponent::RegenerateStamina()
{
    // Only server regenerates stamina
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    // Don't regenerate if already at max
    if (CurrentStamina >= MaxStamina)
    {
        CurrentStamina = MaxStamina;
        return;
    }

    const float PreviousStamina = CurrentStamina;
    CurrentStamina = FMath::Clamp(CurrentStamina + RegenerationAmount, 0.0f, MaxStamina);

    if (CurrentStamina != PreviousStamina)
    {
        UE_LOG(LogSeattle, Log, TEXT("UStaminaComponent::RegenerateStamina - Current=%f Owner=%s"), 
            CurrentStamina, *GetNameSafe(GetOwner()));

        // Notify clients of stamina change
        OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
    }
}

float UStaminaComponent::GetStaminaPercent() const
{
    if (MaxStamina <= 0.0f)
    {
        return 0.0f;
    }
    return FMath::Clamp(CurrentStamina / MaxStamina, 0.0f, 1.0f);
}

void UStaminaComponent::SetStamina(float NewStamina)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    CurrentStamina = FMath::Clamp(NewStamina, 0.0f, MaxStamina);
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}

void UStaminaComponent::StartRegenerationTimer()
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    // Clear any existing timer
    StopRegenerationTimer();

    // Set up periodic regeneration
    if (RegenerationInterval > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            RegenerationTimerHandle,
            this,
            &UStaminaComponent::RegenerateStamina,
            RegenerationInterval,
            true // Loop
        );
    }
}

void UStaminaComponent::StopRegenerationTimer()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RegenerationTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(RegenerationDelayTimerHandle);
    }
}

void UStaminaComponent::OnStaminaConsumed()
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        return;
    }

    // Stop current regeneration
    StopRegenerationTimer();

    // Start regeneration after delay
    if (RegenerationDelay > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            RegenerationDelayTimerHandle,
            this,
            &UStaminaComponent::StartRegenerationTimer,
            RegenerationDelay,
            false // Once
        );
    }
    else
    {
        // No delay, start immediately
        StartRegenerationTimer();
    }
}
