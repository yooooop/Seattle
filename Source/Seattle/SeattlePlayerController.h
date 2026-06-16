// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/PlayerController.h"
#include "SeattlePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class ASeattleCharacter;
class UCameraComponent;

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
