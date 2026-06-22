#pragma once

#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "CombatAction.h"
#include "BTService_UtilitySelector.generated.h"

/**
 * Utility-based selector service: evaluates possible combat actions each tick and writes the best one to the blackboard.
 */
UCLASS(DisplayName = "Utility Selector Service")
class SEATTLE_API UBTService_UtilitySelector : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_UtilitySelector();

    /** How often to evaluate (seconds) */
    UPROPERTY(EditAnywhere, Category = "Utility")
    float EvaluationInterval = 0.15f;

    /** Attack effective range (cm) */
    UPROPERTY(EditAnywhere, Category = "Utility")
    float AttackRange = 200.f;

    /** Dodge effective range (cm). Only consider dodges when target is within this range */
    UPROPERTY(EditAnywhere, Category = "Utility")
    float DodgeRange = 250.f;

    /** How far beyond AttackRange SlideForward is preferred (cm) */
    UPROPERTY(EditAnywhere, Category = "Utility")
    float SlideForwardPadding = 100.f;

    /** Cooldowns after attacks (seconds) */
    UPROPERTY(EditAnywhere, Category = "Utility")
    float JabCooldown = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Utility")
    float HookCooldown = 0.55f;

    /** Chance to attempt dodge when player is attacking and AI has stamina (0..1) */
    UPROPERTY(EditAnywhere, Category = "Utility")
    float DodgeChance = 0.5f;

    /** Scoring tuning (exposed for Blueprint/editor) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float JabBaseScore = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float JabRandomRange = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float HookBaseScore = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float HookRandomRange = 30.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float SlideLeftBase = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float SlideRightBase = 85.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float SlideBackBase = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float SlideRandomRange = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float SlideForwardNearScore = 65.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float SlideForwardRandom = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float SlideForwardFarScore = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float MoveToFarScore = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float MoveToCloseScore = 60.f;

    /** Idle score default is very low so it won't be chosen unless no stamina */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float IdleDefaultScore = -1000.f;

    /** Idle score to use when out of stamina */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    float IdleNoStaminaScore = 50.f;

    /** Minimum stamina required to allow sliding (inclusive). If CurrentStamina <= MinStaminaToSlide, sliding is disabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Utility|Scoring")
    int32 MinStaminaToSlide = 2;

protected:
    virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    void EvaluateActions(UBehaviorTreeComponent& OwnerComp);

    // Blackboard keys (names expected in user's Blackboard asset)
    FBlackboard::FKey TargetActorKey;
    FBlackboard::FKey DistanceKey;
    FBlackboard::FKey PlayerIsAttackingKey;
    FBlackboard::FKey CurrentStaminaKey;
    FBlackboard::FKey HasStaminaKey;
    FBlackboard::FKey BestActionKey;
    FBlackboard::FKey IsActingKey;
};
