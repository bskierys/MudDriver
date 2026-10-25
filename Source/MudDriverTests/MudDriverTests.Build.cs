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
			"FunctionalTesting"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"ChaosVehicles",
			"MudDriver"
		});
	}
}
