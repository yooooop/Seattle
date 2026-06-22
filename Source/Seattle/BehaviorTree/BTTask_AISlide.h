#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BTTask_AISlide.generated.h"

/** Simple BT task that instructs the AI pawn to perform a slide. Optionally uses a blackboard vector key as the target location to compute slide direction. */
UCLASS(DisplayName = "AI Slide Task")
class SEATTLE_API UBTTask_AISlide : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_AISlide();

    /** If set, read this blackboard vector key as a world location and slide toward it. Otherwise slide forward. */
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetLocationKey;

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
