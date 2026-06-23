require("vstudio")

local ROOT = path.getabsolute("./") .. "/"
local TOOLS = path.getabsolute("./tools") .. "/"
local EXTERNAL = path.getabsolute("./external") .. "/"

nvcfg = {}


nvcfg.SL_BUILD_DLSS_DN = true

-- Whether or not DLSS-D should be available publicly at an SDK level.
--
-- When this flag is false, DLSS-D related content will not be included into the
-- public Streamline SDK archives. Content may include the following:
-- - Documentation
-- - Headers
-- - Libraries
-- - Symbols
--
-- Note: DLSS-D will still be supported by these builds at an sl.interposer
-- level so that developers can make and test changes unrelated to DLSS-D whilst
-- maintaining DLSS-D support.
nvcfg.SL_DLSS_DN_PUBLIC_SDK = true









nvcfg.SL_BUILD_LATEWARP = true






-- SL DirectSR plugin build config option. Enabling this will enable the DirectSR plugin build
nvcfg.SL_BUILD_DIRECTSR = true







newoption {
	trigger = "with-nvllvk",
	description = "Use NvLowLatencyVk",
	allowed = {
		{"yes", "yes"},
		{"no", "no"}
	},
	default = "yes"
}

newoption {
	trigger = "with-toolset",
	description = "Specify premake toolset"
}

filter_platforms = "platforms:x64"

newoption {
	trigger = "with-pack",
	description = "Output targets to package path",
	allowed = {
		{"yes", "yes"},
		{"no", "no"}
	},
	default = "no"
}

function out_dir()
	return ROOT .. "_artifacts/"
end

-- Default:
-- return ROOT .. "%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}/"
-- With project:
-- return ROOT .. project .. "/%{cfg.buildcfg}_%{cfg.platform}/"
function out_project_dir(project)
	prefix = out_dir()
	if _OPTIONS['with-pack'] == "yes" then
		return prefix .. "%{cfg.buildcfg}/"
	else
		-- Make sure always have trailing "/"
		project = (project or "%{prj.name}") .. "/%{cfg.buildcfg}_%{cfg.platform}/"
		return prefix .. project
	end
end


-- Output path for dynamic libraries
function out_dynamic_lib_dir()
	local path = out_project_dir()
	if _OPTIONS['with-pack'] == "yes" then
		return path .. "bin/%{cfg.platform}/"
	else
		return path
	end
end

-- Output path for static libraries
function out_static_lib_dir(project)
	local path = out_project_dir(project)
	if _OPTIONS['with-pack'] == "yes" then
		return path .. "lib/%{cfg.platform}/"
	else
		return path
	end
end

-- Output path for intermediary object files
function out_obj_dir()
	local path = out_project_dir()
	if _OPTIONS['with-pack'] == "yes" then
		-- MSVC automatically appends "%{cfg.platform}/${cfg.buildcfg}/", not sure about other toolchains
		return path .. "obj/"
	else
		return path
	end
end

-- Output path for symbols (ie Windows pdb)
function out_symbols_dir()
	local path = out_project_dir()
	if _OPTIONS['with-pack'] == "yes" then
		return path .. "symbols/"
	else
		return path
	end
end

