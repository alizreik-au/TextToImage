// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextToImage : ModuleRules
{
	public TextToImage(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"RenderCore",
			"RHI"
		});

	}
}
