// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SeattleAI.generated.h"

UCLASS()
class SEATTLE_API ASeattleAI : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASeattleAI();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float Health = 100.0f;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "Health")
	virtual void ApplyHealthChange(float Change);

};
