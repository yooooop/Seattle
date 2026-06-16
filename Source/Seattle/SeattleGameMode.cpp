// Copyright Epic Games, Inc. All Rights Reserved.

#include "SeattleGameMode.h"
#include "SeattleCharacter.h"
#include "SeattlePlayerController.h"
#include "GameFramework/PlayerController.h"

ASeattleGameMode::ASeattleGameMode()
{
	// stub
}

APawn* ASeattleGameMode::SpawnDefaultPawnFor_Implementation(AController* NewController, AActor* StartSpot)
{
	if (ShouldControllerSharePawn(NewController))
	{
		return nullptr;
	}

	APawn* SpawnedPawn = Super::SpawnDefaultPawnFor_Implementation(NewController, StartSpot);
	if (ASeattleCharacter* SeattlePawn = Cast<ASeattleCharacter>(SpawnedPawn))
	{
		SharedCharacterPawn = SeattlePawn;
		SeattlePawn->SetSharedPawnEnabled(true);
		SeattlePawn->RegisterPrimaryController(Cast<APlayerController>(NewController));
	}

	return SpawnedPawn;
}

void ASeattleGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (ShouldControllerSharePawn(NewPlayer))
	{
		InitializeSecondarySharedController(NewPlayer);
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	InitializePrimarySharedController(NewPlayer);
}

void ASeattleGameMode::RestartPlayer(AController* NewPlayer)
{
	if (ShouldControllerSharePawn(NewPlayer))
	{
		InitializeSecondarySharedController(Cast<APlayerController>(NewPlayer));
		return;
	}

	Super::RestartPlayer(NewPlayer);
}

bool ASeattleGameMode::ShouldControllerSharePawn(AController* Controller) const
{
	if (!Controller || !SharedCharacterPawn || !IsValid(SharedCharacterPawn))
	{
		return false;
	}

	if (SharedCharacterPawn->GetController() == Controller)
	{
		return false;
	}

	if (GetActivePlayerControllerCount() > MaxSharedControlPlayers)
	{
		return false;
	}

	return SharedCharacterPawn->CanAcceptSecondaryController(Cast<APlayerController>(Controller));
}

void ASeattleGameMode::InitializeSecondarySharedController(APlayerController* NewPlayer)
{
	if (!NewPlayer || !SharedCharacterPawn)
	{
		return;
	}

	if (ASeattlePlayerController* SeattlePC = Cast<ASeattlePlayerController>(NewPlayer))
	{
		SeattlePC->InitializeAsSecondarySharer(SharedCharacterPawn);
	}

	SharedCharacterPawn->RegisterSecondaryController(NewPlayer);

	// Client 1 has no pawn but is an active co-op player, not a spectator.
	NewPlayer->ChangeState(NAME_Playing);
	NewPlayer->ClientGotoState(NAME_Playing);
}

bool ASeattleGameMode::MustSpectate_Implementation(APlayerController* NewPlayerController) const
{
	if (ShouldControllerSharePawn(NewPlayerController))
	{
		return false;
	}

	return Super::MustSpectate_Implementation(NewPlayerController);
}

void ASeattleGameMode::InitializePrimarySharedController(APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	if (ASeattlePlayerController* SeattlePC = Cast<ASeattlePlayerController>(NewPlayer))
	{
		SeattlePC->InitializeAsPrimaryOwner(SharedCharacterPawn);
	}
}

int32 ASeattleGameMode::GetActivePlayerControllerCount() const
{
	int32 Count = 0;

	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (Iterator->Get())
			{
				++Count;
			}
		}
	}

	return Count;
}
