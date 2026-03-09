#include "LockstepPlayerRegistrySubsystem.h"

#include "Engine/World.h"
#include "Misc/ScopeExit.h"

bool ULockstepPlayerRegistrySubsystem::RegisterExistingPlayerActor(const int32 PlayerId, AActor* Actor)
{
    if (PlayerId < 0 || !IsValid(Actor))
    {
        return false;
    }

    PlayerActorMap.FindOrAdd(PlayerId) = Actor;
    return true;
}

bool ULockstepPlayerRegistrySubsystem::SpawnOrGetPlayerActor(const FLockstepPlayerDesc& PlayerDesc, AActor*& OutActor)
{
    OutActor = nullptr;

    if (PlayerDesc.PlayerId < 0)
    {
        return false;
    }

    if (TObjectPtr<AActor>* Existing = PlayerActorMap.Find(PlayerDesc.PlayerId))
    {
        OutActor = *Existing;
        return IsValid(OutActor);
    }

    if (PendingSpawnPlayerIds.Contains(PlayerDesc.PlayerId))
    {
        UE_LOG(LogTemp, Warning, TEXT("Rejected recursive player spawn for PlayerId=%d"), PlayerDesc.PlayerId);
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    UClass* PawnClass = StaticLoadClass(AActor::StaticClass(), nullptr, *PlayerPawnClassPath);
    if (!PawnClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Lockstep spawn failed. Invalid PlayerPawnClassPath: %s"), *PlayerPawnClassPath);
        return false;
    }

    PendingSpawnPlayerIds.Add(PlayerDesc.PlayerId);
    ON_SCOPE_EXIT
    {
        PendingSpawnPlayerIds.Remove(PlayerDesc.PlayerId);
    };

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* SpawnedActor = World->SpawnActor<AActor>(
        PawnClass,
        PlayerDesc.SpawnLocation,
        PlayerDesc.SpawnRotation,
        SpawnParams);

    if (!IsValid(SpawnedActor))
    {
        return false;
    }

    PlayerActorMap.Add(PlayerDesc.PlayerId, SpawnedActor);
    OutActor = SpawnedActor;
    return true;
}

AActor* ULockstepPlayerRegistrySubsystem::FindPlayerActor(const int32 PlayerId) const
{
    if (const TObjectPtr<AActor>* Existing = PlayerActorMap.Find(PlayerId))
    {
        return *Existing;
    }
    return nullptr;
}

void ULockstepPlayerRegistrySubsystem::ClearAllPlayerActors()
{
    PlayerActorMap.Reset();
}
