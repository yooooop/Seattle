// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "SeattleCharacter.h"
#include "GameFramework/PlayerController.h"
#include "SeattlePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class ASeattleCharacter;
class UCameraComponent;
class UInputAction;

UENUM(BlueprintType)
enum class ESeattleSharedControlRole : uint8
{
	None UMETA(DisplayName = "None"),
	PrimaryOwner UMETA(DisplayName = "Primary Owner"),
	SecondarySharer UMETA(DisplayName = "Secondary Sharer")
};

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ASeattlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ASeattlePlayerController();


	/** Client tells server to start the match for all players (pressed Start) */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestStartGame();

	/** Server calls this on clients to instruct them to hide their main menu/start the game */
	UFUNCTION(Client, Reliable)
	void Client_StartGame();

    /** Server notifies all clients and stops AI when end-game occurs */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_NotifyEndGame();

	/** Client RPC to start the end-sequence UI locally */
	UFUNCTION(Client, Reliable)
	void Client_StartEndSequence(AActor* DownedActor);

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Whether this controller owns the shared pawn or forwards input to it. */
	UPROPERTY(Replicated, BlueprintReadOnly, EditAnywhere, Category = "Shared Control")
	ESeattleSharedControlRole SharedControlRole = ESeattleSharedControlRole::None;

	/** Cached shared pawn reference for secondary controllers and Blueprint access. */
	UPROPERTY(ReplicatedUsing = OnRep_SharedCharacterPawn, BlueprintReadOnly, EditAnywhere, Category = "Shared Control")
	TObjectPtr<ASeattleCharacter> SharedCharacterPawn;

	/** Per-player orbit camera used when sharing one pawn (look is local-only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shared Control|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> SharedLocalFollowCamera;

	/** Arm length for the per-player shared-pawn camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Control|Camera")
	float SharedLocalCameraArmLength = 400.f;

	/** Extra vertical offset added on top of the pawn capsule center for the orbit pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Control|Camera")
	float SharedLocalCameraHeightOffset = 0.f;

	/** Pull the camera closer when geometry blocks the orbit arm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shared Control|Camera")
	bool bSharedLocalCameraCollisionTest = true;

	/** True when this local controller is driving the shared-pawn orbit camera. */
	UPROPERTY(BlueprintReadOnly, Category = "Shared Control|Camera")
	bool bUsingSharedLocalCamera = false;

	/** Cached POV written each tick for CalcCamera / PlayerCameraManager. */
	FMinimalViewInfo SharedLocalCameraView;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void OnPossess(APawn* InPawn) override;

	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	ASeattleCharacter* ResolveSharedCharacterPawn() const;
	FVector GetSharedPawnCameraPivot(const ASeattleCharacter* TargetPawn) const;
	void UpdateSharedLocalCamera();

	UFUNCTION()
	void OnRep_SharedCharacterPawn();

	UFUNCTION(Client, Reliable)
	void Client_ConfigureSecondarySharedView(ASeattleCharacter* InSharedPawn);

	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SendSharedMovementInput(FVector2D MovementInput);

	/** Convenience: client calls this to request an attack (packages aim + montage + server hit) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RequestAttack(bool bRightSide, ESeattleAttackType AttackType, float Range = 200.f, float Radius = 20.f, float Damage = 10.f);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestAttack(bool bRightSide, ESeattleAttackType AttackType, FVector AimDir, float Range, float Radius, float Damage);

	/** Aim replication helper: start/stop periodic sending of aim to server */
	UFUNCTION(BlueprintCallable, Category = "Aim")
	void StartAimReplication(float Rate = 10.f);

	UFUNCTION(BlueprintCallable, Category = "Aim")
	void StopAimReplication();

protected:
	/** Timer handle for aim replication */
	FTimerHandle AimReplicationTimerHandle;

	/** Aim replication tick called by timer */
	void AimReplicationTick();

	/** Aim replication rate in Hz (default 10Hz) */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float AimReplicationRate = 10.f;

	/** The InputAction assets for attack inputs - set these in the PlayerController BP to match the mapping context */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LeftAttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LeftHookAction; // optional separate action for double-tap/hook

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LeftKickAction; // optional hold action

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* RightAttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* RightHookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* RightKickAction;

	/** Local handlers bound on the controller for local input (client only) */
	UFUNCTION()
	void LocalLeftAttackPressed();

	UFUNCTION()
	void LocalLeftHookPressed();

	UFUNCTION()
	void LocalLeftKickPressed();

	UFUNCTION()
	void LocalRightAttackPressed();

	UFUNCTION()
	void LocalRightHookPressed();

	UFUNCTION()
	void LocalRightKickPressed();

	/** Server RPCs the client calls to request actions on the shared pawn */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestLeftAttack();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestLeftHook();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestLeftKick();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestRightAttack();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestRightHook();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestRightKick();

	/* Double-tap handling (implemented on controller so double-click works reliably on clients):
	 - Controller will detect double-tap and call Server_RequestLeftHook instead of Server_RequestLeftAttack.
	 - Single-tap is delayed by DoubleTapThreshold to allow detection of a second tap.
	*/
	UPROPERTY(EditAnywhere, Category = "Input|Attack")
	float DoubleTapThreshold = 0.25f;

	FTimerHandle LeftTapTimer;
	FTimerHandle RightTapTimer;

	float LastLeftTapTime = 0.f;
	float LastRightTapTime = 0.f;

	/** Called when Left single-tap timer expires (perform single attack) */
	void LeftTapTimerExpired();

	/** Called when Right single-tap timer expires */
	void RightTapTimerExpired();

public:

	/** Called by the game mode when this controller is the listen-server host / primary owner. */
	void InitializeAsPrimaryOwner(ASeattleCharacter* InSharedPawn);

	/** Called by the game mode when this controller joins as Client 1 without spawning a pawn. */
	void InitializeAsSecondarySharer(ASeattleCharacter* InSharedPawn);

	/** Activates the per-player orbit camera that follows SharedCharacterPawn. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control|Camera")
	void EnableSharedLocalCamera();

	/** Applies look input to this player's local control rotation and camera only. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control|Input")
	void ApplyLocalSharedLookInput(FVector2D LookInput);

	UFUNCTION(BlueprintPure, Category = "Shared Control")
	bool IsPrimarySharedControlOwner() const;

	UFUNCTION(BlueprintPure, Category = "Shared Control")
	bool IsSecondarySharedControlSharer() const;

	UFUNCTION(BlueprintPure, Category = "Shared Control")
	bool IsUsingSharedLocalCamera() const { return bUsingSharedLocalCamera; }

	UFUNCTION(BlueprintPure, Category = "Shared Control")
	ASeattleCharacter* GetSharedCharacterPawn() const { return SharedCharacterPawn; }

	/** Blueprint entry point for Enhanced Input move bindings on Client 1. */
	UFUNCTION(BlueprintCallable, Category = "Shared Control|Input")
	void SendSharedMovementInput(FVector2D MovementInput);

	/** Blueprint entry point for Enhanced Input look bindings (local camera only). */
	UFUNCTION(BlueprintCallable, Category = "Shared Control|Input")
	void SendSharedLookInput(FVector2D LookInput);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
