#include "LockstepSubsystem.h"

#include "IPAddress.h"
#include "Misc/ConfigCacheIni.h"
#include "SocketSubsystem.h"
#include "LockstepPacketCodec.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LockstepPlayerRegistrySubsystem.h"
#include "LogHelper.h"

void ULockstepSubsystem::Deinitialize()
{
    Disconnect();
    Super::Deinitialize();
}

bool ULockstepSubsystem::Connect(const FString& InHost, const int32 InPort)
{
    Disconnect();

    bool bIsValid = false;
    const TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
    Addr->SetIp(*InHost, bIsValid);
    Addr->SetPort(InPort);

    if (!bIsValid)
    {
        UE_LOG(LogTemp, Error, TEXT("Lockstep connect failed. Invalid host %s"), *InHost);
        return false;
    }

    // 这里使用自建 UDP 通道，不使用 UE Replication。
    Socket = FUdpSocketBuilder(TEXT("LockstepClientSocket"))
        .AsReusable()
        .WithReceiveBufferSize(2 * 1024 * 1024)
        .WithSendBufferSize(2 * 1024 * 1024)
        .BoundToPort(0)
        .Build();

    if (!Socket)
    {
        UE_LOG(LogTemp, Error, TEXT("Lockstep connect failed. Could not create socket."));
        return false;
    }

    ServerAddr = Addr;

    SocketReceiver = MakeUnique<FUdpSocketReceiver>(
        Socket,
        FTimespan::FromMilliseconds(1),
        TEXT("LockstepClientReceiver"));
    SocketReceiver->OnDataReceived().BindUObject(this, &ULockstepSubsystem::HandleDatagram);
    SocketReceiver->Start();

    bConnected = true;
    ClientId = -1;
    SessionId = 0;
    FixedFps = 60;
    RosterVersion = 0;
    return true;
}

bool ULockstepSubsystem::ConnectFromConfig()
{
    FString Host = TEXT("127.0.0.1");
    int32 Port = 7777;
    GConfig->GetString(TEXT("Lockstep"), TEXT("ServerHost"), Host, GGameIni);
    GConfig->GetInt(TEXT("Lockstep"), TEXT("ServerPort"), Port, GGameIni);
    return Connect(Host, Port);
}

void ULockstepSubsystem::Disconnect()
{
    bConnected = false;

    if (SocketReceiver)
    {
        SocketReceiver->Stop();
        SocketReceiver.Reset();
    }

    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }

    ServerAddr.Reset();
    ClientId = -1;
    SessionId = 0;
    FixedFps = 60;
    RosterVersion = 0;

    FScopeLock ScopeLock(&BufferMutex);
    FrameInputs.Reset();
}

bool ULockstepSubsystem::SendPacket(const FLockstepPacket& Packet)
{
    if (!Socket || !ServerAddr.IsValid())
    {
        return false;
    }

    TArray<uint8> Bytes;
    if (!FLockstepPacketCodec::Encode(Packet, Bytes))
    {
        return false;
    }

    int32 BytesSent = 0;
    return Socket->SendTo(Bytes.GetData(), Bytes.Num(), BytesSent, *ServerAddr);
}

bool ULockstepSubsystem::SubmitLocalInputFrame(const FLockstepInputFrame& Frame)
{
    // PrintLog(FString::Printf(TEXT("SubmitLocalInputFrame, %d"), Frame.FrameIndex));
    
    // 客户端只上传“输入帧”，不上传世界状态。
    FLockstepPacket Packet;
    Packet.Type = ELockstepPacketType::Input;
    Packet.SessionId = SessionId;
    Packet.ClientId = ClientId;
    Packet.StartFrame = Frame.FrameIndex;
    Packet.EndFrame = Frame.FrameIndex;
    Packet.Frames.Add(Frame);
    return SendPacket(Packet);
}

bool ULockstepSubsystem::SendHello()
{
    FLockstepPacket Packet;
    Packet.Type = ELockstepPacketType::Hello;
    return SendPacket(Packet);
}

bool ULockstepSubsystem::SendJoinRequest()
{
    PrintLog(TEXT("SendJoinRequest"));

    FLockstepPacket Packet;
    Packet.Type = ELockstepPacketType::JoinRequest;
    Packet.SessionId = SessionId;
    Packet.ClientId = ClientId;
    return SendPacket(Packet);
}

bool ULockstepSubsystem::SendReady()
{
    PrintLog(TEXT("SendReady"));
    
    FLockstepPacket Packet;
    Packet.Type = ELockstepPacketType::Ready;
    Packet.SessionId = SessionId;
    Packet.ClientId = ClientId;
    Packet.RosterVersion = RosterVersion;
    return SendPacket(Packet);
}

