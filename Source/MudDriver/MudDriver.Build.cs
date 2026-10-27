// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MudDriver : ModuleRules
{
	public MudDriver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"UMG",
			"Slate",
			"NetImgui",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"DeveloperSettings"
		});

		PublicIncludePaths.AddRange(new string[] {
			"MudDriver",
			"MudDriver/SportsCar",
			"MudDriver/OffroadCar",
			"MudDriver/Variant_OffRoad",
			"MudDriver/Variant_TimeTrial",
			"MudDriver/Variant_TimeTrial/UI",
			"MudDriver/Debug",
			"MudDriver/Fuel",
			"MudDriver/GAS",
			"MudDriver/Mud",
			"MudDriver/Mud/Sim",
			"MudDriver/Mud/Debug",
			"MudDriver/Player",
			"MudDriver/Vehicle",
			"MudDriver/Winch"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "ChaosVehiclesCore", "Niagara", "ProceduralMeshComponent", "CableComponent", "Chaos" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
