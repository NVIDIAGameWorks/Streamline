--
-- cuda_vstudio.lua
-- CUDA integration for vstudio.
-- Copyright (c) 2012-2015 Manu Evans and the Premake project
--

	local p = premake
	local m = p.modules.cuda

	m.elements = {}

	local vstudio = p.vstudio
	local vc2015 = vstudio.vc2010

	m.element = vc2015.element


---- C/C++ projects ----

	function m.cudaPropertyGroup(prj)
		p.push('<PropertyGroup>')
		m.element("CUDAPropsPath", "Condition=\"'$(CUDAPropsPath)'==''\"", "$(VCTargetsPath)\\BuildCustomizations")
		p.w("<CudaToolkitCustomDir>..\\..\\external\\cuda\\11.1</CudaToolkitCustomDir>")
		p.pop('</PropertyGroup>')
	end
	p.override(vc2015.elements, "project", function(oldfn, prj)
		local sections = oldfn(prj)
		table.insertafter(sections, vc2015.project, m.cudaPropertyGroup)
		return sections
	end)

	function m.cudaToolkitPath(prj)
		p.w("<CudaToolkitCustomDir>..\\..\\external\\cuda\\11.1</CudaToolkitCustomDir>")
	end
	p.override(vc2015.elements, "globals", function(oldfn, prj)
		local globals = oldfn(prj)
		table.insertafter(globals, m.cudaToolkitPath)
		return globals
	end)


	function m.cudaProps(prj)
		p.w("<Import Project=\"../../external/cuda/11.1/extras/visual_studio_integration/MSBuildExtensions/CUDA 11.1.props\" />")
	end
	p.override(vc2015.elements, "importExtensionSettings", function(oldfn, prj)
		local importExtensionSettings = oldfn(prj)
		table.insert(importExtensionSettings, m.cudaProps)
		return importExtensionSettings
	end)


	function m.cudaRuntime(cfg)
		if cfg.cudaruntime then
--			m.element("JSONFile", nil, cfg.cudaruntime)
		end
	end

	m.elements.cudaCompile = function(cfg)
		return {
			m.cudaRuntime
		}
	end

	function m.cudaCompile(cfg)
		p.push('<CudaCompile>')
		p.callArray(m.elements.cudaCompile, cfg)
		p.w("<CodeGeneration>compute_86,sm_86</CodeGeneration>");
		p.w("<NvccCompilation>cubin</NvccCompilation>");
		p.w("<TargetMachinePlatform>64</TargetMachinePlatform>");
		p.pop('</CudaCompile>')
	end
	p.override(vc2015.elements, "itemDefinitionGroup", function(oldfn, cfg)
		local cuda = oldfn(cfg)
		table.insertafter(cuda, vc2015.clCompile, m.cudaCompile)
		return cuda
	end)

	function m.cudaTargets(prj)
		p.w("<Import Project=\"../../external/cuda/11.1/extras/visual_studio_integration/MSBuildExtensions/CUDA 11.1.targets\" />")
	end
	p.override(vc2015.elements, "importExtensionTargets", function(oldfn, prj)
		local targets = oldfn(prj)
		table.insert(targets, m.cudaTargets)
		return targets
	end)


---
-- CudaCompile group
---
	vc2015.categories.CudaCompile = {
		name       = "CudaCompile",
		extensions = { ".cu" },
		priority   = 2,

		emitFiles = function(prj, group)
			local fileCfgFunc = function(fcfg, condition)
				if fcfg then
					return {
						vc2015.excludedFromBuild,
						--m.TargetMachinePlatform("--machine 64")
						-- TODO: D per-file options
--						m.objectFileName,
--						m.clCompilePreprocessorDefinitions,
--						m.clCompileUndefinePreprocessorDefinitions,
						--m.optimization,
--						m.forceIncludes,
--						m.precompiledHeader,
--						m.enableEnhancedInstructionSet,
--						m.additionalCompileOptions,
--						m.disableSpecificWarnings,
--						m.treatSpecificWarningsAsErrors
					}
				else
					return {
						vc2015.excludedFromBuild
					}
				end
			end

			vc2015.emitFiles(prj, group, "CudaCompile", {vc2015.generatedFile}, fileCfgFunc)
		end,

		emitFilter = function(prj, group)
			vc2015.filterGroup(prj, group, "CudaCompile")
		end
	}
