#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LockstepTypes.h"

#include "LockstepControllablePawnInterface.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class LOCKSTEPFPS_API ULockstepControllablePawnInterface : public UInterface
{
    GENERATED_BODY()
};

class LOCKSTEPFPS_API ILockstepControllablePawnInterface
{
    GENERATED_BODY()

public:
    // 在固定帧中对指定角色应用该玩家输入。
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lockstep")
    void ApplyLockstepInput(const FLockstepInputFrame& InputFrame);
};
