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
#include "Animation/AnimInstance.h"
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
		UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: EnhancedInputComponent found. LocalRole=%d IsLocallyControlled=%d HasAuthority=%d"),
			*GetNameSafe(this), (int32)GetLocalRole(), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASeattleCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASeattleCharacter::MoveStopped);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASeattleCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASeattleCharacter::Look);

		// Attacking: bind on the Character only when this pawn is locally controlled on a client (i.e. IsLocallyControlled && not authority).
		// This prevents server-local controllers (listen-server) from firing attacks via the character input binding.
		if (IsLocallyControlled() && !HasAuthority())
		{
			if (LeftAttackAction)
			{
				EnhancedInputComponent->BindAction(LeftAttackAction, ETriggerEvent::Started, this, &ASeattleCharacter::StartLeftAttack);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: Bound LeftAttackAction on character (client-local)"), *GetNameSafe(this));
			}

			if (LeftHookAction)
			{
				EnhancedInputComponent->BindAction(LeftHookAction, ETriggerEvent::Started, this, &ASeattleCharacter::StartLeftHook);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: Bound LeftHookAction on character (client-local)"), *GetNameSafe(this));
			}

			if (LeftKickAction)
			{
				EnhancedInputComponent->BindAction(LeftKickAction, ETriggerEvent::Started, this, &ASeattleCharacter::StartLeftKick);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: Bound LeftKickAction on character (client-local)"), *GetNameSafe(this));
			}

			if (RightAttackAction)
			{
				EnhancedInputComponent->BindAction(RightAttackAction, ETriggerEvent::Started, this, &ASeattleCharacter::StartRightAttack);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: Bound RightAttackAction on character (client-local)"), *GetNameSafe(this));
			}

			if (RightHookAction)
			{
				EnhancedInputComponent->BindAction(RightHookAction, ETriggerEvent::Started, this, &ASeattleCharacter::StartRightHook);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: Bound RightHookAction on character (client-local)"), *GetNameSafe(this));
			}

			if (RightKickAction)
			{
				EnhancedInputComponent->BindAction(RightKickAction, ETriggerEvent::Started, this, &ASeattleCharacter::StartRightKick);
				UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: Bound RightKickAction on character (client-local)"), *GetNameSafe(this));
			}
		}
		else
		{
			UE_LOG(LogSeattle, Log, TEXT("[%s] SetupPlayerInputComponent: Skipping character attack bindings. IsLocallyControlled=%d HasAuthority=%d"), *GetNameSafe(this), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);
		}
	}
	else
	{
		UE_LOG(LogSeattle, Error, TEXT("[%s] SetupPlayerInputComponent: Failed to find an Enhanced Input component!"), *GetNameSafe(this));
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
	UE_LOG(LogSeattle, Log, TEXT("[%s] StartLeftAttack called. LocalRole=%d IsLocallyControlled=%d HasAuthority=%d"),
		*GetNameSafe(this), (int32)GetLocalRole(), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);

	// Update replicated attack state so AnimBP/state machines can react
	NotifyAttackInput(ESeattleAttackType::SingleTap);

	// Owned-pawn (client) case: Request server to play (RequestPlayAttackMontage will call Server RPC when appropriate).
	if (!HasAuthority())
	{
		if (LeftAttackMontage)
		{
			RequestPlayAttackMontage(LeftAttackMontage, LeftAttackPlayRate);
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("[%s] StartLeftAttack: LeftAttackMontage is null on client-owned pawn."), *GetNameSafe(this));
		}
		return;
	}

	// Server-authoritative case: play and multicast directly
	if (LeftAttackMontage)
	{
		Multicast_PlayAttackMontage(LeftAttackMontage, LeftAttackPlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] StartLeftAttack: LeftAttackMontage is null on authoritative pawn."), *GetNameSafe(this));
	}
}

void ASeattleCharacter::StartLeftHook()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] StartLeftHook called. LocalRole=%d IsLocallyControlled=%d HasAuthority=%d"),
		*GetNameSafe(this), (int32)GetLocalRole(), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);

	NotifyAttackInput(ESeattleAttackType::DoubleTap);

	if (!HasAuthority())
	{
		if (LeftHookMontage)
		{
			RequestPlayAttackMontage(LeftHookMontage, LeftHookPlayRate);
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("[%s] StartLeftHook: LeftHookMontage is null on client-owned pawn."), *GetNameSafe(this));
		}
		return;
	}

	if (LeftHookMontage)
	{
		Multicast_PlayAttackMontage(LeftHookMontage, LeftHookPlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] StartLeftHook: LeftHookMontage is null on authoritative pawn."), *GetNameSafe(this));
	}
}