workspace "streamline"

	-- _ACTION is the argument you passed into premake5 when you ran it.
	local project_action = "UNDEFINED"
	if _ACTION ~= nill then project_action = _ACTION end

	-- Where the project files (vs project, solution, etc) go
	location( ROOT .. "_project/" .. project_action)


	configurations { "Debug", "Develop", "Production" }
	platforms {
		"x64",
	}
	architecture "x64"
	language "c++"
	preferredtoolarchitecture "x86_64"
	if (_OPTIONS["with-toolset"]) then
		toolset (_OPTIONS["with-toolset"])
	end
	
		  
	includedirs 
	{ 
		".", ROOT,
		EXTERNAL .. "json/include/",
		EXTERNAL .. "perf-sdk/include",
		EXTERNAL .. "perf-sdk/include",
		EXTERNAL .. "perf-sdk/include/windows-desktop-x64",
		EXTERNAL .. "perf-sdk/NvPerfUtility/include",
		EXTERNAL .. "vulkan/Include"
	}
   	 
	systemversion "latest"
	defines { "SL_SDK", "SL_WINDOWS", "WIN32" , "WIN64" , "_CONSOLE", "NOMINMAX"}

	filter "platforms:x64"
		architecture "x64"
	filter{}

	-- when building any visual studio project
	filter {"system:windows", "action:vs*"}
		flags { "MultiProcessorCompile", "NoMinimalRebuild"}
		
	flags { "FatalWarnings" }
	-- Enable additional warnings: https://premake.github.io/docs/warnings
	--warnings "Extra"

	-- building makefiles
	cppdialect "C++20"
	
	filter "configurations:Debug"
		defines { "DEBUG", "SL_ENABLE_TIMING=1", "SL_ENABLE_PROFILING=1", "SL_DEBUG" }
		if _OPTIONS["use-debug-runtime"] then
			defines { "_DEBUG" }
			runtime "Debug"
		else
			runtime "Release"
		end
		symbols "Full"
				
	filter "configurations:Develop"
		defines { "NDEBUG","SL_ENABLE_TIMING=0","SL_ENABLE_PROFILING=0", "SL_DEVELOP" }
		optimize "On"
		flags { "LinkTimeOptimization" }

	filter "configurations:Production"
		defines { "NDEBUG","SL_ENABLE_TIMING=0","SL_ENABLE_PROFILING=0","SL_PRODUCTION" }
		optimize "On"
		flags { "LinkTimeOptimization" }

	filter { "files:**.hlsl" }
		buildmessage 'Compiling shader %{file.relpath} to DXBC/SPIRV with slang'
		local shaders_output = out_dir() .. "shaders/"
        buildcommands {
			path.translate(EXTERNAL .. "slang/bin/windows-x64/release/") .. 'slangc "%{file.relpath}" -entry main -target spirv -o "' .. shaders_output .. '%{file.basename}.spv"',
			path.translate(EXTERNAL .. "slang/bin/windows-x64/release/") .. 'slangc "%{file.relpath}" -profile sm_5_0 -entry main -target dxbc -o "' .. shaders_output .. '%{file.basename}.cs"',
			'pushd ' .. path.translate(shaders_output),
			'powershell.exe -NoProfile -ExecutionPolicy Bypass ' .. path.translate(TOOLS) .. 'bin2cheader.ps1 -i "%{file.basename}.spv" > "%{file.basename}_spv.h"',
			'powershell.exe -NoProfile -ExecutionPolicy Bypass ' .. path.translate(TOOLS) .. 'bin2cheader.ps1 -i "%{file.basename}.cs" > "%{file.basename}_cs.h"',
			'popd'
		 }
		 buildoutputs { shaders_output .. "%{file.basename}.spv", shaders_output .. "%{file.basename}.cs" }

	filter { "files:**.json" }
		buildmessage 'Compiling %{file.relpath} to %{file.basename}_json.h'
		local json_output = out_dir() .. "json/"
		buildcommands {
			'copy "%{file.relpath}" "' .. json_output .. '%{file.name}"',
			'pushd ' .. path.translate(json_output),
			'powershell.exe -NoProfile -ExecutionPolicy Bypass ' .. path.translate(TOOLS) .. 'bin2cheader.ps1 -i "%{file.basename}.json" > "' .. json_output .. '%{file.basename}_json.h"',
			'popd'
		}
		-- One or more outputs resulting from the build (required)
		buildoutputs { json_output .. "%{file.basename}.json"}
		-- One or more additional dependencies for this build command (optional)
		-- buildinputs { 'path/to/file1.ext', 'path/to/file2.ext' }

	filter {}

	filter {} -- clear filter when you know you no longer need it!
	
	vpaths { ["shaders"] = {"**.hlsl or **.slang" } }

