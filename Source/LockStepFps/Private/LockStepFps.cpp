#include "LockStepFps.h"
#include "Modules/ModuleManager.h"

class FLockStepFpsModule final : public IModuleInterface
{
};

IMPLEMENT_PRIMARY_GAME_MODULE(FLockStepFpsModule, LockStepFps, "LockStepFps");
