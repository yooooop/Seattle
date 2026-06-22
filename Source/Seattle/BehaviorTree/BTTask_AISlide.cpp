#include "BTTask_AISlide.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "SeattleAI.h"

UBTTask_AISlide::UBTTask_AISlide()
{
    NodeName = TEXT("AI Slide");
}

EBTNodeResult::Type UBTTask_AISlide::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC)
    {
        return EBTNodeResult::Failed;
    }

    APawn* Pawn = AIC->GetPawn();
    if (!Pawn)
    {
        return EBTNodeResult::Failed;
    }

    ASeattleAI* AI = Cast<ASeattleAI>(Pawn);
    if (!AI)
    {
        return EBTNodeResult::Failed;
    }

    FVector Direction = Pawn->GetActorForwardVector();

    if (TargetLocationKey.SelectedKeyName != NAME_None)
    {
        if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
        {
            const FVector Target = BB->GetValueAsVector(TargetLocationKey.SelectedKeyName);
            if (!Target.IsNearlyZero())
            {
                Direction = (Target - Pawn->GetActorLocation()).GetSafeNormal();
            }
        }
    }

    AI->DoAISlide(Direction);

    return EBTNodeResult::Succeeded;
}
