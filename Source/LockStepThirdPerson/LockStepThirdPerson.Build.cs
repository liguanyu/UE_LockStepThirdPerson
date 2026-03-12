using UnrealBuildTool;

public class LockStepThirdPerson : ModuleRules
{
    public LockStepThirdPerson(ReadOnlyTargetRules Target) : base(Target)
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
