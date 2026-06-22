// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "CombatAttacker.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"
#include "SeattleCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAnimMontage;
struct FInputActionValue;
class APlayerController;
class AImpactFXActor;
class UStaminaComponent;

class UCameraShakeBase;

DECLARE_MULTICAST_DELEGATE_TwoParams(FSeattlePlayerHealthChangedSignature, float, float);

UENUM(BlueprintType)
enum class ESeattleInputContributor : uint8
{
	Primary UMETA(DisplayName = "Primary Owner"),
	Secondary UMETA(DisplayName = "Secondary Sharer")
};

UENUM(BlueprintType)
enum class ESeattleAttackType : uint8
{
	None UMETA(DisplayName = "None"),
	SingleTap UMETA(DisplayName = "Single Tap"),
	DoubleTap UMETA(DisplayName = "Double Tap"),
	Hold UMETA(DisplayName = "Hold")
};

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ASeattleCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

public:
	/** Current health for player character */
	UPROPERTY(ReplicatedUsing=OnRep_Health, BlueprintReadWrite, EditAnywhere, Category = "Health")
	float Health = 100.f;

	/** Max health */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.f;

	/** Spawned impact FX class used when AI hits player (assign in BP) */
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TSubclassOf<class AImpactFXActor> ImpactFXActorAIHitClass;

	/** Camera shake to play when player is hit (assign in BP) */
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TSubclassOf<class UCameraShakeBase> HitCameraShakeClass;

	/** Returns current health percent (0..1) */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealthPercent() const;

	// ICombatDamageable
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;
	virtual void HandleDeath() override;
	virtual void ApplyHealing(float Healing, AActor* Healer) override;
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	UFUNCTION()
	void OnRep_Health();

    /** Health changed delegate (C++ only) */
	FSeattlePlayerHealthChangedSignature OnHealthChanged;

	/** Client-side effects when damaged (camera shake, local FX) */
	UFUNCTION(Client, Unreliable)
	void Client_PlayHitEffects();

	/** Whether this character is knocked down (for anim graph) */
	UPROPERTY(ReplicatedUsing=OnRep_bIsKnockedDown, BlueprintReadOnly, EditAnywhere, Category = "State")
	bool bIsKnockedDown = false;

	UFUNCTION()
	void OnRep_bIsKnockedDown();

	/** Request a slide in a world direction (client calls, server executes) */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void RequestSlide(FVector Direction);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestSlide(FVector Direction);

	/** Execute slide on server (authority) */
	void DoSlide(FVector Direction);

	/** Client-side slide effects (blur overlay) */
	UFUNCTION(Client, Reliable)
	void Client_PlaySlideEffects(float Duration);

	/** Slide tuning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SlideDistance = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SlideDuration = 0.2f;

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	/** Stamina component for managing action costs */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaminaComponent* StaminaComponent;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Optional attack actions when character is locally possessed by a client-owned pawn */
	UPROPERTY(EditAnywhere, Category = "Input|Attack")
	UInputAction* LeftAttackAction;

	UPROPERTY(EditAnywhere, Category = "Input|Attack")
	UInputAction* LeftHookAction;

	UPROPERTY(EditAnywhere, Category = "Input|Attack")
	UInputAction* LeftKickAction;

	UPROPERTY(EditAnywhere, Category = "Input|Attack")
	UInputAction* RightAttackAction;

	UPROPERTY(EditAnywhere, Category = "Input|Attack")
	UInputAction* RightHookAction;

	UPROPERTY(EditAnywhere, Category = "Input|Attack")
	UInputAction* RightKickAction;

public:

	/** Constructor */
	ASeattleCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	void MoveStopped();

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Character-side handlers (used when pawn is locally possessed on client) */
	void StartLeftAttack();
	void StartLeftHook();
	void StartLeftKick();
	void StartRightAttack();
	void StartRightHook();
	void StartRightKick();

	/** Latest move input from the primary (host) controller. */
	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = "Shared Control|Input")
	FVector2D PrimaryMoveInput = FVector2D::ZeroVector;

	/** Latest move input from the secondary (Client 1) controller. */
	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = "Shared Control|Input")
	FVector2D SecondaryMoveInput = FVector2D::ZeroVector;

	/** Latest look input from the primary (host) controller. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shared Control|Input")
	FVector2D PrimaryLookInput = FVector2D::ZeroVector;

	/** Latest look input from the secondary (Client 1) controller. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Shared Control|Input")
	FVector2D SecondaryLookInput = FVector2D::ZeroVector;

	/** True when this pawn is being shared by multiple controllers. */
	UPROPERTY(ReplicatedUsing = OnRep_bSharedPawnEnabled, BlueprintReadWrite, EditAnywhere, Category = "Shared Control")
	bool bSharedPawnEnabled = false;

	/** Listen-server / host controller registered as the primary owner. */
	UPROPERTY(Replicated, BlueprintReadOnly, EditAnywhere, Category = "Shared Control")
	TObjectPtr<APlayerController> RegisteredPrimaryController;

	/** Client 1 controller registered as the secondary input sharer. */
	UPROPERTY(Replicated, BlueprintReadOnly, EditAnywhere, Category = "Shared Control")
	TObjectPtr<APlayerController> RegisteredSecondaryController;

	void RefreshMergedMovement();
	void ApplyMovementForContributor(FVector2D MovementInput, APlayerController* ViewController);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_PlayAttackMontage(UAnimMontage* Montage, float PlayRate);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_NotifyAttackInput(ESeattleAttackType AttackType);

	UFUNCTION()
	void OnRep_bSharedPawnEnabled();

