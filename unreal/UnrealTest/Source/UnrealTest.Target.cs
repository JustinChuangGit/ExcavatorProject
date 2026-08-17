using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealTestTarget : TargetRules
{
    public UnrealTestTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnrealTest");
    }
}
