#include "BTTask_ExecuteBestAction.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "SeattleAI.h"
#include "SeattleAIController.h"
#include "SeattlePlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_ExecuteBestAction::UBTTask_ExecuteBestAction()
{
    NodeName = TEXT("Execute Best Action");
}

EBTNodeResult::Type UBTTask_ExecuteBestAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
  //  UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI stestinghere this meant execute task actually ran"));

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
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

    ASeattleAI* AI = Cast<ASeattleAI>(Pawn);
    if (!AI)
    {
        return EBTNodeResult::Failed;
    }

    const int32 BestInt = BB->GetValueAsInt(FName("BestAction"));
    ECombatAction BestAction = (ECombatAction)BestInt;

    // Log raw read and verify key id
    const int32 BestKeyId = BB->GetKeyID(FName("BestAction"));

    // mark acting lock (set CanAct = false to indicate we're now executing)
    BB->SetValueAsBool(FName("CanAct"), false);
    // mark AI as performing an action so selector can skip polling

    if (BestAction != ECombatAction::MoveTo && BestAction != ECombatAction::None && BestAction != ECombatAction::Idle)
    {
        AI->bIsPerformingAction = true;
    }
    


    // Execute
    float ActionDuration = 0.1f;

    switch (BestAction)
    {
    case ECombatAction::Jab:
    {
        // pick left or right randomly
        const bool bRight = FMath::FRand() > 0.5f;
        UAnimMontage* Montage = bRight ? AI->RightJabMontage : AI->LeftAttackMontage;
        float PlayRate = bRight ? AI->RightJabPlayRate : AI->LeftAttackPlayRate;
        if (Montage)
        {
            // Validate distance before attacking
            float Dist = BB->GetValueAsFloat(FName("DistanceToTarget"));
            if (Dist - 20.f > AI->MeleeRange)
            {
                // fallback to MoveTo
                AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
                if (TargetActor)
                {
                    // Compute acceptance derived from AI melee range and buffer so we stop slightly inside melee range
                    const float Acceptance = FMath::Max(3.f, AI->MeleeRange - AI->AttackBuffer);
                    EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(TargetActor, Acceptance);
                    if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
                    {
                        FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
                        UWorld* World = OwnerComp.GetWorld();
                        if (World)
                        {
                            FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_ExecuteBestAction::OnMovePoll, &OwnerComp, NodeMemory);
                            World->GetTimerManager().SetTimer(Memory->TimerHandle, Delegate, 0.05f, true);
                            // initialize movement safety net
                            if (TargetActor && AI)
                            {
                                Memory->LastDistance = FVector::Dist(TargetActor->GetActorLocation(), AI->GetActorLocation());
                                Memory->StalePollCount = 0;
                            }
                            return EBTNodeResult::InProgress;
                        }
                    }
                    else
                    {
                        // move request failed -> clear CanAct and fail
                        BB->SetValueAsBool(FName("CanAct"), false);
                        return EBTNodeResult::Failed;
                    }
                }
            }
            else
            {
                AI->RequestPlayAttackMontage(Montage, PlayRate);
                // record jab time for cooldown
                if (OwnerComp.GetWorld())
                {
                    AI->LastJabTime = OwnerComp.GetWorld()->GetTimeSeconds();
                }
                ActionDuration = Montage->GetPlayLength() / FMath::Max(PlayRate, 0.0001f);

            }
        }
        break;
    }

    case ECombatAction::KeepDistance:
    {
        // special keep-distance action: perform a slide-back and enable keep-distance mode on AI
        /*UE_LOG(LogTemp, Warning, TEXT("asdfjkl slide should not actually perform?"));
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
        FVector Dir = AI->GetActorForwardVector();
        if (Target)
        {
            FVector ToTarget = (Target->GetActorLocation() - AI->GetActorLocation()).GetSafeNormal();
            Dir = -ToTarget;
        }
        AI->DoAISlide(Dir);
        AI->RecordAction(ECombatAction::SlideBack);
        // mark keep-distance consumed so we only do this once until health changes
        AI->bKeepDistanceConsumed = true;
        AI->bKeepDistanceActive = false;
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI KeepDistance initiated; LastAction=SlideBack"));
        ActionDuration = AI->SlideDuration > 0.f ? AI->SlideDuration : DefaultSlideDuration;
        break;*/
    }
    case ECombatAction::Hook:
    {
        const bool bRight = FMath::FRand() > 0.5f;
        UAnimMontage* Montage = bRight ? AI->RightHookMontage : AI->LeftHookMontage;
        float PlayRate = bRight ? AI->RightHookPlayRate : AI->LeftHookPlayRate;
        if (Montage)
        {
            // Validate distance before attacking
            float Dist = BB->GetValueAsFloat(FName("DistanceToTarget"));
            if (Dist - 20.f > AI->MeleeRange)
            {
                AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
                if (TargetActor)
                {
                    const float Acceptance = FMath::Max(3.f, AI->MeleeRange - AI->AttackBuffer);
                   // UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo accepted. AcceptanceRadius=%.1f (MeleeRange=%.1f AttackBuffer=%.1f)"), Acceptance, AI->MeleeRange, AI->AttackBuffer);
                    EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(TargetActor, Acceptance);
                    if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
                    {
                        FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
                        UWorld* World = OwnerComp.GetWorld();
                        if (World)
                        {
                            FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_ExecuteBestAction::OnMovePoll, &OwnerComp, NodeMemory);
                            World->GetTimerManager().SetTimer(Memory->TimerHandle, Delegate, 0.05f, true);
                            // initialize movement safety net
                            if (TargetActor && AI)
                            {
                                Memory->LastDistance = FVector::Dist(TargetActor->GetActorLocation(), AI->GetActorLocation());
                                Memory->StalePollCount = 0;
                            }
                            return EBTNodeResult::InProgress;
                        }
                    }
                    else
                    {

                        BB->SetValueAsBool(FName("IsActing"), false);
                        BB->SetValueAsInt(FName("IsActingInt"), 0);
                        if (AICon->BrainComponent) AICon->BrainComponent->RestartLogic();
                        return EBTNodeResult::Failed;
                    }
                }
            }
                else
                {
                    AI->RequestPlayAttackMontage(Montage, PlayRate);
                    // record hook time for cooldown
                    if (OwnerComp.GetWorld())
                    {
                        AI->LastHookTime = OwnerComp.GetWorld()->GetTimeSeconds();
                    }
                    // record action history
                    AI->RecordAction(ECombatAction::Hook);
                 //   UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI LastAction=Hook"));
                    ActionDuration = Montage->GetPlayLength() / FMath::Max(PlayRate, 0.0001f);

            }
        }
        break;
    }
    case ECombatAction::SlideLeft:
    {
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
        FVector Dir = AI->GetActorForwardVector();
        if (Target)
        {
            FVector ToTarget = (Target->GetActorLocation() - AI->GetActorLocation()).GetSafeNormal();
            FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal();
            Dir = -Right;
        }
        AI->DoAISlide(Dir);
        AI->RecordAction(ECombatAction::SlideLeft);
    //    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI LastAction=SlideLeft"));
        ActionDuration = AI->SlideDuration > 0.f ? AI->SlideDuration : DefaultSlideDuration;
        break;
    }

    case ECombatAction::SlideRight:
    {
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
        FVector Dir = AI->GetActorForwardVector();
        if (Target)
        {
            FVector ToTarget = (Target->GetActorLocation() - AI->GetActorLocation()).GetSafeNormal();
            FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal();
            Dir = Right;
        }
        AI->DoAISlide(Dir);
        AI->RecordAction(ECombatAction::SlideRight);
     //   UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI LastAction=SlideRight"));
        ActionDuration = AI->SlideDuration > 0.f ? AI->SlideDuration : DefaultSlideDuration;
        break;
    }

    case ECombatAction::SlideBack:
    {
      //  UE_LOG(LogTemp, Warning, TEXT("asdfjkl trying to slide back"));
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
        FVector Dir = AI->GetActorForwardVector();
        if (Target)
        {
            FVector ToTarget = (Target->GetActorLocation() - AI->GetActorLocation()).GetSafeNormal();
            Dir = -ToTarget;
        }
        AI->DoAISlide(Dir);
        AI->RecordAction(ECombatAction::SlideBack);
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI LastAction=SlideBack"));
        ActionDuration = AI->SlideDuration > 0.f ? AI->SlideDuration : DefaultSlideDuration;
        break;
    }
    case ECombatAction::MoveTo:
    {
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
        if (Target)
        {
            const float Acceptance = FMath::Max(3.f, AI->MeleeRange - AI->AttackBuffer); // aim to stop inside melee range
       //     UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo accepted. AcceptanceRadius=%.1f (MeleeRange=%.1f AttackBuffer=%.1f)"), Acceptance, AI->MeleeRange, AI->AttackBuffer);
            EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(Target, Acceptance);

            // If move request started, poll distance and trigger immediate reevaluation when within range
            if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
            {
                FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
                UWorld* World = OwnerComp.GetWorld();
                    if (World)
                    {
                        float Distance = BB->GetValueAsFloat(FName("DistanceToTarget"));
                        FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_ExecuteBestAction::OnMovePoll, &OwnerComp, NodeMemory);
                        World->GetTimerManager().SetTimer(Memory->TimerHandle, Delegate, 0.05f, true);
                        // initialize movement safety net
                        Memory->LastDistance = Distance;
                        Memory->StalePollCount = 0;
                        return EBTNodeResult::InProgress;
                    }
            }
            else
            {
                // Move request failed; clear acting so selector can try another action
       //         UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [ExecuteBestAction] MoveTo request failed (result=%d). Clearing IsActing and restarting BT."), (int)MoveResult);
                if (UBlackboardComponent* BBB = OwnerComp.GetBlackboardComponent())
                {
                    BBB->SetValueAsBool(FName("IsActing"), false);
                    BBB->SetValueAsInt(FName("IsActingInt"), 0);
                }
                if (AICon->BrainComponent)
                {
                    AICon->BrainComponent->RestartLogic();
                }
                return EBTNodeResult::Failed;
            }
        }
        break;
    }
    case ECombatAction::Idle:
    default:
		//UE_LOG(LogTemp, Warning, TEXT("stestinghere HUH?"));
        // do nothing
        break;
    }

    // If waiting enabled, set timer and be latent
    if (bWaitForAction && ActionDuration > 0.f)
    {
        FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
        UWorld* World = OwnerComp.GetWorld();
        if (World)
        {
            FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_ExecuteBestAction::OnActionTimerExpired, &OwnerComp, NodeMemory);
            World->GetTimerManager().SetTimer(Memory->TimerHandle, Delegate, ActionDuration, false);
            return EBTNodeResult::InProgress;
        }
    }

    // not waiting: clear CanAct and succeed
    BB->SetValueAsBool(FName("CanAct"), false);
    return EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTask_ExecuteBestAction::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{


    FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
    if (OwnerComp.GetWorld())
    {
        OwnerComp.GetWorld()->GetTimerManager().ClearTimer(Memory->TimerHandle);
    }

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        BB->SetValueAsBool(FName("CanAct"), false);
    }

    // clear performing flag on AI so selector can resume
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (AICon)
    {
        APawn* Pawn = AICon->GetPawn();
        if (Pawn)
        {
            if (ASeattleAI* AI = Cast<ASeattleAI>(Pawn))
            {
                AI->bIsPerformingAction = false;
            }
        }
    }
    return EBTNodeResult::Aborted;
}

