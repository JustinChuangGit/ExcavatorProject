using UnrealBuildTool;

public class ExcavatorROS : ModuleRules
{
    public ExcavatorROS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "ChaosVehicles",
                "InputCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "ImageWrapper",
                "Json",
                "JsonUtilities",
                "WebSockets"
            }
        );
    }
}
