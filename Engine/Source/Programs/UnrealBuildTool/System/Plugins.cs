// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Linq;

namespace UnrealBuildTool
{
	public class PluginInfo
	{
		public enum LoadedFromType
		{
			// Plugin is built-in to the engine
			Engine,

			// Project-specific plugin, stored within a game project directory
			GameProject
		};
		
		// Whether or not the plugin should be built
		public bool bShouldBuild;

		// Plugin name
		public string Name;

		// Path to the plugin's root directory
		public string Directory;

		// The plugin descriptor
		public PluginDescriptor Descriptor;

		// Whether this plugin is enabled by default
		public bool bEnabledByDefault;

		// Where does this plugin live?
		public LoadedFromType LoadedFrom;

		public override string ToString()
		{
			return Path.Combine(Directory, Name + ".uplugin");
		}
	}

	public class Plugins
	{
		/// File extension of plugin descriptor files.  NOTE: This constant exists in UnrealBuildTool code as well.
		/// NOTE: This constant exists in PluginManager C++ code as well.
		private static string PluginDescriptorFileExtension = ".uplugin";



		/// <summary>
		/// Loads a plugin descriptor file and fills out a new PluginInfo structure.  Throws an exception on failure.
		/// </summary>
		/// <param name="PluginFile">The path to the plugin file to load</param>
		/// <param name="LoadedFrom">Where the plugin was loaded from</param>
		/// <returns>New PluginInfo for the loaded descriptor.</returns>
		private static PluginInfo LoadPluginDescriptor( FileInfo PluginFileInfo, PluginInfo.LoadedFromType LoadedFrom )
		{
			// Create the plugin info object
			PluginInfo Info = new PluginInfo();
			Info.LoadedFrom = LoadedFrom;
			Info.Directory = PluginFileInfo.Directory.FullName;
			Info.Name = Path.GetFileName(Info.Directory);
			Info.Descriptor = PluginDescriptor.FromFile(PluginFileInfo.FullName);
			return Info;
		}



		/// <summary>
		/// Recursively locates all plugins in the specified folder, appending to the incoming list
		/// </summary>
		/// <param name="PluginsDirectory">Directory to search</param>
		/// <param name="LoadedFrom">Where we're loading these plugins from</param>
		/// <param name="Plugins">List of plugins found so far</param>
		private static void FindPluginsRecursively(string PluginsDirectory, PluginInfo.LoadedFromType LoadedFrom, ref List<PluginInfo> Plugins)
		{
			// NOTE: The logic in this function generally matches that of the C++ code for FindPluginsRecursively
			//       in the core engine code.  These routines should be kept in sync.

			// Each sub-directory is possibly a plugin.  If we find that it contains a plugin, we won't recurse any
			// further -- you can't have plugins within plugins.  If we didn't find a plugin, we'll keep recursing.

			var PluginsDirectoryInfo = new DirectoryInfo(PluginsDirectory);
			foreach( var PossiblePluginDirectory in PluginsDirectoryInfo.EnumerateDirectories() )
			{
				// Do we have a plugin descriptor in this directory?
				bool bFoundPlugin = false;
				foreach (var PluginDescriptorFileName in Directory.GetFiles(PossiblePluginDirectory.FullName, "*" + PluginDescriptorFileExtension))
				{
					// Found a plugin directory!  No need to recurse any further, but make sure it's unique.
					if (!Plugins.Any(x => x.Directory == PossiblePluginDirectory.FullName))
					{
						// Load the plugin info and keep track of it
						var PluginDescriptorFile = new FileInfo(PluginDescriptorFileName);
						var PluginInfo = LoadPluginDescriptor(PluginDescriptorFile, LoadedFrom);

						Plugins.Add(PluginInfo);
						bFoundPlugin = true;
						Log.TraceVerbose("Found plugin in: " + PluginInfo.Directory);
					}

					// No need to search for more plugins
					break;
				}

				if (!bFoundPlugin)
				{
					// Didn't find a plugin in this directory.  Continue to look in subfolders.
					FindPluginsRecursively(PossiblePluginDirectory.FullName, LoadedFrom, ref Plugins);
				}
			}
		}


