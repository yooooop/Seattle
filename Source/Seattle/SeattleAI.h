// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SeattleCharacter.h"
#include "BehaviorTree/CombatAction.h"
#include "Variant_Combat/Interfaces/CombatAttacker.h"
#include "Animation/AnimMontage.h"
#include "SeattleAI.generated.h"

class UStaminaComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSeattleAIHealthChangedSignature, float, NewHealth, float, HealthDelta);

UCLASS()
class SEATTLE_API ASeattleAI : public ACharacter, public ICombatAttacker
{
	GENERATED_BODY()

public:
    /** Returns stamina component */
	FORCEINLINE class UStaminaComponent* GetStaminaComponent() const { return StaminaComponent; }
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

	/** Animation / combat replication */
	UPROPERTY(ReplicatedUsing = OnRep_bIsAttacking, BlueprintReadWrite, EditAnywhere, Category = "Animation")
	bool bIsAttacking = false;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveAttackType, BlueprintReadOnly, EditAnywhere, Category = "Animation")
	ESeattleAttackType ActiveAttackType = ESeattleAttackType::None;

	/** Last timestamps for attack cooldowns (server time) */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Combat")
	float LastJabTime = -10000.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Combat")
	float LastHookTime = -10000.f;

	/** Montages for AI attacks (assign in BP) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* LeftAttackMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* LeftHookMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* LeftKickMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* RightJabMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* RightHookMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* RightKickMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float LeftAttackPlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float LeftHookPlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float LeftKickPlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float RightJabPlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float RightHookPlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float RightKickPlayRate = 1.f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FSeattleAIHealthChangedSignature OnHealthChanged;

	/** Stamina component for AI actions */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaminaComponent* StaminaComponent;

	/** Current stamina value (units) - kept in sync for Blackboard use */
	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = "Stamina")
	float CurrentStamina = 0.f;

	/** Slide tuning for AI (distance in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SlideDistance = 150.f;

	/** Slide duration for AI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SlideDuration = 0.2f;

public:
	/** Perform a slide for AI in the given direction (server authoritative) */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void DoAISlide(FVector Direction);

	/** Return current stamina percent 0..1 */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	float GetStaminaPercent() const;

protected:
	UFUNCTION()
	void OnStaminaChanged(float NewStamina, float MaxStamina);

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

	/** Set the replicated attack type for AnimBP/state machines */
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	void SetActiveAttackType(ESeattleAttackType NewAttackType);

	/** Play montage request - will handle authority and multicast like the character version */
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	void RequestPlayAttackMontage(UAnimMontage* Montage, float PlayRate = 1.f);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_PlayAttackMontage(UAnimMontage* Montage, float PlayRate);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* Montage, float PlayRate);

	UFUNCTION()
	void OnRep_bIsAttacking();

	UFUNCTION()
	void OnRep_ActiveAttackType();

	/** Returns current health as 0..1 percent */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealthPercent() const;

	// ~begin CombatAttacker interface
	public:
	virtual void DoAttackTrace(FName DamageSourceBone) override;
	virtual void CheckCombo() override {}
	virtual void CheckChargedAttack() override {}

	/** AI melee tuning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Melee")
	float MeleeRange = 100.f;

	/** Buffer (cm) to stop inside MeleeRange so AI finishes slightly within attack distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Melee")
	float AttackBuffer = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Melee")
	float MeleeRadius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Melee")
	float MeleeDamage = 10.f;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_DoAttackTrace(FName DamageSourceBone);
	// ~end CombatAttacker interface

protected:

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ApplyHealthChange(float Change, AActor* DamageInstigator);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    /** Last action performed by the AI (client-visible for debugging). Used to apply repeat penalties. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	ECombatAction LastActionPerformed = ECombatAction::None;

	/** Recent action history to avoid repeating patterns (most recent at index 0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	TArray<ECombatAction> RecentActions;

	/** How many recent actions to remember when applying penalties */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat")
	int32 RecentActionMemorySize = 3;

	/** Record an action into history (server authoritative) */
	UFUNCTION()
	void RecordAction(ECombatAction Action);
};
