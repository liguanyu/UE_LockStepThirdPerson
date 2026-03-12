#pragma once

#include "CoreMinimal.h"
#include "LockstepTypes.h"

class LOCKSTEPTHIRDPERSON_API FLockstepPacketCodec
{
public:
    static bool Encode(const FLockstepPacket& Packet, TArray<uint8>& OutBytes);
    static bool Decode(const uint8* Data, int32 NumBytes, FLockstepPacket& OutPacket);
};
