// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SeattleAI.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSeattleAIHealthChangedSignature, float, NewHealth, float, HealthDelta);

UCLASS()
class SEATTLE_API ASeattleAI : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASeattleAI();

public:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, EditAnywhere, Category = "Health")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.0f;

	UPROPERTY(ReplicatedUsing = OnRep_bIsKnockedDown, BlueprintReadOnly, EditAnywhere, Category = "Health")
	bool bIsKnockedDown = false;

	UPROPERTY(ReplicatedUsing = OnRep_bIsStunned, BlueprintReadWrite, EditAnywhere, Category = "Health")
	bool bIsStunned = false;

	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere)
	int32 ComboCount = 0;

	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere)
	float LastAttackTime = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FSeattleAIHealthChangedSignature OnHealthChanged;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Server-authoritative health change. Call RequestApplyHealthChange from clients / Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	virtual void ApplyHealthChange(float Change);

	/** Routes damage to the server so both co-op players hit the same replicated AI. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void RequestApplyHealthChange(float Change, AActor* DamageInstigator = nullptr);

	UFUNCTION(BlueprintImplementableEvent)
	void ApplyStun();

	UFUNCTION(BlueprintImplementableEvent)
	void ApplyKnockDown();

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_bIsKnockedDown();

	UFUNCTION()
	void OnRep_bIsStunned();

protected:

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ApplyHealthChange(float Change, AActor* DamageInstigator);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
