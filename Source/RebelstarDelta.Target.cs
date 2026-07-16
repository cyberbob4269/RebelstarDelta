// Rebelstar Delta

using UnrealBuildTool;
using System.Collections.Generic;

public class RebelstarDeltaTarget : TargetRules
{
	public RebelstarDeltaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bOverrideBuildEnvironment = true;
		ExtraModuleNames.Add("RebelstarDelta");
	}
}
