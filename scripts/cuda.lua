
	local p = premake

--
-- always include _preload so that the module works even when not embedded.
--
	if premake.extensions == nil or premake.extensions.cuda == nil then
		include ( "_preload.lua" )
	end


--
-- Create an emscripten namespace to isolate the additions
--
	p.modules.cuda = {}

	local m = p.modules.cuda
	m._VERSION = "0.0.1"


--	include("cuda_nvcc.lua")
	include("cuda_vstudio.lua")

	return m
