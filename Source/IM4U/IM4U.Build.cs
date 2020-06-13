// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class IM4U : ModuleRules
	{
		private string ModulePath
		{
			get { return ModuleDirectory; }
		}

		private string ThirdPartyPath
		{
			get { return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/")); }
		}

		private string LibraryPath
		{
			get { return Path.GetFullPath(Path.Combine(ThirdPartyPath, "LibEncodeHelperWin", "lib")); }
		}
		
		public IM4U(ReadOnlyTargetRules Target) : base(Target)
		{
			string LibEHWinSourcePath = ThirdPartyPath + "LibEncodeHelperWin/";

			string LibEHWinIncPath = LibEHWinSourcePath + "LibEncodeHelperWin/";
			PublicSystemIncludePaths.Add(LibEHWinIncPath);

			string LibEHWinLibPath = LibEHWinSourcePath + "lib/";
			PublicLibraryPaths.Add(LibEHWinLibPath);

			bool FoundLibEHWinDirs = true;
			if (!Directory.Exists(LibEHWinSourcePath))
			{
				System.Console.WriteLine(string.Format("LibEncodeHelperWin source path not found: {0}", LibEHWinSourcePath));
				FoundLibEHWinDirs = false;
			}

			string LibName;
			if ((Target.Platform == UnrealTargetPlatform.Win64 ||
				 Target.Platform == UnrealTargetPlatform.Win32)
				 && FoundLibEHWinDirs)
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					LibName = "LibEncodeHelperWin64";
				}
				else
				{
					LibName = "LibEncodeHelperWin32";
				}

				bool HaveDebugLib = File.Exists(LibEHWinLibPath + LibName + "D" + ".dll");

				if (HaveDebugLib &&
					Target.Configuration == UnrealTargetConfiguration.Debug &&
					Target.bDebugBuildsActuallyUseDebugCRT)
				{
					LibName += "D";
				}

				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, LibName + ".lib"));

			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.Add("/usr/lib/libiconv.dylib");
			}
			
			
			PublicIncludePaths.AddRange(
        new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"IM4U/Private"
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine", 
					"InputCore", 
					"UnrealEd",
					"AssetTools",
					"Slate" ,
					"SlateCore",
					"RawMesh" ,
					"MessageLog",
					"MainFrame",
					"PropertyEditor",
          "RHI",
          "RenderCore",
          "ContentBrowser",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorStyle",
					"EditorWidgets"
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
        {
          "AssetRegistry",
				}
				);
		}
	}
}
