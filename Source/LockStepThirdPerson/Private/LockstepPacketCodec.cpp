#include "LockstepPacketCodec.h"

#include "HAL/UnrealMemory.h"

namespace
{
// 按原始字节写入 POD，保证 UE 客户端与独立 relay 的二进制协议一致。
template <typename T>
void WritePod(TArray<uint8>& Buffer, const T Value)
{
    const int32 Offset = Buffer.AddUninitialized(sizeof(T));
    FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(T));
}

template <typename T>
bool ReadPod(const uint8*& Cursor, int32& Remaining, T& OutValue)
{
    if (Remaining < static_cast<int32>(sizeof(T)))
    {
        return false;
    }

    FMemory::Memcpy(&OutValue, Cursor, sizeof(T));
    Cursor += sizeof(T);
    Remaining -= sizeof(T);
    return true;
}
}

bool FLockstepPacketCodec::Encode(const FLockstepPacket& Packet, TArray<uint8>& OutBytes)
{
    // 线性二进制布局：
    // [PacketHeader][ResultCode][RosterVersion][PlayerCount][Players...][FrameCount][Frames...]
    OutBytes.Reset();
    OutBytes.Reserve(96 + Packet.Players.Num() * 44 + Packet.Frames.Num() * 48);

    WritePod<uint8>(OutBytes, static_cast<uint8>(Packet.Type));
    WritePod<int32>(OutBytes, Packet.SessionId);
    WritePod<int32>(OutBytes, Packet.ClientId);
    WritePod<int32>(OutBytes, Packet.StartFrame);
    WritePod<int32>(OutBytes, Packet.EndFrame);
    WritePod<int32>(OutBytes, Packet.Seed);
    WritePod<int32>(OutBytes, Packet.FixedFps);
    WritePod<int32>(OutBytes, Packet.ResultCode);
    WritePod<int32>(OutBytes, Packet.RosterVersion);
    WritePod<int32>(OutBytes, Packet.Players.Num());

    for (const FLockstepPlayerDesc& Player : Packet.Players)
    {
        WritePod<int32>(OutBytes, Player.ClientId);
        WritePod<int32>(OutBytes, Player.PlayerId);
        WritePod<int32>(OutBytes, Player.PawnTypeId);
        WritePod<float>(OutBytes, Player.SpawnLocation.X);
        WritePod<float>(OutBytes, Player.SpawnLocation.Y);
        WritePod<float>(OutBytes, Player.SpawnLocation.Z);
        WritePod<float>(OutBytes, Player.SpawnRotation.Roll);
        WritePod<float>(OutBytes, Player.SpawnRotation.Pitch);
        WritePod<float>(OutBytes, Player.SpawnRotation.Yaw);
    }

    WritePod<int32>(OutBytes, Packet.Frames.Num());

    for (const FLockstepInputFrame& Frame : Packet.Frames)
    {
        // bool 在网络层统一编码为 uint8，避免不同编译器布局差异。
        const uint8 JumpPressed = Frame.bJumpPressed ? 1u : 0u;

        WritePod<int32>(OutBytes, Frame.FrameIndex);
        WritePod<int32>(OutBytes, Frame.PlayerId);
        WritePod<float>(OutBytes, Frame.MoveAxis.X);
        WritePod<float>(OutBytes, Frame.MoveAxis.Y);
        WritePod<float>(OutBytes, Frame.LookAxis.X);
        WritePod<float>(OutBytes, Frame.LookAxis.Y);
        WritePod<uint8>(OutBytes, JumpPressed);
        WritePod<int32>(OutBytes, Frame.ActionBits);
        WritePod<int64>(OutBytes, Frame.Timestamp);
    }

    return true;
}

