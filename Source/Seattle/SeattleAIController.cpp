// Fill out your copyright notice in the Description page of Project Settings.

#include "SeattleAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Seattle.h"
// Include HUD and PlayerController headers used for broadcasting stamina updates
#include "UI/SeattleHUD.h"
#include "SeattlePlayerController.h"

ASeattleAIController::ASeattleAIController()
{
    // Do not start AI logic on possess; game will start AI when match begins
    bStartAILogicOnPossess = false;

    // Attach to pawn for proper AI functionality
    bAttachToPawn = true;
}

void ASeattleAIController::BroadcastOpponentStamina(float StaminaPercent)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
            {
                // If this is the local controller on this process, update HUD directly
                if (PC->IsLocalController())
                {
                    if (ASeattleHUD* HUD = Cast<ASeattleHUD>(PC->GetHUD()))
                    {
                        HUD->UpdateOpponentStamina(StaminaPercent);
                    }
                }
                else
                {
                    // Remote client - call client RPC to update their HUD
                    SPC->Client_UpdateOpponentStamina(StaminaPercent);
                }
            }
        }
    }
}

void ASeattleAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] OnPossess called. Controller=%s PossessedPawn=%s"), *GetNameSafe(this), *GetNameSafe(InPawn));

    // Initialize blackboard if behavior tree is assigned
    if (BehaviorTreeAsset)
    {
        UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
        if (!BlackboardComp && BehaviorTreeAsset->BlackboardAsset)
        {
            UseBlackboard(BehaviorTreeAsset->BlackboardAsset, BlackboardComp);
            if (BlackboardComp)
            {
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] Blackboard initialized: %s"), *GetNameSafe(BehaviorTreeAsset->BlackboardAsset));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] Blackboard initialization FAILED: asset=%s"), *GetNameSafe(BehaviorTreeAsset->BlackboardAsset));
            }
        }
    }
}

void ASeattleAIController::StartAI()
{
    if (!BehaviorTreeAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] StartAI - No BehaviorTreeAsset assigned on %s"), *GetNameSafe(this));
        return;
    }

    // Run the behavior tree
    if (RunBehaviorTree(BehaviorTreeAsset))
    {
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] StartAI - Started behavior tree on %s (BT=%s)"), *GetNameSafe(this), *GetNameSafe(BehaviorTreeAsset));

        // Get the player character and set it as the target
        if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
        {
            ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
            if (PlayerCharacter)
            {
                // Set the target actor in the blackboard
                BlackboardComp->SetValueAsObject(TargetActorKeyName, PlayerCharacter);

                // Set focus on the player character
                SetFocus(PlayerCharacter);

                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] StartAI - Set target and focus to %s"), *GetNameSafe(PlayerCharacter));
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] Blackboard asset=%s, TargetActorKey=%s"), *GetNameSafe(BehaviorTreeAsset->BlackboardAsset), *TargetActorKeyName.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] StartAI - Could not find player character"));
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [AIController] StartAI - Failed to start behavior tree on %s"), *GetNameSafe(this));
    }
}

void ASeattleAIController::StopAI()
{
    // Stop the behavior tree
    UBrainComponent* BrainComp = BrainComponent;
    if (BrainComp)
    {
        BrainComp->StopLogic("Stopped by game");
        UE_LOG(LogSeattle, Log, TEXT("SeattleAIController::StopAI - Stopped AI logic on %s"), *GetNameSafe(this));
    }

    // Clear focus
    ClearFocus(EAIFocusPriority::Gameplay);
}