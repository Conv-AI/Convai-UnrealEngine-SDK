// Copyright 2022 Convai Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;



public class Convai : ModuleRules
{
	
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../ThirdParty/")); }
	}
	
	private string AudioDecoderLibraryPath
    {
		get
		{
			return Path.GetFullPath(Path.Combine(ThirdPartyPath, "AudioDecoder", "Lib"));
		}
	}

    private string gRPCLibraryPath
    {
       get
       {
           return Path.GetFullPath(Path.Combine(ThirdPartyPath, "gRPC", "gRPC", "lib"));
       }
    }

    private string ProtobufLibraryPath
    {
       get
       {
           return Path.GetFullPath(Path.Combine(ThirdPartyPath, "gRPC", "protobuf", "lib"));
       }
    }

    private string zlibLibraryPath
    {
       get
       {
           return Path.GetFullPath(Path.Combine(ThirdPartyPath, "zlib"));
       }
    }


	// private string CPRLibraryPath
    // {
	// 	get
	// 	{
	// 		return Path.GetFullPath(Path.Combine(ThirdPartyPath, "cpr", "Lib"));
	// 	}
	// }

    public Convai(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
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
                    Path.Combine(ThirdPartyPath, "AudioDecoder", "Include"),
					Path.Combine(ThirdPartyPath, "gRPC", "gRPC", "Include"),
					//Path.Combine(ThirdPartyPath, "gRPC", "protobuf", "Include"),
					// Path.Combine(ThirdPartyPath, "cpr", "Include"),
				// ... add other private include paths required here ...
			});

        // Audio decoding/encoding library
        PublicAdditionalLibraries.Add(Path.Combine(AudioDecoderLibraryPath, "AudioDecoder.lib"));

        // gRPC libraries
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "address_sorting.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "gpr.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc++.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc++_alts.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc++_error_details.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc++_reflection.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpcpp_channelz.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "upb.lib"));

        PublicAdditionalLibraries.AddRange(
            Directory.GetFiles(Path.Combine(gRPCLibraryPath))
        );

        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "address_sorting.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "cares.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "gpr.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc_unsecure.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc++_unsecure.lib"));

        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpc++.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "grpcpp_channelz.lib"));

        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "libprotobuf.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "upb.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_base.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_malloc_internal.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_raw_logging_internal.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_spinlock_wait.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_throw_delegate.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_time.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_time_zone.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_graphcycles_internal.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_synchronization.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_cord.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_str_format_internal.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_strings.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_strings_internal.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_status.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_statusor.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_bad_optional_access.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_stacktrace.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_symbolize.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(gRPCLibraryPath, "absl_int128.lib"));


        //      PublicAdditionalLibraries.Add(Path.Combine(ProtobufLibraryPath, "libprotobuf.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(zlibLibraryPath, "zlib.lib"));
        //PublicAdditionalLibraries.Add(Path.Combine(zlibLibraryPath, "zlibstatic.lib"));

        // curl cpr libraries
        // PublicAdditionalLibraries.Add(Path.Combine(CPRLibraryPath, "cpr.lib"));
        // PublicAdditionalLibraries.Add(Path.Combine(CPRLibraryPath, "libcurl.lib"));

        //if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			PrecompileForTargets = PrecompileTargetsType.Any;
		}
    }
}