public:

	/** Multicast RPC to play montage on server + all clients */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayAttackMontage(UAnimMontage* Montage, float PlayRate);

	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = "Animation")
	float ForwardInput = 0.f;

	UPROPERTY(Replicated, BlueprintReadWrite, EditAnywhere, Category = "Animation")
	float RightInput = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_bIsAttacking, BlueprintReadWrite, EditAnywhere, Category = "Animation")
	bool bIsAttacking = false;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveAttackType, BlueprintReadOnly, EditAnywhere, Category = "Animation")
	ESeattleAttackType ActiveAttackType = ESeattleAttackType::None;

	/** Montages to play for attacks (assign these in Blueprint) */
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

	/** Playback rates for each montage */
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

	/** Enables shared-pawn mode on the authoritative instance. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control")
	void SetSharedPawnEnabled(bool bEnabled);

	/** Records the host controller that possesses this pawn. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control")
	void RegisterPrimaryController(APlayerController* InController);

	/** Records Client 1 without changing possession away from the host. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control")
	void RegisterSecondaryController(APlayerController* InController);

	/** True when no secondary controller is registered yet, or the provided controller already holds that role. */
	UFUNCTION(BlueprintPure, Category = "Shared Control")
	bool CanAcceptSecondaryController(const APlayerController* InController) const;

	/** Applies movement from a specific controller and merges it with the other sharer. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control|Input")
	void ApplySharedMovementInput(FVector2D MovementInput, ESeattleInputContributor Contributor);

	/** Stores look input for Blueprint/animation diagnostics. Camera rotation is handled per-player on PlayerController. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control|Input")
	void ApplySharedLookInput(FVector2D LookInput, ESeattleInputContributor Contributor);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Sets the replicated attack type for AnimBP state machines / blend spaces. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	void SetActiveAttackType(ESeattleAttackType NewAttackType);

	/** Call from Blueprint after input classification (single / double / hold). */
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	void NotifyAttackInput(ESeattleAttackType AttackType);

	/** Server-authoritative montage playback for both co-op players. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	void RequestPlayAttackMontage(UAnimMontage* Montage, float PlayRate = 1.f);

	/** Convenience: choose montage from attack type and side and request play */
	UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
	void PlayMontageForAttack(bool bRightSide, ESeattleAttackType AttackType);

	/** Aim replication: clients call SetReplicatedAimRotation to send their aim to the server. */
	UFUNCTION(BlueprintCallable, Category = "Aim")
	void SetReplicatedAimRotation(FRotator NewAimRotation);

	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SetAimRotation(FRotator NewAimRotation);

	UPROPERTY(ReplicatedUsing = OnRep_AimRotation, BlueprintReadOnly, Category = "Aim")
	FRotator ReplicatedAimRotation;

	UFUNCTION()
	void OnRep_AimRotation();

	/** Returns the aim delta (Yaw, Pitch) relative to the actor rotation. Useful for AnimBP. */
	UFUNCTION(BlueprintCallable, Category = "Aim")
	void GetAimDeltaAngles(float& OutYaw, float& OutPitch) const;

	/** Server-side melee attack request that performs authoritative trace and applies damage. */
    UFUNCTION(Server, Reliable, WithValidation)
	void Server_PerformMeleeAttack(FVector NormalizedAimDir, float Range, float Radius, float Damage);

	/** Convenience blueprint-callable wrapper that will route to server when needed. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RequestPerformMeleeAttack(FVector AimDirection, float Range = 200.f, float Radius = 20.f, float Damage = 10.f);

    /** Impact VFX actor class to spawn on hits (multicast) */
	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TSubclassOf<AImpactFXActor> ImpactFXActorClass;

    UFUNCTION(NetMulticast, Reliable)
	void Multicast_SpawnImpactFX(FVector Location);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SpawnImpactFXClass(TSubclassOf<AImpactFXActor> FXClass, FVector Location);

	/** Melee attack tuning (used by player controller to request server melee) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Melee")
	float MeleeRange = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Melee")
	float MeleeRadius = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Melee")
	float MeleeDamage = 10.f;

	// ~begin CombatAttacker interface
public:
	virtual void DoAttackTrace(FName DamageSourceBone) override;
	virtual void CheckCombo() override;
	virtual void CheckChargedAttack() override;
	// ~end CombatAttacker interface

	UFUNCTION()
	void OnRep_bIsAttacking();

	UFUNCTION()
	void OnRep_ActiveAttackType();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns StaminaComponent subobject **/
	FORCEINLINE class UStaminaComponent* GetStaminaComponent() const { return StaminaComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
