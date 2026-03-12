#include "LockstepSpawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

ALockstepSpawnPoint::ALockstepSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    ArrowComponent->SetupAttachment(SceneRoot);
    ArrowComponent->ArrowSize = 1.5f;
    ArrowComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
}
