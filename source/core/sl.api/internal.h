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

#pragma once

#include <string>
#include <vector>
#ifdef SL_WINDOWS
#include <windows.h>
#endif

#include "include/sl_struct.h"

// Forward defines so we can reduce the overall number of includes
struct NVSDK_NGX_Parameter;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkInstance_T;
struct VkImageCreateInfo;
struct VkInstanceCreateInfo;
struct VkDeviceCreateInfo;
struct VkPresentInfoKHR;
struct VkSwapchainCreateInfoKHR;
struct VkAllocationCallbacks;
struct VkWin32SurfaceCreateInfoKHR;
struct VkSurfaceKHR_T;
struct VkSwapchainKHR_T;
struct VkPhysicalDevice_T;
struct VkImage_T;
struct VkFence_T;
struct VkSemaphore_T;
struct VkQueue_T;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice = VkDevice_T*;
using VkInstance = VkInstance_T*;
using VkSurfaceKHR = VkSurfaceKHR_T*;
using VkSwapchainKHR = VkSwapchainKHR_T*;
using VkImage = VkImage_T*;
using VkFence = VkFence_T*;
using VkSemaphore = VkSemaphore_T*;
using VkQueue = VkQueue_T*;
enum VkResult : int;

struct ID3D12Device;
struct ID3D12Resource;
enum D3D12_BARRIER_LAYOUT;

#ifdef SL_LINUX
using HMODULE = void*;
#define GetProcAddress dlsym
#define FreeLibrary dlclose
#define LoadLibraryA(lib) dlopen(lib, RTLD_LAZY)
#define LoadLibraryW(lib) dlopen(sl::extra::toStr(lib).c_str(), RTLD_LAZY)
#else

constexpr uint32_t kTemporaryAppId = 100721531;
//! Special marker
constexpr uint32_t kReflexMarkerSleep = 0x1000;

//! Dummy interface allowing us to extract the underlying base interface
struct DECLSPEC_UUID("ADEC44E2-61F0-45C3-AD9F-1B37379284FF") StreamlineRetrieveBaseInterface : IUnknown
{

};

#endif

