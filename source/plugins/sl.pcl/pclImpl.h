#pragma once

#include "source/core/sl.api/internalDataSharing.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "source/plugins/sl.pcl/pclstats.h"
#include "source/plugins/sl.reflex/reflex_shared.h"

namespace sl
{
namespace pcl
{

Result implSetData(const BaseStructure* inputs, sl::PCLOptions& constants)
{	
    auto marker = findStruct<PCLHelper>(inputs);
    auto consts = findStruct<PCLOptions>(inputs);
    auto frame = findStruct<FrameToken>(inputs);
    
    if (marker && frame)
    {
        auto evd_id = (PCLSTATS_LATENCY_MARKER_TYPE)to_underlying(marker->get());
        PCLSTATS_MARKER(evd_id, *frame);
    }
    else if (consts)
    {
        PCLSTATS_SET_ID_THREAD(consts->idThread);
        PCLSTATS_SET_VIRTUAL_KEY(to_underlying(consts->virtualKey));

        constants = *consts;
    }
    else
    {
        return Result::eErrorMissingInputParameter;
    }
    
    return Result::eOk;
}

Result implGetData(BaseStructure* outputs)
{
    auto settings = findStruct<PCLState>(outputs);
    if (!settings)
    {
        return Result::eErrorMissingInputParameter;
    }
    // Allow host to check Windows messages for the special low latency message
    settings->statsWindowMessage = g_PCLStatsWindowMessage;
    return Result::eOk;
}


//! Allows other plugins to set GPU agnostic stats
void setPCLStatsMarker(PCLMarker marker, uint32_t frameId)
{
    PCLSTATS_MARKER(to_underlying(marker), frameId);
}

void implOnPluginStartup(sl::param::IParameters* parameters, plugin_manager::PFun_slGetDataInternal* getter, plugin_manager::PFun_slSetDataInternal* setter)
{
	//! Allow other plugins to set PCL stats
    parameters->set(param::pcl::kPFunSetPCLStatsMarker, setPCLStatsMarker);
    // DEPRECATED (reflex-pcl):
    parameters->set(param::latency::kPFunSetLatencyStatsMarker, setPCLStatsMarker);

    // DEPRECATED (reflex-pcl):
    // Expose functions so Reflex plugin can call PCL
    parameters->set(param::_deprecated_reflex_pcl::kSlGetData, getter);
    parameters->set(param::_deprecated_reflex_pcl::kSlSetData, setter);

    PCLSTATS_INIT(0);
}

void implOnPluginShutdown(sl::param::IParameters* parameters)
{
	//! GPU agnostic latency stats shutdown
    PCLSTATS_SHUTDOWN();

    parameters->set(param::pcl::kPFunSetPCLStatsMarker, nullptr);
    // DEPRECATED (reflex-pcl):
    parameters->set(param::latency::kPFunSetLatencyStatsMarker, nullptr);
    parameters->set(param::_deprecated_reflex_pcl::kSlGetData, nullptr);
    parameters->set(param::_deprecated_reflex_pcl::kSlSetData, nullptr);
}

sl::Result implSetMarker(PFun_slPCLSetMarker** reflexSetMarker, sl::PCLOptions& constants, sl::PCLMarker marker, const sl::FrameToken& frame)
{
    // If Reflex is enabled (i.e. the Reflex plugin is loaded) we need to set the marker through the Reflex plugin so it
    // can notify the NV driver (via ICompute::setReflexMarker > NVAPI).
    // If Reflex is NOT enabled, we set the marker through PCL plugin.
    //
    // Current Reflex plugin behaviour is:
    // - slIsFeatureSupported(kFeatureReflex) returns true
    // - ReflexState::lowLatencyAvailable indicates if Reflex is available (but PCL always is)
    // In this scenario, the Reflex plugin is always loaded (when requested), and here markers will always being set through it
    // (although LatencyContext::lowLatencyAvailable will limit NVAPI calls to NV GPUs).
    //
    // A future breaking change will cause Reflex plugin to only be loaded when NV GPU is detected.
    // In that scenario, here markers would bypass the Reflex plugin and only be set through PCL plugin.

    if (!*reflexSetMarker)
    {
        // Get shared data from Reflex
        internal::shared::PFun_GetSharedData* featureGetSharedData{};
        if (param::getPointerParam(api::getContext()->parameters, internal::shared::getParameterNameForFeature(kFeatureReflex).c_str(), &featureGetSharedData))
        {
            reflex::ReflexInternalSharedData data{};
            if (SL_FAILED_SHARED(res, featureGetSharedData(&data, nullptr)))
            {
                SL_LOG_ERROR("Feature kFeatureReflex is not sharing required data, status %u", res);
                return Result::eErrorInvalidState;
            }
            *reflexSetMarker = data.slReflexSetMarker;
        }
        else
        {
            SL_LOG_INFO_ONCE("Feature kFeatureReflex does not seem to be loaded, using PCL-only path");
        }
    }
    if (*reflexSetMarker)
    {
        return (*reflexSetMarker)(marker, frame);
    }
    else
    {
        sl::PCLHelper inputs(marker);
        inputs.next = (BaseStructure*)&frame;
        return implSetData(&inputs, constants);    
    }
}

} // namespace pcl
} // namespace sl
