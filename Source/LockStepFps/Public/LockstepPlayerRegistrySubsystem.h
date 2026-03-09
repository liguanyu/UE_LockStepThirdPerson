#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LockstepTypes.h"

#include "LockstepPlayerRegistrySubsystem.generated.h"

class AActor;

UCLASS(Config=Game)
class LOCKSTEPFPS_API ULockstepPlayerRegistrySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool RegisterExistingPlayerActor(int32 PlayerId, AActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    bool SpawnOrGetPlayerActor(const FLockstepPlayerDesc& PlayerDesc, AActor*& OutActor);

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    AActor* FindPlayerActor(int32 PlayerId) const;

    UFUNCTION(BlueprintCallable, Category = "Lockstep")
    void ClearAllPlayerActors();

private:
    UPROPERTY(Config)
    FString PlayerPawnClassPath = TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C");

    UPROPERTY()
    TMap<int32, TObjectPtr<AActor>> PlayerActorMap;

    TSet<int32> PendingSpawnPlayerIds;
};
