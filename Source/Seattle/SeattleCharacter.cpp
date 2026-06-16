// Copyright Epic Games, Inc. All Rights Reserved.

#include "SeattleCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"
#include "Seattle.h"
#include "SeattlePlayerController.h"

ASeattleCharacter::ASeattleCharacter()
{
	bReplicates = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ASeattleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASeattleCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASeattleCharacter::MoveStopped);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASeattleCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASeattleCharacter::Look);

		// Attacking
		EnhancedInputComponent->BindAction(LeftAttackAction, ETriggerEvent::Started, this, &ASeattleCharacter::StartLeftAttack);
	}
	else
	{
		UE_LOG(LogSeattle, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ASeattleCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ASeattleCharacter::MoveStopped()
{
	ApplySharedMovementInput(FVector2D::ZeroVector, ESeattleInputContributor::Primary);
}

void ASeattleCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ASeattleCharacter::StartLeftAttack()
{
	NotifyAttackInput(ESeattleAttackType::SingleTap);
}

void ASeattleCharacter::SetSharedPawnEnabled(bool bEnabled)
{
	if (HasAuthority())
	{
		bSharedPawnEnabled = bEnabled;
	}

	if (FollowCamera)
	{
		FollowCamera->SetActive(!bEnabled);
	}
}

void ASeattleCharacter::OnRep_bSharedPawnEnabled()
{
	if (FollowCamera)
	{
		FollowCamera->SetActive(!bSharedPawnEnabled);
	}
}

void ASeattleCharacter::RegisterPrimaryController(APlayerController* InController)
{
	if (HasAuthority())
	{
		RegisteredPrimaryController = InController;
	}
}

void ASeattleCharacter::RegisterSecondaryController(APlayerController* InController)
{
	if (HasAuthority())
	{
		RegisteredSecondaryController = InController;
	}
}

void ASeattleCharacter::ApplySharedMovementInput(FVector2D MovementInput, ESeattleInputContributor Contributor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Contributor == ESeattleInputContributor::Primary)
	{
		PrimaryMoveInput = MovementInput;
	}
	else
	{
		SecondaryMoveInput = MovementInput;
	}

	RefreshMergedMovement();
}

void ASeattleCharacter::ApplySharedLookInput(FVector2D LookInput, ESeattleInputContributor Contributor)
{
	if (Contributor == ESeattleInputContributor::Primary)
	{
		PrimaryLookInput = LookInput;
	}
	else
	{
		SecondaryLookInput = LookInput;
	}
}

void ASeattleCharacter::ApplyMovementForContributor(FVector2D MovementInput, APlayerController* ViewController)
{
	if (!ViewController || MovementInput.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.f, ViewController->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementInput.Y);
	AddMovementInput(RightDirection, MovementInput.X);
}

void ASeattleCharacter::RefreshMergedMovement()
{
	const float CombinedForward = FMath::Clamp(PrimaryMoveInput.Y + SecondaryMoveInput.Y, -1.f, 1.f);
	const float CombinedRight = FMath::Clamp(PrimaryMoveInput.X + SecondaryMoveInput.X, -1.f, 1.f);

	ForwardInput = CombinedForward;
	RightInput = CombinedRight;

	if (bSharedPawnEnabled)
	{
		ApplyMovementForContributor(PrimaryMoveInput, RegisteredPrimaryController);
		ApplyMovementForContributor(SecondaryMoveInput, RegisteredSecondaryController);
		return;
	}

	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, CombinedForward);
		AddMovementInput(RightDirection, CombinedRight);
	}
}

bool ASeattleCharacter::CanAcceptSecondaryController(const APlayerController* InController) const
{
	return InController != nullptr
		&& (!RegisteredSecondaryController || RegisteredSecondaryController == InController);
}

void ASeattleCharacter::DoMove(float Right, float Forward)
{
	ApplySharedMovementInput(FVector2D(Right, Forward), ESeattleInputContributor::Primary);
}

void ASeattleCharacter::DoLook(float Yaw, float Pitch)
{
	ApplySharedLookInput(FVector2D(Yaw, Pitch), ESeattleInputContributor::Primary);

	if (bSharedPawnEnabled)
	{
		if (ASeattlePlayerController* SeattlePC = Cast<ASeattlePlayerController>(GetController()))
		{
			SeattlePC->ApplyLocalSharedLookInput(FVector2D(Yaw, Pitch));
		}
		return;
	}

	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ASeattleCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void ASeattleCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void ASeattleCharacter::SetActiveAttackType(ESeattleAttackType NewAttackType)
{
	if (HasAuthority())
	{
		ActiveAttackType = NewAttackType;
		bIsAttacking = NewAttackType != ESeattleAttackType::None;
	}
}

void ASeattleCharacter::NotifyAttackInput(ESeattleAttackType AttackType)
{
	if (HasAuthority())
	{
		SetActiveAttackType(AttackType);
	}
	else
	{
		Server_NotifyAttackInput(AttackType);
	}
}

bool ASeattleCharacter::Server_NotifyAttackInput_Validate(ESeattleAttackType AttackType)
{
	return AttackType != ESeattleAttackType::None;
}

void ASeattleCharacter::Server_NotifyAttackInput_Implementation(ESeattleAttackType AttackType)
{
	SetActiveAttackType(AttackType);
}

void ASeattleCharacter::RequestPlayAttackMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		return;
	}

	if (HasAuthority())
	{
		PlayAnimMontage(Montage, PlayRate);
	}
	else
	{
		Server_PlayAttackMontage(Montage, PlayRate);
	}
}

bool ASeattleCharacter::Server_PlayAttackMontage_Validate(UAnimMontage* Montage, float PlayRate)
{
	return Montage != nullptr && PlayRate > 0.f;
}

void ASeattleCharacter::Server_PlayAttackMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	if (Montage)
	{
		PlayAnimMontage(Montage, PlayRate);
	}
}

void ASeattleCharacter::OnRep_bIsAttacking()
{
	// Hook for Blueprint anim/UI reactions when attack state replicates.
}

void ASeattleCharacter::OnRep_ActiveAttackType()
{
	bIsAttacking = ActiveAttackType != ESeattleAttackType::None;
}

void ASeattleCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeattleCharacter, bSharedPawnEnabled);
	DOREPLIFETIME(ASeattleCharacter, RegisteredPrimaryController);
	DOREPLIFETIME(ASeattleCharacter, RegisteredSecondaryController);
	DOREPLIFETIME(ASeattleCharacter, PrimaryMoveInput);
	DOREPLIFETIME(ASeattleCharacter, SecondaryMoveInput);
	DOREPLIFETIME(ASeattleCharacter, ForwardInput);
	DOREPLIFETIME(ASeattleCharacter, RightInput);
	DOREPLIFETIME(ASeattleCharacter, bIsAttacking);
	DOREPLIFETIME(ASeattleCharacter, ActiveAttackType);
}
