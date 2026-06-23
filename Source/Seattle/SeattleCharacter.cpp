// Copyright Epic Games, Inc. All Rights Reserved.

#include "SeattleCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "UI/SeattleHUD.h"
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
#include "SeattleAI.h"
#include "ImpactFXActor.h"
#include "DrawDebugHelpers.h"
#include "CombatAttacker.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"
#include "Variant_Combat/AI/CombatEnemy.h"
#include "Camera/CameraShakeBase.h"
#include "StaminaComponent.h"

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

	// Create stamina component
	StaminaComponent = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComponent"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ASeattleCharacter::Multicast_SpawnImpactFXClass_Implementation(TSubclassOf<AImpactFXActor> FXClass, FVector Location)
{

	UE_LOG(LogSeattle, Log, TEXT("spawned fx class testing"));
	
	TSubclassOf<AImpactFXActor> UseClass = FXClass ? FXClass : ImpactFXActorClass;
	if (!UseClass)
	{
		return;
	}


	//UE_LOG(LogSeattle, Log, TEXT("jkljkljkljkl 2nd"));

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}


	//UE_LOG(LogSeattle, Log, TEXT("jkljkljkljkl 3rd"));


	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
     AImpactFXActor* FX = World->SpawnActor<AImpactFXActor>(UseClass, Location, FRotator::ZeroRotator, Params);
	if (FX)
	{
		UE_LOG(LogSeattle, Log, TEXT("Multicast_SpawnImpactFXClass: Spawned FX class %s at %s (class=%s)"), *GetNameSafe(FX), *Location.ToString(), *GetNameSafe(UseClass->GetDefaultObject()));
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("Multicast_SpawnImpactFXClass: Failed to spawn FX class %s at %s"), *GetNameSafe(UseClass->GetDefaultObject()), *Location.ToString());
	}
}

float ASeattleCharacter::GetHealthPercent() const
{
	if (MaxHealth <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(Health / MaxHealth, 0.f, 1.f);
}

void ASeattleCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
   // UE_LOG(LogSeattle, Log, TEXT("ASeattleCharacter::ApplyDamage called on %s by %s Damage=%f HasAuthority=%d"), *GetNameSafe(this), DamageCauser ? *GetNameSafe(DamageCauser) : TEXT("null"), Damage, HasAuthority() ? 1 : 0);

	UE_LOG(LogSeattle, Log, TEXT("fx class apply damage"));

	if (!HasAuthority())
	{
		return;
	}

	// always apply fixed 10 damage when hit by AI, otherwise use provided damage
	float Applied = Damage;
	if (DamageCauser && (DamageCauser->IsA(ASeattleAI::StaticClass()) || DamageCauser->IsA(AActor::StaticClass())))
	{
		// if instigator is AI (ASeattleAI or CombatEnemy), use 10
		if (DamageCauser->IsA(ASeattleAI::StaticClass()) || DamageCauser->IsA(ACombatEnemy::StaticClass()))
		{
			Applied = 10.f;
		}
	}


	//UE_LOG(LogSeattle, Log, TEXT("jkljkljkljkl -1st"));

    const float Previous = Health;
	Health = FMath::Clamp(Health - Applied, 0.f, MaxHealth);

	const float HealthDelta = Previous - Health;

	// spawn different impact FX when hit by AI
	if (DamageCauser && (DamageCauser->IsA(ASeattleAI::StaticClass()) || DamageCauser->IsA(ACombatEnemy::StaticClass())))
	{
		UE_LOG(LogSeattle, Log, TEXT("fx class apply damage"));
		// prefer AI-specific FX class if set, otherwise fallback to generic
		TSubclassOf<AImpactFXActor> UseClass = ImpactFXActorAIHitClass ? ImpactFXActorAIHitClass : ImpactFXActorClass;
		if (UseClass)
		{
			UE_LOG(LogSeattle, Log, TEXT("fx class final fun"));
            // spawn via multicast so all clients see it
			Multicast_SpawnImpactFXClass(UseClass, DamageLocation);
		}
	}
	else
	{
		// generic impact FX
        Multicast_SpawnImpactFX(DamageLocation);
	}

    // Broadcast health change to listeners (HUD, etc.)
	OnHealthChanged.Broadcast(Health, HealthDelta);

	// Trigger client-side effects on the owning client
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
	if (ASeattlePlayerController* PC = Cast<ASeattlePlayerController>(It->Get()))
		{
			PC->Client_PlayHitEffects(HitCameraShakeClass);
		}
	}

    if (Health <= 0.f && Previous > 0.f)
	{
		// Enter knocked-down state and notify clients
		bIsKnockedDown = true;
		OnRep_bIsKnockedDown();
		HandleDeath();
	}
}

