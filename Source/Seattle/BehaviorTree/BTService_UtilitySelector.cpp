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
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] OnBecomeRelevant - Cached Blackboard keys: BestActionKey=%d IsActingKey=%d"), (int)BestActionKey, (int)IsActingKey);
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

    // Do not evaluate if already acting. Support both Bool and Int variants for editor decorator compatibility.
    const int32 IsActingInt = BB->GetValueAsInt(FName("IsActingInt"));
    if (IsActingInt > 0)
    {
        return;
    }
    if (BB->GetValueAsBool(FName("IsActing")))
    {
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
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] ASeattleAI cast failed for Pawn=%s"), *GetNameSafe(Pawn));
    }

    // Gather inputs
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
    float Distance = BB->GetValueAsFloat(FName("DistanceToTarget"));
    bool bPlayerAttacking = BB->GetValueAsBool(FName("PlayerIsAttacking"));
    int32 CurrentStamina = BB->GetValueAsInt(FName("CurrentStamina"));
    bool bHasStamina = BB->GetValueAsBool(FName("HasStamina"));

    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Evaluate - AI=%s Target=%s Distance=%.1f CurrentStamina=%d HasStamina=%d PlayerIsAttacking=%d IsActing=%d IsActingInt=%d"),
        *GetNameSafe(AI), *GetNameSafe(Target), Distance, CurrentStamina, bHasStamina ? 1 : 0, bPlayerAttacking ? 1 : 0,
        BB->GetValueAsBool(FName("IsActing")) ? 1 : 0, BB->GetValueAsInt(FName("IsActingInt")));

    // If target is out of attack range, force MoveTo as the only action: prioritize moving into range.
    if (Distance > AttackRange)
    {
        int32 ChosenInt = (int32)ECombatAction::MoveTo;
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Target out of range (Distance=%.1f AttackRange=%.1f) -> forcing MoveTo"), Distance, AttackRange);
        BB->SetValueAsInt(FName("BestAction"), ChosenInt);
        int32 Verify = BB->GetValueAsInt(FName("BestAction"));
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Forced BestAction int=%d ReadBack=%d"), ChosenInt, Verify);
        return;
    }

    // If no stamina, deprioritize attacks/dodges
    if (!bHasStamina || CurrentStamina <= 0)
    {
        // choose MoveTo when far, else Idle
        ECombatAction Chosen = (Distance > AttackRange * 1.2f) ? ECombatAction::MoveTo : ECombatAction::Idle;
        int32 ChosenInt = (int32)Chosen;
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] No stamina -> Selected %d (int)"), ChosenInt);
        BB->SetValueAsInt(FName("BestAction"), ChosenInt);
        // verify write
        int32 Verify = BB->GetValueAsInt(FName("BestAction"));
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Wrote BestAction int=%d ReadBack=%d"), ChosenInt, Verify);
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
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Player attacking but dodge rejected. HasStamina=%d Distance=%.1f DodgeRange=%.1f CurrentStamina=%d MinStaminaToSlide=%d"), bHasStamina ? 1 : 0, Distance, DodgeRange, CurrentStamina, MinStaminaToSlide);
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
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Player attacking -> dodge roll=%.2f DodgeChance=%.2f"), dodgeRoll, DodgeChance);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Player attacking -> dodge roll failed (%.2f)"), dodgeRoll);
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
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Jab penalized due to cooldown (since=%.2f cooldown=%.2f)"), TimeSinceJab, JabCooldown);
        }
        float TimeSinceHook = Now - AI->LastHookTime;
        if (TimeSinceHook < HookCooldown)
        {
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::Hook;})->Value = 1.f;
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Hook penalized due to cooldown (since=%.2f cooldown=%.2f)"), TimeSinceHook, HookCooldown);
        }
    }

    // Attack vs distance
    if (Distance <= AttackRange)
    {
        // Jab favored due to speed
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::Jab;})->Value = JabBaseScore + FMath::FRandRange(0.f, JabRandomRange);
        // Hook slower but higher damage
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::Hook;})->Value = HookBaseScore + FMath::FRandRange(0.f, HookRandomRange);
        // small chance to step forward and attack (less likely when already in range)
        Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideForward;})->Value = MoveToCloseScore * 0.5f;
    }
    else if (Distance <= AttackRange * 1.5f)
    {
        // close but just outside: prefer SlideForward only if within padding beyond AttackRange
        if (Distance <= AttackRange + SlideForwardPadding)
        {
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideForward;})->Value = SlideForwardNearScore + FMath::FRandRange(0.f, SlideForwardRandom);
        }
        else
        {
            // otherwise prefer MoveTo
            Scores.FindByPredicate([&](const TPair<ECombatAction,float>& P){return P.Key==ECombatAction::SlideForward;})->Value = SlideForwardFarScore;
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] SlideForward rejected because target too far for forward slide (Distance=%.1f AttackRange=%.1f Padding=%.1f)"), Distance, AttackRange, SlideForwardPadding);
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

    // log each score
    for (const auto& Pair : Scores)
    {
        FString Name;
        switch (Pair.Key)
        {
            case ECombatAction::Jab: Name = TEXT("Jab"); break;
            case ECombatAction::Hook: Name = TEXT("Hook"); break;
            case ECombatAction::SlideLeft: Name = TEXT("SlideLeft"); break;
            case ECombatAction::SlideRight: Name = TEXT("SlideRight"); break;
            case ECombatAction::SlideBack: Name = TEXT("SlideBack"); break;
            case ECombatAction::SlideForward: Name = TEXT("SlideForward"); break;
            case ECombatAction::MoveTo: Name = TEXT("MoveTo"); break;
            case ECombatAction::Idle: Name = TEXT("Idle"); break;
            default: Name = TEXT("Unknown"); break;
        }
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] %s Score = %.2f"), *Name, Pair.Value);
    }

    // log each score
    for (const auto& Pair : Scores)
    {
        FString Name;
        switch (Pair.Key)
        {
            case ECombatAction::Jab: Name = TEXT("Jab"); break;
            case ECombatAction::Hook: Name = TEXT("Hook"); break;
            case ECombatAction::SlideLeft: Name = TEXT("SlideLeft"); break;
            case ECombatAction::SlideRight: Name = TEXT("SlideRight"); break;
            case ECombatAction::SlideBack: Name = TEXT("SlideBack"); break;
            case ECombatAction::SlideForward: Name = TEXT("SlideForward"); break;
            case ECombatAction::MoveTo: Name = TEXT("MoveTo"); break;
            case ECombatAction::Idle: Name = TEXT("Idle"); break;
            default: Name = TEXT("Unknown"); break;
        }
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] %s Score = %.2f"), *Name, Pair.Value);
    }

    // write to blackboard as integer (ensure ExecuteTask reads same key)
    int32 BestIntVal = (int32)Best;
    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] About to write BestAction int=%d (key=BestAction)"), BestIntVal);
    BB->SetValueAsInt(FName("BestAction"), BestIntVal);
    int32 ReadBack = BB->GetValueAsInt(FName("BestAction"));
    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Wrote BestAction int=%d ReadBack=%d"), BestIntVal, ReadBack);
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
    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UtilitySelector] Selected BestAction = %s BestScore = %.2f"), *BestName, BestScore);
}
