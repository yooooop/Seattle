#pragma once

#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_UpdateBlackboard.generated.h"

/**
 * Copies AI/target state into the Blackboard so other services/tasks can use it.
 * - DistanceToTarget (float)
 * - PlayerIsAttacking (bool)
 * - CurrentStamina (int)
 * - HasStamina (bool)
 */
UCLASS(DisplayName = "Update Blackboard From AI")
class SEATTLE_API UBTService_UpdateBlackboard : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_UpdateBlackboard();

protected:
    virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    // Blackboard keys cached
    FBlackboard::FKey TargetActorKey;
    FBlackboard::FKey DistanceKey;
    FBlackboard::FKey PlayerIsAttackingKey;
    FBlackboard::FKey CurrentStaminaKey;
    FBlackboard::FKey HasStaminaKey;

    // previous distance used to detect entering attack range
    bool bPreviouslyInAttackRange = false;
    float PreviousDistance = FLT_MAX;
};
