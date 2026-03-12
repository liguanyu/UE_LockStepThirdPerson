using UnrealBuildTool;
using System.Collections.Generic;

public class LockStepThirdPersonEditorTarget : TargetRules
{
    public LockStepThirdPersonEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("LockStepThirdPerson");
    }
}