void ASeattleCharacter::OnRep_bIsKnockedDown()
{
	UE_LOG(LogSeattle, Log, TEXT("ASeattleCharacter::OnRep_bIsKnockedDown: %s bIsKnockedDown=%d"), *GetNameSafe(this), bIsKnockedDown ? 1 : 0);
	// Blueprint / AnimGraph can bind to this replicated property or implement logic in BP on replication.
}

void ASeattleCharacter::RequestSlide(FVector Direction)
{
	if (bIsKnockedDown)
	{
		return;
	}

	if (HasAuthority())
	{
		DoSlide(Direction);
	}
	else
	{
        UE_LOG(LogSeattle, Log, TEXT("RequestSlide: sending Server_RequestSlide from %s dir=%s"), *GetNameSafe(this), *Direction.ToString());
		Server_RequestSlide(Direction);
	}
}

bool ASeattleCharacter::Server_RequestSlide_Validate(FVector Direction)
{
	return true;
}

void ASeattleCharacter::Server_RequestSlide_Implementation(FVector Direction)
{
	if (bIsKnockedDown)
	{
		return;
	}
    UE_LOG(LogSeattle, Log, TEXT("Server_RequestSlide_Implementation: received on server for %s dir=%s"), *GetNameSafe(this), *Direction.ToString());
	DoSlide(Direction);
}

void ASeattleCharacter::DoSlide(FVector Direction)
{
	// Check stamina availability (server-side)
	if (HasAuthority() && StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("DoSlide: Not enough stamina for %s"), *GetNameSafe(this));
		return;
	}

	if (!Controller)
	{
		Controller = GetController();
	}

	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FVector DirNorm = Direction.GetSafeNormal();
	// desired speed to cover SlideDistance within SlideDuration
	const float Speed = (SlideDuration > 0.f) ? (SlideDistance / SlideDuration) : SlideDistance;
	const FVector Velocity = DirNorm * Speed;

	LaunchCharacter(Velocity, true, true);

	// Consume stamina on server
	if (HasAuthority() && StaminaComponent)
	{
		StaminaComponent->ConsumeStamina();
	}

	UE_LOG(LogTemp, Warning,
		TEXT("Character=%s Owner=%s LocalRole=%d RemoteRole=%d"),
		*GetName(),
		*GetNameSafe(GetOwner()),
		(int32)GetLocalRole(),
		(int32)GetRemoteRole());

	// Trigger client-side overlay/effects on owner
    UE_LOG(LogSeattle, Log, TEXT("DoSlide: Executing slide for %s dir=%s distance=%f duration=%f"), *GetNameSafe(this), *Direction.ToString(), SlideDistance, SlideDuration);
	if (HasAuthority())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ASeattlePlayerController* PC = Cast<ASeattlePlayerController>(It->Get()))
			{
				PC->Client_StartSlideOverlay(SlideDuration);
			}
		}
	}
	//Client_PlaySlideEffects(SlideDuration);
}

void ASeattleCharacter::Client_PlaySlideEffects_Implementation(float Duration)
{
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	UE_LOG(LogTemp, Warning,
		TEXT("Client_PlaySlideEffects on %s"),
		GIsServer ? TEXT("SERVER") : TEXT("CLIENT"));

	UE_LOG(LogTemp, Warning,
		TEXT("LocalPC = %s"),
		*GetNameSafe(LocalPC));

	UE_LOG(LogTemp, Warning,
		TEXT("Character Owner = %s"),
		*GetNameSafe(GetOwner()));

	if (LocalPC)
	{
		if (ASeattleHUD* HUD = Cast<ASeattleHUD>(LocalPC->GetHUD()))
		{
			UE_LOG(LogTemp, Warning, TEXT("HUD FOUND"));
			HUD->StartSlideOverlay(Duration);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("HUD NOT FOUND"));
		}
	}
}

