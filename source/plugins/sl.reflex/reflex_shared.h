/*
* Copyright (c) 2022-2023 NVIDIA CORPORATION. All rights reserved
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "include/sl_reflex.h"
#include "include/sl_struct.h"
#include "source/plugins/sl.pcl/pcl.h"

#include "source/platforms/sl.chi/compute.h"

using PFun_slReflexSetMarker = sl::Result(sl::PCLMarker marker, const sl::FrameToken& frame);

using PFun_slReflexGetCameraData = sl::Result(const sl::ViewportHandle& viewport, const uint32_t frame, sl::ReflexCameraData& outCameraData);
using PFun_slReflexSetCameraDataFence = sl::Result(const sl::ViewportHandle& viewport, sl::chi::Fence fence, const uint32_t syncValue, sl::chi::ICommandListContext* cmdList);
using PFun_slReflexSetCameraDataCallback = sl::Result(std::function<void(uint32_t frame, const sl::ReflexCameraData& cameraData)> callback);


using PFun_slReflexGetSimulationDeltaUs = sl::Result(uint32_t frameId, uint64_t& outDeltaTimeUs);

namespace sl
{

namespace reflex
{
//! Internal shared data for Reflex
//! 
//! {9FB3064E-B6B6-44D8-82D8-709472F48951}
//!
SL_STRUCT_BEGIN(ReflexInternalSharedData, StructType({ 0x9fb3064e, 0xb6b6, 0x44d8, { 0x82, 0xd8, 0x70, 0x94, 0x72, 0xf4, 0x89, 0x51 } }), kStructVersion6)
    //! BACKWARDS COMPATIBILITY MUST BE PRESERVED ALWAYS - NEVER CHANGE OR MOVE OLDER MEMBERS IN THIS STRUCTURE
    //! 
    //! v1 Members
    PFun_slReflexSetMarker* slReflexSetMarker;

    //! v2 Members
    PFun_slReflexGetCameraData* slReflexGetCameraData;

    //! v3 Members
    PFun_slReflexSetCameraDataFence* slReflexSetCameraDataFence;
    void* reservedV4;

    //! v5 Members
    PFun_slReflexGetSimulationDeltaUs* slReflexGetSimulationDeltaUs;

    //! v6 Members
    PFun_slReflexSetCameraDataCallback* slReflexSetCameraDataCallback;

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
SL_STRUCT_END()

//! Enforcing offsets at the compile time to ensure members are not moved around, and that implied feature flag dependencies are maintained
//! 
static_assert(offsetof(sl::reflex::ReflexInternalSharedData, slReflexSetCameraDataCallback) == 72, "new elements can only be added at the end of each structure");
static_assert(offsetof(sl::reflex::ReflexInternalSharedData, slReflexGetSimulationDeltaUs) == 64, "new elements can only be added at the end of each structure");
static_assert(offsetof(sl::reflex::ReflexInternalSharedData, slReflexSetCameraDataFence) == 48, "new elements can only be added at the end of each structure");
static_assert(offsetof(sl::reflex::ReflexInternalSharedData, slReflexSetMarker) == 32, "new elements can only be added at the end of each structure");

}
}