void ASeattleCharacter::StartLeftKick()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] StartLeftKick called. LocalRole=%d IsLocallyControlled=%d HasAuthority=%d"),
		*GetNameSafe(this), (int32)GetLocalRole(), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);

	NotifyAttackInput(ESeattleAttackType::Hold);

	if (!HasAuthority())
	{
		if (LeftKickMontage)
		{
			RequestPlayAttackMontage(LeftKickMontage, LeftKickPlayRate);
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("[%s] StartLeftKick: LeftKickMontage is null on client-owned pawn."), *GetNameSafe(this));
		}
		return;
	}

	if (LeftKickMontage)
	{
		Multicast_PlayAttackMontage(LeftKickMontage, LeftKickPlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] StartLeftKick: LeftKickMontage is null on authoritative pawn."), *GetNameSafe(this));
	}
}

void ASeattleCharacter::StartRightAttack()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] StartRightAttack called. LocalRole=%d IsLocallyControlled=%d HasAuthority=%d"),
		*GetNameSafe(this), (int32)GetLocalRole(), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);

	NotifyAttackInput(ESeattleAttackType::SingleTap);

	if (!HasAuthority())
	{
		if (RightJabMontage)
		{
			RequestPlayAttackMontage(RightJabMontage, RightJabPlayRate);
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("[%s] StartRightAttack: RightJabMontage is null on client-owned pawn."), *GetNameSafe(this));
		}
		return;
	}

	if (RightJabMontage)
	{
		Multicast_PlayAttackMontage(RightJabMontage, RightJabPlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] StartRightAttack: RightJabMontage is null on authoritative pawn."), *GetNameSafe(this));
	}
}

void ASeattleCharacter::StartRightHook()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] StartRightHook called. LocalRole=%d IsLocallyControlled=%d HasAuthority=%d"),
		*GetNameSafe(this), (int32)GetLocalRole(), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);

	NotifyAttackInput(ESeattleAttackType::DoubleTap);

	if (!HasAuthority())
	{
		if (RightHookMontage)
		{
			RequestPlayAttackMontage(RightHookMontage, RightHookPlayRate);
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("[%s] StartRightHook: RightHookMontage is null on client-owned pawn."), *GetNameSafe(this));
		}
		return;
	}

	if (RightHookMontage)
	{
		Multicast_PlayAttackMontage(RightHookMontage, RightHookPlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] StartRightHook: RightHookMontage is null on authoritative pawn."), *GetNameSafe(this));
	}
}

void ASeattleCharacter::StartRightKick()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] StartRightKick called. LocalRole=%d IsLocallyControlled=%d HasAuthority=%d"),
		*GetNameSafe(this), (int32)GetLocalRole(), IsLocallyControlled() ? 1 : 0, HasAuthority() ? 1 : 0);

	NotifyAttackInput(ESeattleAttackType::Hold);

	if (!HasAuthority())
	{
		if (RightKickMontage)
		{
			RequestPlayAttackMontage(RightKickMontage, RightKickPlayRate);
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("[%s] StartRightKick: RightKickMontage is null on client-owned pawn."), *GetNameSafe(this));
		}
		return;
	}

	if (RightKickMontage)
	{
		Multicast_PlayAttackMontage(RightKickMontage, RightKickPlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] StartRightKick: RightKickMontage is null on authoritative pawn."), *GetNameSafe(this));
	}
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
	UE_LOG(LogSeattle, Log, TEXT("[%s] OnRep_bSharedPawnEnabled: bSharedPawnEnabled=%d"), *GetNameSafe(this), bSharedPawnEnabled ? 1 : 0);
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
		UE_LOG(LogSeattle, Log, TEXT("[%s] RegisterPrimaryController: registered %s"), *GetNameSafe(this), *GetNameSafe(InController));
	}
}

void ASeattleCharacter::RegisterSecondaryController(APlayerController* InController)
{
	if (HasAuthority())
	{
		RegisteredSecondaryController = InController;
		UE_LOG(LogSeattle, Log, TEXT("[%s] RegisterSecondaryController: registered %s"), *GetNameSafe(this), *GetNameSafe(InController));
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
		UE_LOG(LogSeattle, Log, TEXT("[%s] SetActiveAttackType: ActiveAttackType=%d bIsAttacking=%d"), *GetNameSafe(this), (int32)ActiveAttackType, bIsAttacking ? 1 : 0);
	}
}

