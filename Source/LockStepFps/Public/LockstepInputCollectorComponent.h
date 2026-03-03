#pragma once

#include "Components/ActorComponent.h"
#include "LockstepTypes.h"

#include "LockstepInputCollectorComponent.generated.h"

UCLASS(ClassGroup=(Lockstep), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class LOCKSTEPFPS_API ULockstepInputCollectorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void SetMoveAxis(const FVector2D& InMoveAxis);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void SetLookAxis(const FVector2D& InLookAxis);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void SetJumpPressed(bool bPressed);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void SetActionBits(int32 InActionBits);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    FLockstepInputFrame BuildInputFrame(int32 FrameIndex, int32 PlayerId, int32 InputDelayFrames) const;

private:
    UPROPERTY(Transient)
    FVector2D MoveAxis = FVector2D::ZeroVector;

    UPROPERTY(Transient)
    FVector2D LookAxis = FVector2D::ZeroVector;

    UPROPERTY(Transient)
    bool bJumpPressed = false;

    UPROPERTY(Transient)
    int32 ActionBits = 0;
};
