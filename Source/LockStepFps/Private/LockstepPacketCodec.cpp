#include "LockstepPacketCodec.h"

#include "HAL/UnrealMemory.h"

namespace
{
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
    OutBytes.Reset();
    OutBytes.Reserve(64 + Packet.Frames.Num() * 48);

    WritePod<uint8>(OutBytes, static_cast<uint8>(Packet.Type));
    WritePod<int32>(OutBytes, Packet.SessionId);
    WritePod<int32>(OutBytes, Packet.ClientId);
    WritePod<int32>(OutBytes, Packet.StartFrame);
    WritePod<int32>(OutBytes, Packet.EndFrame);
    WritePod<int32>(OutBytes, Packet.Seed);
    WritePod<int32>(OutBytes, Packet.FixedFps);
    WritePod<int32>(OutBytes, Packet.Frames.Num());

    for (const FLockstepInputFrame& Frame : Packet.Frames)
    {
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
        !ReadPod<int32>(Cursor, Remaining, OutPacket.FixedFps))
    {
        return false;
    }

    int32 FrameCount = 0;
    if (!ReadPod<int32>(Cursor, Remaining, FrameCount))
    {
        return false;
    }

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
