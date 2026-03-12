#include "LockstepSimulationDriverSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/Actor.h"
#include "LockstepSubsystem.h"
#include "LockstepPlayerRegistrySubsystem.h"
#include "LockstepControllablePawnInterface.h"
#include "LogHelper.h"

void ULockstepSimulationDriverSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    GConfig->GetInt(TEXT("Lockstep"), TEXT("FPS"), FixedFps, GGameIni);
    GConfig->GetInt(TEXT("Lockstep"), TEXT("InputDelayFrames"), InputDelayFrames, GGameIni);
}

void ULockstepSimulationDriverSubsystem::Deinitialize()
{
    StopSimulation();
    Super::Deinitialize();
}

void ULockstepSimulationDriverSubsystem::Tick(const float DeltaTime)
{
    if (!bRunning)
    {
        return;
    }

    // 固定步长仿真：渲染帧不稳定，逻辑帧固定。
    const float Step = 1.0f / FMath::Max(FixedFps, 1);
    Accumulator += DeltaTime;

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    ULockstepSubsystem* Lockstep = GameInstance ? GameInstance->GetSubsystem<ULockstepSubsystem>() : nullptr;
    ULockstepPlayerRegistrySubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<ULockstepPlayerRegistrySubsystem>() : nullptr;
    if (!Lockstep)
    {
        return;
    }

    while (Accumulator >= Step)
    {
        TArray<FLockstepInputFrame> Inputs;
        if (!Lockstep->ConsumeFrameInputs(CurrentFrame, Inputs))
        {
            // 没有凑齐该帧输入时停止推进，等待网络包补齐。
            break;
        }

        // 先按 playerId 路由输入到对应 Actor，确保“谁的输入作用到谁的角色”。
        if (Registry)
        {
            for (const FLockstepInputFrame& Input : Inputs)
            {
                AActor* TargetActor = Registry->FindPlayerActor(Input.PlayerId);
                if (!IsValid(TargetActor))
                {
                    UE_LOG(LogTemp, Warning, TEXT("No actor bound for PlayerId=%d at Frame=%d"), Input.PlayerId, CurrentFrame);
                    continue;
                }

                PrintLog(FString::Printf(TEXT("Applying input for PlayerId=%d at Frame=%d, moveX=%f"), Input.PlayerId, CurrentFrame, Input.MoveAxis.X));
                if (TargetActor->GetClass()->ImplementsInterface(ULockstepControllablePawnInterface::StaticClass()))
                {
                    ILockstepControllablePawnInterface::Execute_ApplyLockstepInput(TargetActor, Input);
                }
                else
                {
                    UE_LOG(LogTemp, Verbose, TEXT("Actor for PlayerId=%d does not implement lockstep interface"), Input.PlayerId);
                }
            }
        }

        PrintLog(FString::Printf(TEXT("Accumulator=%f"), Accumulator));
        // 仍保留广播事件，便于蓝图额外监听。
        OnSimTick.Broadcast(CurrentFrame, Inputs);
        ++CurrentFrame;
        Accumulator -= Step;
    }
}

TStatId ULockstepSimulationDriverSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(ULockstepSimulationDriverSubsystem, STATGROUP_Tickables);
}

void ULockstepSimulationDriverSubsystem::StartSimulation()
{
    PrintLog(TEXT("StartSimulation"));

    bRunning = true;
    Accumulator = 0.0f;
    CurrentFrame = 0;
}

void ULockstepSimulationDriverSubsystem::StopSimulation()
{
    bRunning = false;
}
