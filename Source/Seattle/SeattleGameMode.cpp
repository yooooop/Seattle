// Copyright Epic Games, Inc. All Rights Reserved.

#include "SeattleGameMode.h"
#include "SeattleCharacter.h"
#include "SeattlePlayerController.h"
#include "GameFramework/PlayerController.h"
#include "UI/SeattleHUD.h"
#include "EngineUtils.h"
#include "SeattleAI.h"
#include "Variant_Combat/CombatCharacter.h"
#include "StaminaComponent.h"

ASeattleGameMode::ASeattleGameMode()
{
    // set default HUD class to our Seattle HUD
	HUDClass = ASeattleHUD::StaticClass();
	bGameStarted = false;
}

void ASeattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Capture initial transforms and health/stamina for characters and AI so we can restore them on reset
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			// store for player characters, AI, and combat characters
			if (Actor->IsA(ASeattleCharacter::StaticClass()) || Actor->IsA(ASeattleAI::StaticClass()) || Actor->IsA(ACombatCharacter::StaticClass()))
			{
				InitialActorTransforms.Add(Actor, Actor->GetActorTransform());

				// record health if available
				if (ASeattleCharacter* SC = Cast<ASeattleCharacter>(Actor))
				{
					InitialActorHealth.Add(Actor, SC->Health);
				}
				else if (ASeattleAI* AI = Cast<ASeattleAI>(Actor))
				{
					InitialActorHealth.Add(Actor, AI->Health);
				}

				// record stamina if component exists
				if (UStaminaComponent* St = Actor->FindComponentByClass<UStaminaComponent>())
				{
					InitialActorStamina.Add(Actor, St->GetStaminaPercent());
				}
			}
		}
	}
}

void ASeattleGameMode::ResetMatch()
{
	UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [GameMode] ResetMatch called on server"));

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [GameMode] ResetMatch called on non-authority"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// Restore transforms
	for (const auto& Pair : InitialActorTransforms)
	{
		AActor* Actor = Pair.Key.Get();
		if (!Actor) continue;

		const FTransform& T = Pair.Value;
		Actor->SetActorLocationAndRotation(T.GetLocation(), T.GetRotation().Rotator(), false, nullptr, ETeleportType::TeleportPhysics);

		// reset health/stamina/knockdown where applicable
            if (ASeattleCharacter* SC = Cast<ASeattleCharacter>(Actor))
			{
				float* InitHealth = InitialActorHealth.Find(Actor);
				if (InitHealth)
				{
					SC->Health = *InitHealth;
				}
				else
				{
					SC->Health = SC->MaxHealth;
				}
				SC->bIsKnockedDown = false;
				SC->bIsAttacking = false;

				if (UStaminaComponent* St = SC->FindComponentByClass<UStaminaComponent>())
				{
					float* InitSt = InitialActorStamina.Find(Actor);
					if (InitSt)
					{
						St->SetStamina((*InitSt) * St->MaxStamina);
					}
					else
					{
						St->SetStamina(St->MaxStamina);
					}
				}
			}
		else if (ASeattleAI* AI = Cast<ASeattleAI>(Actor))
		{
			float* InitHealth = InitialActorHealth.Find(Actor);
			if (InitHealth)
			{
				AI->Health = *InitHealth;
			}
			else
			{
				AI->Health = AI->MaxHealth;
			}
			AI->bIsKnockedDown = false;
			AI->bIsAttacking = false;
			AI->ComboCount = 0;
			AI->LastAttackTime = 0.f;
			AI->LastJabTime = -10000.f;
			AI->LastHookTime = -10000.f;

			if (UStaminaComponent* St = AI->FindComponentByClass<UStaminaComponent>())
			{
				float* InitSt = InitialActorStamina.Find(Actor);
				if (InitSt)
				{
					St->SetStamina((*InitSt) * St->MaxStamina);
				}
				else
				{
					St->SetStamina(St->MaxStamina);
				}
			}
		}
		else if (ACombatCharacter* CC = Cast<ACombatCharacter>(Actor))
		{
			// For other combat characters, reset basic values
			CC->SetActorLocationAndRotation(T.GetLocation(), T.GetRotation().Rotator());
			if (UStaminaComponent* St = CC->FindComponentByClass<UStaminaComponent>())
			{
				float* InitSt = InitialActorStamina.Find(Actor);
				if (InitSt)
				{
					St->SetStamina((*InitSt) * St->MaxStamina);
				}
				else
				{
					St->SetStamina(St->MaxStamina);
				}
			}
		}
	}

	// Ensure player controllers get reset input / UI - restart players
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			RestartPlayer(PC);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [GameMode] ResetMatch completed"));
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
