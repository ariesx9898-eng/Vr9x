using UnrealBuildTool;

public class MushokuRPGEditorTarget : TargetRules
{
	public MushokuRPGEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MushokuRPG");
	}
}