		/// <summary>
		/// Finds all plugins in the specified base directory
		/// </summary>
		/// <param name="PluginsDirectory">Base directory to search.  All subdirectories will be searched, except directories within other plugins.</param>
		/// <param name="LoadedFrom">Where we're loading these plugins from</param>
		/// <param name="Plugins">List of all of the plugins we found</param>
		public static void FindPluginsIn(string PluginsDirectory, PluginInfo.LoadedFromType LoadedFrom, ref List<PluginInfo> Plugins)
		{
			if (Directory.Exists(PluginsDirectory))
			{
				FindPluginsRecursively(PluginsDirectory, LoadedFrom, ref Plugins);
			}
		}


		/// <summary>
		/// Discovers all plugins
		/// </summary>
		private static void DiscoverAllPlugins()
		{
			if( AllPluginsVar == null )	// Only do this search once per session
			{
				AllPluginsVar = new List< PluginInfo >();

				// Engine plugins
				var EnginePluginsDirectory = Path.Combine( ProjectFileGenerator.EngineRelativePath, "Plugins" );
				Plugins.FindPluginsIn( EnginePluginsDirectory, PluginInfo.LoadedFromType.Engine, ref AllPluginsVar );

				// Game plugins
				if (RulesCompiler.AllGameFolders != null)
				{
					foreach (var GameProjectFolder in RulesCompiler.AllGameFolders)
					{
						var GamePluginsDirectory = Path.Combine(GameProjectFolder, "Plugins");
						Plugins.FindPluginsIn(GamePluginsDirectory, PluginInfo.LoadedFromType.GameProject, ref AllPluginsVar);
					}
				}

				// Also keep track of which modules map to which plugins
				ModulesToPluginMapVar = new Dictionary<string,PluginInfo>( StringComparer.InvariantCultureIgnoreCase );
				foreach( var CurPluginInfo in AllPlugins )
				{
					foreach( var Module in CurPluginInfo.Descriptor.Modules )
					{
						// Make sure a different plugin doesn't already have a module with this name
						// @todo plugin: Collisions like this could happen because of third party plugins added to a project, which isn't really ideal.
						if( ModuleNameToPluginMap.ContainsKey( Module.Name ) )
						{
							throw new BuildException( "Found a plugin in '{0}' which describes a module '{1}', but a module with this name already exists in plugin '{2}'!", CurPluginInfo.Directory, Module.Name, ModuleNameToPluginMap[ Module.Name ].Directory );
						}
						ModulesToPluginMapVar.Add( Module.Name, CurPluginInfo );						
					}
				}
			}
		}


		/// <summary>
		/// Returns true if the specified module name is part of a plugin
		/// </summary>
		/// <param name="ModuleName">Name of the module to check</param>
		/// <returns>True if this is a plugin module</returns>
		public static bool IsPluginModule( string ModuleName )
		{
			return ModuleNameToPluginMap.ContainsKey( ModuleName );
		}

		
		/// <summary>
		/// Checks to see if this module is a plugin module, and if it is, returns the PluginInfo for that module, otherwise null.
		/// </summary>
		/// <param name="ModuleName">Name of the module to check</param>
		/// <returns>The PluginInfo for this module, if the module is a plugin module.  Otherwise, returns null</returns>
		public static PluginInfo GetPluginInfoForModule( string ModuleName )
		{
			PluginInfo FoundPluginInfo;
			if( ModuleNameToPluginMap.TryGetValue( ModuleName, out FoundPluginInfo ) )
			{
				return FoundPluginInfo;
			}
			else
			{
				return null;
			}
		}

	
		/// Access the list of all plugins.  We'll scan for plugins when this is called the first time.
		public static List< PluginInfo > AllPlugins
		{
			get
			{
				DiscoverAllPlugins();
				return AllPluginsVar;
			}
		}

		/// Access a mapping of modules to their respective owning plugin.  Dictionary is case-insensitive.
		private static Dictionary< string, PluginInfo > ModuleNameToPluginMap
		{
			get
			{
				DiscoverAllPlugins();
				return ModulesToPluginMapVar;
			}
		}

	

		/// List of all plugins we've found so far in this session
		private static List< PluginInfo > AllPluginsVar = null;

		/// Maps plugin modules to the plugin that owns them
		private static Dictionary< string, PluginInfo > ModulesToPluginMapVar = null;
	}
}
