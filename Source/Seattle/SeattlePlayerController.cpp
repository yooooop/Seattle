// Copyright Epic Games, Inc. All Rights Reserved.

#include "SeattlePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "Seattle.h"
#include "SeattleCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "TimerManager.h"
#include "SeattleAIController.h"
#include "SeattleAI.h"
#include "SeattleGameMode.h"
#include "UI/SeattleHUD.h"
#include "EngineUtils.h"

ASeattlePlayerController::ASeattlePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	SharedLocalFollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SharedLocalFollowCamera"));
	SharedLocalFollowCamera->bUsePawnControlRotation = false;
	SharedLocalFollowCamera->SetActive(false);
}

bool ASeattlePlayerController::Server_RequestStartGame_Validate()
{
	return true;
}

void ASeattlePlayerController::Server_RequestStartGame_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("Server_RequestStartGame_Implementation: request from %s"), *GetNameSafe(this));

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ASeattleGameMode* GM = World->GetAuthGameMode<ASeattleGameMode>();
	if (!GM)
	{
		return;
	}

	if (GM->IsGameStarted())
	{
		UE_LOG(LogSeattle, Log, TEXT("Server_RequestStartGame_Implementation: game already started"));
		return;
	}

	// mark started
	GM->SetGameStarted(true);

	// Notify all player controllers (client-side) to hide main menu
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
			{
				SPC->Client_StartGame();
			}
		}
	}

	// Start AI logic for all AI controllers now that the match has started
	for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		if (!C)
		{
			continue;
		}

		// Seattle AI controller
		if (ASeattleAIController* SAI = Cast<ASeattleAIController>(C))
		{
			if (SAI->GetLocalRole() == ROLE_Authority)
			{
				UE_LOG(LogSeattle, Log, TEXT("Starting AI on %s"), *GetNameSafe(SAI));
				SAI->StartAI();
			}
		}
	}
}

void ASeattlePlayerController::Client_StartGame_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("Client_StartGame_Implementation: running on %s"), *GetNameSafe(this));
	if (APlayerController* PC = Cast<APlayerController>(this))
	{
		if (ASeattleHUD* HUD = Cast<ASeattleHUD>(PC->GetHUD()))
		{
			HUD->HideMainMenu();
		}
	}
}

bool ASeattlePlayerController::Server_NotifyEndGame_Validate()
{
	return true;
}

void ASeattlePlayerController::Server_NotifyEndGame_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("Server_NotifyEndGame_Implementation: received from %s"), *GetNameSafe(this));

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Mark game as ended on server
	ASeattleGameMode* GM = World->GetAuthGameMode<ASeattleGameMode>();
	if (GM)
	{
		GM->SetGameStarted(false);
	}

	// Stop all AI controllers on server
	for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
	{
		AController* C = It->Get();
		if (!C)
		{
			continue;
		}

		if (ASeattleAIController* SAI = Cast<ASeattleAIController>(C))
		{
			if (SAI->GetLocalRole() == ROLE_Authority)
			{
				UE_LOG(LogSeattle, Log, TEXT("Server_NotifyEndGame: Stopping AI %s"), *GetNameSafe(SAI));
				SAI->StopAI();
			}
		}
	}

	// Find a downed actor to focus on: prefer player characters, then combat characters, then AI
	AActor* DownedActor = nullptr;

	for (TActorIterator<ASeattleCharacter> It(World); It; ++It)
	{
		if (ASeattleCharacter* SC = *It)
		{
			if (SC->Health <= 0.f)
			{
				DownedActor = SC;
				break;
			}
		}
	}

	if (!DownedActor)
	{
		for (TActorIterator<ASeattleAI> It(World); It; ++It)
		{
			if (ASeattleAI* AI = *It)
			{
				if (AI->GetHealthPercent() <= 0.f)
				{
					DownedActor = AI;
					break;
				}
			}
		}
	}

	// Notify all player controllers to start end sequence UI with the chosen downed actor (may be null)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (ASeattlePlayerController* SPC = Cast<ASeattlePlayerController>(PC))
			{
				SPC->Client_StartEndSequence(DownedActor);
			}
		}
	}
}

