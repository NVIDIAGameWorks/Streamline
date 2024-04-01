/*
* Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
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

#include "include/sl.h"
#include "include/sl_reflex.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/platforms/sl.chi/vulkan.h"

namespace sl
{
namespace chi
{

class IReflexVk
{
public:
	virtual ComputeStatus init(VkDevice device, param::IParameters* params) = 0;
	virtual ComputeStatus shutdown() = 0;
	virtual void initDispatchTable(VkLayerDispatchTable table) = 0;
	virtual ComputeStatus setSleepMode(const ReflexOptions& consts) = 0;
	virtual ComputeStatus getSleepStatus(ReflexState& settings) = 0;
	virtual ComputeStatus getReport(ReflexState& settings) = 0;
	virtual ComputeStatus sleep() = 0;
	virtual ComputeStatus setMarker(PCLMarker marker, uint64_t frameId) = 0;
	virtual ComputeStatus notifyOutOfBandCommandQueue(CommandQueue queue, OutOfBandCommandQueueType type) = 0;
	virtual ComputeStatus setAsyncFrameMarker(CommandQueue queue, PCLMarker marker, uint64_t frameId) = 0;
};

#ifdef SL_WITH_NVLLVK

IReflexVk* CreateNvLowLatencyVk(VkDevice device, param::IParameters* params);

#endif //SL_WITH_NVLLVK

}
}