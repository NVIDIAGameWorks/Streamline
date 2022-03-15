require("vstudio")

local ROOT = "./"

function os.winSdkVersion()
    local reg_arch = iif( os.is64bit(), "\\Wow6432Node\\", "\\" )
    local sdk_version = os.getWindowsRegistry( "HKLM:SOFTWARE" .. reg_arch .."Microsoft\\Microsoft SDKs\\Windows\\v10.0\\ProductVersion" )
    if sdk_version ~= nil then return sdk_version end
end

workspace "streamline"

	-- _ACTION is the argument you passed into premake5 when you ran it.
	local project_action = "UNDEFINED"
	if _ACTION ~= nill then project_action = _ACTION end

	-- Where the project files (vs project, solution, etc) go
	location( ROOT .. "_project/" .. project_action)

	configurations { "Debug", "Release", "Production" }
	platforms { "x64"}
	architecture "x64"
	language "c++"
	preferredtoolarchitecture "x86_64"
		  
	local externaldir = (ROOT .."external/")

	includedirs 
	{ 
		".", ROOT		
	}
   	 
	systemversion(os.winSdkVersion() .. ".0")
	defines { "SL_SDK", "SL_WINDOWS", "WIN32" , "WIN64" , "_CONSOLE", "NOMINMAX"}
	
	-- when building any visual studio project
	filter {"system:windows", "action:vs*"}
		flags { "MultiProcessorCompile", "NoMinimalRebuild"}		
		
		
	-- building makefiles
	cppdialect "C++17"
	
	filter "configurations:Debug"
		defines { "DEBUG", "_DEBUG" }
		symbols "On"
				
	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		flags { "LinkTimeOptimization" }		
		
	filter "configurations:Production"
		defines { "SL_PRODUCTION" }
		optimize "On"
		flags { "LinkTimeOptimization" }		

		filter { "files:**.hlsl" }
		buildmessage 'Compiling shader %{file.relpath} to DXBC/SPIRV with slang'
        buildcommands {				
			path.translate("../../external/slang_internal/bin/windows-x64/release/")..'slangc "%{file.relpath}" -entry main -target spirv -o "../../_artifacts/shaders/%{file.basename}.spv"',
			path.translate("../../external/slang_internal/bin/windows-x64/release/")..'slangc "%{file.relpath}" -profile sm_5_0 -entry main -target dxbc -o "../../_artifacts/shaders/%{file.basename}.cs"',
			'pushd '..path.translate("../../_artifacts/shaders"),
			path.translate("../../tools/")..'xxd --include "%{file.basename}.spv"  > "%{file.basename}_spv.h"',
			path.translate("../../tools/")..'xxd --include "%{file.basename}.cs"  > "%{file.basename}_cs.h"',
			'popd'
		 }	  
		 -- One or more outputs resulting from the build (required)
		 buildoutputs { ROOT .. "_artifacts/shaders/%{file.basename}.spv", ROOT .. "_artifacts/shaders/%{file.basename}.cs" }	  
		 -- One or more additional dependencies for this build command (optional)
		 --buildinputs { 'path/to/file1.ext', 'path/to/file2.ext' }

	filter {}

	filter {} -- clear filter when you know you no longer need it!
	
	vpaths { ["cuda"] = "**.cu", ["shaders"] = {"**.hlsl" } }
		
group "core"

project "sl.interposer"
	kind "SharedLib"		
	targetdir (ROOT .. "_artifacts/%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}")
	objdir (ROOT .. "_artifacts/%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}") 
	characterset ("MBCS")
	staticruntime "off"
	
	prebuildcommands { path.translate("../../tools/").."gitVersion.bat" }

	files { 
		"./include/**.h",
		"./source/core/sl.interposer/**.h", 
		"./source/core/sl.interposer/**.cpp", 				
		"./source/core/sl.interposer/**.rc",
		"./source/core/sl.api/**.h",
		"./source/core/sl.api/**.cpp",
		"./source/core/sl.param/**.h",
		"./source/core/sl.param/**.cpp",
		"./source/core/sl.log/**.h",
		"./source/core/sl.log/**.cpp",
		"./source/core/sl.security/**.h",
		"./source/core/sl.security/**.cpp",
		"./source/core/sl.plugin-manager/**.h",
		"./source/core/sl.plugin-manager/**.cpp",		
	}

	defines {"VK_USE_PLATFORM_WIN32_KHR", "NOMINMAX"}
	vpaths { ["proxies/d3d12"] = {"./source/core/sl.interposer/d3d12/**.h", "./source/core/sl.interposer/d3d12/**.cpp" }}	
	vpaths { ["proxies/d3d11"] = {"./source/core/sl.interposer/d3d11/**.h", "./source/core/sl.interposer/d3d11/**.cpp" }}	
	vpaths { ["proxies/dxgi"] = {"./source/core/sl.interposer/dxgi/**.h", "./source/core/sl.interposer/dxgi/**.cpp" }}	
	vpaths { ["security"] = {"./source/security/**.h","./source/security/**.cpp"}}

	linkoptions { "/DEF:../../source/core/sl.interposer/exports.def" }
	
	vpaths { ["hook"] = {"./source/core/sl.interposer/hook**"}}		
	vpaths { ["proxies/vulkan"] = {"./source/core/sl.interposer/vulkan/**.h", "./source/core/sl.interposer/vulkan/**.cpp" }}		
	vpaths { ["manager"] = {"./source/core/sl.plugin-manager/**.h", "./source/core/sl.plugin-manager/**.cpp" }}
	vpaths { ["api"] = {"./source/core/sl.api/**.h","./source/core/sl.api/**.cpp"}}
	vpaths { ["include"] = {"./include/**.h"}}
	vpaths { ["log"] = {"./source/core/sl.log/**.h","./source/core/sl.log/**.cpp"}}
	vpaths { ["params"] = {"./source/core/sl.param/**.h","./source/core/sl.param/**.cpp"}}
	vpaths { ["security"] = {"./source/core/sl.security/**.h","./source/core/sl.security/**.cpp"}}
	vpaths { ["version"] = {"./source/core/sl.interposer/versions.h","./source/core/sl.interposer/resource.h","./source/core/sl.interposer/**.rc"}}

	removefiles 
	{ 	
		"./source/core/sl.plugin-manager/pluginManagerEntry.cpp","./source/core/sl.api/plugin-manager.h"
	}

group ""