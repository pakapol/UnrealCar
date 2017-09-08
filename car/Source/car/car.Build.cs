// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class car : ModuleRules
{
	public car(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

	  // PhysXVehicles is added to handle the WheeledVehicle types
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "PhysXVehicles"});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// From README: Need to add ALL paths to additional C++ libraries and archive files
    PublicIncludePaths.Add("/usr/local/include");
    PublicAdditionalLibraries.Add("/usr/local/lib/libzmq.a");

	}
}