void ASeattlePlayerController::Client_StartEndSequence_Implementation(AActor* DownedActor)
{
	UE_LOG(LogSeattle, Log, TEXT("Client_StartEndSequence_Implementation: running on %s DownedActor=%s"), *GetNameSafe(this), DownedActor ? *GetNameSafe(DownedActor) : TEXT("null"));
	if (APlayerController* PC = Cast<APlayerController>(this))
	{
		if (ASeattleHUD* HUD = Cast<ASeattleHUD>(PC->GetHUD()))
		{
			HUD->StartEndSequence(DownedActor);
		}
	}
}

void ASeattlePlayerController::StartAimReplication(float Rate)
{
	if (!IsLocalController())
	{
		return;
	}

	AimReplicationRate = (Rate > 0.f) ? Rate : AimReplicationRate;

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(AimReplicationTimerHandle);
		const float Interval = 1.f / AimReplicationRate;
		GetWorldTimerManager().SetTimer(AimReplicationTimerHandle, this, &ASeattlePlayerController::AimReplicationTick, Interval, true);
	}
}

void ASeattlePlayerController::StopAimReplication()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(AimReplicationTimerHandle);
	}
}

void ASeattlePlayerController::AimReplicationTick()
{
	if (!IsLocalController())
	{
		return;
	}

	// Compute aim rotator from control rotation (could be replaced with deproject logic)
	const FRotator AimRot = GetControlRotation();

	if (ASeattleCharacter* PawnChar = Cast<ASeattleCharacter>(GetPawn()))
	{
		PawnChar->SetReplicatedAimRotation(AimRot);
	}
	else if (ASeattleCharacter* Shared = ResolveSharedCharacterPawn())
	{
		// In shared-pawn setups, forward aim to the shared pawn
		Shared->SetReplicatedAimRotation(AimRot);
	}
}


void ASeattlePlayerController::RequestAttack(bool bRightSide, ESeattleAttackType AttackType, float Range, float Radius, float Damage)
{
	// Local prediction: play montage locally for responsiveness
	if (IsLocalController())
	{
		if (ASeattleCharacter* Target = ResolveSharedCharacterPawn())
		{
			Target->PlayMontageForAttack(bRightSide, AttackType);
		}

		// package aim direction from this controller
		const FVector AimDir = GetControlRotation().Vector();

		// send to server
		Server_RequestAttack(bRightSide, AttackType, AimDir, Range, Radius, Damage);
	}
	else
	{
		// If not local (server-side), perform directly on possessed pawn
		if (ASeattleCharacter* Target = ResolveSharedCharacterPawn())
		{
			Target->SetReplicatedAimRotation(GetControlRotation());
			Target->PlayMontageForAttack(bRightSide, AttackType);
			Target->RequestPerformMeleeAttack(GetControlRotation().Vector(), Range, Radius, Damage);
		}
	}
}

bool ASeattlePlayerController::Server_RequestAttack_Validate(bool bRightSide, ESeattleAttackType AttackType, FVector AimDir, float Range, float Radius, float Damage)
{
	return AimDir.IsNormalized() || !AimDir.IsNearlyZero();
}

void ASeattlePlayerController::Server_RequestAttack_Implementation(bool bRightSide, ESeattleAttackType AttackType, FVector AimDir, float Range, float Radius, float Damage)
{
	UE_LOG(LogSeattle, Log, TEXT("%s Server_RequestAttack_Implementation: side=%d type=%d Aim=%s"), *GetNameSafe(this), bRightSide ? 1 : 0, (int32)AttackType, *AimDir.ToString());

	ASeattleCharacter* Target = ResolveSharedCharacterPawn();
	if (!Target)
	{
		UE_LOG(LogSeattle, Warning, TEXT("Server_RequestAttack_Implementation: no shared character pawn"));
		return;
	}

	// set replicated aim on the pawn
	Target->SetReplicatedAimRotation(GetControlRotation());

	// play montage and perform melee trace on server (authority)
	Target->PlayMontageForAttack(bRightSide, AttackType);
	Target->RequestPerformMeleeAttack(AimDir.GetSafeNormal(), Range, Radius, Damage);
}

void ASeattlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogSeattle, Error, TEXT("[%s] BeginPlay: Could not spawn mobile controls widget."), *GetNameSafe(this));

		}

	}

	if (ResolveSharedCharacterPawn() && (IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer()))
	{
		EnableSharedLocalCamera();
	}
}

void ASeattlePlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocalPlayerController()
		&& (IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer())
		&& !bUsingSharedLocalCamera
		&& ResolveSharedCharacterPawn())
	{
		EnableSharedLocalCamera();
	}

	if (bUsingSharedLocalCamera)
	{
		UpdateSharedLocalCamera();
	}
}

void ASeattlePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (IsLocalPlayerController() && IsPrimarySharedControlOwner())
	{
		SharedCharacterPawn = Cast<ASeattleCharacter>(InPawn);
		EnableSharedLocalCamera();
	}
}

void ASeattlePlayerController::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bUsingSharedLocalCamera && IsLocalPlayerController() && ResolveSharedCharacterPawn())
	{
		UpdateSharedLocalCamera();
		OutResult = SharedLocalCameraView;
		return;
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

void ASeattlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
					UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: Added InputMappingContext %s"), *GetNameSafe(this), *GetNameSafe(CurrentContext));
				}
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					if (CurrentContext)
					{
						Subsystem->AddMappingContext(CurrentContext, 0);
					}
				}
			}
		}

		// Bind left/right attack actions on controller so the owning client can send a server request even if they don't possess the pawn.
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		{
			// left attack (tap/double detection handled here)
			if (LeftAttackAction)
			{
				EIC->BindAction(LeftAttackAction, ETriggerEvent::Started, this, &ASeattlePlayerController::LocalLeftAttackPressed);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: Bound LeftAttackAction on controller (LeftAttackAction=%s)"), *GetNameSafe(this), *GetNameSafe(LeftAttackAction));
			}
			else
			{
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: LeftAttackAction is null on controller"), *GetNameSafe(this));
			}

			// optional separate bindings (if you prefer separate action assets for hook/hold)
			if (LeftHookAction)
			{
				EIC->BindAction(LeftHookAction, ETriggerEvent::Started, this, &ASeattlePlayerController::LocalLeftHookPressed);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: Bound LeftHookAction on controller (LeftHookAction=%s)"), *GetNameSafe(this), *GetNameSafe(LeftHookAction));
			}
			if (LeftKickAction)
			{
				EIC->BindAction(LeftKickAction, ETriggerEvent::Started, this, &ASeattlePlayerController::LocalLeftKickPressed);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: Bound LeftKickAction on controller (LeftKickAction=%s)"), *GetNameSafe(this), *GetNameSafe(LeftKickAction));
			}
			if (RightAttackAction)
			{
				EIC->BindAction(RightAttackAction, ETriggerEvent::Started, this, &ASeattlePlayerController::LocalRightAttackPressed);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: Bound RightAttackAction on controller (RightAttackAction=%s)"), *GetNameSafe(this), *GetNameSafe(RightAttackAction));
			}
			if (RightHookAction)
			{
				EIC->BindAction(RightHookAction, ETriggerEvent::Started, this, &ASeattlePlayerController::LocalRightHookPressed);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: Bound RightHookAction on controller (RightHookAction=%s)"), *GetNameSafe(this), *GetNameSafe(RightHookAction));
			}
			if (RightKickAction)
			{
				EIC->BindAction(RightKickAction, ETriggerEvent::Started, this, &ASeattlePlayerController::LocalRightKickPressed);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupInputComponent: Bound RightKickAction on controller (RightKickAction=%s)"), *GetNameSafe(this), *GetNameSafe(RightKickAction));
			}
		}
	}
}

bool ASeattlePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

ASeattleCharacter* ASeattlePlayerController::ResolveSharedCharacterPawn() const
{
	if (SharedCharacterPawn && IsValid(SharedCharacterPawn))
	{
		return SharedCharacterPawn;
	}

	return Cast<ASeattleCharacter>(GetPawn());
}

FVector ASeattlePlayerController::GetSharedPawnCameraPivot(const ASeattleCharacter* TargetPawn) const
{
	if (!TargetPawn)
	{
		return FVector::ZeroVector;
	}

	if (const UCapsuleComponent* Capsule = TargetPawn->GetCapsuleComponent())
	{
		return Capsule->GetComponentLocation() + FVector(0.f, 0.f, SharedLocalCameraHeightOffset);
	}

	return TargetPawn->GetActorLocation() + FVector(0.f, 0.f, SharedLocalCameraHeightOffset);
}

void ASeattlePlayerController::InitializeAsPrimaryOwner(ASeattleCharacter* InSharedPawn)
{
	SharedControlRole = ESeattleSharedControlRole::PrimaryOwner;
	SharedCharacterPawn = InSharedPawn;

	if (IsLocalPlayerController())
	{
		EnableSharedLocalCamera();
	}
}