void ASeattleCharacter::NotifyAttackInput(ESeattleAttackType AttackType)
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] NotifyAttackInput called. HasAuthority=%d, AttackType=%d"), *GetNameSafe(this), HasAuthority() ? 1 : 0, (int32)AttackType);

	if (HasAuthority())
	{
		SetActiveAttackType(AttackType);
	}
	else
	{
		Server_NotifyAttackInput(AttackType);
		UE_LOG(LogSeattle, Log, TEXT("[%s] NotifyAttackInput: Sent Server_NotifyAttackInput RPC"), *GetNameSafe(this));
	}
}

bool ASeattleCharacter::Server_NotifyAttackInput_Validate(ESeattleAttackType AttackType)
{
	return AttackType != ESeattleAttackType::None;
}

void ASeattleCharacter::Server_NotifyAttackInput_Implementation(ESeattleAttackType AttackType)
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_NotifyAttackInput_Implementation: Received AttackType=%d on server (LocalRole=%d)"), *GetNameSafe(this), (int32)AttackType, (int32)GetLocalRole());
	SetActiveAttackType(AttackType);
}

void ASeattleCharacter::RequestPlayAttackMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] RequestPlayAttackMontage: Montage is null"), *GetNameSafe(this));
		return;
	}

	// If we're the authoritative instance (server/host), multicast so everyone plays.
	if (HasAuthority())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] RequestPlayAttackMontage: HasAuthority -> calling Multicast. Montage=%s PlayRate=%f"), *GetNameSafe(this), *Montage->GetName(), PlayRate);
		Multicast_PlayAttackMontage(Montage, PlayRate);
	}
	else
	{
		// Ask the server to play (server will multicast)
		UE_LOG(LogSeattle, Log, TEXT("[%s] RequestPlayAttackMontage: Not authority -> calling Server_PlayAttackMontage RPC. Montage=%s PlayRate=%f"), *GetNameSafe(this), *Montage->GetName(), PlayRate);
		Server_PlayAttackMontage(Montage, PlayRate);
	}
}

bool ASeattleCharacter::Server_PlayAttackMontage_Validate(UAnimMontage* Montage, float PlayRate)
{
	return Montage != nullptr && PlayRate > 0.f;
}

void ASeattleCharacter::Server_PlayAttackMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Server_PlayAttackMontage_Implementation: server received request Montage=%s PlayRate=%f LocalRole=%d"), *GetNameSafe(this), Montage ? *Montage->GetName() : TEXT("null"), PlayRate, (int32)GetLocalRole());
	if (Montage)
	{
		// Server initiates the multicast so everyone (including server) plays the montage
		Multicast_PlayAttackMontage(Montage, PlayRate);
	}
}

void ASeattleCharacter::Multicast_PlayAttackMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] Multicast_PlayAttackMontage_Implementation: Called locally. LocalRole=%d Montage=%s PlayRate=%f"), *GetNameSafe(this), (int32)GetLocalRole(), Montage ? *Montage->GetName() : TEXT("null"), PlayRate);

	if (!Montage)
	{
		UE_LOG(LogSeattle, Warning, TEXT("[%s] Multicast_PlayAttackMontage_Implementation: Montage null, aborting"), *GetNameSafe(this));
		return;
	}

	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel)
	{
		UE_LOG(LogSeattle, Error, TEXT("[%s] Multicast_PlayAttackMontage_Implementation: GetMesh() returned null"), *GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInst = Skel->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogSeattle, Error, TEXT("[%s] Multicast_PlayAttackMontage_Implementation: GetAnimInstance() returned null"), *GetNameSafe(this));
		return;
	}

	const float PlayResult = AnimInst->Montage_Play(Montage, PlayRate);
	UE_LOG(LogSeattle, Log, TEXT("[%s] Multicast_PlayAttackMontage_Implementation: Montage_Play called, return %f"), *GetNameSafe(this), PlayResult);
}

void ASeattleCharacter::OnRep_bIsAttacking()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] OnRep_bIsAttacking: bIsAttacking=%d LocalRole=%d"), *GetNameSafe(this), bIsAttacking ? 1 : 0, (int32)GetLocalRole());
	// Hook for Blueprint anim/UI reactions when attack state replicates.
}

void ASeattleCharacter::OnRep_ActiveAttackType()
{
	UE_LOG(LogSeattle, Log, TEXT("[%s] OnRep_ActiveAttackType: ActiveAttackType=%d LocalRole=%d"), *GetNameSafe(this), (int32)ActiveAttackType, (int32)GetLocalRole());
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