// Copyright Epic Games, Inc. All Rights Reserved.


#include "SeattlePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Seattle.h"
#include "SeattleCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "Widgets/Input/SVirtualJoystick.h"

ASeattlePlayerController::ASeattlePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	SharedLocalFollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SharedLocalFollowCamera"));
	SharedLocalFollowCamera->bUsePawnControlRotation = false;
	SharedLocalFollowCamera->SetActive(false);
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

			UE_LOG(LogSeattle, Error, TEXT("Could not spawn mobile controls widget."));

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
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
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

void ASeattlePlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeattlePlayerController, SharedControlRole);
	DOREPLIFETIME(ASeattlePlayerController, SharedCharacterPawn);
}
