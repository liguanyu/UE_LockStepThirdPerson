#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Common/UdpSocketReceiver.h"
#include "Common/UdpSocketBuilder.h"
#include "LockstepTypes.h"

#include "LockstepSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLockstepPacketReceivedSignature, const FLockstepPacket&, Packet);

UCLASS()
class LOCKSTEPFPS_API ULockstepSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool Connect(const FString& InHost, int32 InPort);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool ConnectFromConfig();

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void Disconnect();

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool SendPacket(const FLockstepPacket& Packet);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool SendHello();

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool SendReady();

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool SubmitLocalInputFrame(const FLockstepInputFrame& Frame);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool ConsumeFrameInputs(int32 FrameIndex, TArray<FLockstepInputFrame>& OutInputs);

    UFUNCTION(BlueprintPure, Category = "Lockstep")
    bool IsConnected() const { return bConnected; }

    UFUNCTION(BlueprintPure, Category = "Lockstep")
    int32 GetClientId() const { return ClientId; }

    UFUNCTION(BlueprintPure, Category = "Lockstep")
    int32 GetSessionId() const { return SessionId; }

    UFUNCTION(BlueprintPure, Category = "Lockstep")
    int32 GetFixedFps() const { return FixedFps; }

    UPROPERTY(BlueprintAssignable, Category = "Lockstep")
    FLockstepPacketReceivedSignature OnPacketReceived;

private:
    // UDP 收包回调：解析包后写入缓冲并广播给蓝图。
    void HandleDatagram(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
    // 仅处理与本地状态相关的包（WELCOME/START/INPUT_BUNDLE）。
    void HandleIncomingPacket(const FLockstepPacket& Packet);

    // 每帧聚合后的输入缓冲：FrameIndex -> 所有玩家输入。
    TMap<int32, TArray<FLockstepInputFrame>> FrameInputs;

    FSocket* Socket = nullptr;
    TUniquePtr<FUdpSocketReceiver> SocketReceiver;
    TSharedPtr<FInternetAddr> ServerAddr;

    FCriticalSection BufferMutex;

    bool bConnected = false;
    int32 ClientId = -1;
    int32 SessionId = 0;
    int32 FixedFps = 60;
};
