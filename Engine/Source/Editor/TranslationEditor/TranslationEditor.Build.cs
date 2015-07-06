// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TranslationEditor : ModuleRules
{
	public TranslationEditor(TargetInfo Target)
	{
		PublicIncludePathModuleNames.Add("LevelEditor");
		PublicIncludePathModuleNames.Add("WorkspaceMenuStructure");
        
        PrivateIncludePathModuleNames.Add("LocalizationService");

        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
                "MessageLog",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Internationalization",
				"Json",
                "PropertyEditor",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
                "GraphEditor",
				"SourceControl",
                "MessageLog",
                "Documentation",
                "LocalizationService",
                "LocalizationDashboard",
			}
		);

        PublicDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"CoreUObject",
				"Engine",
            }
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"WorkspaceMenuStructure",
				"DesktopPlatform",
			}
		);
	}
}