bool FLockstepPacketCodec::Decode(const uint8* Data, int32 NumBytes, FLockstepPacket& OutPacket)
{
    if (Data == nullptr || NumBytes <= 0)
    {
        return false;
    }

    const uint8* Cursor = Data;
    int32 Remaining = NumBytes;

    uint8 Type = 0;
    if (!ReadPod<uint8>(Cursor, Remaining, Type))
    {
        return false;
    }

    OutPacket.Type = static_cast<ELockstepPacketType>(Type);
    if (!ReadPod<int32>(Cursor, Remaining, OutPacket.SessionId) ||
        !ReadPod<int32>(Cursor, Remaining, OutPacket.ClientId) ||
        !ReadPod<int32>(Cursor, Remaining, OutPacket.StartFrame) ||
        !ReadPod<int32>(Cursor, Remaining, OutPacket.EndFrame) ||
        !ReadPod<int32>(Cursor, Remaining, OutPacket.Seed) ||
        !ReadPod<int32>(Cursor, Remaining, OutPacket.FixedFps) ||
        !ReadPod<int32>(Cursor, Remaining, OutPacket.ResultCode) ||
        !ReadPod<int32>(Cursor, Remaining, OutPacket.RosterVersion))
    {
        return false;
    }

    int32 PlayerCount = 0;
    if (!ReadPod<int32>(Cursor, Remaining, PlayerCount))
    {
        return false;
    }

    if (PlayerCount < 0 || PlayerCount > 256)
    {
        return false;
    }

    OutPacket.Players.Reset(PlayerCount);
    for (int32 Index = 0; Index < PlayerCount; ++Index)
    {
        FLockstepPlayerDesc Player;
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
        float Roll = 0.0f;
        float Pitch = 0.0f;
        float Yaw = 0.0f;
        if (!ReadPod<int32>(Cursor, Remaining, Player.ClientId) ||
            !ReadPod<int32>(Cursor, Remaining, Player.PlayerId) ||
            !ReadPod<int32>(Cursor, Remaining, Player.PawnTypeId) ||
            !ReadPod<float>(Cursor, Remaining, X) ||
            !ReadPod<float>(Cursor, Remaining, Y) ||
            !ReadPod<float>(Cursor, Remaining, Z) ||
            !ReadPod<float>(Cursor, Remaining, Roll) ||
            !ReadPod<float>(Cursor, Remaining, Pitch) ||
            !ReadPod<float>(Cursor, Remaining, Yaw))
        {
            return false;
        }
        Player.SpawnLocation = FVector(X, Y, Z);
        Player.SpawnRotation = FRotator(Pitch, Yaw, Roll);
        OutPacket.Players.Add(Player);
    }

    int32 FrameCount = 0;
    if (!ReadPod<int32>(Cursor, Remaining, FrameCount))
    {
        return false;
    }

    // 保护上限，防止异常包导致内存膨胀。
    if (FrameCount < 0 || FrameCount > 4096)
    {
        return false;
    }

    OutPacket.Frames.Reset(FrameCount);

    for (int32 Index = 0; Index < FrameCount; ++Index)
    {
        FLockstepInputFrame Frame;
        uint8 JumpPressed = 0;
        float MoveX = 0.0f;
        float MoveY = 0.0f;
        float LookX = 0.0f;
        float LookY = 0.0f;

        if (!ReadPod<int32>(Cursor, Remaining, Frame.FrameIndex) ||
            !ReadPod<int32>(Cursor, Remaining, Frame.PlayerId) ||
            !ReadPod<float>(Cursor, Remaining, MoveX) ||
            !ReadPod<float>(Cursor, Remaining, MoveY) ||
            !ReadPod<float>(Cursor, Remaining, LookX) ||
            !ReadPod<float>(Cursor, Remaining, LookY) ||
            !ReadPod<uint8>(Cursor, Remaining, JumpPressed) ||
            !ReadPod<int32>(Cursor, Remaining, Frame.ActionBits) ||
            !ReadPod<int64>(Cursor, Remaining, Frame.Timestamp))
        {
            return false;
        }

        Frame.MoveAxis.X = MoveX;
        Frame.MoveAxis.Y = MoveY;
        Frame.LookAxis.X = LookX;
        Frame.LookAxis.Y = LookY;
        Frame.bJumpPressed = JumpPressed != 0u;
        OutPacket.Frames.Add(Frame);
    }

    return true;
}