void ASeattlePlayerController::InitializeAsSecondarySharer(ASeattleCharacter* InSharedPawn)
{
	SharedControlRole = ESeattleSharedControlRole::SecondarySharer;
	SharedCharacterPawn = InSharedPawn;

	if (HasAuthority())
	{
		Client_ConfigureSecondarySharedView(InSharedPawn);
	}

	if (IsLocalPlayerController())
	{
		EnableSharedLocalCamera();
	}
}

void ASeattlePlayerController::EnableSharedLocalCamera()
{
	ASeattleCharacter* TargetPawn = ResolveSharedCharacterPawn();
	if (!IsLocalPlayerController() || !TargetPawn)
	{
		return;
	}

	bUsingSharedLocalCamera = true;

	// Prevent the engine from switching the view target back to the possessed pawn camera.
	bAutoManageActiveCameraTarget = false;

	if (SharedLocalFollowCamera)
	{
		SharedLocalFollowCamera->SetActive(true);
	}

	// Match the initial orbit yaw to the shared character so the camera starts behind the body.
	const FRotator InitialRotation(0.f, TargetPawn->GetActorRotation().Yaw, 0.f);
	SetControlRotation(InitialRotation);

	SetViewTargetWithBlend(this, 0.f);
	UpdateSharedLocalCamera();
}

void ASeattlePlayerController::UpdateSharedLocalCamera()
{
	if (!IsLocalPlayerController() || !SharedLocalFollowCamera)
	{
		return;
	}

	ASeattleCharacter* TargetPawn = ResolveSharedCharacterPawn();
	if (!TargetPawn)
	{
		return;
	}

	const FVector Pivot = GetSharedPawnCameraPivot(TargetPawn);
	const FRotator OrbitRotation = GetControlRotation();
	const FRotator OrbitRotationNoRoll(OrbitRotation.Pitch, OrbitRotation.Yaw, 0.f);

	const FVector DesiredCameraLocation = Pivot + OrbitRotationNoRoll.RotateVector(FVector(-SharedLocalCameraArmLength, 0.f, 0.f));
	FVector FinalCameraLocation = DesiredCameraLocation;

	if (bSharedLocalCameraCollisionTest && GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SharedLocalCamera), false, TargetPawn);
		QueryParams.AddIgnoredActor(this);

		const float ProbeRadius = 12.f;
		if (GetWorld()->SweepSingleByChannel(
			Hit,
			Pivot,
			DesiredCameraLocation,
			FQuat::Identity,
			ECC_Camera,
			FCollisionShape::MakeSphere(ProbeRadius),
			QueryParams))
		{
			FinalCameraLocation = Hit.Location;
		}
	}

	const FRotator CameraRotation = (Pivot - FinalCameraLocation).IsNearlyZero()
		? OrbitRotationNoRoll
		: (Pivot - FinalCameraLocation).GetSafeNormal().Rotation();

	if (SharedLocalFollowCamera)
	{
		SharedLocalFollowCamera->SetWorldLocationAndRotation(FinalCameraLocation, CameraRotation);
		SharedLocalFollowCamera->GetCameraView(0.f, SharedLocalCameraView);
	}
	else
	{
		SharedLocalCameraView.Location = FinalCameraLocation;
		SharedLocalCameraView.Rotation = CameraRotation;
		SharedLocalCameraView.FOV = PlayerCameraManager ? PlayerCameraManager->GetFOVAngle() : 90.f;
	}
}

void ASeattlePlayerController::ApplyLocalSharedLookInput(FVector2D LookInput)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
}

bool ASeattlePlayerController::IsPrimarySharedControlOwner() const
{
	return SharedControlRole == ESeattleSharedControlRole::PrimaryOwner;
}

bool ASeattlePlayerController::IsSecondarySharedControlSharer() const
{
	return SharedControlRole == ESeattleSharedControlRole::SecondarySharer;
}

void ASeattlePlayerController::SendSharedMovementInput(FVector2D MovementInput)
{
	if (!IsSecondarySharedControlSharer() || !IsLocalPlayerController())
	{
		return;
	}

	Server_SendSharedMovementInput(MovementInput);
}

