#include "LockstepInputCollectorComponent.h"
#include "Misc/DateTime.h"

void ULockstepInputCollectorComponent::SetMoveAxis(const FVector2D& InMoveAxis)
{
    MoveAxis = InMoveAxis;
}

void ULockstepInputCollectorComponent::SetLookAxis(const FVector2D& InLookAxis)
{
    LookAxis = InLookAxis;
}

void ULockstepInputCollectorComponent::SetJumpPressed(const bool bPressed)
{
    bJumpPressed = bPressed;
}

void ULockstepInputCollectorComponent::SetActionBits(const int32 InActionBits)
{
    ActionBits = InActionBits;
}

FLockstepInputFrame ULockstepInputCollectorComponent::BuildInputFrame(
    const int32 FrameIndex,
    const int32 PlayerId,
    const int32 InputDelayFrames) const
{
    FLockstepInputFrame Frame;
    // 输入延迟：把本地输入写到未来帧，减少网络抖动影响。
    Frame.FrameIndex = FrameIndex + FMath::Max(InputDelayFrames, 0);
    Frame.PlayerId = PlayerId;
    Frame.MoveAxis = MoveAxis;
    Frame.LookAxis = LookAxis;
    Frame.bJumpPressed = bJumpPressed;
    Frame.ActionBits = ActionBits;
    Frame.Timestamp = FDateTime::UtcNow().GetTicks();
    return Frame;
}
