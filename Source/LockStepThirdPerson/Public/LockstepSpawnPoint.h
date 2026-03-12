#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "LockstepSpawnPoint.generated.h"

class UArrowComponent;
class USceneComponent;

UCLASS(BlueprintType, Blueprintable)
class LOCKSTEPTHIRDPERSON_API ALockstepSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    ALockstepSpawnPoint();

    UFUNCTION(BlueprintPure, Category = "Lockstep")
    int32 GetClientId() const { return ClientId; }

private:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Lockstep", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Lockstep", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UArrowComponent> ArrowComponent;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Lockstep", meta = (AllowPrivateAccess = "true"))
    int32 ClientId = 1;
};
