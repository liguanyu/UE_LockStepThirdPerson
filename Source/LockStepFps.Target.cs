using UnrealBuildTool;
using System.Collections.Generic;

public class LockStepFpsTarget : TargetRules
{
    public LockStepFpsTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("LockStepFps");
    }
}