void ASeattleCharacter::HandleDeath()
{
	// default behavior: disable input and ragdoll
	GetCharacterMovement()->DisableMovement();
	GetMesh()->SetSimulatePhysics(true);
}

void ASeattleCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	if (!HasAuthority()) return;
	Health = FMath::Clamp(Health + Healing, 0.f, MaxHealth);
}

void ASeattleCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// optional: react to incoming attacks
}

void ASeattleCharacter::OnRep_Health()
{
    OnHealthChanged.Broadcast(Health, 0.f);
}

void ASeattleCharacter::Client_PlayHitEffects_Implementation()
{
    // Play camera shake on owning client
	APlayerController* LocalPC = nullptr;
	if (GetWorld())
	{
		LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	}

	if (LocalPC && HitCameraShakeClass)
	{
		LocalPC->ClientStartCameraShake(HitCameraShakeClass);
	}

	// Also trigger HUD damage overlay on the owning client
	if (LocalPC)
	{
		if (ASeattleHUD* HUD = Cast<ASeattleHUD>(LocalPC->GetHUD()))
		{
			UE_LOG(LogSeattle, Log, TEXT("Client_PlayHitEffects_Implementation: triggering damage overlay on %s"), *GetNameSafe(LocalPC));
			HUD->StartDamageOverlay();
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("Client_PlayHitEffects_Implementation: HUD not found on controller %s"), *GetNameSafe(LocalPC));
		}
	}
}