void UBTTask_ExecuteBestAction::OnActionTimerExpired(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{

  //  UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI ONACTION TIMER EXPIRED"));
    if (!OwnerComp) return;
    if (UBlackboardComponent* BB = OwnerComp->GetBlackboardComponent())
    {
        BB->SetValueAsBool(FName("CanAct"), false);
   //     UE_LOG(LogTemp, Verbose, TEXT("[ExecuteBestAction] Action Complete - CanAct cleared"));
    }

  //  UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [ExecuteBestAction] Action Complete"));
    FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}

void UBTTask_ExecuteBestAction::OnMovePoll(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{
    if (!OwnerComp) return;
    UBlackboardComponent* BB = OwnerComp->GetBlackboardComponent();
    if (!BB) return;

    AAIController* AICon = OwnerComp->GetAIOwner();
    if (!AICon) return;

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn) return;

    ASeattleAI* AI = Cast<ASeattleAI>(Pawn);
    if (!AI) return;
    //UE_LOG(LogTemp, Log, TEXT("stestinghere move poll keeps running"));

    // Prefer direct distance calculation to avoid blackboard lag
    float Distance = 0.f;
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
    if (Target)
    {
        Distance = FVector::Dist(Target->GetActorLocation(), AI->GetActorLocation());
    }

    FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;

    // Movement completion check
    if (Distance - 20.f <= AI->MeleeRange) // 10.f subtracted here as leeway to account for animations and offsets, etc.
    {
        // arrived within attack range
        if (OwnerComp->GetWorld())
        {
            OwnerComp->GetWorld()->GetTimerManager().ClearTimer(Memory->TimerHandle);
        }

        // arrived: clear CanAct so selector runs on next tick and finish
        BB->SetValueAsBool(FName("CanAct"), false);
       // UE_LOG(LogTemp, Verbose, TEXT("[ExecuteBestAction] MoveTo arrived - CanAct cleared"));
        FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    // Safety net: detect if distance is not decreasing across polls
    const int32 StaleThreshold = 3; // number of polls without progress before reissuing MoveTo
    if (Memory->LastDistance == FLT_MAX)
    {
        // first poll: record distance
        Memory->LastDistance = Distance;
        Memory->StalePollCount = 0;
        return;
    }

    // Consider movement progressed if distance decreased by any small amount
    if (Distance < Memory->LastDistance - KINDA_SMALL_NUMBER)
    {
        Memory->LastDistance = Distance;
        Memory->StalePollCount = 0;
        return;
    }

    // no progress this poll
    Memory->StalePollCount++;

    if (Memory->StalePollCount >= StaleThreshold)
    {
        // attempt to reissue MoveTo to recover from stuck state
        if (Target)
        {
            const float Acceptance = FMath::Max(3.f, AI->MeleeRange - AI->AttackBuffer);
            EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(Target, Acceptance);
           // UE_LOG(LogTemp, Warning, TEXT("[ExecuteBestAction] Reissuing MoveTo due to stalled polls (count=%d) MoveResult=%d"), Memory->StalePollCount, (int)MoveResult);
            // reset counters if we successfully reissued
            if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
            {
                Memory->LastDistance = Distance;
                Memory->StalePollCount = 0;
                return;
            }
            else
            {
                // reissue failed: give up, clear timer and allow selector to pick another action
                if (OwnerComp->GetWorld())
                {
                    OwnerComp->GetWorld()->GetTimerManager().ClearTimer(Memory->TimerHandle);
                }
                BB->SetValueAsBool(FName("CanAct"), false);
               // UE_LOG(LogTemp, Warning, TEXT("[ExecuteBestAction] Reissue MoveTo failed; clearing CanAct to allow re-evaluation"));
                FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
                return;
            }
        }
    }
}
