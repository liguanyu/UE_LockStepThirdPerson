#pragma once

#include "CoreMinimal.h"
#include "LockstepTypes.generated.h"

UENUM(BlueprintType)
enum class ELockstepPacketType : uint8
{
    Hello = 0,
    Welcome,
    Ready,
    Start,
    Input,
    InputBundle,
    Ping,
    Pong
};

USTRUCT(BlueprintType)
struct FLockstepInputFrame
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 FrameIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 PlayerId = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    FVector2D MoveAxis = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    FVector2D LookAxis = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    bool bJumpPressed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 ActionBits = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int64 Timestamp = 0;
};

USTRUCT(BlueprintType)
struct FLockstepPacket
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    ELockstepPacketType Type = ELockstepPacketType::Hello;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 SessionId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 ClientId = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 StartFrame = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 EndFrame = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 FixedFps = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    TArray<FLockstepInputFrame> Frames;
};
