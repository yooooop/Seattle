#pragma once

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "TimerManager.h"
#include "BTTask_PlayBlackboardMontage.generated.h"

class UAnimMontage;

/** Memory layout for the task - stores a timer handle when waiting */
struct FBTMontageTaskMemory
{
    FTimerHandle TimerHandle;
};

UCLASS(DisplayName = "Play Montage From Blackboard", meta = (HiddenTitleAsset))
class UBTTask_PlayBlackboardMontage : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_PlayBlackboardMontage();

    /** Blackboard key that should hold an AnimMontage (object) to play */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector MontageKey;

    /** Playback rate passed to montage play */
    UPROPERTY(EditAnywhere, Category = "Task")
    float PlayRate = 1.f;

    /** If true the task will return InProgress and wait for montage end */
    UPROPERTY(EditAnywhere, Category = "Task")
    bool bWaitForMontageEnd = false;

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTMontageTaskMemory); }
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
    /** Timer callback when waiting for montage end */
    void OnMontageTimerExpired(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory);
};