void ASeattlePlayerController::SendSharedLookInput(FVector2D LookInput)
{
	// Look is local-only: each player orbits the shared pawn with their own control rotation.
	ApplyLocalSharedLookInput(LookInput);
}

void ASeattlePlayerController::OnRep_SharedCharacterPawn()
{
	if ((IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer()) && IsLocalPlayerController())
	{
		EnableSharedLocalCamera();
	}
}

void ASeattlePlayerController::Client_ConfigureSecondarySharedView_Implementation(ASeattleCharacter* InSharedPawn)
{
	SharedCharacterPawn = InSharedPawn;

	if (IsLocalPlayerController())
	{
		EnableSharedLocalCamera();
	}
}

bool ASeattlePlayerController::Server_SendSharedMovementInput_Validate(FVector2D MovementInput)
{
	return IsSecondarySharedControlSharer()
		&& SharedCharacterPawn != nullptr
		&& MovementInput.X >= -1.f && MovementInput.X <= 1.f
		&& MovementInput.Y >= -1.f && MovementInput.Y <= 1.f;
}

void ASeattlePlayerController::Server_SendSharedMovementInput_Implementation(FVector2D MovementInput)
{
	if (SharedCharacterPawn && IsSecondarySharedControlSharer())
	{
		SharedCharacterPawn->ApplySharedMovementInput(MovementInput, ESeattleInputContributor::Secondary);
	}
}

void ASeattlePlayerController::LocalLeftAttackPressed()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] LocalLeftAttackPressed: local controller input. IsLocal=%d HasAuthority=%d SharedPawn=%s Role=%d"),
		*GetNameSafe(this), IsLocalController() ? 1 : 0, HasAuthority() ? 1 : 0, *GetNameSafe(ResolveSharedCharacterPawn()), (int32)SharedControlRole);

	// Only clients should send the server request; ignore on authoritative server-local controllers
	if (!(IsLocalController() && !HasAuthority()))
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] LocalLeftAttackPressed: Ignored on server/local-authority controller."), *GetNameSafe(this));
		return;
	}

	// Double-tap detection: delay the single-tap action until threshold expires, allow second tap to convert to hook.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (Now - LastLeftTapTime <= DoubleTapThreshold)
	{
		// second tap within threshold -> treat as hook
		if (GetWorldTimerManager().IsTimerActive(LeftTapTimer))
		{
			GetWorldTimerManager().ClearTimer(LeftTapTimer);
		}
		LastLeftTapTime = 0.f;
		Server_RequestLeftHook();
		UE_LOG(LogSeattle, Log, TEXT("[%s] LocalLeftAttackPressed: Detected double-tap -> requesting LeftHook on server"), *GetNameSafe(this));
	}
	else
	{
		// first tap: start timer to fire single-tap if no second tap arrives
		LastLeftTapTime = Now;
		GetWorldTimerManager().SetTimer(LeftTapTimer, this, &ASeattlePlayerController::LeftTapTimerExpired, DoubleTapThreshold, false);
		UE_LOG(LogSeattle, Log, TEXT("[%s] LocalLeftAttackPressed: queued single LeftAttack (waiting for double-tap)"), *GetNameSafe(this));
	}
}

void ASeattlePlayerController::LeftTapTimerExpired()
{
	// timer fired -> perform single attack
	if (IsLocalController() && !HasAuthority())
	{
		Server_RequestLeftAttack();
		UE_LOG(LogSeattle, Log, TEXT("[%s] LeftTapTimerExpired: no double-tap detected -> requesting LeftAttack on server"), *GetNameSafe(this));
	}
	LastLeftTapTime = 0.f;
}

void ASeattlePlayerController::LocalLeftHookPressed()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] LocalLeftHookPressed: local controller input. IsLocal=%d HasAuthority=%d SharedPawn=%s Role=%d"),
		*GetNameSafe(this), IsLocalController() ? 1 : 0, HasAuthority() ? 1 : 0, *GetNameSafe(ResolveSharedCharacterPawn()), (int32)SharedControlRole);

	if (IsLocalController() && !HasAuthority())
	{
		Server_RequestLeftHook();
	}
}

void ASeattlePlayerController::LocalLeftKickPressed()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] LocalLeftKickPressed: local controller input. IsLocal=%d HasAuthority=%d SharedPawn=%s Role=%d"),
		*GetNameSafe(this), IsLocalController() ? 1 : 0, HasAuthority() ? 1 : 0, *GetNameSafe(ResolveSharedCharacterPawn()), (int32)SharedControlRole);

	if (IsLocalController() && !HasAuthority())
	{
		Server_RequestLeftKick();
	}
}

