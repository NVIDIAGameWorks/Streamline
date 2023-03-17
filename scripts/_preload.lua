--
-- Name:        cuda/_preload.lua
-- Purpose:     Define the CUDA API's.
-- Author:      Manu Evans
-- Copyright:   (c) 2013-2015 Manu Evans and the Premake project
--

	local p = premake
	local api = p.api

--
-- To avoid cuda.lua re-including _preload
--
	p.extensions.cuda = true


	p.tools.nvcc = {}

--
-- Register the CUDA extension
--

	p.CUDA = "cuda"
	p.NVCC = "nvcc"

	api.addAllowed("language", { p.CUDA })
--	api.addAllowed("toolset", { p.NVCC })
	api.addAllowed("flags", {
		"RelocatableDeviceCode"
	})


--
-- Register CUDA properties
--

	api.register {
		name = "cudaruntime",
		scope = "config",
		kind = "string",
		allowed = {
			"None",
			"Static",
			"Shared"
		}
	}

	api.register {
		name = "cudaobjtype",
		scope = "config",
		kind = "string",
		allowed = {
			"obj",
			"c",
			"gpu",
			"cubin",
			"ptx"
		}
	}

	api.register {
		name = "cudadebug",
		scope = "config",
		kind = "string",
		allowed = {
			"Default",
			"Yes",
			"No"
		}
	}

	configuration { "**.cu" }
		language "cuda" --p.CUDA
--		toolset "nvcc"
	configuration {}


--
-- Patch the path table to provide knowledge of CUDA file extenstions
--
	function path.iscudafile(fname)
		return path.hasextension(fname, { ".cu" })
	end


--
-- Decide when the full module should be loaded.
--

	return function(cfg)
		if cfg.language == p.CUDA then
			return true
		end

		local prj = cfg.project
		if p.project.iscpp(prj) then
			if cfg.project.hascudafiles == nil then
				cfg.project.hascudafiles = false
				-- scan for CUDA files
				local tr = p.project.getsourcetree(prj)
				p.tree.traverse(tr, {
					onleaf = function(node)
						if not prj.hascudafiles then
							prj.hascudafiles = path.iscudafile(node.name) -- HACK: we should check language, not filename!
						end
					end
				})
			end
		end
		return cfg.project.hascudafiles
	end
