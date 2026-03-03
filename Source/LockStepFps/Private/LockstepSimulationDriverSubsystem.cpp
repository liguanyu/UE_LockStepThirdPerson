#include "LockstepSimulationDriverSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "LockstepSubsystem.h"

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

    const float Step = 1.0f / FMath::Max(FixedFps, 1);
    Accumulator += DeltaTime;

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    ULockstepSubsystem* Lockstep = GameInstance ? GameInstance->GetSubsystem<ULockstepSubsystem>() : nullptr;
    if (!Lockstep)
    {
        return;
    }

    while (Accumulator >= Step)
    {
        TArray<FLockstepInputFrame> Inputs;
        if (!Lockstep->ConsumeFrameInputs(CurrentFrame, Inputs))
        {
            break;
        }

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
    bRunning = true;
    Accumulator = 0.0f;
    CurrentFrame = 0;
}

void ULockstepSimulationDriverSubsystem::StopSimulation()
{
    bRunning = false;
}
