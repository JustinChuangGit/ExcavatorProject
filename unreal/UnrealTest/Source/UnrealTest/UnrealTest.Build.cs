using UnrealBuildTool;

public class UnrealTest : ModuleRules
{
    public UnrealTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "ExcavatorROS",
                "InputCore",
                "Landscape",
                "ProceduralMeshComponent"
            }
        );

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
