#include "LockstepSubsystem.h"

#include "IPAddress.h"
#include "Misc/ConfigCacheIni.h"
#include "SocketSubsystem.h"
#include "LockstepPacketCodec.h"

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

bool ULockstepSubsystem::SendReady()
{
    FLockstepPacket Packet;
    Packet.Type = ELockstepPacketType::Ready;
    Packet.SessionId = SessionId;
    Packet.ClientId = ClientId;
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
    if (Packet.Type == ELockstepPacketType::Welcome)
    {
        SessionId = Packet.SessionId;
        ClientId = Packet.ClientId;
        if (Packet.FixedFps > 0)
        {
            FixedFps = Packet.FixedFps;
        }
        return;
    }

    if (Packet.Type == ELockstepPacketType::Start)
    {
        SessionId = Packet.SessionId;
        if (Packet.FixedFps > 0)
        {
            FixedFps = Packet.FixedFps;
        }
        return;
    }

    if (Packet.Type != ELockstepPacketType::InputBundle)
    {
        return;
    }

    FScopeLock ScopeLock(&BufferMutex);
    for (const FLockstepInputFrame& Frame : Packet.Frames)
    {
        TArray<FLockstepInputFrame>& Bucket = FrameInputs.FindOrAdd(Frame.FrameIndex);
        Bucket.Add(Frame);
    }
}
