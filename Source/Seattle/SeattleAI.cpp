// Fill out your copyright notice in the Description page of Project Settings.


#include "SeattleAI.h"
#include "Net/UnrealNetwork.h"
#include "Seattle.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Actor.h"
#include "SeattleGameMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"

ASeattleAI::ASeattleAI()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
}

void ASeattleAI::DoAttackTrace(FName DamageSourceBone)
{
	UE_LOG(LogSeattle, Log, TEXT("%s DoAttackTrace called. Authority=%d Bone=%s"), *GetName(), HasAuthority() ? 1 : 0, *DamageSourceBone.ToString());

	// If not authoritative, route to server
	if (!HasAuthority())
	{
		Server_DoAttackTrace(DamageSourceBone);
		return;
	}

	USkeletalMeshComponent* Skel = GetMesh();
	FVector Start = Skel ? Skel->GetSocketLocation(DamageSourceBone) : (GetActorLocation() + FVector(0.f, 0.f, 70.f));
	FVector End = Start + (GetActorForwardVector() * MeleeRange);

	TArray<FHitResult> OutHits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AI_Melee), true, this);
	Params.AddIgnoredActor(this);

	const bool bHit = GetWorld()->SweepMultiByChannel(OutHits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(MeleeRadius), Params);

	if (bHit)
	{
		// Deduplicate per actor (closest impact)
		TMap<AActor*, FHitResult> BestHitPerActor;
		BestHitPerActor.Reserve(OutHits.Num());

		for (const FHitResult& Hit : OutHits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor) continue;

			const float DistSqr = (Hit.ImpactPoint - Start).SizeSquared();

			if (FHitResult* Existing = BestHitPerActor.Find(HitActor))
			{
				const float ExistingDistSqr = (Existing->ImpactPoint - Start).SizeSquared();
				if (DistSqr < ExistingDistSqr)
				{
					BestHitPerActor[HitActor] = Hit;
				}
			}
			else
			{
				BestHitPerActor.Add(HitActor, Hit);
			}
		}

		for (const TPair<AActor*, FHitResult>& Pair : BestHitPerActor)
		{
			AActor* HitActor = Pair.Key;
			const FHitResult& Hit = Pair.Value;

			if (!HitActor) continue;

			// Prefer generic combat damageable interface
			if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor))
			{
				UE_LOG(LogSeattle, Log, TEXT("%s DoAttackTrace: Applying %.1f damage to %s"), *GetName(), MeleeDamage, *GetNameSafe(HitActor));
				Damageable->ApplyDamage(MeleeDamage, this, Hit.ImpactPoint, FVector::ZeroVector);
			}
			else if (ASeattleCharacter* Char = Cast<ASeattleCharacter>(HitActor))
			{
				UE_LOG(LogSeattle, Log, TEXT("%s DoAttackTrace: Applying %.1f damage to SeattleCharacter %s"), *GetName(), MeleeDamage, *GetNameSafe(Char));
				Char->ApplyDamage(MeleeDamage, this, Hit.ImpactPoint, FVector::ZeroVector);
			}

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("%s hit %s for %.1f"), *GetName(), *GetNameSafe(HitActor), MeleeDamage));
			}
		}
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("%s DoAttackTrace: no hit"), *GetName()));
		}
	}
}

bool ASeattleAI::Server_DoAttackTrace_Validate(FName DamageSourceBone)
{
	return true;
}

void ASeattleAI::Server_DoAttackTrace_Implementation(FName DamageSourceBone)
{
	// Server will execute DoAttackTrace authoritative logic
	DoAttackTrace(DamageSourceBone);
}

float ASeattleAI::GetHealthPercent() const
{
	if (MaxHealth <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(Health / MaxHealth, 0.f, 1.f);
}

void ASeattleAI::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		Health = MaxHealth;
	}
}

void ASeattleAI::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

	// If the match hasn't started yet, don't perform AI logic on the server
	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (ASeattleGameMode* GM = World->GetAuthGameMode<ASeattleGameMode>())
			{
				if (!GM->IsGameStarted())
				{
					return;
				}
			}
		}
	}
}

void ASeattleAI::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ASeattleAI::RequestApplyHealthChange(float Change, AActor* DamageInstigator)
{
	if (FMath::IsNearlyZero(Change))
	{
		return;
	}

	if (HasAuthority())
	{
		ApplyHealthChange(Change);
	}
	else
	{
		Server_ApplyHealthChange(Change, DamageInstigator);
	}
}

bool ASeattleAI::Server_ApplyHealthChange_Validate(float Change, AActor* DamageInstigator)
{
	return Change >= 0.f && Change <= MaxHealth;
}

void ASeattleAI::Server_ApplyHealthChange_Implementation(float Change, AActor* DamageInstigator)
{
	ApplyHealthChange(Change);
}

