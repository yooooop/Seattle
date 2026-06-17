// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "SeattleCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAnimMontage;
struct FInputActionValue;
class APlayerController;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

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
class ASeattleCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
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

	UFUNCTION()
	void OnRep_bIsAttacking();

	UFUNCTION()
	void OnRep_ActiveAttackType();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
