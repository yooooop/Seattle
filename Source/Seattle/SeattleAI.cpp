// Fill out your copyright notice in the Description page of Project Settings.


#include "SeattleAI.h"

// Sets default values
ASeattleAI::ASeattleAI()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASeattleAI::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASeattleAI::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ASeattleAI::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ASeattleAI::ApplyHealthChange(float Change)
{
	Health -= Change;
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("Health: %f"), Health));
	
	if (Health <= 0.1f)
	{
		bIsKnockedDown = true;
		ApplyKnockDown();
	}
	else
	{
		ApplyStun();
	}

}