// Copyright 2022 Convai Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;



public class Convai : ModuleRules
{
    private static ConvaiPlatform ConvaiPlatformInstance;

    private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../ThirdParty/")); }
	}

    private ConvaiPlatform GetConvaiPlatformInstance(ReadOnlyTargetRules Target)
    {
        var ConvaiPlatformType = System.Type.GetType("ConvaiPlatform_" + Target.Platform.ToString());
        if (ConvaiPlatformType == null)
        {
            throw new BuildException("Convai does not support platform " + Target.Platform.ToString());
        }

        var PlatformInstance = Activator.CreateInstance(ConvaiPlatformType) as ConvaiPlatform;
        if (PlatformInstance == null)
        {
            throw new BuildException("Convai could not instantiate platform " + Target.Platform.ToString());
        }

        return PlatformInstance;
    }

    private bool ConfigurePlatform(string Platform, UnrealTargetConfiguration Configuration)
    {
        //Convai thirdparty libraries root path
        string root = ThirdPartyPath;
        foreach (var arch in ConvaiPlatformInstance.Architectures())
        {
            string fullPath = root + "grpc/" + "lib/" + ConvaiPlatformInstance.LibrariesPath + arch;
            PublicAdditionalLibraries.AddRange(Directory.GetFiles(fullPath));
            // foreach (var a in PublicAdditionalLibraries)
            //     Console.WriteLine(a);
        }
        return false;
    }

    public Convai(ReadOnlyTargetRules Target) : base(Target)
	{
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        ConvaiPlatformInstance = GetConvaiPlatformInstance(Target);
        bUsePrecompiled = true;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", 
			"CoreUObject",
			"Engine",
			"InputCore" ,
			"HTTP", 
			"Json",
			"JsonUtilities",        
			"AudioMixer",
			"AudioCaptureCore",
			"AudioCapture",
			"Voice",
            "SignalProcessing",
			"libOpus",
            "OpenSSL",
            "zlib"
        });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "AndroidPermission" });
            string BuildPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(BuildPath, "Convai_AndroidAPL.xml"));
        }


        PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Opus codec thirdparty dependency
		AddEngineThirdPartyPrivateStaticDependencies(Target, "libOpus");

        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");



        //PublicDefinitions.Add("PROTOBUF_INLINE_NOT_IN_HEADERS=0");
        //PublicDefinitions.Add("GPR_FORBID_UNREACHABLE_CODE=0");
        PublicDefinitions.Add("ConvaiDebugMode=1");

        PublicDefinitions.Add("GOOGLE_PROTOBUF_NO_RTTI");
        PublicDefinitions.Add("GPR_FORBID_UNREACHABLE_CODE");
        PublicDefinitions.Add("GRPC_ALLOW_EXCEPTIONS=0");

        PrivateIncludePaths.AddRange(
            new string[] {
					Path.Combine(ThirdPartyPath, "gRPC", "Include"),
			});



        //if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			PrecompileForTargets = PrecompileTargetsType.Any;
		}

        //ThirdParty Libraries
        ConfigurePlatform(Target.Platform.ToString(), Target.Configuration);
    }
}



public abstract class ConvaiPlatform
{
    public virtual string ConfigurationDir(UnrealTargetConfiguration Configuration)
    {
        if (Configuration == UnrealTargetConfiguration.Debug || Configuration == UnrealTargetConfiguration.DebugGame)
        {
            return "Debug/";
        }
        else
        {
            return "Release/";
        }
    }
    public abstract string LibrariesPath { get; }
    public abstract List<string> Architectures();
    public abstract string LibraryPrefixName { get; }
    public abstract string LibraryPostfixName { get; }
}

public class ConvaiPlatform_Win64 : ConvaiPlatform
{
    public override string LibrariesPath { get { return "win64/"; } }
    public override List<string> Architectures() { return new List<string> { "" }; }
    public override string LibraryPrefixName { get { return ""; } }
    public override string LibraryPostfixName { get { return ".lib"; } }
}

public class ConvaiPlatform_Android : ConvaiPlatform
{
    public override string LibrariesPath { get { return "android/"; } }
    public override List<string> Architectures() { return new List<string> { "armeabi-v7a/", "arm64-v8a/", "x86_64/" }; }
    public override string LibraryPrefixName { get { return "lib"; } }
    public override string LibraryPostfixName { get { return ".a"; } }
}

//public class ConvaiPlatform_Linux : ConvaiPlatform
//{
//    public override string LibrariesPath { get { return "linux/"; } }
//    public override List<string> Architectures() { return new List<string> { "" }; }
//    public override string LibraryPrefixName { get { return "lib"; } }
//    public override string LibraryPostfixName { get { return ".a"; } }
//}

//public class ConvaiPlatform_PS5 : ConvaiPlatform
//{
//    public override string LibrariesPath { get { return "ps5/"; } }
//    public override List<string> Architectures() { return new List<string> { "" }; }
//    public override string LibraryPrefixName { get { return "lib"; } }
//    public override string LibraryPostfixName { get { return ".a"; } }
//}

//public class ConvaiPlatform_Mac : ConvaiPlatform
//{
//    public override string ConfigurationDir(UnrealTargetConfiguration Configuration)
//    {
//        return "";
//    }
//    public override string LibrariesPath { get { return "mac/"; } }
//    public override List<string> Architectures() { return new List<string> { "" }; }
//    public override string LibraryPrefixName { get { return "lib"; } }
//    public override string LibraryPostfixName { get { return ".a"; } }
//}

//public class ConvaiPlatform_IOS : ConvaiPlatform
//{
//    public override string ConfigurationDir(UnrealTargetConfiguration Configuration)
//    {
//        return "";
//    }
//    public override string LibrariesPath { get { return "ios/"; } }
//    public override List<string> Architectures() { return new List<string> { "" }; }
//    public override string LibraryPrefixName { get { return "lib"; } }
//    public override string LibraryPostfixName { get { return ".a"; } }
//}
