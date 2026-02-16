// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextToImageEditor : ModuleRules
{
	public TextToImageEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"TextToImage",
			"Slate",
			"SlateCore",
			"InputCore",
			"Projects",
			"AssetRegistry",
			"AssetTools",
			"EditorStyle",
			"UnrealEd",
			"ContentBrowser",
			"ToolMenus",
			"EditorWidgets",
			"RenderCore",
			"RHI"
		});
	}
}