group ""


group "core"

project "sl.interposer"
	kind "SharedLib"
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	staticruntime "off"
	
	prebuildcommands { 'pushd ' .. path.translate(out_dir()), path.translate(TOOLS) .. "gitVersion.bat", 'popd' }

	
	filter { filter_platforms }
		includedirs { "./external/nvapi" }
	filter{}


	defines {"SL_INTERPOSER"}

	files {
		"./source/core/sl.interposer/**.h",
		"./source/core/sl.interposer/**.cpp",
		"./source/core/sl.interposer/**.rc",
	}

	vpaths { ["proxies/d3d12"] = {"./source/core/sl.interposer/d3d12/**.h", "./source/core/sl.interposer/d3d12/**.cpp" }}
	vpaths { ["proxies/d3d11"] = {"./source/core/sl.interposer/d3d11/**.h", "./source/core/sl.interposer/d3d11/**.cpp" }}
	vpaths { ["proxies/dxgi"] = {"./source/core/sl.interposer/dxgi/**.h", "./source/core/sl.interposer/dxgi/**.cpp" }}

	linkoptions { "/DEF:../../source/core/sl.interposer/exports.def" }

	vpaths { ["proxies/vulkan"] = {"./source/core/sl.interposer/vulkan/**.h", "./source/core/sl.interposer/vulkan/**.cpp" }}
	vpaths { ["hook"] = {"./source/core/sl.interposer/hook**"}}

	files {
		"./include/**.h",
		"./source/core/sl.api/**.h",
		"./source/core/sl.api/**.cpp",
		"./source/core/sl.param/**.h",
		"./source/core/sl.param/**.cpp",
		"./source/core/sl.log/**.h",
		"./source/core/sl.log/**.cpp",
		"./source/core/sl.exception/**.h",
		"./source/core/sl.exception/**.cpp",
		"./source/core/sl.ota/**.h",
		"./source/core/sl.ota/**.cpp",
		"./source/core/sl.security/**.h",
		"./source/core/sl.security/**.cpp",
		"./source/core/sl.plugin-manager/**.h",
		"./source/core/sl.plugin-manager/**.cpp",
		"./source/core/sl.plugin/inter_plugin_communication.h",
	}


	defines {"VK_USE_PLATFORM_WIN32_KHR", "SL_ENABLE_EXCEPTION_HANDLING"}
	vpaths { ["security"] = {"./source/security/**.h","./source/security/**.cpp"}}

	links {"dbghelp.lib"}

	filter { filter_platforms }
		links {EXTERNAL .. "nvapi/amd64/nvapi64.lib"}
	filter{}
	
	vpaths { ["manager"] = {"./source/core/sl.plugin-manager/**.h", "./source/core/sl.plugin-manager/**.cpp" }}
	vpaths { ["api"] = {"./source/core/sl.api/**.h","./source/core/sl.api/**.cpp"}}
	vpaths { ["include"] = {"./include/**.h"}}
	vpaths { ["log"] = {"./source/core/sl.log/**.h","./source/core/sl.log/**.cpp"}}
	vpaths { ["exception"] = {"./source/core/sl.exception/**.h","./source/core/sl.exception/**.cpp"}}
	vpaths { ["params"] = {"./source/core/sl.param/**.h","./source/core/sl.param/**.cpp"}}
	vpaths { ["security"] = {"./source/core/sl.security/**.h","./source/core/sl.security/**.cpp"}}
	vpaths { ["version"] = {"./source/core/sl.interposer/versions.h","./source/core/sl.interposer/resource.h","./source/core/sl.interposer/**.rc"}}
	vpaths { ["plugin"] = {"./source/core/sl.plugin/inter_plugin_communication.h",}}
	vpaths { ["ota"] = {"./source/core/sl.ota/**.h", "./source/core/sl.ota/**.cpp" }}

	removefiles
	{
		"./source/core/sl.plugin-manager/pluginManagerEntry.cpp","./source/core/sl.api/plugin-manager.h"
	}
   
	

