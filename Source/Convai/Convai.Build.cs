// Copyright 2022 Convai Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;
// using Tools.DotNETCommon;



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

    private bool ConfigurePlatform(ReadOnlyTargetRules Target, UnrealTargetConfiguration Configuration)
    {
        //Convai thirdparty libraries root path
        string root = ThirdPartyPath;
        foreach (var arch in ConvaiPlatformInstance.Architectures())
        {
            string grpcPath = root + "gRPC/" + "lib/" + ConvaiPlatformInstance.LibrariesPath + arch;
            
            // Add files that end with .lib
            PublicAdditionalLibraries.AddRange(Directory.GetFiles(grpcPath, "*.lib"));

            // Add files that end with .a
            PublicAdditionalLibraries.AddRange(Directory.GetFiles(grpcPath, "*.a"));
        }
        return false;
    }

    public Convai(ReadOnlyTargetRules Target) : base(Target)
    {
        // Common Settings
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrecompileForTargets = PrecompileTargetsType.Any;
        ConvaiPlatformInstance = GetConvaiPlatformInstance(Target);

        PrivateIncludePaths.Add("Convai/Private");

        if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
        {
            // Include .mm file only for Mac
            PrivateIncludePaths.Add("Convai/Private/Mac");
        }


        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" , "HTTP", "Json", "JsonUtilities", "AudioMixer", "AudioCaptureCore", "AudioCapture", "Voice", "SignalProcessing", "libOpus", "OpenSSL", "zlib", "SSL" });
        PrivateDependencyModuleNames.AddRange(new string[] {"Projects"});
        PublicDefinitions.AddRange(new string[] { "ConvaiDebugMode=1", "GOOGLE_PROTOBUF_NO_RTTI", "GPR_FORBID_UNREACHABLE_CODE", "GRPC_ALLOW_EXCEPTIONS=0" });

        // Target Platform Specific Settings
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            bUsePrecompiled = true;
        }

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "AndroidPermission" });
            string BuildPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(BuildPath, "Convai_AndroidAPL.xml"));
            PublicDefinitions.AddRange(new string[] { "__NDK_MAJOR=21" });
        }

        if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicDefinitions.AddRange(new string[] { "GOOGLE_PROTOBUF_INTERNAL_DONATE_STEAL_INLINE=0", "GOOGLE_PROTOBUF_USE_UNALIGNED=0", "PROTOBUF_ENABLE_DEBUG_LOGGING_MAY_LEAK_PII=0" });
            PrivateIncludePaths.AddRange(new string[] { Path.Combine(ThirdPartyPath, "gRPC", "Include_1.50.x") });
        }
        else 
        {
            PrivateIncludePaths.AddRange(new string[] { Path.Combine(ThirdPartyPath, "gRPC", "Include") });
        }

        // ThirdParty Libraries
        AddEngineThirdPartyPrivateStaticDependencies(Target, "libOpus");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");

        ConfigurePlatform(Target, Target.Configuration);
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

public class ConvaiPlatform_Mac : ConvaiPlatform
{
   public override string LibrariesPath { get { return "mac/"; } }
   public override List<string> Architectures() { return new List<string> { "" }; }
   public override string LibraryPrefixName { get { return "lib"; } }
   public override string LibraryPostfixName { get { return ".a"; } }
}


public class ConvaiPlatform_Linux : ConvaiPlatform
{
   public override string LibrariesPath { get { return "linux/"; } }
   public override List<string> Architectures() { return new List<string> { "" }; }
   public override string LibraryPrefixName { get { return "lib"; } }
   public override string LibraryPostfixName { get { return ".a"; } }
}

//public class ConvaiPlatform_PS5 : ConvaiPlatform
//{
//    public override string LibrariesPath { get { return "ps5/"; } }
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