void ASeattleCharacter::Multicast_SpawnImpactFX_Implementation(FVector Location)
{
	//UE_LOG(LogSeattle, Log, TEXT("fx class"));


    if (!ImpactFXActorClass)
	{
		UE_LOG(LogSeattle, Warning, TEXT("Multicast_SpawnImpactFX: ImpactFXActorClass is null on %s"), *GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSeattle, Warning, TEXT("Multicast_SpawnImpactFX: World is null on %s"), *GetNameSafe(this));
		return;
	}

	// Slight upward offset to avoid spawning inside geometry
	FVector SpawnLocation = Location + FVector(0.f, 0.f, 6.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AImpactFXActor* FX = World->SpawnActor<AImpactFXActor>(ImpactFXActorClass, SpawnLocation, FRotator::ZeroRotator, Params);
    if (FX)
	{
		UE_LOG(LogSeattle, Log, TEXT("Multicast_SpawnImpactFX: Spawned FX class %s at %s (class=%s)"), *GetNameSafe(FX), *SpawnLocation.ToString(), *GetNameSafe(ImpactFXActorClass->GetDefaultObject()));
		//UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [SeattleCharacter] Multicast_SpawnImpactFX spawned FX at %s"), *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("Multicast_SpawnImpactFX: Failed to spawn FX class %s at %s"), *GetNameSafe(ImpactFXActorClass->GetDefaultObject()), *SpawnLocation.ToString());
		//UE_LOG(LogTemp, Warning, TEXT("TESTINGAIAI [SeattleCharacter] Multicast_SpawnImpactFX FAILED at %s"), *SpawnLocation.ToString());
	}
}

void ASeattleCharacter::PlayMontageForAttack(bool bRightSide, ESeattleAttackType AttackType)
{
	UAnimMontage* MontageToPlay = nullptr;
	float PlayRate = 1.f;

	if (bRightSide)
	{
		switch (AttackType)
		{
		case ESeattleAttackType::SingleTap:
			MontageToPlay = RightJabMontage;
			PlayRate = RightJabPlayRate;
			break;
		case ESeattleAttackType::DoubleTap:
			MontageToPlay = RightHookMontage;
			PlayRate = RightHookPlayRate;
			break;
		case ESeattleAttackType::Hold:
			MontageToPlay = RightKickMontage;
			PlayRate = RightKickPlayRate;
			break;
		default:
			break;
		}
	}
	else
	{
		switch (AttackType)
		{
		case ESeattleAttackType::SingleTap:
			MontageToPlay = LeftAttackMontage;
			PlayRate = LeftAttackPlayRate;
			break;
		case ESeattleAttackType::DoubleTap:
			MontageToPlay = LeftHookMontage;
			PlayRate = LeftHookPlayRate;
			break;
		case ESeattleAttackType::Hold:
			MontageToPlay = LeftKickMontage;
			PlayRate = LeftKickPlayRate;
			break;
		default:
			break;
		}
	}

	if (MontageToPlay)
	{
		RequestPlayAttackMontage(MontageToPlay, PlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Warning, TEXT("PlayMontageForAttack: No montage assigned for side=%d type=%d"), bRightSide ? 1 : 0, (int32)AttackType);
	}
}

void ASeattleCharacter::SetReplicatedAimRotation(FRotator NewAimRotation)
{
    // Always update locally so the owning client sees immediate aim feedback.
	ReplicatedAimRotation = NewAimRotation;

	// If we're not authoritative, send the value to server for replication to other clients.
	if (!HasAuthority())
	{
	//	Server_SetAimRotation(NewAimRotation);
	}
}

bool ASeattleCharacter::Server_SetAimRotation_Validate(FRotator NewAimRotation)
{
	// basic validation - rotation components within reasonable range
	return FMath::IsFinite(NewAimRotation.Pitch) && FMath::IsFinite(NewAimRotation.Yaw) && FMath::IsFinite(NewAimRotation.Roll);
}

void ASeattleCharacter::Server_SetAimRotation_Implementation(FRotator NewAimRotation)
{
	ReplicatedAimRotation = NewAimRotation;
}

void ASeattleCharacter::OnRep_AimRotation()
{
	UE_LOG(LogSeattle, Verbose, TEXT("%s OnRep_AimRotation: %s"), *GetNameSafe(this), *ReplicatedAimRotation.ToCompactString());
}

void ASeattleCharacter::GetAimDeltaAngles(float& OutYaw, float& OutPitch) const
{
	// Compute aim delta relative to actor rotation in a normalized way
	const FRotator ActorRot = GetActorRotation();
	FRotator AimRot = ReplicatedAimRotation;

	// Normalize both rotators
	const float AimYaw = FRotator::NormalizeAxis(AimRot.Yaw);
	const float AimPitch = FRotator::NormalizeAxis(AimRot.Pitch);
	const float ActorYaw = FRotator::NormalizeAxis(ActorRot.Yaw);
	const float ActorPitch = FRotator::NormalizeAxis(ActorRot.Pitch);

	float DeltaYaw = AimYaw - ActorYaw;
	float DeltaPitch = AimPitch - ActorPitch;

	DeltaYaw = FRotator::NormalizeAxis(DeltaYaw);
	DeltaPitch = FRotator::NormalizeAxis(DeltaPitch);

	// Clamp pitch to conventional limits
	DeltaPitch = FMath::Clamp(DeltaPitch, -90.f, 90.f);

	OutYaw = DeltaYaw;
	OutPitch = DeltaPitch;
}

void ASeattleCharacter::RequestPerformMeleeAttack(FVector AimDirection, float Range, float Radius, float Damage)
{
    UE_LOG(LogSeattle, Log, TEXT("request perform melee attack %s"), *GetNameSafe(this));
    // Only non-authoritative owning clients should request the server to perform the melee attack.
	if (!HasAuthority())
	{
		Server_PerformMeleeAttack(AimDirection.GetSafeNormal(), Range, Radius, Damage);
		return;
	}

	// Ignore client-style requests on authoritative server instances (prevents server-local BPs from performing attacks).
	UE_LOG(LogSeattle, Verbose, TEXT("RequestPerformMeleeAttack ignored on authoritative instance for %s"), *GetNameSafe(this));
}

bool ASeattleCharacter::Server_PerformMeleeAttack_Validate(FVector NormalizedAimDir, float Range, float Radius, float Damage)
{
    UE_LOG(LogSeattle, Log, TEXT("Server_PerformMeleeAttack_Validate: Aim=%s Range=%f Radius=%f Damage=%f"), *NormalizedAimDir.ToString(), Range, Radius, Damage);
	const bool bValid = (NormalizedAimDir.IsNearlyZero() == false) && (Range > 0.f) && (Radius >= 0.f) && (Damage >= 0.f);
	if (!bValid)
	{
		UE_LOG(LogSeattle, Warning, TEXT("Server_PerformMeleeAttack_Validate: validation failed (Aim zero or invalid params)."));
	}
	return bValid;
}

void ASeattleCharacter::Server_PerformMeleeAttack_Implementation(FVector NormalizedAimDir, float Range, float Radius, float Damage)
{
	// Authority: perform trace and apply damage to any ASeattleAI hit
    UE_LOG(LogSeattle, Log, TEXT("%s Server_PerformMeleeAttack_Implementation: Aim=%s Range=%f Radius=%f Damage=%f"), *GetNameSafe(this), *NormalizedAimDir.ToString(), Range, Radius, Damage);

	UE_LOG(LogSeattle, Log, TEXT("fx class maybe"));
	// Server-authoritative stamina check: prevent client from performing melee if not enough stamina
	if (StaminaComponent)
	{
		if (!StaminaComponent->HasEnoughStamina())
		{
			UE_LOG(LogSeattle, Log, TEXT("%s Server_PerformMeleeAttack_Implementation: Not enough stamina to perform melee"), *GetNameSafe(this));
			return;
		}

		// consume stamina for this melee action
		StaminaComponent->ConsumeStamina();
	}

	FVector Start = GetActorLocation() + FVector(0.f, 0.f, 70.f);
	FVector End = Start + NormalizedAimDir * Range;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeAttack), true, this);
	Params.AddIgnoredActor(this);

	bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);


	UE_LOG(LogSeattle, Log, TEXT("fx class ye ahbest"));
	if (bHit && Hit.GetActor())
	{
		UE_LOG(LogSeattle, Log, TEXT("%s Melee hit actor %s at location %s"), *GetNameSafe(this), *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString());

		if (ASeattleAI* AI = Cast<ASeattleAI>(Hit.GetActor()))
		{
			AI->ApplyHealthChange(Damage);
		}
		else
		{
			// Could add damage interface support here
		}

		// Optionally draw debug
		//DrawDebugSphere(GetWorld(), Hit.ImpactPoint, Radius, 12, FColor::Red, false, 2.f);

        // Spawn impact VFX on all clients (multicast).
		// Prefer the class-based multicast so callers can explicitly choose a FX class if needed.
		if (ImpactFXActorClass)
		{

			Multicast_SpawnImpactFXClass(ImpactFXActorClass, Hit.ImpactPoint);
		}
		else
		{
			UE_LOG(LogSeattle, Warning, TEXT("Server_PerformMeleeAttack_Implementation: No ImpactFXActorClass set, cannot spawn impact FX class for hit at %s"), *Hit.ImpactPoint.ToString());
		}
	}
	else
	{
		//DrawDebugLine(GetWorld(), Start, End, FColor::Blue, false, 1.f, 0, 2.f);
	}
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

	// Server-authoritative case: check stamina before playing
	if (StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] StartLeftAttack: Not enough stamina"), *GetNameSafe(this));
		return;
	}

	// Server-authoritative case: play and multicast directly
	if (LeftAttackMontage)
	{
		if (StaminaComponent)
		{
			//StaminaComponent->ConsumeStamina();
		}
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

	// Check stamina on server
	if (StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] StartLeftHook: Not enough stamina"), *GetNameSafe(this));
		return;
	}

	if (LeftHookMontage)
	{
		if (StaminaComponent)
		{
			//StaminaComponent->ConsumeStamina();
		}
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

	// Check stamina on server
	if (StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] StartLeftKick: Not enough stamina"), *GetNameSafe(this));
		return;
	}

	if (LeftKickMontage)
	{
		if (StaminaComponent)
		{
			//StaminaComponent->ConsumeStamina();
		}
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

	// Check stamina on server
	if (StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] StartRightAttack: Not enough stamina"), *GetNameSafe(this));
		return;
	}

	if (RightJabMontage)
	{
		if (StaminaComponent)
		{
			//StaminaComponent->ConsumeStamina();
		}
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

	// Check stamina on server
	if (StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] StartRightHook: Not enough stamina"), *GetNameSafe(this));
		return;
	}

	if (RightHookMontage)
	{
		if (StaminaComponent)
		{
			//StaminaComponent->ConsumeStamina();
		}
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

	// Check stamina on server
	if (StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] StartRightKick: Not enough stamina"), *GetNameSafe(this));
		return;
	}

	if (RightKickMontage)
	{
		if (StaminaComponent)
		{
			//StaminaComponent->ConsumeStamina();
		}
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

// ~begin CombatAttacker interface
void ASeattleCharacter::DoAttackTrace(FName DamageSourceBone)
{
	UE_LOG(LogSeattle, Log, TEXT("NOT POSSIBLE NOT POSSIBLE  DoAttackTrace called on %s; DamageSourceBone=%s HasAuthority=%d"), *GetNameSafe(this), *DamageSourceBone.ToString(), HasAuthority() ? 1 : 0);

	// Compute aim direction: prefer replicated aim rotation if available, else controller forward, else actor forward
	FVector AimDir = GetActorForwardVector();
	if (!ReplicatedAimRotation.IsZero())
	{
		AimDir = ReplicatedAimRotation.Vector();
	}
	else if (AController* C = GetController())
	{
		AimDir = C->GetControlRotation().Vector();
	}

    UE_LOG(LogSeattle, Log, TEXT("DoAttackTrace: computed AimDir=%s. Requesting melee attack."), *AimDir.ToString());

	// If we're the authority, perform the melee sweep directly. Otherwise request the server.
	if (HasAuthority())
	{
		UE_LOG(LogSeattle, Log, TEXT("DoAttackTrace: running authoritative melee sweep locally on %s"), *GetNameSafe(this));
		Server_PerformMeleeAttack(AimDir.GetSafeNormal(), MeleeRange, MeleeRadius, MeleeDamage);
	}
	else
	{
		RequestPerformMeleeAttack(AimDir.GetSafeNormal(), MeleeRange, MeleeRadius, MeleeDamage);
	}
}

void ASeattleCharacter::CheckCombo()
{
	// Not used in this simple character implementation
}

void ASeattleCharacter::CheckChargedAttack()
{
	// Not used in this simple character implementation
}
// ~end CombatAttacker interface

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
	
	// Check stamina availability on server
	if (StaminaComponent && !StaminaComponent->HasEnoughStamina())
	{
		UE_LOG(LogSeattle, Log, TEXT("[%s] Server_PlayAttackMontage_Implementation: Not enough stamina to attack"), *GetNameSafe(this));
		return;
	}

	if (Montage)
	{
		// Consume stamina before playing attack
		if (StaminaComponent)
		{
			StaminaComponent->ConsumeStamina();
		}

		// Server initiates the multicast so everyone (including server) plays the montage
		Multicast_PlayAttackMontage(Montage, PlayRate);

		// On server, schedule clearing of ActiveAttackType after the montage duration so clients/AI know attack finished
		if (HasAuthority())
		{
			UWorld* World = GetWorld();
			if (World)
			{
				// compute duration from montage length and requested play rate
				const float Duration = Montage->GetPlayLength() / FMath::Max(PlayRate, 0.0001f);
				// clear any existing timer
				if (AttackClearTimerHandle.IsValid())
				{
					World->GetTimerManager().ClearTimer(AttackClearTimerHandle);
				}
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ASeattleCharacter::SetActiveAttackType, ESeattleAttackType::None);
				World->GetTimerManager().SetTimer(AttackClearTimerHandle, Delegate, Duration, false);
			}
		}
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

    // Mark attacking locally when montage starts so AI sees the player as attacking immediately
	bIsAttacking = true;
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
    DOREPLIFETIME(ASeattleCharacter, ReplicatedAimRotation);
    DOREPLIFETIME(ASeattleCharacter, Health);
    DOREPLIFETIME(ASeattleCharacter, bIsKnockedDown);
}