void ASeattlePlayerController::LocalRightAttackPressed()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] LocalRightAttackPressed: local controller input. IsLocal=%d HasAuthority=%d SharedPawn=%s Role=%d"),
		*GetNameSafe(this), IsLocalController() ? 1 : 0, HasAuthority() ? 1 : 0, *GetNameSafe(ResolveSharedCharacterPawn()), (int32)SharedControlRole);

	if (!(IsLocalController() && !HasAuthority()))
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] LocalRightAttackPressed: Ignored on server/local-authority controller."), *GetNameSafe(this));
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastRightTapTime <= DoubleTapThreshold)
	{
		if (GetWorldTimerManager().IsTimerActive(RightTapTimer))
		{
			GetWorldTimerManager().ClearTimer(RightTapTimer);
		}
		LastRightTapTime = 0.f;
		Server_RequestRightHook();
		UE_LOG(LogSeattle, Log, TEXT("[%s] LocalRightAttackPressed: Detected double-tap -> requesting RightHook on server"), *GetNameSafe(this));
	}
	else
	{
		LastRightTapTime = Now;
		GetWorldTimerManager().SetTimer(RightTapTimer, this, &ASeattlePlayerController::RightTapTimerExpired, DoubleTapThreshold, false);
		UE_LOG(LogSeattle, Log, TEXT("[%s] LocalRightAttackPressed: queued single RightAttack (waiting for double-tap)"), *GetNameSafe(this));
	}
}

void ASeattlePlayerController::RightTapTimerExpired()
{
	if (IsLocalController() && !HasAuthority())
	{
		Server_RequestRightAttack();
		UE_LOG(LogSeattle, Log, TEXT("[%s] RightTapTimerExpired: no double-tap detected -> requesting RightAttack on server"), *GetNameSafe(this));
	}
	LastRightTapTime = 0.f;
}

void ASeattlePlayerController::LocalRightHookPressed()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] LocalRightHookPressed: local controller input. IsLocal=%d HasAuthority=%d SharedPawn=%s Role=%d"),
		*GetNameSafe(this), IsLocalController() ? 1 : 0, HasAuthority() ? 1 : 0, *GetNameSafe(ResolveSharedCharacterPawn()), (int32)SharedControlRole);

	if (IsLocalController() && !HasAuthority())
	{
		Server_RequestRightHook();
	}
}

void ASeattlePlayerController::LocalRightKickPressed()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] LocalRightKickPressed: local controller input. IsLocal=%d HasAuthority=%d SharedPawn=%s Role=%d"),
		*GetNameSafe(this), IsLocalController() ? 1 : 0, HasAuthority() ? 1 : 0, *GetNameSafe(ResolveSharedCharacterPawn()), (int32)SharedControlRole);

	if (IsLocalController() && !HasAuthority())
	{
		Server_RequestRightKick();
	}
}

// ----------------- Server RPC implementations -----------------
// Keep existing validation logic; logs will show if RPCs arrive.

bool ASeattlePlayerController::Server_RequestLeftAttack_Validate()
{
	return IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer();
}

void ASeattlePlayerController::Server_RequestLeftAttack_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestLeftAttack_Implementation: Received request from client. Server role=%d SharedPawn=%s"),
		*GetNameSafe(this), (int32)GetLocalRole(), *GetNameSafe(ResolveSharedCharacterPawn()));

	ASeattleCharacter* Target = ResolveSharedCharacterPawn();
	if (!Target)
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Server_RequestLeftAttack_Implementation: No SharedCharacterPawn resolved"), *GetNameSafe(this));
		return;
	}

	Target->SetActiveAttackType(ESeattleAttackType::SingleTap);

    if (Target->LeftAttackMontage)
	{
		Target->Multicast_PlayAttackMontage(Target->LeftAttackMontage, Target->LeftAttackPlayRate);
		UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestLeftAttack_Implementation: Triggered LeftAttackMontage on %s"), *GetNameSafe(this), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Server_RequestLeftAttack_Implementation: Target has no LeftAttackMontage assigned."), *GetNameSafe(this));
	}
}

