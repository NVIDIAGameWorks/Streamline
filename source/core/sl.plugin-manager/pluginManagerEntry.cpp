/*
* Copyright (c) 2022 NVIDIA CORPORATION. All rights reserved
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

#include <sstream>
#include <random>

#include "source/core/sl.api/internal.h"
#include "source/core/sl.api/plugin-manager.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.param/parameters.h"
#include "pluginManager.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{                                                              
    return TRUE;                                                                             
}

namespace sl::api
{

SL_EXPORT void nvsLoadPlugins(ID3D12Device *device, VkInstance instance, VkPhysicalDevice vkPD, VkDevice vkDevice)
{
    plugin_manager::getInterface()->setD3D12Device(device);
    plugin_manager::getInterface()->setVulkanDevice(vkPD, vkDevice, instance);
}

SL_EXPORT void nvsUnloadPlugins()
{
    plugin_manager::getInterface()->unloadPlugins();
}

SL_EXPORT void slGetBeforeHooks(const char *key, Hook **list, uint32_t &count)
{
    auto hooks = plugin_manager::getInterface()->getBeforeHooks(key);
    count = (uint32_t)hooks.size();
    *list = new Hook[count];
    memcpy(*list, hooks.data(), sizeof(Hook) * count);    
}

SL_EXPORT void slGetAfterHooks(const char *key, Hook **list, uint32_t &count)
{
    auto hooks = plugin_manager::getInterface()->getAfterHooks(key);
    count = (uint32_t)hooks.size();
    *list = new Hook[count];
    memcpy(*list, hooks.data(), sizeof(Hook) * count);
}

SL_EXPORT param::IParameters *slGetParameters()
{
    return sl::param::getInterface();
}

}
