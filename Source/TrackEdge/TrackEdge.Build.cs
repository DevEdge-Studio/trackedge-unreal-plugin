// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

using UnrealBuildTool;

public class TrackEdge : ModuleRules
{
	public TrackEdge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
			"DeveloperSettings",
			"RHI",
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"Engine"
			});
	}
}