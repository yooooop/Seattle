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
    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI testinghere this meant execute task actually ran"));

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

    // mark acting (set both Bool and Int forms for editor compatibility)
    bool PrevBool = BB->GetValueAsBool(FName("IsActing"));
    int32 PrevInt = BB->GetValueAsInt(FName("IsActingInt"));
    BB->SetValueAsBool(FName("IsActing"), true);
    BB->SetValueAsInt(FName("IsActingInt"), 1);


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
            if (Dist > AI->MeleeRange)
            {
                // fallback to MoveTo
                AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
                if (TargetActor)
                {
                    // Compute acceptance derived from AI melee range and buffer so we stop slightly inside melee range
                    const float Acceptance = FMath::Max(3.f, AI->MeleeRange - AI->AttackBuffer);
                    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo accepted. AcceptanceRadius=%.1f (MeleeRange=%.1f AttackBuffer=%.1f)"), Acceptance, AI->MeleeRange, AI->AttackBuffer);
                    EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(TargetActor, Acceptance);
                    if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
                    {
                        FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
                        UWorld* World = OwnerComp.GetWorld();
                        if (World)
                        {
                            FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_ExecuteBestAction::OnMovePoll, &OwnerComp, NodeMemory);
                            World->GetTimerManager().SetTimer(Memory->TimerHandle, Delegate, 0.05f, true);
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
    case ECombatAction::Hook:
    {
        const bool bRight = FMath::FRand() > 0.5f;
        UAnimMontage* Montage = bRight ? AI->RightHookMontage : AI->LeftHookMontage;
        float PlayRate = bRight ? AI->RightHookPlayRate : AI->LeftHookPlayRate;
        if (Montage)
        {
            // Validate distance before attacking
            float Dist = BB->GetValueAsFloat(FName("DistanceToTarget"));
            if (Dist > AI->MeleeRange)
            {
                AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
                if (TargetActor)
                {
                    const float Acceptance = FMath::Max(3.f, AI->MeleeRange - AI->AttackBuffer);
                    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo accepted. AcceptanceRadius=%.1f (MeleeRange=%.1f AttackBuffer=%.1f)"), Acceptance, AI->MeleeRange, AI->AttackBuffer);
                    EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(TargetActor, Acceptance);
                    if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
                    {
                        FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
                        UWorld* World = OwnerComp.GetWorld();
                        if (World)
                        {
                            FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_ExecuteBestAction::OnMovePoll, &OwnerComp, NodeMemory);
                            World->GetTimerManager().SetTimer(Memory->TimerHandle, Delegate, 0.05f, true);
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
                    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI LastAction=Hook"));
                    ActionDuration = Montage->GetPlayLength() / FMath::Max(PlayRate, 0.0001f);

            }
        }
        break;
    }
    case ECombatAction::SlideLeft:
    case ECombatAction::SlideRight:
    case ECombatAction::SlideBack:
    case ECombatAction::SlideForward:
    {
        // compute direction relative to player
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
        FVector Dir = AI->GetActorForwardVector();
        if (Target)
        {
            FVector ToTarget = (Target->GetActorLocation() - AI->GetActorLocation()).GetSafeNormal();
            // lateral vector
            FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget).GetSafeNormal();
            if (BestAction == ECombatAction::SlideLeft)
            {
                Dir = -Right;
            }
            else if (BestAction == ECombatAction::SlideRight)
            {
                Dir = Right;
            }
            else if (BestAction == ECombatAction::SlideBack)
            {
                Dir = -ToTarget;
            }
            else // forward
            {
                Dir = ToTarget;
            }
        }
        AI->DoAISlide(Dir);
        // record slide action
        if (BestAction == ECombatAction::SlideLeft) AI->RecordAction(ECombatAction::SlideLeft);
        else if (BestAction == ECombatAction::SlideRight) AI->RecordAction(ECombatAction::SlideRight);
        else if (BestAction == ECombatAction::SlideBack) AI->RecordAction(ECombatAction::SlideBack);
        else AI->RecordAction(ECombatAction::SlideForward);
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI LastAction=%s"), *StaticEnum<ECombatAction>()->GetNameStringByValue((int64)BestAction));
        ActionDuration = AI->SlideDuration > 0.f ? AI->SlideDuration : DefaultSlideDuration;
        break;
    }
    case ECombatAction::MoveTo:
    {
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
        if (Target)
        {
            const float Acceptance = FMath::Max(3.f, AI->MeleeRange - AI->AttackBuffer); // aim to stop inside melee range
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo accepted. AcceptanceRadius=%.1f (MeleeRange=%.1f AttackBuffer=%.1f)"), Acceptance, AI->MeleeRange, AI->AttackBuffer);
            EPathFollowingRequestResult::Type MoveResult = AICon->MoveToActor(Target, Acceptance);

            // If move request started, poll distance and trigger immediate reevaluation when within range
            if (MoveResult == EPathFollowingRequestResult::RequestSuccessful || MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
            {
                FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
                UWorld* World = OwnerComp.GetWorld();
                    if (World)
                    {
                        FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UBTTask_ExecuteBestAction::OnMovePoll, &OwnerComp, NodeMemory);
                        World->GetTimerManager().SetTimer(Memory->TimerHandle, Delegate, 0.05f, true);
                        return EBTNodeResult::InProgress;
                    }
            }
            else
            {
                // Move request failed; clear acting so selector can try another action
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [ExecuteBestAction] MoveTo request failed (result=%d). Clearing IsActing and restarting BT."), (int)MoveResult);
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

    // not waiting: clear IsActing immediately and succeed (clear both Bool and Int forms)
    {
        bool PrevB = BB->GetValueAsBool(FName("IsActing"));
        int32 PrevI = BB->GetValueAsInt(FName("IsActingInt"));
        BB->SetValueAsBool(FName("IsActing"), false);
        BB->SetValueAsInt(FName("IsActingInt"), 0);
    }
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
        bool PrevB = BB->GetValueAsBool(FName("IsActing"));
        int32 PrevI = BB->GetValueAsInt(FName("IsActingInt"));
        BB->SetValueAsBool(FName("IsActing"), false);
        BB->SetValueAsInt(FName("IsActingInt"), 0);
    }
    return EBTNodeResult::Aborted;
}

void UBTTask_ExecuteBestAction::OnActionTimerExpired(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{

    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI ONACTION TIMER EXPIRED"));
    if (!OwnerComp) return;
    if (UBlackboardComponent* BB = OwnerComp->GetBlackboardComponent())
    {
        bool PrevB = BB->GetValueAsBool(FName("IsActing"));
        int32 PrevI = BB->GetValueAsInt(FName("IsActingInt"));
        BB->SetValueAsBool(FName("IsActing"), false);
        BB->SetValueAsInt(FName("IsActingInt"), 0);
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [ExecuteBestAction] Action Complete. IsActing previous: Bool=%d Int=%d -> new Bool=0 Int=0"), PrevB ? 1 : 0, PrevI);
    }

    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [ExecuteBestAction] Action Complete"));
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

    // Prefer direct distance calculation to avoid blackboard lag
    float Distance = 0.f;
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
    if (Target)
    {
        Distance = FVector::Dist(Target->GetActorLocation(), AI->GetActorLocation());
    }
    if (Distance <= AI->MeleeRange)
    {
        // arrived within attack range
        FExecActionMemory* Memory = (FExecActionMemory*)NodeMemory;
        if (OwnerComp->GetWorld())
        {
            OwnerComp->GetWorld()->GetTimerManager().ClearTimer(Memory->TimerHandle);
        }

        // clear IsActing so selector can run immediately
        bool PrevB = BB->GetValueAsBool(FName("IsActing"));
        int32 PrevI = BB->GetValueAsInt(FName("IsActingInt"));
        BB->SetValueAsBool(FName("IsActing"), false);
        BB->SetValueAsInt(FName("IsActingInt"), 0);
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo finished. DistanceToTarget=%.1f MeleeRange=%.1f"), Distance, AI->MeleeRange);

        // trigger immediate BT reevaluation
            if (AICon->BrainComponent)
            {
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI AI entered combat range: AI=%s"), *GetNameSafe(AI));
                AICon->BrainComponent->RestartLogic();
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo completed - triggered immediate utility reevaluation via BrainComponent->RestartLogic()"));
            }

        FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
    }
    else
    {
        // still moving; nothing to do (poll will continue)
    }
}