void ASeattleAI::ApplyHealthChange(float Change)
{
	if (!HasAuthority())
	{
		RequestApplyHealthChange(Change);
		return;
	}

	if (bIsKnockedDown || FMath::IsNearlyZero(Change))
	{
		return;
	}

	const float PreviousHealth = Health;
	Health = FMath::Clamp(Health - Change, 0.f, MaxHealth);

	OnHealthChanged.Broadcast(Health, PreviousHealth - Health);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("%s Health: %.1f"), *GetName(), Health));
	}
	
	if (Health <= 0.1f)
	{
		bIsKnockedDown = true;
		ApplyKnockDown();
	}
	else
	{
		bIsStunned = true;
		ApplyStun();
	}
}

void ASeattleAI::OnRep_Health()
{
	OnHealthChanged.Broadcast(Health, 0.f);
}

void ASeattleAI::OnRep_bIsKnockedDown()
{
	if (bIsKnockedDown)
	{
		ApplyKnockDown();
	}
}

void ASeattleAI::OnRep_bIsStunned()
{
	if (bIsStunned)
	{
		ApplyStun();
	}
}

void ASeattleAI::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASeattleAI, Health);
	DOREPLIFETIME(ASeattleAI, bIsKnockedDown);
	DOREPLIFETIME(ASeattleAI, bIsStunned);
	DOREPLIFETIME(ASeattleAI, ComboCount);
	DOREPLIFETIME(ASeattleAI, LastAttackTime);

	// Animation state replication
	DOREPLIFETIME(ASeattleAI, bIsAttacking);
	DOREPLIFETIME(ASeattleAI, ActiveAttackType);
}

void ASeattleAI::SetActiveAttackType(ESeattleAttackType NewAttackType)
{
	if (HasAuthority())
	{
		ActiveAttackType = NewAttackType;
		bIsAttacking = NewAttackType != ESeattleAttackType::None;
		UE_LOG(LogSeattle, Log, TEXT("%s SetActiveAttackType: ActiveAttackType=%d bIsAttacking=%d"), *GetName(), (int32)ActiveAttackType, bIsAttacking ? 1 : 0);
	}
}

bool ASeattleAI::Server_PlayAttackMontage_Validate(UAnimMontage* Montage, float PlayRate)
{
	return Montage != nullptr && PlayRate > 0.f;
}

void ASeattleAI::Server_PlayAttackMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	UE_LOG(LogSeattle, Log, TEXT("%s Server_PlayAttackMontage_Implementation: server received request Montage=%s PlayRate=%f"), *GetName(), Montage ? *Montage->GetName() : TEXT("null"), PlayRate);
	if (Montage)
	{
		Multicast_PlayAttackMontage(Montage, PlayRate);
	}
}

void ASeattleAI::Multicast_PlayAttackMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	UE_LOG(LogSeattle, Log, TEXT("%s Multicast_PlayAttackMontage_Implementation: Called locally. Montage=%s PlayRate=%f"), *GetName(), Montage ? *Montage->GetName() : TEXT("null"), PlayRate);

	if (!Montage)
	{
		UE_LOG(LogSeattle, Warning, TEXT("%s Multicast_PlayAttackMontage_Implementation: Montage null, aborting"), *GetName());
		return;
	}

	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel)
	{
		UE_LOG(LogSeattle, Error, TEXT("%s Multicast_PlayAttackMontage_Implementation: GetMesh() returned null"), *GetName());
		return;
	}

	UAnimInstance* AnimInst = Skel->GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogSeattle, Error, TEXT("%s Multicast_PlayAttackMontage_Implementation: GetAnimInstance() returned null"), *GetName());
		return;
	}

	const float PlayResult = AnimInst->Montage_Play(Montage, PlayRate);
	UE_LOG(LogSeattle, Log, TEXT("%s Multicast_PlayAttackMontage_Implementation: Montage_Play called, return %f"), *GetName(), PlayResult);
}

void ASeattleAI::RequestPlayAttackMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		UE_LOG(LogSeattle, Warning, TEXT("%s RequestPlayAttackMontage: Montage is null"), *GetName());
		return;
	}

	if (HasAuthority())
	{
		UE_LOG(LogSeattle, Log, TEXT("%s RequestPlayAttackMontage: HasAuthority -> calling Multicast. Montage=%s PlayRate=%f"), *GetName(), *Montage->GetName(), PlayRate);
		Multicast_PlayAttackMontage(Montage, PlayRate);
	}
	else
	{
		UE_LOG(LogSeattle, Log, TEXT("%s RequestPlayAttackMontage: Not authority -> calling Server_PlayAttackMontage RPC. Montage=%s PlayRate=%f"), *GetName(), *Montage->GetName(), PlayRate);
		Server_PlayAttackMontage(Montage, PlayRate);
	}
}

void ASeattleAI::OnRep_bIsAttacking()
{
	UE_LOG(LogSeattle, Log, TEXT("%s OnRep_bIsAttacking: bIsAttacking=%d"), *GetName(), bIsAttacking ? 1 : 0);
}

void ASeattleAI::OnRep_ActiveAttackType()
{
	UE_LOG(LogSeattle, Log, TEXT("%s OnRep_ActiveAttackType: ActiveAttackType=%d"), *GetName(), (int32)ActiveAttackType);
	bIsAttacking = ActiveAttackType != ESeattleAttackType::None;
}
