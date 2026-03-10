#include "LockstepPlayerRegistrySubsystem.h"

#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LockstepSpawnPoint.h"
#include "Misc/ScopeExit.h"

namespace
{
FTransform AdjustTransformForActorClass(const FTransform& SourceTransform, UClass* ActorClass)
{
    FTransform AdjustedTransform = SourceTransform;

    if (ActorClass && ActorClass->IsChildOf(ACharacter::StaticClass()))
    {
        const ACharacter* CharacterCDO = Cast<ACharacter>(ActorClass->GetDefaultObject());
        const UCapsuleComponent* Capsule = CharacterCDO ? CharacterCDO->GetCapsuleComponent() : nullptr;
        if (Capsule)
        {
            FVector Location = AdjustedTransform.GetLocation();
            Location.Z += Capsule->GetScaledCapsuleHalfHeight();
            AdjustedTransform.SetLocation(Location);
        }
    }

    return AdjustedTransform;
}

void ConfigureActorForLockstep(AActor* Actor)
{
    ACharacter* Character = Cast<ACharacter>(Actor);
    if (!Character)
    {
        return;
    }

    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->bRunPhysicsWithNoController = true;
    }
}
}

bool ULockstepPlayerRegistrySubsystem::RegisterExistingPlayerActor(const int32 PlayerId, AActor* Actor)
{
    if (PlayerId < 0 || !IsValid(Actor))
    {
        return false;
    }

    PlayerActorMap.FindOrAdd(PlayerId) = Actor;
    return true;
}

bool ULockstepPlayerRegistrySubsystem::TryGetSpawnTransformForClientId(const int32 ClientId, FTransform& OutTransform) const
{
    if (ClientId < 0)
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    for (TActorIterator<ALockstepSpawnPoint> It(World); It; ++It)
    {
        if (It->GetClientId() == ClientId)
        {
            OutTransform = It->GetActorTransform();
            return true;
        }
    }

    return false;
}

bool ULockstepPlayerRegistrySubsystem::MoveActorToClientSpawnPoint(const int32 ClientId, AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }

    FTransform SpawnTransform;
    if (!TryGetSpawnTransformForClientId(ClientId, SpawnTransform))
    {
        return false;
    }

    SpawnTransform = AdjustTransformForActorClass(SpawnTransform, Actor->GetClass());

    Actor->SetActorLocationAndRotation(
        SpawnTransform.GetLocation(),
        SpawnTransform.GetRotation().Rotator(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    ConfigureActorForLockstep(Actor);
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

    FVector SpawnLocation = PlayerDesc.SpawnLocation;
    FRotator SpawnRotation = PlayerDesc.SpawnRotation;

    FTransform SpawnTransform;
    if (TryGetSpawnTransformForClientId(PlayerDesc.ClientId, SpawnTransform))
    {
        SpawnTransform = AdjustTransformForActorClass(SpawnTransform, PawnClass);
        SpawnLocation = SpawnTransform.GetLocation();
        SpawnRotation = SpawnTransform.GetRotation().Rotator();
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* SpawnedActor = World->SpawnActor<AActor>(
        PawnClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParams);

    if (!IsValid(SpawnedActor))
    {
        return false;
    }

    ConfigureActorForLockstep(SpawnedActor);
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
