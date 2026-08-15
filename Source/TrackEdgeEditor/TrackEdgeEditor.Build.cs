// Copyright (c) 2026 DevEdge Studio.
// All rights reserved.

using UnrealBuildTool;

public class TrackEdgeEditor : ModuleRules
{
	public TrackEdgeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EditorStyle",
			"UnrealEd",
			"ToolMenus",
			"LevelEditor",
			"HTTP",
			"Json",
			"JsonUtilities",
			
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"RenderCore",
			"RHI",
			"TrackEdge"
		});
	}
}