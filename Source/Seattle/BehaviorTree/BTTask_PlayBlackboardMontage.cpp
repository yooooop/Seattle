#include "BTTask_PlayBlackboardMontage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "SeattleAI.h"
#include "Animation/AnimMontage.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "SeattleAIController.h"
#include "SeattlePlayerController.h"
#include "UI/SeattleHUD.h"

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

    // Update opponent stamina UI via the AI controller (preferred) or fallback to direct HUD update.
    {
        const float StaminaPercent = Char->GetStaminaPercent();
        if (ASeattleAIController* AIConCast = Cast<ASeattleAIController>(AICon))
        {
            AIConCast->BroadcastOpponentStamina(StaminaPercent);
        }
        else
        {
            // Fallback: directly update all player HUDs
            if (UWorld* World = OwnerComp.GetWorld())
            {
                for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                {
                    if (APlayerController* PC = It->Get())
                    {
                        if (PC->IsLocalController())
                        {
                            if (ASeattleHUD* HUD = Cast<ASeattleHUD>(PC->GetHUD()))
                            {
                                HUD->UpdateOpponentStamina(StaminaPercent);
                            }
                        }
                        else if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
                        {
                            SPC->Client_UpdateOpponentStamina(StaminaPercent);
                        }
                    }
                }
            }
        }
    }

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

    // Before finishing, update opponent stamina UI similar to immediate path
    if (OwnerComp)
    {
        AAIController* AICon = OwnerComp->GetAIOwner();
        if (AICon)
        {
            if (APawn* Pawn = AICon->GetPawn())
            {
                if (ASeattleAI* Char = Cast<ASeattleAI>(Pawn))
                {
                    const float StaminaPercent = Char->GetStaminaPercent();
                    if (ASeattleAIController* AIConCast = Cast<ASeattleAIController>(AICon))
                    {
                        AIConCast->BroadcastOpponentStamina(StaminaPercent);
                    }
                    else if (UWorld* World = OwnerComp->GetWorld())
                    {
                        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                        {
                            if (APlayerController* PC = It->Get())
                            {
                                if (PC->IsLocalController())
                                {
                                    if (ASeattleHUD* HUD = Cast<ASeattleHUD>(PC->GetHUD()))
                                    {
                                        HUD->UpdateOpponentStamina(StaminaPercent);
                                    }
                                }
                                else if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
                                {
                                    SPC->Client_UpdateOpponentStamina(StaminaPercent);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}
