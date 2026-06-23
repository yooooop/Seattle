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

    UE_LOG(LogTemp, Warning, TEXT("asdfjkl no way right"));

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return;

    // Only evaluate when not currently acting and when target exists
    bool bCanAct = BB->GetValueAsBool(FName("CanAct"));
    if (bCanAct) return;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));
    if (!Target) return;

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon) return;

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn) return;

    UE_LOG(LogTemp, Warning, TEXT("asdfjkl no way right2"));

    ASeattleAI* AI = Cast<ASeattleAI>(Pawn);

    if (AI)
    {
        if (AI->Health < AI->LastRecordedHealth)
        {
            AI->LastRecordedHealth = AI->Health;
            AI->bRetreatedThisHealthLoss = false;
        }
    }

    // Gather context
    float Distance = BB->GetValueAsFloat(FName("DistanceToTarget"));
    if (AI && Target)
    {
        Distance = FVector::Dist(Target->GetActorLocation(), AI->GetActorLocation());
        BB->SetValueAsFloat(FName("DistanceToTarget"), Distance);
    }

    // If we've successfully backed away enough, resume normal combat spacing.
    if (AI &&
        AI->DesiredCombatDistance > AI->MeleeRange &&
        Distance >= AI->DesiredCombatDistance - 20.f)
    {
        AI->DesiredCombatDistance = AI->MeleeRange;
    }

    bool bPlayerAttacking = BB->GetValueAsBool(FName("PlayerIsAttacking"));
    int32 CurrentStamina = BB->GetValueAsInt(FName("CurrentStamina"));

    // If keep-distance mode active, enforce it: slide back when too close, clear mode when desired distance reached
    /*if (AI && AI->bKeepDistanceActive && !AI->bKeepDistanceConsumed)
    {
        const float Keep = AI->KeepDistanceRadius;
        const float Tolerance = 10.f;
        if (Distance < Keep - Tolerance)
        {
            // too close -> slide back
            BB->SetValueAsInt(FName("BestAction"), (int32)ECombatAction::SlideBack);
            BB->SetValueAsBool(FName("CanAct"), true);
            return;
        }
        else if (Distance >= Keep)
        {
            // reached or exceeded desired distance: disable keep-distance and resume normal behavior
            AI->bKeepDistanceActive = false;
            // fallthrough to normal behavior
        }
        else
        {
            // within tolerance: do nothing
            return;
        }
    }*/

    UE_LOG(LogTemp, Warning, TEXT("asdfjkl no way right3"));

    // Simplified rules: only MoveTo or attacks (Jab/Hook). Do not consider player attacking or slides.
    float MeleeRange = AI ? AI->MeleeRange : 100.f;
    float AttackBuffer = AI ? AI->AttackBuffer : 30.f;

    const float DesiredDistance =
        AI ? AI->DesiredCombatDistance : MeleeRange;

    const float DistanceTolerance = 30.f;

    if (Distance > DesiredDistance + DistanceTolerance)
    {
        BB->SetValueAsInt(FName("BestAction"),
            (int32)ECombatAction::MoveTo);

        BB->SetValueAsBool(FName("CanAct"), true);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("asdfjkl no way right4"));

    // If in range but no stamina, do nothing (keep CanAct=false). Never set Idle as BestAction.
    if (CurrentStamina < 1)
    {
        return;
    }


    UE_LOG(LogTemp, Warning, TEXT("asdfjkl AI Health %.1f"), AI->Health);
    // Low-health defensive behavior: prefer sliding back with configurable chance
    if (AI &&
        AI->Health < LowHealthThreshold &&
        !AI->bRetreatedThisHealthLoss)
    {
        AI->bRetreatedThisHealthLoss = true;

        AI->DesiredCombatDistance = AI->MeleeRange + 300.f;

        BB->SetValueAsInt(FName("BestAction"),
            (int32)ECombatAction::SlideBack);

        BB->SetValueAsBool(FName("CanAct"), true);
        return;
    }

    // If player is attacking, 50% chance to slide (if stamina) vs 50% to attack
    if (bPlayerAttacking && CurrentStamina >= MinStaminaToSlide)
    {
        const float R = FMath::FRand();
        if (R < 0.5f)
        {
            // choose a slide direction randomly
            const float R2 = FMath::FRand();
            ECombatAction SlideChoice = (R2 < 0.33f) ? ECombatAction::SlideLeft : (R2 < 0.66f) ? ECombatAction::SlideRight : ECombatAction::SlideBack;
            // avoid repeating slide category
            if (AI && (AI->LastActionPerformed == ECombatAction::SlideLeft || AI->LastActionPerformed == ECombatAction::SlideRight || AI->LastActionPerformed == ECombatAction::SlideBack || AI->LastActionPerformed == ECombatAction::SlideForward))
            {
                // fallback to attack
            }
            else
            {
                BB->SetValueAsInt(FName("BestAction"), (int32)SlideChoice);
                BB->SetValueAsBool(FName("CanAct"), true);
                return;
            }
        }
        // else fallthrough to attack selection
    }

    // In range and has stamina -> choose Jab or Hook, respecting cooldowns and avoiding repeats
    float Now = OwnerComp.GetWorld() ? OwnerComp.GetWorld()->GetTimeSeconds() : 0.f;
    bool bJabOnCooldown = AI && ((Now - AI->LastJabTime) < JabCooldown);
    bool bHookOnCooldown = AI && ((Now - AI->LastHookTime) < HookCooldown);

    ECombatAction Chosen = ECombatAction::Jab;
    if (bJabOnCooldown && !bHookOnCooldown) Chosen = ECombatAction::Hook;
    else if (!bJabOnCooldown && bHookOnCooldown) Chosen = ECombatAction::Jab;
    else if (!bJabOnCooldown && !bHookOnCooldown) Chosen = (FMath::FRand() < 0.5f) ? ECombatAction::Jab : ECombatAction::Hook;
    else
    {
        float JabRem = AI ? FMath::Max(0.f, JabCooldown - (Now - AI->LastJabTime)) : JabCooldown;
        float HookRem = AI ? FMath::Max(0.f, HookCooldown - (Now - AI->LastHookTime)) : HookCooldown;
        Chosen = (JabRem <= HookRem) ? ECombatAction::Jab : ECombatAction::Hook;
    }

    // Avoid repeating the same action type twice in a row
    if (AI && AI->LastActionPerformed == Chosen)
    {
        Chosen = (Chosen == ECombatAction::Jab) ? ECombatAction::Hook : ECombatAction::Jab;
    }

    BB->SetValueAsInt(FName("BestAction"), (int32)Chosen);
    BB->SetValueAsBool(FName("CanAct"), true);
    return;
}
