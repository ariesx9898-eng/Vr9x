using UnrealBuildTool;

public class MushokuRPG : ModuleRules
{
	public MushokuRPG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory + "/Public");

		// Animation needs only "Engine" (UAnimInstance, FAnimInstanceProxy, UAnimSequence, montages) plus
		// "AnimGraphRuntime" for UKismetAnimationLibrary::CalculateDirection. No anim-graph editor modules.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"GameplayTags", "AIModule", "NavigationSystem", "Niagara",
			"MotionWarping", "AnimGraphRuntime", "UMG", "Slate", "SlateCore", "Json", "JsonUtilities",
			"DeveloperSettings", "PhysicsCore", "Landscape", "Foliage"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
		}
	}
}
