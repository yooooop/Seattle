#pragma once

#include "CoreMinimal.h"
#include "CombatAction.generated.h"

UENUM(BlueprintType)
enum class ECombatAction : uint8
{
    None UMETA(DisplayName = "None"),
    Jab UMETA(DisplayName = "Jab"),
    Hook UMETA(DisplayName = "Hook"),
    SlideLeft UMETA(DisplayName = "SlideLeft"),
    SlideRight UMETA(DisplayName = "SlideRight"),
    SlideBack UMETA(DisplayName = "SlideBack"),
    KeepDistance UMETA(DisplayName = "KeepDistance"),
    SlideForward UMETA(DisplayName = "SlideForward"),
    MoveTo UMETA(DisplayName = "MoveTo"),
    Idle UMETA(DisplayName = "Idle")
};
