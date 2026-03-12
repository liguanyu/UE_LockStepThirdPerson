#pragma once

#include "CoreMinimal.h"
#include "LockstepTypes.generated.h"

UENUM(BlueprintType)
enum class ELockstepPacketType : uint8
{
    Hello = 0,
    Welcome,
    JoinRequest,
    JoinAccept,
    PlayerSpawn,
    PlayerSpawnAck,
    Ready,
    Start,
    Input,
    InputBundle,
    RoomClosed,
    Ping,
    Pong
};

USTRUCT(BlueprintType)
struct FLockstepPlayerDesc
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 ClientId = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 PlayerId = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 PawnTypeId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    FVector SpawnLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    FRotator SpawnRotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FLockstepInputFrame
{
    GENERATED_BODY()

    // 目标仿真帧号。通常本地输入会写入 (当前帧 + 输入延迟帧)，
    // 用于吸收网络抖动，避免因为单帧丢包而卡顿。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 FrameIndex = 0;

    // 由 relay server 在 WELCOME 中分配的玩家 ID。
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

    // 网络包类型：握手 / 输入数据 / 心跳。
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

    // 0 表示成功，非 0 表示错误码（例如 Join 被拒绝）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 ResultCode = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    int32 RosterVersion = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    TArray<FLockstepPlayerDesc> Players;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockstep")
    TArray<FLockstepInputFrame> Frames;
};
