#include "ImpactFXActor.h"
#include "Components/BillboardComponent.h"
#include "Kismet/GameplayStatics.h"

AImpactFXActor::AImpactFXActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false; // spawned locally on each client via multicast

    SpriteComp = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
    SpriteComp->SetHiddenInGame(false);
    SpriteComp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f)); // face camera if needed
    SpriteComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RootComponent = SpriteComp;

    MaxScale = 1.f;
    TotalDuration = 0.6f;
    GrowFraction = 0.5f;
}

void AImpactFXActor::BeginPlay()
{
    Super::BeginPlay();

    if (ImpactTexture && SpriteComp)
    {
        SpriteComp->SetSprite(ImpactTexture);
    }

    if (ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
    }

    Elapsed = 0.f;

    // start very small
    const float StartScale = 0.05f;
    SetActorScale3D(FVector(StartScale));
}

void AImpactFXActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    Elapsed += DeltaSeconds;
    if (Elapsed >= TotalDuration)
    {
        Destroy();
        return;
    }

    const float GrowTime = FMath::Max(0.001f, TotalDuration * GrowFraction);
    const float ShrinkTime = FMath::Max(0.001f, TotalDuration - GrowTime);

    float CurrentScale = 1.f;
    if (Elapsed <= GrowTime)
    {
        const float Alpha = Elapsed / GrowTime;
        CurrentScale = FMath::Lerp(0.05f, MaxScale, Alpha);
    }
    else
    {
        const float Alpha = (Elapsed - GrowTime) / ShrinkTime;
        CurrentScale = FMath::Lerp(MaxScale, 0.01f, Alpha);
    }

    SetActorScale3D(FVector(CurrentScale));
}