bool ULockstepSubsystem::SendPlayerSpawnAck(const int32 PlayerId)
{
    FLockstepPacket Packet;
    Packet.Type = ELockstepPacketType::PlayerSpawnAck;
    Packet.SessionId = SessionId;
    Packet.ClientId = ClientId;
    Packet.RosterVersion = RosterVersion;

    FLockstepPlayerDesc Ack;
    Ack.PlayerId = PlayerId;
    Packet.Players.Add(Ack);
    return SendPacket(Packet);
}

bool ULockstepSubsystem::ConsumeFrameInputs(const int32 FrameIndex, TArray<FLockstepInputFrame>& OutInputs)
{
    FScopeLock ScopeLock(&BufferMutex);

    TArray<FLockstepInputFrame>* Existing = FrameInputs.Find(FrameIndex);
    if (!Existing)
    {
        return false;
    }

    // 消费语义：拿走该帧数据后立即删除，避免重复执行同一帧。
    OutInputs = MoveTemp(*Existing);
    FrameInputs.Remove(FrameIndex);
    return true;
}

void ULockstepSubsystem::HandleDatagram(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
    if (!Data.IsValid())
    {
        return;
    }

    FLockstepPacket Packet;
    if (!FLockstepPacketCodec::Decode(Data->GetData(), Data->Num(), Packet))
    {
        return;
    }

    HandleIncomingPacket(Packet);
    OnPacketReceived.Broadcast(Packet);
}

void ULockstepSubsystem::HandleIncomingPacket(const FLockstepPacket& Packet)
{
    if (Packet.Type != ELockstepPacketType::InputBundle)
    {
        const UEnum* PacketTypeEnum = StaticEnum<ELockstepPacketType>();
        const FString PacketTypeName = PacketTypeEnum != nullptr
            ? PacketTypeEnum->GetNameStringByValue(static_cast<int64>(Packet.Type))
            : TEXT("Unknown");
        PrintLog(FString::Printf(TEXT("recv packet type %s (%d)"), *PacketTypeName, static_cast<uint8>(Packet.Type)));
    }
    
    if (Packet.Type == ELockstepPacketType::Welcome)
    {
        // WELCOME 同步会话信息与分配好的 ClientId。
        SessionId = Packet.SessionId;
        ClientId = Packet.ClientId;
        if (Packet.FixedFps > 0)
        {
            FixedFps = Packet.FixedFps;
        }
        RosterVersion = Packet.RosterVersion;
        return;
    }

    if (Packet.Type == ELockstepPacketType::JoinAccept)
    {
        RosterVersion = Packet.RosterVersion;
        if (Packet.ResultCode != 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Join rejected. ResultCode=%d"), Packet.ResultCode);
        }
        return;
    }

    if (Packet.Type == ELockstepPacketType::PlayerSpawn)
    {
        RosterVersion = Packet.RosterVersion;

        UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
        ULockstepPlayerRegistrySubsystem* Registry = World ? World->GetSubsystem<ULockstepPlayerRegistrySubsystem>() : nullptr;

        for (const FLockstepPlayerDesc& PlayerDesc : Packet.Players)
        {
            OnPlayerSpawn.Broadcast(PlayerDesc);

            if (Registry)
            {
                AActor* SpawnedActor = nullptr;
                bool bBound = false;

                // 本地玩家优先绑定现有 Pawn，避免与 GameMode 默认生成逻辑重复。
                if (PlayerDesc.PlayerId == ClientId && World)
                {
                    if (APlayerController* LocalPC = World->GetFirstPlayerController())
                    {
                        if (AActor* ExistingPawn = LocalPC->GetPawn())
                        {
                            bBound = Registry->RegisterExistingPlayerActor(PlayerDesc.PlayerId, ExistingPawn);
                            SpawnedActor = ExistingPawn;
                        }
                    }
                }

                if (!bBound)
                {
                    bBound = Registry->SpawnOrGetPlayerActor(PlayerDesc, SpawnedActor);
                }

                if (bBound)
                {
                    SendPlayerSpawnAck(PlayerDesc.PlayerId);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to spawn player actor for PlayerId=%d"), PlayerDesc.PlayerId);
                }
            }
        }
        return;
    }

    if (Packet.Type == ELockstepPacketType::Start)
    {
        // START 表示房间进入可推进阶段。
        SessionId = Packet.SessionId;
        if (Packet.FixedFps > 0)
        {
            FixedFps = Packet.FixedFps;
        }
        RosterVersion = Packet.RosterVersion;
        return;
    }

    if (Packet.Type == ELockstepPacketType::RoomClosed)
    {
        OnRoomClosed.Broadcast(Packet.ResultCode);
        return;
    }

    if (Packet.Type != ELockstepPacketType::InputBundle)
    {
        return;
    }

    FScopeLock ScopeLock(&BufferMutex);
    for (const FLockstepInputFrame& Frame : Packet.Frames)
    {
        // INPUT_BUNDLE 内通常是“同一帧的所有玩家输入”。
        TArray<FLockstepInputFrame>& Bucket = FrameInputs.FindOrAdd(Frame.FrameIndex);
        Bucket.Add(Frame);
    }
}
