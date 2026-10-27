using UnrealBuildTool;

public class MudDriverTests : ModuleRules
{
	public MudDriverTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"FunctionalTesting",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"ChaosVehicles",
			"GameplayAbilities",
			"MudDriver"
		});
	}
}
