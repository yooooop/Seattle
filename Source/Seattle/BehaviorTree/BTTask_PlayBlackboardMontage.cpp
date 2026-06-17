#include "BTTask_PlayBlackboardMontage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "SeattleAI.h"
#include "TimerManager.h"
#include "Engine/World.h"

UBTTask_PlayBlackboardMontage::UBTTask_PlayBlackboardMontage()
{
    NodeName = TEXT("Play Blackboard Montage");
}

EBTNodeResult::Type UBTTask_PlayBlackboardMontage::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
        return EBTNodeResult::Failed;
    }

    UObject* Obj = BB->GetValueAsObject(MontageKey.SelectedKeyName);
    UAnimMontage* Montage = Cast<UAnimMontage>(Obj);
    if (!Montage)
    {
        UE_LOG(LogTemp, Warning, TEXT("BTTask_PlayBlackboardMontage: Blackboard key '%s' did not contain a valid AnimMontage."), *MontageKey.SelectedKeyName.ToString());
        return EBTNodeResult::Failed;
    }

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
    {
        return EBTNodeResult::Failed;
    }

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn)
    {
        return EBTNodeResult::Failed;
    }

    ASeattleAI* Char = Cast<ASeattleAI>(Pawn);
    if (!Char)
    {
        UE_LOG(LogTemp, Warning, TEXT("BTTask_PlayBlackboardMontage: Controlled pawn is not ASeattleCharacter."));
        return EBTNodeResult::Failed;
    }

    // Request the character to play the montage. RequestPlayAttackMontage handles authority vs client.
    Char->RequestPlayAttackMontage(Montage, PlayRate);

    // If configured to wait, set a timer for the montage length and return InProgress so BT will pause here.
    if (bWaitForMontageEnd && Montage->GetPlayLength() > 0.f)
    {
        FBTMontageTaskMemory* MyMemory = reinterpret_cast<FBTMontageTaskMemory*>(NodeMemory);
        UWorld* World = OwnerComp.GetWorld();
        if (World)
        {
            const float Duration = Montage->GetPlayLength() / FMath::Max(PlayRate, 0.0001f);
            FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_PlayBlackboardMontage::OnMontageTimerExpired, &OwnerComp, NodeMemory);
            World->GetTimerManager().SetTimer(MyMemory->TimerHandle, Delegate, Duration, false);
            return EBTNodeResult::InProgress;
        }
    }

    // Not waiting: finish immediately
    return EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTask_PlayBlackboardMontage::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FBTMontageTaskMemory* MyMemory = reinterpret_cast<FBTMontageTaskMemory*>(NodeMemory);
    if (OwnerComp.GetWorld())
    {
        OwnerComp.GetWorld()->GetTimerManager().ClearTimer(MyMemory->TimerHandle);
    }

    return EBTNodeResult::Aborted;
}

void UBTTask_PlayBlackboardMontage::OnMontageTimerExpired(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{
    // Timer fired, finish the latent task
    // Clear timer just in case
    FBTMontageTaskMemory* MyMemory = reinterpret_cast<FBTMontageTaskMemory*>(NodeMemory);
    if (OwnerComp && OwnerComp->GetWorld())
    {
        OwnerComp->GetWorld()->GetTimerManager().ClearTimer(MyMemory->TimerHandle);
    }

    FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}
