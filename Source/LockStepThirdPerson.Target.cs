using UnrealBuildTool;
using System.Collections.Generic;

public class LockStepThirdPersonTarget : TargetRules
{
    public LockStepThirdPersonTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("LockStepThirdPerson");
    }
}
