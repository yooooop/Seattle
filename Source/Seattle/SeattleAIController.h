// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SeattleAIController.generated.h"

class UBehaviorTree;
class UBlackboardComponent;

/**
 * AI Controller for Seattle AI characters with behavior tree support
 */
UCLASS()
class SEATTLE_API ASeattleAIController : public AAIController
{
	GENERATED_BODY()

public:
	/** Constructor */
	ASeattleAIController();

	/** Behavior Tree to run when AI starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	/** Blackboard key name for the target actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FName TargetActorKeyName = "TargetActor";

	/** Start the AI logic (run behavior tree, set target, set focus) */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StartAI();

	/** Stop the AI logic */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StopAI();

	/** Broadcast AI opponent stamina percent to all player HUDs (server-side) */
	UFUNCTION(BlueprintCallable, Category = "AI")
	void BroadcastOpponentStamina(float StaminaPercent);

protected:
	/** Called when the controller possesses a pawn */
	virtual void OnPossess(APawn* InPawn) override;
};
