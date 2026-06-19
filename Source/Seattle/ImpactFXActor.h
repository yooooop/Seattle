#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactFXActor.generated.h"

class UBillboardComponent;
class USoundBase;

UCLASS()
class SEATTLE_API AImpactFXActor : public AActor
{
    GENERATED_BODY()

public:
    AImpactFXActor();

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

public:
    /** Sprite texture to display (set in BP or instance) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    UTexture2D* ImpactTexture = nullptr;

    /** Sound to play when spawned */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    USoundBase* ImpactSound = nullptr;

    /** Maximum scale applied at peak */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    float MaxScale = 1.0f;

    /** Total duration of the effect in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    float TotalDuration = 0.6f;

    /** Fraction of TotalDuration spent growing (0..1). Remaining fraction is shrink */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
    float GrowFraction = 0.5f;

protected:
    UPROPERTY(VisibleDefaultsOnly)
    UBillboardComponent* SpriteComp;

    float Elapsed = 0.f;
};
