// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SeattleGameMode.generated.h"

class ASeattleCharacter;
class ASeattlePlayerController;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class ASeattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Reset the match to initial state (server only) */
	UFUNCTION(BlueprintCallable, Category = "Match")
	void ResetMatch();

protected:
	virtual void BeginPlay() override;

	/** Stored initial transforms for actors to restore on reset */
	TMap<TWeakObjectPtr<AActor>, FTransform> InitialActorTransforms;
	/** Stored initial health values for actors */
	TMap<TWeakObjectPtr<AActor>, float> InitialActorHealth;
	/** Stored initial stamina values for actors (if any) */
	TMap<TWeakObjectPtr<AActor>, float> InitialActorStamina;

public:
	
	/** Constructor */
	ASeattleGameMode();

	/** Returns the single shared character pawn used by all co-op controllers. */
	UFUNCTION(BlueprintPure, Category = "Shared Control")
	ASeattleCharacter* GetSharedCharacterPawn() const { return SharedCharacterPawn; }

protected:

	/** The single pawn that primary and secondary controllers share. */
	UPROPERTY(BlueprintReadOnly, Category = "Shared Control")
	TObjectPtr<ASeattleCharacter> SharedCharacterPawn;

	/** Maximum number of player controllers allowed to influence the shared pawn (host + Client 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Control", meta = (ClampMin = "2"))
	int32 MaxSharedControlPlayers = 2;

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewController, AActor* StartSpot) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual bool MustSpectate_Implementation(APlayerController* NewPlayerController) const override;

	/** True when this controller should not receive its own pawn and instead share the host pawn. */
	bool ShouldControllerSharePawn(AController* Controller) const;

	/** Configures a joining client controller to drive the existing shared pawn. */
	void InitializeSecondarySharedController(APlayerController* NewPlayer);

	/** Marks the host controller as the primary owner of the shared pawn. */
	void InitializePrimarySharedController(APlayerController* NewPlayer);

	/** Counts active player controllers in the world. */
	int32 GetActivePlayerControllerCount() const;

	/** Whether the match/game has started (main menu dismissed) */
	UPROPERTY(BlueprintReadOnly, Category = "Match")
	bool bGameStarted = false;

public:
	UFUNCTION(BlueprintCallable, Category = "Match")
	bool IsGameStarted() const { return bGameStarted; }

	UFUNCTION(BlueprintCallable, Category = "Match")
	void SetGameStarted(bool bStarted) { bGameStarted = bStarted; }
};
