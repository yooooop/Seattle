// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimNotify_DoAttackTrace.h"
#include "CombatAttacker.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"

void UAnimNotify_DoAttackTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// cast the owner to the attacker interface
    if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_DoAttackTrace: MeshComp is null"));
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_DoAttackTrace: MeshComp->GetOwner() is null"));
		return;
	}

    UE_LOG(LogTemp, Log, TEXT("AnimNotify_DoAttackTrace: Notify fired on owner=%s"), *GetNameSafe(Owner));

	if (ICombatAttacker* AttackerInterface = Cast<ICombatAttacker>(Owner))
	{
		UE_LOG(LogTemp, Log, TEXT("NOT POSSIBLE NOT POSSIBLE AnimNotify_DoAttackTrace: Owner implements ICombatAttacker, calling DoAttackTrace"));
		AttackerInterface->DoAttackTrace(AttackBoneName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_DoAttackTrace: Owner %s does NOT implement ICombatAttacker"), *GetNameSafe(Owner));
	}
}

FString UAnimNotify_DoAttackTrace::GetNotifyName_Implementation() const
{
	return FString("Do Attack Trace");
}