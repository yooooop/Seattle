#include "BTService_UtilitySelector.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "SeattleAI.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"

UBTService_UtilitySelector::UBTService_UtilitySelector()
{
    bCreateNodeInstance = false;
    Interval = EvaluationInterval;
    // Node will tick at Interval value set by user
}

void UBTService_UtilitySelector::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Super::OnBecomeRelevant(OwnerComp, NodeMemory);

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        TargetActorKey = BB->GetKeyID(FName("TargetActor"));
        DistanceKey = BB->GetKeyID(FName("DistanceToTarget"));
        PlayerIsAttackingKey = BB->GetKeyID(FName("PlayerIsAttacking"));
        CurrentStaminaKey = BB->GetKeyID(FName("CurrentStamina"));
        HasStaminaKey = BB->GetKeyID(FName("HasStamina"));
        BestActionKey = BB->GetKeyID(FName("BestAction"));
        IsActingKey = BB->GetKeyID(FName("IsActing"));

    }
}

void UBTService_UtilitySelector::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    EvaluateActions(OwnerComp);
}

void UBTService_UtilitySelector::EvaluateActions(UBehaviorTreeComponent& OwnerComp)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
        return;
    }

    // Simple existence log so we can verify the selector runs
 
    // Do not evaluate if already acting. Support both Bool and Int variants for editor decorator compatibility.
    const int32 IsActingInt = BB->GetValueAsInt(FName("IsActingInt"));
    UE_LOG(LogTemp, Warning, TEXT("testinghere UtilitySelector: IsActingInt=%d Controller=%s Pawn=%s Target=%s DistanceKey=%d"), IsActingInt, *GetNameSafe(OwnerComp.GetAIOwner()), *GetNameSafe(OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr), *GetNameSafe(Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")))), (int)DistanceKey);

    if (IsActingInt > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("testinghere IsActing skip (IsActingInt>0) Controller=%s"), *GetNameSafe(OwnerComp.GetAIOwner()));
        return;
    }
    if (BB->GetValueAsBool(FName("IsActing")))
    {
        UE_LOG(LogTemp, Warning, TEXT("testinghere IsActing skip (IsActing=true) Controller=%s"), *GetNameSafe(OwnerComp.GetAIOwner()));
        return;
    }

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
    {
        return;
    }

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn)
    {
        return;
    }

    ASeattleAI* AI = Cast<ASeattleAI>(Pawn);

    if (!AI)
    {
        // AI cast failed
    }

    // Gather inputs
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
    float Distance = BB->GetValueAsFloat(FName("DistanceToTarget"));
    // prefer direct distance calculation when possible
    if (Target && AI)
    {
        Distance = FVector::Dist(Target->GetActorLocation(), AI->GetActorLocation());
        BB->SetValueAsFloat(FName("DistanceToTarget"), Distance);
    }

    // If target is out of attack range, force MoveTo regardless of IsActing so AI closes distance
    float MeleeRange = AI ? AI->MeleeRange : 0.f;
    float EffectiveStopRange = MeleeRange - (AI ? AI->AttackBuffer : 0.f);
    if (Distance > EffectiveStopRange)
    {
        int32 ChosenInt = (int32)ECombatAction::MoveTo;
        // clear acting state so selector/task can immediately run MoveTo
        BB->SetValueAsBool(FName("IsActing"), false);
        BB->SetValueAsInt(FName("IsActingInt"), 0);
        BB->SetValueAsInt(FName("BestAction"), ChosenInt);
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI MoveTo override: AI=%s Distance=%.1f MeleeRange=%.1f EffectiveStopRange=%.1f"), *GetNameSafe(AI), Distance, MeleeRange, EffectiveStopRange);
        return;
    }
    bool bPlayerAttacking = BB->GetValueAsBool(FName("PlayerIsAttacking"));
    int32 CurrentStamina = BB->GetValueAsInt(FName("CurrentStamina"));
    bool bHasStamina = BB->GetValueAsBool(FName("HasStamina"));

    // Evaluation inputs gathered

    // If target is out of attack range, prioritize MoveTo
    if (Distance > EffectiveStopRange)
    {
        int32 ChosenInt = (int32)ECombatAction::MoveTo;
        BB->SetValueAsInt(FName("BestAction"), ChosenInt);
        return;
    }

    // If no stamina, deprioritize attacks/dodges
    if (!bHasStamina || CurrentStamina <= 0)
    {
        // choose MoveTo when far, else Idle (use authoritative MeleeRange)
        ECombatAction Chosen = (Distance > MeleeRange * 1.2f) ? ECombatAction::MoveTo : ECombatAction::Idle;
        int32 ChosenInt = (int32)Chosen;

        BB->SetValueAsInt(FName("BestAction"), ChosenInt);
        // verify write
        int32 Verify = BB->GetValueAsInt(FName("BestAction"));

        return;
    }

    // compute base scores
    TArray<TPair<ECombatAction, float>> Scores;

    // default scores
    Scores.Add({ECombatAction::Jab, 0.f});
    Scores.Add({ECombatAction::Hook, 0.f});
    Scores.Add({ECombatAction::SlideLeft, 0.f});
    Scores.Add({ECombatAction::SlideRight, 0.f});
    Scores.Add({ECombatAction::SlideBack, 0.f});
    Scores.Add({ECombatAction::SlideForward, 0.f});
    Scores.Add({ECombatAction::MoveTo, 0.f});
    Scores.Add({ECombatAction::Idle, IdleDefaultScore});

    // Random seed
    const float Rand01 = FMath::FRand();

    // Dodge logic: if player attacking, high scores for dodge
    if (bPlayerAttacking)
    {
        // Only consider dodges if close enough, AI has stamina, and AI has enough stamina to slide
        if (!bHasStamina || Distance > DodgeRange || CurrentStamina < MinStaminaToSlide)
        {

            // deprioritize dodge actions
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideLeft;})->Value = 1.f;
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideRight;})->Value = 1.f;
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideBack;})->Value = 1.f;
        }
        else
        {
            const float dodgeRoll = FMath::FRand();
            if (dodgeRoll < DodgeChance)
            {
                // prefer dodge directions using tuned values
                Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideLeft;})->Value = SlideLeftBase + FMath::FRandRange(0.f, SlideRandomRange);
                Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideRight;})->Value = SlideRightBase + FMath::FRandRange(0.f, SlideRandomRange);
                Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideBack;})->Value = SlideBackBase + FMath::FRandRange(0.f, SlideRandomRange);

            }
            else
            {

            }
        }
    }

    // Attack cooldown handling: penalize Jab/Hook if within cooldown
    float Now = OwnerComp.GetWorld() ? OwnerComp.GetWorld()->GetTimeSeconds() : 0.f;
    if (AI)
    {
        float TimeSinceJab = Now - AI->LastJabTime;
        if (TimeSinceJab < JabCooldown)
        {
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::Jab;})->Value = 1.f;

        }
        float TimeSinceHook = Now - AI->LastHookTime;
        if (TimeSinceHook < HookCooldown)
        {
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::Hook;})->Value = 1.f;

        }
    }

    // Attack vs distance
    if (Distance <= MeleeRange)
    {
        // Jab favored due to speed
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::Jab;})->Value = JabBaseScore + FMath::FRandRange(0.f, JabRandomRange);
        // Hook slower but higher damage
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::Hook;})->Value = HookBaseScore + FMath::FRandRange(0.f, HookRandomRange);
        // small chance to step forward and attack (less likely when already in range)
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideForward;})->Value = MoveToCloseScore * 0.5f;
    }
    else if (Distance <= MeleeRange * 1.5f)
    {
        // close but just outside: prefer SlideForward only if within padding beyond AttackRange
        if (Distance <= MeleeRange + SlideForwardPadding)
        {
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideForward;})->Value = SlideForwardNearScore + FMath::FRandRange(0.f, SlideForwardRandom);
        }
        else
        {
            // otherwise prefer MoveTo
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideForward;})->Value = SlideForwardFarScore;

        }
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::MoveTo;})->Value = MoveToCloseScore + FMath::FRandRange(0.f,20.f);
    }
    else
    {
        // far: move towards
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::MoveTo;})->Value = MoveToFarScore + FMath::FRandRange(0.f,10.f);
    }

    // Small randomness to encourage variability
    for (auto& Pair : Scores)
    {
        Pair.Value += FMath::FRandRange(-5.f, 5.f);
    }

    // Apply repeat penalties based on recent actions recorded on AI (reduce scores, do not forbid)
    if (AI)
    {
        // Count occurrences in recent actions
        for (auto& Pair : Scores)
        {
            int32 Count = 0;
            for (auto& Recent : AI->RecentActions)
            {
                if ((ECombatAction)Recent == Pair.Key) Count++;
            }
            if (Count > 0)
            {
                float Penalty = 0.f;
                switch (Pair.Key)
                {
                    case ECombatAction::Jab: Penalty = JabRepeatPenalty * Count; break;
                    case ECombatAction::Hook: Penalty = HookRepeatPenalty * Count; break;
                    case ECombatAction::SlideLeft:
                    case ECombatAction::SlideRight:
                    case ECombatAction::SlideForward:
                    case ECombatAction::SlideBack: Penalty = SlideRepeatPenalty * Count; break;
                    default: Penalty = 0.f; break;
                }
                if (Penalty > 0.f)
                {
                    Pair.Value = FMath::Max(0.f, Pair.Value - Penalty);
                    FString ActionName = StaticEnum<ECombatAction>()->GetNameStringByValue((int64)Pair.Key);
                    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI Applying repeat penalty to %s (count=%d penalty=%.1f) NewScore=%.2f"), *ActionName, Count, Penalty, Pair.Value);
                }
            }
        }
    }

    // pick highest score
    ECombatAction Best = ECombatAction::Idle;
    float BestScore = -FLT_MAX;
    for (const auto& Pair : Scores)
    {
        if (Pair.Value > BestScore)
        {
            BestScore = Pair.Value;
            Best = Pair.Key;
        }
    }

    // Single consolidated score line for easy filtering/debugging
    {
        FString Consolidated = TEXT("");
        for (const auto& P : Scores)
        {
            FString N;
            switch (P.Key)
            {
                case ECombatAction::Jab: N = TEXT("Jab"); break;
                case ECombatAction::Hook: N = TEXT("Hook"); break;
                case ECombatAction::SlideLeft: N = TEXT("SlideLeft"); break;
                case ECombatAction::SlideRight: N = TEXT("SlideRight"); break;
                case ECombatAction::SlideBack: N = TEXT("SlideBack"); break;
                case ECombatAction::SlideForward: N = TEXT("SlideForward"); break;
                case ECombatAction::MoveTo: N = TEXT("MoveTo"); break;
                case ECombatAction::Idle: N = TEXT("Idle"); break;
                default: N = TEXT("Unknown"); break;
            }
            Consolidated += FString::Printf(TEXT("%s=%.2f "), *N, P.Value);
        }
        UE_LOG(LogTemp, Warning, TEXT("testinghere Scores: %s"), *Consolidated);
    }

    // scoring evaluation complete
    int32 BestIntVal = (int32)Best;
    // Log chosen action for debugging
    {
        FString BestName;
        switch (Best)
        {
            case ECombatAction::Jab: BestName = TEXT("Jab"); break;
            case ECombatAction::Hook: BestName = TEXT("Hook"); break;
            case ECombatAction::SlideLeft: BestName = TEXT("SlideLeft"); break;
            case ECombatAction::SlideRight: BestName = TEXT("SlideRight"); break;
            case ECombatAction::SlideBack: BestName = TEXT("SlideBack"); break;
            case ECombatAction::SlideForward: BestName = TEXT("SlideForward"); break;
            case ECombatAction::MoveTo: BestName = TEXT("MoveTo"); break;
            case ECombatAction::Idle: BestName = TEXT("Idle"); break;
            default: BestName = TEXT("Unknown"); break;
        }
        UE_LOG(LogTemp, Warning, TEXT("testinghere Selected=%s BestScore=%.2f"), *BestName, BestScore);
    }
    BB->SetValueAsInt(FName("BestAction"), BestIntVal);
}
