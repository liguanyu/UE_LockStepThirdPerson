using UnrealBuildTool;

public class LockStepFps : ModuleRules
{
    public LockStepFps(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "NetCore",
            "Sockets",
            "Networking",
            "Json",
            "JsonUtilities"
        });
    }
}
