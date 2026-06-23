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
        BestActionKey = BB->GetKeyID(FName("BestAction"));
        CanActKey = BB->GetKeyID(FName("CanAct"));
    }
}

void UBTService_UtilitySelector::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    // If AI is currently performing an action (montage/slide/move) skip evaluation to reduce polling
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (AICon)
    {
        APawn* Pawn = AICon->GetPawn();
        if (Pawn)
        {
            if (ASeattleAI* AI = Cast<ASeattleAI>(Pawn))
            {
                if (AI->bIsPerformingAction)
                {
                    if (AI->Health < LowHealthThreshold) 
                    {

                    }
                    else
                    {
                        return; // skip scoring while action in progress
                    }
                }
            }
        }
    }

    EvaluateActions(OwnerComp);
}

void UBTService_UtilitySelector::EvaluateActions(UBehaviorTreeComponent& OwnerComp)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return;

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon) return;

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn) return;

    ASeattleAI* AI = Cast<ASeattleAI>(Pawn);
    if (!AI) return;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
    if (!Target) return;

    const bool bCanAct = BB->GetValueAsBool(FName("CanAct"));
    if (bCanAct) return;

    const float Distance = FVector::Dist(Target->GetActorLocation(), AI->GetActorLocation());
    BB->SetValueAsFloat(FName("DistanceToTarget"), Distance);

    const float MeleeRange = AI->MeleeRange;
    const float AttackBuffer = AI->AttackBuffer;

    const float DesiredDistance = AI->DesiredCombatDistance;
    const float DistanceTolerance = 30.f;

    const float AIHealth = AI->MaxHealth > 0.f ? AI->Health / AI->MaxHealth : 0.f;

    float PlayerHealth = 1.f;
    if (ASeattleCharacter* Player = Cast<ASeattleCharacter>(Target))
    {
        PlayerHealth = Player->MaxHealth > 0.f ? Player->Health / Player->MaxHealth : 1.f;
    }

    const bool bLowStamina = BB->GetValueAsInt(FName("CurrentStamina")) < 1;

    if (bLowStamina)
    {
        BB->SetValueAsInt(FName("BestAction"), (int32)ECombatAction::Idle);
        BB->SetValueAsBool(FName("CanAct"), true);
        return;
    }

    float BestScore = -FLT_MAX;
    ECombatAction BestAction = ECombatAction::Idle;

    auto ApplyNoise = []()
        {
            return FMath::FRandRange(0.f, 10.f);
        };

    auto AddRepeatPenalty = [&](ECombatAction Action, float Score)
        {
            if (AI->LastActionPerformed == Action)
            {
                Score -= 20.f;
            }
            return Score;
        };

    // =========================
    // MOVE TO
    // =========================
    float MoveScore = 0.f;

    if (Distance > DesiredDistance + DistanceTolerance)
    {
        MoveScore += 200.f;
    }

    // discourage chasing when low health
    MoveScore *= (0.5f + AIHealth);

    MoveScore += ApplyNoise();
    MoveScore = AddRepeatPenalty(ECombatAction::MoveTo, MoveScore);

    if (MoveScore > BestScore)
    {
        BestScore = MoveScore;
        BestAction = ECombatAction::MoveTo;
    }

    // =========================
    // JAB
    // =========================
    float JabScore = 0.f;

    if (Distance <= MeleeRange + AttackBuffer)
    {
        JabScore += 80.f;
    }

    JabScore += (AIHealth > PlayerHealth) ? 40.f : 10.f;

    JabScore += ApplyNoise();
    JabScore = AddRepeatPenalty(ECombatAction::Jab, JabScore);

    if (JabScore > BestScore)
    {
        BestScore = JabScore;
        BestAction = ECombatAction::Jab;
    }

    // =========================
    // HOOK
    // =========================
    float HookScore = 0.f;

    if (Distance <= MeleeRange + AttackBuffer)
    {
        HookScore += 75.f;
    }

    HookScore += (AIHealth > PlayerHealth) ? 35.f : 15.f;

    HookScore += ApplyNoise();
    HookScore = AddRepeatPenalty(ECombatAction::Hook, HookScore);

    if (HookScore > BestScore)
    {
        BestScore = HookScore;
        BestAction = ECombatAction::Hook;
    }

    // =========================
    // SLIDES (DEFENSIVE)
    // =========================
    float SlideBase = 50.f;

    float SlideLeftScore = SlideBase;
    float SlideRightScore = SlideBase;
    float SlideBackScore = SlideBase;

    // If player stronger → increase defensive movement
    if (PlayerHealth > AIHealth)
    {
        SlideLeftScore += 25.f;
        SlideRightScore += 25.f;
        SlideBackScore += 35.f;
    }

    // If AI is stronger → reduce panic movement
    if (AIHealth > PlayerHealth)
    {
        SlideLeftScore -= 20.f;
        SlideRightScore -= 20.f;
        SlideBackScore -= 30.f;
    }

    // low health = more evasive
    if (AIHealth < 0.3f)
    {
        SlideBackScore += 60.f;
        SlideLeftScore += 25.f;
        SlideRightScore += 25.f;
    }

    SlideLeftScore += ApplyNoise();
    SlideRightScore += ApplyNoise();
    SlideBackScore += ApplyNoise();

    SlideLeftScore = AddRepeatPenalty(ECombatAction::SlideLeft, SlideLeftScore);
    SlideRightScore = AddRepeatPenalty(ECombatAction::SlideRight, SlideRightScore);
    SlideBackScore = AddRepeatPenalty(ECombatAction::SlideBack, SlideBackScore);

    if (SlideLeftScore > BestScore)
    {
        BestScore = SlideLeftScore;
        BestAction = ECombatAction::SlideLeft;
    }

    if (SlideRightScore > BestScore)
    {
        BestScore = SlideRightScore;
        BestAction = ECombatAction::SlideRight;
    }

    if (SlideBackScore > BestScore)
    {
        BestScore = SlideBackScore;
        BestAction = ECombatAction::SlideBack;
    }

    // =========================
    // FINAL WRITE
    // =========================
    BB->SetValueAsInt(FName("BestAction"), (int32)BestAction);
    BB->SetValueAsBool(FName("CanAct"), true);

    AI->LastActionPerformed = BestAction;
}