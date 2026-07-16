// Rebelstar Delta

using UnrealBuildTool;
using System.Collections.Generic;

public class RebelstarDeltaEditorTarget : TargetRules
{
	public RebelstarDeltaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		bOverrideBuildEnvironment = true;
		ExtraModuleNames.Add("RebelstarDelta");
	}
}
