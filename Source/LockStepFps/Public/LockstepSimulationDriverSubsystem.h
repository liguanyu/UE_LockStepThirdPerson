#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LockstepTypes.h"

#include "LockstepSimulationDriverSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLockstepSimTickSignature, int32, FrameIndex, const TArray<FLockstepInputFrame>&, Inputs);

UCLASS(Config=Game)
class LOCKSTEPFPS_API ULockstepSimulationDriverSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableInEditor() const override { return true; }

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void StartSimulation();

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void StopSimulation();

    UFUNCTION(BlueprintPure, Category = "Lockstep")
    int32 GetCurrentFrame() const { return CurrentFrame; }

    UPROPERTY(BlueprintAssignable, Category = "Lockstep")
    FLockstepSimTickSignature OnSimTick;

    UFUNCTION(BlueprintPure, Category = "Lockstep")
    bool IsRunning() const { return bRunning; }
    
private:
    UPROPERTY(Config)
    int32 FixedFps = 60;

    UPROPERTY(Config)
    int32 InputDelayFrames = 3;

    bool bRunning = false;
    float Accumulator = 0.0f;
    int32 CurrentFrame = 0;
};