namespace sl
{

template<typename T>
T* findStruct(const void* ptr)
{
    auto base = static_cast<const BaseStructure*>(ptr);
    while (base && base->structType != T::s_structType)
    {
        base = base->next;
    }
    return (T*)base;
}

template<typename T>
T* findStruct(void* ptr)
{
    auto base = static_cast<const BaseStructure*>(ptr);
    while (base && base->structType != T::s_structType)
    {
        base = base->next;
    }
    return (T*)base;
}

//! Find a struct of type T, but stop the search if we find a struct of type S
template<typename T, typename S>
T* findStruct(void* ptr)
{
    auto base = static_cast<const BaseStructure*>(ptr);
    while (base && base->structType != T::s_structType)
    {
        base = base->next;

        // If we find a struct of type S, we know should stop the search
        if (base->structType == S::s_structType)
        {
            return nullptr;
        }
    }
    return (T*)base;
}

template<typename T>
T* findStruct(const void** ptr, uint32_t count)
{
    const BaseStructure* base{};
    for (uint32_t i = 0; base == nullptr && i < count; i++)
    {
        base = static_cast<const BaseStructure*>(ptr[i]);
        while (base && base->structType != T::s_structType)
        {
            base = base->next;
        }
    }
    return (T*)base;
}

template<typename T>
bool findStructs(const void** ptr, uint32_t count, std::vector<T*>& structs)
{
    for (uint32_t i = 0; i < count; i++)
    {
        auto base = static_cast<const BaseStructure*>(ptr[i]);
        while (base)
        {
            if (base->structType == T::s_structType)
            {
                structs.push_back((T*)base);
            }
            base = base->next;
        }
    }
    return structs.size() > 0;
}

struct VkDevices
{
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physical;
};

namespace param
{
struct IParameters;
IParameters* getInterface();
void destroyInterface();
}

namespace api
{

// Core API, each plugin must implement these
using PFuncOnPluginLoad = bool(sl::param::IParameters* params, const char* loaderJSON, const char** pluginJSON);
using PFuncOnPluginStartup = bool(const char* loaderJSON, void* device);
using PFuncOnPluginShutdown = void(void);
using PFuncGetPluginFunction = void* (const char* name);

} // namespace api

//! IMPORTANT: 
//! 
//! - Functions with 'Skip' parameter at the end ALWAYS represent BEFORE hooks, if any of the hooks sets 'Skip' to true base method call MUST be skipped
//! - Functions ending with 'Before' are called before base method and MUST be accompanied with their 'After' counterpart.
//! 
#if defined(__dxgi_h__)
using PFunCreateSwapChainBefore = HRESULT(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain, bool& Skip);
using PFunCreateSwapChainForHwndBefore = HRESULT(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFulScreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain, bool& Skip);
using PFunCreateSwapChainForCoreWindowBefore = HRESULT(IDXGIFactory2* pFactory, IUnknown* pDevice, IUnknown* pWindow, const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain, bool& Skip);
using PFunCreateSwapChainAfter = HRESULT(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain);
using PFunCreateSwapChainForHwndAfter = HRESULT(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFulScreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain);
using PFunCreateSwapChainForCoreWindowAfter = HRESULT(IDXGIFactory2* pFactory, IUnknown* pDevice, IUnknown* pWindow, const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain);

using PFunSwapchainDestroyedBefore = void(IDXGISwapChain*);
using PFunPresentBefore = HRESULT(IDXGISwapChain* SwapChain, UINT SyncInterval, UINT Flags, bool& Skip);
using PFunPresent1Before = HRESULT(IDXGISwapChain* SwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters, bool& Skip);
using PFunGetBufferBefore = HRESULT(IDXGISwapChain* SwapChain, UINT Buffer, REFIID riid, void** ppSurface, bool& Skip);
using PFunGetCurrentBackBufferIndexBefore = UINT(IDXGISwapChain* SwapChain, bool& Skip);
using PFunSetFullscreenStateBefore = HRESULT(IDXGISwapChain* SwapChain, BOOL pFullscreen, IDXGIOutput* ppTarget, bool& Skip);
using PFunSetFullscreenStateAfter = HRESULT(IDXGISwapChain* SwapChain, BOOL pFullscreen, IDXGIOutput* ppTarget);
using PFunResizeBuffersBefore = HRESULT(IDXGISwapChain* SwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT& SwapChainFlags, bool& Skip);
using PFunResizeBuffersAfter = HRESULT(IDXGISwapChain* SwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT& SwapChainFlags);
using PFunResizeBuffers1Before = HRESULT(IDXGISwapChain* SwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT* pCreationNodeMask, IUnknown* const* ppPresentQueue, bool& Skip);
using PFunResizeBuffers1After = HRESULT(IDXGISwapChain* SwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT* pCreationNodeMask, IUnknown* const* ppPresentQueue);
#endif

//! All below hooks are of type 'eHookTypeAfter' and they do NOT have 'eHookTypeBefore' counterpart
//! 
#if defined(__d3d12_h__)
using PFunCreateCommittedResourceAfter = HRESULT(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pResourceDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource, void** ppvResource);
using PFunCreatePlacedResourceAfter = HRESULT(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource);
using PFunCreateReservedResourceAfter = HRESULT(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource);
using PFunCreateCommandQueueAfter = HRESULT(const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid, void** ppCommandQueue);
using PFunCreateCommittedResource1After = HRESULT(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riidResource, void** ppvResource);
using PFunCreateReservedResource1After = HRESULT(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riid, void** ppvResource);
using PFunCreateCommittedResource3After = HRESULT(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1* pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, UINT32 NumCastableFormats, DXGI_FORMAT* pCastableFormats, REFIID riidResource, void** ppvResource);
using PFunCreatePlacedResource2After = HRESULT(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1* pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE* pOptimizedClearValue, UINT32 NumCastableFormats, DXGI_FORMAT* pCastableFormats, REFIID riid, void** ppvResource);
using PFunCreateCommittedResource2After = HRESULT(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riidResource, void** ppvResource);
using PFunCreatePlacedResource1After = HRESULT(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource);
using PFunCreateReservedResource2After = HRESULT(const D3D12_RESOURCE_DESC* pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, UINT32 NumCastableFormats, DXGI_FORMAT* pCastableFormats, REFIID riid, void** ppvResource);

using PFunResourceBarrierAfter = void(ID3D12GraphicsCommandList* pCmdList, UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers);
#endif

using PFunVkDeviceWaitIdleBefore = VkResult(VkDevice Device, bool& Skip);
using PFunVkCreateSwapchainKHRBefore = VkResult(VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSwapchainKHR* Swapchain, bool& Skip);
using PFunVkCreateSwapchainKHRAfter = VkResult(VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSwapchainKHR* Swapchain);
using PFunVkGetSwapchainImagesKHRBefore = VkResult(VkDevice Device, VkSwapchainKHR Swapchain, uint32_t* SwapchainImageCount, VkImage* SwapchainImages, bool& Skip);
using PFunVkAcquireNextImageKHRBefore = VkResult(VkDevice Device, VkSwapchainKHR Swapchain, uint64_t Timeout, VkSemaphore Semaphore, VkFence Fence, uint32_t* ImageIndex, bool& Skip);
using PFunVkQueuePresentKHRBefore = VkResult(VkQueue Queue, const VkPresentInfoKHR* PresentInfo, bool& Skip);
using PFunVkDestroySwapchainKHRBefore = void(VkDevice Device, VkSwapchainKHR Swapchain, const VkAllocationCallbacks* Allocator, bool& Skip);
using PFunVkCreateWin32SurfaceKHRBefore = VkResult(VkInstance Instance, const VkWin32SurfaceCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSurfaceKHR* Surface, bool& Skip);
using PFunVkCreateWin32SurfaceKHRAfter = VkResult(VkInstance Instance, const VkWin32SurfaceCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSurfaceKHR* Surface);
using PFunVkDestroySurfaceKHRBefore = void(VkInstance Instance, VkSurfaceKHR Surface, const VkAllocationCallbacks* Allocator, bool& Skip);

} // namespace sl
