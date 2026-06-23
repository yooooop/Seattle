#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "CombatAction.h"
#include "BTTask_ExecuteBestAction.generated.h"

struct FExecActionMemory
{
    FTimerHandle TimerHandle;
    // Last polled distance to target (used to detect stalled movement)
    float LastDistance = FLT_MAX;
    // Number of consecutive polls where distance did not decrease
    int32 StalePollCount = 0;
};

UCLASS(DisplayName = "Execute Best Action Task")
class SEATTLE_API UBTTask_ExecuteBestAction : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_ExecuteBestAction();

    /** Whether to wait for action duration before finishing the task (for montages/slides) */
    UPROPERTY(EditAnywhere, Category = "Task")
    bool bWaitForAction = true;

    /** Default slide duration (seconds) if AI doesn't provide one */
    UPROPERTY(EditAnywhere, Category = "Task")
    float DefaultSlideDuration = 0.2f;

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FExecActionMemory); }

    void OnActionTimerExpired(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory);
    void OnMovePoll(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory);
};
