#include "BTService_UpdateBlackboard.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "SeattleAI.h"
#include "SeattleCharacter.h"
#include "StaminaComponent.h"

UBTService_UpdateBlackboard::UBTService_UpdateBlackboard()
{
    bCreateNodeInstance = true;
    Interval = 0.1f; // default faster updates for responsiveness
}

void UBTService_UpdateBlackboard::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Super::OnBecomeRelevant(OwnerComp, NodeMemory);

    if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
    {
        TargetActorKey = BB->GetKeyID(FName("TargetActor"));
        DistanceKey = BB->GetKeyID(FName("DistanceToTarget"));
        PlayerIsAttackingKey = BB->GetKeyID(FName("PlayerIsAttacking"));
        CurrentStaminaKey = BB->GetKeyID(FName("CurrentStamina"));
        HasStaminaKey = BB->GetKeyID(FName("HasStamina"));
    }
}

void UBTService_UpdateBlackboard::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB)
    {
        return;
    }

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
    {
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UpdateBlackboard] AIController is null"));
        return;
    }

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UpdateBlackboard] Pawn is null (Controller=%s)"), *GetNameSafe(AICon));
        return;
    }

    ASeattleAI* AI = Cast<ASeattleAI>(Pawn);

    // get target from blackboard
    AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("TargetActor")));

    // Minimal tick log for visibility
    UE_LOG(LogTemp, Warning, TEXT("testinghere UpdateBlackboard: AI=%s Target=%s"), *GetNameSafe(AI), *GetNameSafe(Target));

    if (!AI)
    {
        // AI cast failed
    }

    if (!Target)
    {
        // Target null
    }

    // update distance
    float Distance = 0.f;
    if (Target)
    {
        Distance = FVector::Dist(Target->GetActorLocation(), Pawn->GetActorLocation());
    }
    BB->SetValueAsFloat(FName("DistanceToTarget"), Distance);

    // player attacking flag -- check target if it's a SeattleCharacter
    bool bPlayerAttacking = false;
    if (Target)
    {
        if (ASeattleCharacter* SC = Cast<ASeattleCharacter>(Target))
        {
            bPlayerAttacking = SC->bIsAttacking;
        }
    }

    BB->SetValueAsBool(FName("PlayerIsAttacking"), bPlayerAttacking);

    // current stamina and hasstamina from this AI (or target if desired)
    int32 StaminaInt = 0;
    bool bHasStamina = false;
    if (AI)
    {
        if (UStaminaComponent* St = AI->GetStaminaComponent())
        {
            const float Percent = St->GetStaminaPercent();
            // map percent to integer 0..5
            StaminaInt = FMath::Clamp(FMath::RoundToInt(Percent * 5.f), 0, 5);
            bHasStamina = St->HasEnoughStamina(1.f);
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UpdateBlackboard] Stamina component found: CurrentPercent=%.2f StaminaInt=%d HasStamina=%d"), Percent, StaminaInt, bHasStamina ? 1 : 0);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [UpdateBlackboard] StaminaComponent is null on AI=%s"), *GetNameSafe(AI));
        }
    }

    BB->SetValueAsInt(FName("CurrentStamina"), StaminaInt);
    BB->SetValueAsBool(FName("HasStamina"), bHasStamina);

    // Detect entering attack range (first frame crossing into melee range) using AI's melee range
    float MeleeRange = 0.f;
    if (AI)
    {
        MeleeRange = AI->MeleeRange;
    }
    bool bNowInRange = (Distance <= MeleeRange);
    if (!bPreviouslyInAttackRange && bNowInRange)
    {
        UE_LOG(LogTemp, Verbose, TEXT("[UpdateBlackboard] AI=%s ENTERED AttackRange (Distance=%.1f MeleeRange=%.1f)"), *GetNameSafe(AI), Distance, MeleeRange);
    }
    bPreviouslyInAttackRange = bNowInRange;
    PreviousDistance = Distance;
}