group ""

group "platforms"

project "sl.compute"
	kind "StaticLib"
	targetdir (out_static_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	staticruntime "off"
	dependson { "sl.interposer"}


	filter { filter_platforms }
		includedirs { "./external/nvapi" }
	filter{}

	if (os.isfile(EXTERNAL .. "slang/bin/windows-x64/release/slangc.exe")) then
	files {
		"./shaders/**.hlsl",
	}
	end
	files {
		"./source/platforms/sl.chi/compute.h",
		"./source/platforms/sl.chi/generic.h",
		"./source/platforms/sl.chi/d3d12.cpp",
		"./source/platforms/sl.chi/d3d12.h",
		"./source/platforms/sl.chi/d3d11.cpp",
		"./source/platforms/sl.chi/d3d11.h",
		"./source/platforms/sl.chi/vulkan.cpp",
		"./source/platforms/sl.chi/vulkan.h",
		"./source/platforms/sl.chi/generic.cpp",
		"./source/core/sl.security/**.h",
		"./source/core/sl.security/**.cpp"
	}
	filter { "options:with-nvllvk=yes" }
		defines { "SL_WITH_NVLLVK" }
		files { "./source/platforms/sl.chi/nvllvk.cpp" }
	filter {}

	vpaths { ["chi"] = {"./source/platforms/sl.chi/**.h","./source/platforms/sl.chi/**.cpp"}}
	vpaths { ["security"] = {"./source/core/sl.security/**.h","./source/core/sl.security/**.cpp"}}

group ""

group "plugins"

function pluginBasicSetup(name)

	filter { filter_platforms }
		includedirs { "./external/nvapi" }
		links {EXTERNAL .. "nvapi/amd64/nvapi64.lib"}
	filter{}

	implibdir (out_static_lib_dir())
	symbolspath (out_symbols_dir() .. "%{cfg.buildtarget.basename}.pdb")
	files { 
		"./source/core/sl.api/**.h",
		"./source/core/sl.log/**.h",
		"./source/core/sl.ota/**.h",
		"./source/core/sl.security/**.h",
		"./source/core/sl.security/**.cpp",
		"./source/core/sl.file/**.h",
		"./source/core/sl.file/**.cpp",
		"./source/core/sl.extra/**.h",
		"./source/core/sl.ota/**.h",
		"./source/core/sl.ota/**.cpp",
		"./source/core/sl.plugin/**.h",
		"./source/core/sl.plugin/**.cpp",
		"./source/plugins/sl." .. name .. "/versions.h",
		"./source/plugins/sl." .. name .. "/resource.h",
		"./source/plugins/sl." .. name .. "/**.rc"
	}
	removefiles {"./source/core/sl.api/plugin-manager.h"}
	
	vpaths { ["api"] = {"./source/core/sl.api/**.h"}}
	vpaths { ["log"] = {"./source/core/sl.log/**.h","./source/core/sl.log/**.cpp"}}
	vpaths { ["ota"] = {"./source/core/sl.ota/**.h", "./source/core/sl.ota/**.cpp"}}
	vpaths { ["file"] = {"./source/core/sl.file/**.h", "./source/core/sl.file/**.cpp"}}
	vpaths { ["extra"] = {"./source/core/sl.extra/**.h", "./source/core/sl.extra/**.cpp"}}
	vpaths { ["plugin"] = {"./source/core/sl.plugin/**.h","./source/core/sl.plugin/**.cpp"}}
	vpaths { ["security"] = {"./source/core/sl.security/**.h","./source/core/sl.security/**.cpp"}}
	vpaths { ["version"] = {"./source/plugins/sl." .. name .. "/resource.h","./source/plugins/sl." .. name .. "/versions.h","./source/plugins/sl." .. name .. "/**.rc"}}
end

project "sl.common"
	kind "SharedLib"
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	dependson { "sl.compute"}

	pluginBasicSetup("common")

	defines {"SL_COMMON_PLUGIN"}

	files { 
		"./source/core/sl.extra/**.cpp",
		"./source/plugins/sl.common/*.json",
		"./source/plugins/sl.common/*.h",
		"./source/plugins/sl.common/*.cpp",
		"./source/core/ngx/**.h",
		"./source/core/ngx/**.cpp",
		"./source/core/sl.ota/**.cpp",
	}
	vpaths { ["imgui"] = {EXTERNAL .. "imgui/**.cpp" }}
	vpaths { ["impl"] = {"./source/plugins/sl.common/*.h", "./source/plugins/sl.common/*.cpp" }}
	--vpaths { ["ngx"] = {"./source/core/ngx/**.h", "./source/core/ngx/**.cpp"}}

	filter { filter_platforms }
		libdirs {EXTERNAL .. "nvapi/amd64", EXTERNAL .. "ngx-sdk/Lib/Windows_x86_64", EXTERNAL .. "pix/bin", EXTERNAL .. "reflex-sdk-vk/lib"}
		links { "nvapi64.lib" }
	filter{}
    links
    {     
		"delayimp.lib", "d3d12.lib", "dxgi.lib", "dxguid.lib", (out_static_lib_dir("sl.compute") .. "sl.compute.lib"),
		"Version.lib"
	}
	-- Release and debug runtimes are not compatible, so we always build against
	-- the release runtime.
	filter "configurations:Debug"
		if _OPTIONS["use-debug-runtime"] then
			links { "nvsdk_ngx_d_dbg.lib" }
		else
			links { "nvsdk_ngx_d.lib"}
		end
	filter "configurations:Develop or Production"
		links { "nvsdk_ngx_d.lib"}
	filter {}

	filter { "options:with-nvllvk=yes" }
		defines { "SL_WITH_NVLLVK" }
		links { "NvLowLatencyVk.lib" }
		linkoptions { "/DELAYLOAD:NvLowLatencyVk.dll" }
	filter {}

	filter { "configurations:Debug", "platforms:x64" }
	-- Match d3d12.cpp's PIX lib pragma scope (SL_ENABLE_PROFILING && !_M_ARM64); only x64 ships PIX binaries.
			linkoptions { "/DELAYLOAD:WinPixEventRuntime.dll" }
	filter {}

if (os.isdir("./source/plugins/sl.dlss_g")) then
	project "sl.dlss_g"
		kind "SharedLib"	
		targetdir (out_dynamic_lib_dir())
		objdir (out_obj_dir())
		characterset ("MBCS")
		dependson { "sl.common"}
		pluginBasicSetup("dlss_g")

		files { 
			"./source/plugins/sl.dlss_g/**.json",
			"./source/plugins/sl.dlss_g/**.h", 
			"./source/plugins/sl.dlss_g/**.cpp",
		}

		links { "Winmm.lib", "Version.lib" }
		filter { filter_platforms }
			links {EXTERNAL .. "nvapi/amd64/nvapi64.lib"}
		filter{}

		vpaths {["impl"] = { "./source/plugins/sl.dlss_g/**.h", "./source/plugins/sl.dlss_g/**.cpp" }}
		
		removefiles {"./source/core/sl.extra/extra.cpp"}
end

project "sl.dlss"
	kind "SharedLib"	
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	dependson { "sl.common"}
	pluginBasicSetup("dlss")
	
	files { 
		"./source/core/ngx/**.h",
		"./source/core/ngx/**.cpp",
		"./source/plugins/sl.dlss/**.json",
		"./source/plugins/sl.dlss/**.h",
		"./source/plugins/sl.dlss/**.cpp"
	}

	vpaths { ["impl"] = {"./source/plugins/sl.dlss/**.h", "./source/plugins/sl.dlss/**.cpp" }}
	vpaths { ["ngx"] = {"./source/core/ngx/**.h", "./source/core/ngx/**.cpp"}}
		
	removefiles {"./source/core/sl.extra/extra.cpp"}
  	

project "sl.reflex"
	kind "SharedLib"	
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	pluginBasicSetup("reflex")
	
	files { 
		"./source/plugins/sl.reflex/**.json", 
		"./source/plugins/sl.reflex/**.h",
		"./source/plugins/sl.reflex/**.cpp"
	}

	vpaths { ["impl"] = {"./source/plugins/sl.reflex/**.h", "./source/plugins/sl.reflex/**.cpp" }}
			
	removefiles {"./source/core/sl.extra/extra.cpp"}

project "sl.pcl"
	kind "SharedLib"
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	pluginBasicSetup("pcl")
	
	files { 
		"./source/plugins/sl.pcl/**.json",
		"./source/plugins/sl.pcl/**.h",
		"./source/plugins/sl.pcl/**.cpp"

	}

	vpaths { ["impl"] = {"./source/plugins/sl.pcl/**.h", "./source/plugins/sl.pcl/**.cpp" }}
			
	removefiles {"./source/core/sl.extra/extra.cpp"}
   	
project "sl.template"
	kind "SharedLib"	
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	pluginBasicSetup("template")
	
	files { 
		"./source/plugins/sl.template/**.json",
		"./source/plugins/sl.template/**.h",
		"./source/plugins/sl.template/**.cpp"
	}

	vpaths { ["impl"] = {"./source/plugins/sl.template/**.h", "./source/plugins/sl.template/**.cpp" }}
			
	removefiles {"./source/core/sl.extra/extra.cpp"}


project "sl.nis"
	kind "SharedLib"
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	dependson { "sl.compute"}
	dependson { "sl.common"}
	pluginBasicSetup("nis")

	files {
		"./source/plugins/sl.nis/**.json",
		"./source/plugins/sl.nis/**.h",
		"./source/plugins/sl.nis/**.cpp"
	}

	vpaths { ["impl"] = {"./source/plugins/sl.nis/**.h", "./source/plugins/sl.nis/**.cpp" }}

	removefiles {"./source/core/sl.extra/extra.cpp"}

project "sl.deepdvc"
	kind "SharedLib"
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	dependson { "sl.compute"}
	dependson { "sl.common"}
	pluginBasicSetup("deepdvc")

	files {
		"./source/core/ngx/**.h",
		"./source/core/ngx/**.cpp",
		"./source/plugins/sl.deepdvc/**.json",
		"./source/plugins/sl.deepdvc/**.h",
		"./source/plugins/sl.deepdvc/**.cpp"
	}

	vpaths { ["impl"] = {"./source/plugins/sl.deepdvc/**.h", "./source/plugins/sl.deepdvc/**.cpp" }}
	vpaths { ["ngx"] = {"./source/core/ngx/**.h", "./source/core/ngx/**.cpp"}}

	removefiles {"./source/core/sl.extra/extra.cpp"}

project "sl.imgui"
	kind "SharedLib"
	targetdir (out_dynamic_lib_dir())
	objdir (out_obj_dir())
	characterset ("MBCS")
	dependson { "sl.compute"}	
	pluginBasicSetup("imgui")

	files {
		"./source/plugins/sl.imgui/**.json",
		"./source/plugins/sl.imgui/**.h",
		"./source/plugins/sl.imgui/**.cpp",
		EXTERNAL .. "imgui/imgui*.cpp",
		EXTERNAL .. "implot/*.h",
		EXTERNAL .. "implot/*.cpp"
	}

	vpaths { ["helpers"] = {"./source/plugins/sl.imgui/imgui_impl**"}}
	vpaths { ["impl"] = {"./source/plugins/sl.imgui/**.h", "./source/plugins/sl.imgui/**.cpp" }}
	vpaths { ["implot"] = {EXTERNAL .. "implot/*.h", EXTERNAL .. "implot/*.cpp"}}
	vpaths { ["imgui"] = {EXTERNAL .. "imgui/**.cpp" }}
	
	defines {"ImDrawIdx=unsigned int"}

	-- For warning in ryml_all.hpp:
	-- warning C4996: 'std::aligned_storage': warning STL4034: std::aligned_storage and std::aligned_storage_t are deprecated in C++23. Prefer alignas(T) std::byte t_buff[sizeof(T)].
	defines {"_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING"}
	
	removefiles {"./source/core/sl.extra/extra.cpp"}


	libdirs {EXTERNAL .."vulkan/Lib"}

	links { "d3d12.lib", "vulkan-1.lib"}
	
if nvcfg.SL_BUILD_DLSS_DN then
    project "sl.dlss_d"
	    kind "SharedLib"
	    targetdir (out_dynamic_lib_dir())
	    objdir (out_obj_dir())
	    characterset ("MBCS")
	    dependson { "sl.common"}
	    pluginBasicSetup("dlss_d")

	    files { 
			"./source/core/ngx/**.h",
			"./source/core/ngx/**.cpp",
			"./source/plugins/sl.dlss_d/**.json",
			"./source/plugins/sl.dlss_d/**.h",
			"./source/plugins/sl.dlss_d/**.cpp"
	    }

	    vpaths { ["impl"] = {"./source/plugins/sl.dlss_d/**.h", "./source/plugins/sl.dlss_d/**.cpp" }}
	    vpaths { ["ngx"] = {"./source/core/ngx/**.h", "./source/core/ngx/**.cpp"}}

	    removefiles {"./source/core/sl.extra/extra.cpp"}
end


if nvcfg.SL_BUILD_DIRECTSR then
    project "sl.directsr"
	    kind "SharedLib"
	    targetdir (ROOT .. "_artifacts/%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}")
	    objdir (ROOT .. "_artifacts/%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}")
	    characterset ("MBCS")
	    dependson { "sl.common" }
	    pluginBasicSetup("directsr")

	    files {
			"./source/core/ngx/**.h",
			"./source/core/ngx/**.cpp",
			"./source/plugins/sl.directsr/**.json",
			"./source/plugins/sl.directsr/**.h",
			"./source/plugins/sl.directsr/**.cpp"
	    }

	    removefiles {"./source/core/sl.extra/extra.cpp"}
end

if (os.isdir("./source/plugins/sl.nvperf")) then
	project "sl.nvperf"
		kind "SharedLib"	
		targetdir (out_dynamic_lib_dir())
		objdir (out_obj_dir())
		characterset ("MBCS")
		dependson { "sl.imgui"}
		pluginBasicSetup("nvperf")
		
		files { 
			"./source/plugins/sl.nvperf/**.json",
			"./source/plugins/sl.nvperf/**.h",
			"./source/plugins/sl.nvperf/**.cpp"
		}

		vpaths { ["impl"] = {"./source/plugins/sl.nvperf/**.h", "./source/plugins/sl.nvperf/**.cpp" }}

		-- For warning in ryml_all.hpp:
		-- warning C4996: 'std::aligned_storage': warning STL4034: std::aligned_storage and std::aligned_storage_t are deprecated in C++23. Prefer alignas(T) std::byte t_buff[sizeof(T)].
		defines {"_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING"}
				
		removefiles {"./source/core/sl.extra/extra.cpp"}
		
		libdirs {EXTERNAL .."vulkan/Lib"}

		links { "d3d12.lib", "vulkan-1.lib"}
end


group ""
