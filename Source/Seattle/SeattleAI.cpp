// Fill out your copyright notice in the Description page of Project Settings.


#include "SeattleAI.h"
#include "Net/UnrealNetwork.h"
#include "Seattle.h"

ASeattleAI::ASeattleAI()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
}

void ASeattleAI::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
	}
}

void ASeattleAI::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASeattleAI::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ASeattleAI::RequestApplyHealthChange(float Change, AActor* DamageInstigator)
{
	if (FMath::IsNearlyZero(Change))
	{
		return;
	}

	if (HasAuthority())
	{
		ApplyHealthChange(Change);
	}
	else
	{
		Server_ApplyHealthChange(Change, DamageInstigator);
	}
}

bool ASeattleAI::Server_ApplyHealthChange_Validate(float Change, AActor* DamageInstigator)
{
	return Change >= 0.f && Change <= MaxHealth;
}

void ASeattleAI::Server_ApplyHealthChange_Implementation(float Change, AActor* DamageInstigator)
{
	ApplyHealthChange(Change);
}

void ASeattleAI::ApplyHealthChange(float Change)
{
	if (!HasAuthority())
	{
		RequestApplyHealthChange(Change);
		return;
	}

	if (bIsKnockedDown || FMath::IsNearlyZero(Change))
	{
		return;
	}

	const float PreviousHealth = Health;
	Health = FMath::Clamp(Health - Change, 0.f, MaxHealth);

	OnHealthChanged.Broadcast(Health, PreviousHealth - Health);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("%s Health: %.1f"), *GetName(), Health));
	}
	
	if (Health <= 0.1f)
	{
		bIsKnockedDown = true;
		ApplyKnockDown();
	}
	else
	{
		bIsStunned = true;
		ApplyStun();
	}
}

void ASeattleAI::OnRep_Health()
{
	OnHealthChanged.Broadcast(Health, 0.f);
}

void ASeattleAI::OnRep_bIsKnockedDown()
{
	if (bIsKnockedDown)
	{
		ApplyKnockDown();
	}
}

void ASeattleAI::OnRep_bIsStunned()
{
	if (bIsStunned)
	{
		ApplyStun();
	}
}

void ASeattleAI::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeattleAI, Health);
	DOREPLIFETIME(ASeattleAI, bIsKnockedDown);
	DOREPLIFETIME(ASeattleAI, bIsStunned);
	DOREPLIFETIME(ASeattleAI, ComboCount);
	DOREPLIFETIME(ASeattleAI, LastAttackTime);
}