bool ASeattlePlayerController::Server_RequestLeftHook_Validate()
{
	return IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer();
}

void ASeattlePlayerController::Server_RequestLeftHook_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestLeftHook_Implementation: Received hook request"), *GetNameSafe(this));
	ASeattleCharacter* Target = ResolveSharedCharacterPawn();
	if (!Target) return;

	Target->SetActiveAttackType(ESeattleAttackType::DoubleTap);

    if (Target->LeftHookMontage)
	{
		Target->Multicast_PlayAttackMontage(Target->LeftHookMontage, Target->LeftHookPlayRate);
		UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestLeftHook_Implementation: Triggered LeftHookMontage on %s"), *GetNameSafe(this), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Server_RequestLeftHook_Implementation: Target has no LeftHookMontage assigned."), *GetNameSafe(this));
	}
}

bool ASeattlePlayerController::Server_RequestLeftKick_Validate()
{
	return IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer();
}

void ASeattlePlayerController::Server_RequestLeftKick_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestLeftKick_Implementation: Received kick request"), *GetNameSafe(this));
	ASeattleCharacter* Target = ResolveSharedCharacterPawn();
	if (!Target) return;

	Target->SetActiveAttackType(ESeattleAttackType::Hold);

    if (Target->LeftKickMontage)
	{
		Target->Multicast_PlayAttackMontage(Target->LeftKickMontage, Target->LeftKickPlayRate);
		UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestLeftKick_Implementation: Triggered LeftKickMontage on %s"), *GetNameSafe(this), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Server_RequestLeftKick_Implementation: Target has no LeftKickMontage assigned."), *GetNameSafe(this));
	}
}

bool ASeattlePlayerController::Server_RequestRightAttack_Validate()
{
	return IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer();
}

void ASeattlePlayerController::Server_RequestRightAttack_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestRightAttack_Implementation: Received right-attack request"), *GetNameSafe(this));
	ASeattleCharacter* Target = ResolveSharedCharacterPawn();
	if (!Target) return;

	Target->SetActiveAttackType(ESeattleAttackType::SingleTap);

    if (Target->RightJabMontage)
	{
		Target->Multicast_PlayAttackMontage(Target->RightJabMontage, Target->RightJabPlayRate);
		UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestRightAttack_Implementation: Triggered RightJabMontage on %s"), *GetNameSafe(this), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Server_RequestRightAttack_Implementation: Target has no RightJabMontage assigned."), *GetNameSafe(this));
	}
}

bool ASeattlePlayerController::Server_RequestRightHook_Validate()
{
	return IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer();
}

void ASeattlePlayerController::Server_RequestRightHook_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestRightHook_Implementation: Received right-hook request"), *GetNameSafe(this));
	ASeattleCharacter* Target = ResolveSharedCharacterPawn();
	if (!Target) return;

	Target->SetActiveAttackType(ESeattleAttackType::DoubleTap);

    if (Target->RightHookMontage)
	{
		Target->Multicast_PlayAttackMontage(Target->RightHookMontage, Target->RightHookPlayRate);
		UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestRightHook_Implementation: Triggered RightHookMontage on %s"), *GetNameSafe(this), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Server_RequestRightHook_Implementation: Target has no RightHookMontage assigned."), *GetNameSafe(this));
	}
}

bool ASeattlePlayerController::Server_RequestRightKick_Validate()
{
	return IsPrimarySharedControlOwner() || IsSecondarySharedControlSharer();
}

void ASeattlePlayerController::Server_RequestRightKick_Implementation()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestRightKick_Implementation: Received right-kick request"), *GetNameSafe(this));
	ASeattleCharacter* Target = ResolveSharedCharacterPawn();
	if (!Target) return;

	Target->SetActiveAttackType(ESeattleAttackType::Hold);

    if (Target->RightKickMontage)
	{
		Target->Multicast_PlayAttackMontage(Target->RightKickMontage, Target->RightKickPlayRate);
		UE_LOG(LogSeattle, Log, TEXT("[%s] Server_RequestRightKick_Implementation: Triggered RightKickMontage on %s"), *GetNameSafe(this), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Server_RequestRightKick_Implementation: Target has no RightKickMontage assigned."), *GetNameSafe(this));
	}
}

void ASeattlePlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeattlePlayerController, SharedControlRole);
	DOREPLIFETIME(ASeattlePlayerController, SharedCharacterPawn);
}
