// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, CurrentStamina, float, MaxStamina);

/**
 * Component that manages stamina for player actions (slides, punches, etc.)
 * Stamina regenerates over time and can be consumed by actions.
 * All properties are Blueprint editable for easy tuning.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SEATTLE_API UStaminaComponent : public UActorComponent
{
    GENERATED_BODY()

public:	
    UStaminaComponent();

    /** Maximum stamina units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "1", ClampMax = "100"))
    float MaxStamina = 5.0f;

    /** Current stamina (replicated) */
    UPROPERTY(ReplicatedUsing=OnRep_CurrentStamina, BlueprintReadOnly, Category = "Stamina")
    float CurrentStamina = 5.0f;

    /** Cost per action (slide, punch, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.1", ClampMax = "10"))
    float StaminaCostPerAction = 1.0f;

    /** Time in seconds between stamina regeneration ticks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0.1", ClampMax = "10"))
    float RegenerationInterval = 2.0f;

    /** Amount of stamina to restore per regeneration tick */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0.1", ClampMax = "10"))
    float RegenerationAmount = 1.0f;

    /** Delay in seconds before stamina starts regenerating after an action */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina|Regeneration", meta = (ClampMin = "0", ClampMax = "10"))
    float RegenerationDelay = 0.5f;

    /** Blueprint event broadcast when stamina changes */
    UPROPERTY(BlueprintAssignable, Category = "Stamina")
    FOnStaminaChanged OnStaminaChanged;

    /** Check if enough stamina is available for an action */
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    bool HasEnoughStamina(float Cost = -1.0f) const;

    /** Consume stamina for an action. Returns true if successful, false if not enough stamina */
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    bool ConsumeStamina(float Cost = -1.0f);

    /** Force regenerate stamina (called by timer) */
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void RegenerateStamina();

    /** Get current stamina as a normalized value (0-1) for UI */
    UFUNCTION(BlueprintPure, Category = "Stamina")
    float GetStaminaPercent() const;

    /** Manually set current stamina (admin/debug use) */
    UFUNCTION(BlueprintCallable, Category = "Stamina")
    void SetStamina(float NewStamina);

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_CurrentStamina();

    /** Timer handle for regeneration */
    FTimerHandle RegenerationTimerHandle;

    /** Timer handle for regeneration delay */
    FTimerHandle RegenerationDelayTimerHandle;

    /** Start the regeneration timer */
    void StartRegenerationTimer();

    /** Stop and clear regeneration timers */
    void StopRegenerationTimer();

    /** Called when stamina is consumed to reset regeneration delay */
    void OnStaminaConsumed();
};
