using UnrealBuildTool;
using System.IO;

public class LandscapeFoliageShortcuts : ModuleRules
{
	public LandscapeFoliageShortcuts(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"LandscapeEditor",
			"FoliageEdit",
			"Foliage",
			"Landscape",
			"EditorFramework",
			"LevelEditor"
		});

		// Include private headers for editor mode access
		string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EnginePath, "Source/Editor/LandscapeEditor/Private"));
		PrivateIncludePaths.Add(Path.Combine(EnginePath, "Source/Editor/FoliageEdit/Private"));
	}
}
