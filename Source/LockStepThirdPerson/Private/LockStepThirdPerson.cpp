#include "LockStepThirdPerson.h"
#include "Modules/ModuleManager.h"

class FLockStepThirdPersonModule final : public IModuleInterface
{
};

IMPLEMENT_PRIMARY_GAME_MODULE(FLockStepThirdPersonModule, LockStepThirdPerson, "LockStepThirdPerson");
