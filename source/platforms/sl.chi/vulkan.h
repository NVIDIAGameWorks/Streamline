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

#if defined(SL_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wrl.h>
#endif
#include <inttypes.h>
#include <mutex>

#include "source/core/sl.thread/thread.h"
#include "source/platforms/sl.chi/generic.h"
#include "source/core/sl.interposer/vulkan/layer.h"

#define CHI_CHECK_VK(f) { auto _r = f; if (_r != chi::ComputeStatus::ComputeStatus::eOk) { SL_LOG_ERROR( "%s failed error %u", #f, _r); return VK_INCOMPLETE; } };

namespace sl
{
namespace chi
{

constexpr int kDescriptorCount = 32;
constexpr int kDynamicOffsetCount = 32;

struct VulkanThreadContext : public CommonThreadContext
{
    VkPipelineBindPoint PipelineBindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    VkPipeline Pipeline = {};
    VkPipelineBindPoint PipelineBindPointDesc = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    VkPipelineLayout Layout = {};
    uint32_t FirstSet = {};
    uint32_t DescriptorCount = {};
    VkDescriptorSet DescriptorSets[kDescriptorCount] = {};
    uint32_t DynamicOffsetCount = {};
    uint32_t DynamicOffsets[kDynamicOffsetCount] = {};
};

struct KernelDataVK : public KernelDataBase
{
    // VULKAN
    VkShaderModule shaderModule{};
    VkPipeline pipeline{};
    VkPipelineLayout pipelineLayout{};
    VkDescriptorSet descriptorSet{};
    VkDescriptorSetLayout descriptorSetLayout{};
    size_t descriptorIndex = 0;
    size_t numDescriptors = 64;
};

struct PoolDescCombo
{
    PoolDescCombo() {};
    PoolDescCombo(const PoolDescCombo& rhs) { operator=(rhs); }
    PoolDescCombo& operator=(const PoolDescCombo& rhs)
    {
        pool = rhs.pool;
        desc = rhs.desc;
        return *this;
    }
    VkDescriptorPool pool;
    std::vector<VkDescriptorSet> desc{};
};

enum class DescriptorType
{
    eSampler,
    eTexture,
    eConstantBuffer,
    eTypelessBuffer,     // ByteAddressBuffer, StructuredBuffer (read-only storage in Vulkan)
    eTypedBuffer,        // Buffer
    eStorageTexture,     // RWTexture
    eStorageBuffer,      // RWByteAddressBuffer, RWStructuredBuffer, AppendStructuredBuffer, ConsumeStructuredBuffer
    eStorageTypedBuffer, // RWBuffer
    eAccelStruct,        // RaytracingAccelerationStructure
    eResource            // 'bindless' indexing.
};

struct BindingSlot
{
    BindingSlot() {};
    BindingSlot(const BindingSlot& rhs) { operator=(rhs); }
    inline BindingSlot& operator=(const BindingSlot& rhs)
    {
        dirty = rhs.dirty;
        mapped = rhs.mapped;
        instance = rhs.instance;
        offsetIndex = rhs.offsetIndex;
        dataRange = rhs.dataRange;
        registerIndex = rhs.registerIndex;
        type = rhs.type;
        handles = rhs.handles;
        return *this;
    }

    // dynamic buffers only
    void* mapped = {};
    uint32_t instance = {};
    uint32_t offsetIndex = {};
    uint32_t dataRange = {};
    // generic
    bool dirty = true;
    uint16_t registerIndex = {};
    DescriptorType type = {};
    std::vector<void*> handles = {};
};

struct ResourceBindingDesc
{
    ResourceBindingDesc() {};
    ResourceBindingDesc(const ResourceBindingDesc& rhs) { operator=(rhs); }
    inline ResourceBindingDesc& operator=(const ResourceBindingDesc& rhs)
    {
        maxDescSets = rhs.maxDescSets;
        descriptors = rhs.descriptors;
        offsets = rhs.offsets;
        return *this;
    }

    uint32_t maxDescSets = 1;
    std::map<uint32_t, BindingSlot> descriptors;
    std::vector<uint32_t> offsets; // for dynamic buffers
};

struct DispatchData
{
    DispatchData() {};
    DispatchData(const DispatchData& rhs) { operator=(rhs); }
    inline DispatchData& operator=(const DispatchData& rhs)
    {
        kernel = rhs.kernel;
        signature = rhs.signature;
        signatureToDesc = rhs.signatureToDesc;
        psoToSignature = rhs.psoToSignature;
        return *this;
    }

    KernelDataVK *kernel;
    ResourceBindingDesc* signature = {};
    std::map<ResourceBindingDesc*, PoolDescCombo> signatureToDesc = {};
    std::map<size_t, ResourceBindingDesc*> psoToSignature = {};
};

struct SemaphoreVk : public sl::Resource
{
    SemaphoreVk(VkSemaphore s)
    {
        type = (sl::ResourceType)ResourceType::eFence;
        native = s;
        memory = {};
        view = {};
    };
};

struct CommandQueueVk : public sl::Resource
{
    CommandQueueVk(VkQueue q, CommandQueueType t, uint32_t f, uint32_t i)
    {
        type = (sl::ResourceType)ResourceType::eCommandQueue;
        native = q;
        memory = {};
        view = {};
        queueType = t;
        family = f;
        index = i;
    };
    CommandQueueType queueType;
    uint32_t family;
    uint32_t index;
};

struct SwapChainVk : public sl::Resource
{
    SwapChainVk(VkSwapchainKHR sc, const VkSwapchainCreateInfoKHR& i)
    {
        native = sc;
        memory = {};
        view = {};
        info = i;
        type = (sl::ResourceType)ResourceType::eSwapchain;
    };
    VkSwapchainCreateInfoKHR info;
};

typedef VkResult(VKAPI_PTR* PFN_vkGetImageViewAddressNVX)(VkDevice device, VkImageView imageView, VkImageViewAddressPropertiesNVX* pProperties);

class Vulkan : public Generic
{
    interposer::VkTable* m_vk;
    VkLayerInstanceDispatchTable m_idt;
    VkLayerDispatchTable m_ddt;

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkInstance m_instance = VK_NULL_HANDLE;

    VkSampler m_sampler[eSamplerCount] = {};

    VkPhysicalDeviceMemoryProperties m_vkPhysicalDeviceMemoryProperties;

    uint32_t m_VisibleNodeMask = 0;

    VkCommandBuffer m_cmdBuffer;

    VkSemaphore m_lowLatencySemaphore{};
    uint64_t reflexSemaphoreValue = 0;

    HMODULE m_hmodReflex{};

    thread::ThreadContext<DispatchData> m_dispatchContext;

    struct PerfData
    {
        VkQueryPool  QueryPool[SL_READBACK_QUEUE_SIZE] = {};
        UINT QueryIdx = 0;
        UINT NumExecutedQueries = 0;
        float AccumulatedTimeMS = 0;
        bool Reset[SL_READBACK_QUEUE_SIZE] = {};
    };
    using MapSectionPerf = std::map<std::string, PerfData>;
    MapSectionPerf m_SectionPerfMap[MAX_NUM_NODES] = {};

    VkDebugUtilsMessengerEXT m_debugUtilsMessenger = {};

    inline static PFN_vkCreateInstance vkCreateInstance{};
    inline static PFN_vkDestroyInstance vkDestroyInstance{};
    inline static PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2{};
    inline static PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2{};
    inline static PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices{};
    inline static PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties{};
    inline static HMODULE s_module{};

    static ComputeStatus getStaticVKMethods();

    ComputeStatus processDescriptors(DispatchData& thread);

    struct {
        VkPipelineLayout pipelineLayout;
        VkDescriptorSet descriptorSet;
        VkDescriptorSetLayout descriptorSetLayout;
        VkPipeline doClear;
    } m_imageViewClear;

    bool isFormatSupported(Format format, VkFormatFeatureFlagBits flag);

    // Some API Specific implementation that can be called only from the Generic API shim
    ComputeStatus transitionResourceImpl(CommandList InCmdList, const ResourceTransition* transisitions, uint32_t count) override final;
    ComputeStatus createTexture2DResourceSharedImpl(ResourceDescription& InOutResourceDesc, Resource& OutResource, bool UseNativeFormat, ResourceState InitialState) override final;
    ComputeStatus createBufferResourceImpl(ResourceDescription& InOutResourceDesc, Resource& OutResource, ResourceState InitialState) override final;


    virtual int destroyResourceDeferredImpl(const Resource InResource) override final;
    virtual std::wstring getDebugName(Resource res) override final;

public:

    virtual ComputeStatus init(Device InDevice, param::IParameters* params);
    virtual ComputeStatus shutdown();

    virtual ComputeStatus getInstance(Instance& instance)  override { instance = m_instance;  return ComputeStatus::eOk; };
    virtual ComputeStatus getPhysicalDevice(PhysicalDevice& device)  override { device = m_physicalDevice;  return ComputeStatus::eOk; };
    virtual ComputeStatus waitForIdle(Device device) override;

    virtual ComputeStatus getVendorId(VendorId& id) override final;
    virtual ComputeStatus getRenderAPI(RenderAPI &OutType);

    virtual ComputeStatus restorePipeline(CommandList cmdList)  override final;

    virtual ComputeStatus getNativeResourceState(ResourceState state, uint32_t& nativeState) override final;
    virtual ComputeStatus getResourceState(uint32_t nativeState, ResourceState& state) override final;

    virtual ComputeStatus createKernel(void *InCubinBlob, unsigned int InCubinBlobSize, const char* fileName, const char *EntryPoint, Kernel &OutKernel);
    virtual ComputeStatus destroyKernel(Kernel& kernel);

    virtual ComputeStatus createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[])  override final;
    virtual ComputeStatus destroyCommandListContext(ICommandListContext* ctx) override final;

    virtual ComputeStatus createFence(FenceFlags flags, uint64_t initialValue, Fence& outFence, const char friendlyName[])  override final;
    virtual ComputeStatus destroyFence(Fence fence) override final;

    virtual ComputeStatus createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[], uint32_t index) override final;
    virtual ComputeStatus destroyCommandQueue(CommandQueue& queue) override final;

    virtual ComputeStatus bindKernel(const Kernel InKernel);
    virtual ComputeStatus bindSharedState(CommandList InCmdList, unsigned int InNode);
    virtual ComputeStatus bindSampler(uint32_t binding, uint32_t reg, Sampler sampler) override;
    virtual ComputeStatus bindConsts(uint32_t binding, uint32_t reg, void *data, size_t dataSize, uint32_t instances) override final;
    virtual ComputeStatus bindTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0, uint32_t mipLevels = 0) override final;
    virtual ComputeStatus bindRWTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0) override final;
    virtual ComputeStatus bindRawBuffer(uint32_t binding, uint32_t reg, Resource resource) override final;
    virtual ComputeStatus dispatch(unsigned int blockX, unsigned int blockY, unsigned int blockZ = 1) override final;

    virtual ComputeStatus getNativeFormat(Format format, NativeFormat& native) override final;
    virtual ComputeStatus getFormat(NativeFormat native, Format& format) override final;

    virtual ComputeStatus insertGPUBarrier(CommandList InCmdList, Resource InResource, BarrierType InBarrierType = eBarrierTypeUAV) override final;
    virtual ComputeStatus copyResource(CommandList InCmdList, Resource InDstResource, Resource InSrcResource) override final;
    virtual ComputeStatus cloneResource(Resource InResource, Resource &OutResource, const char friendlyName[], ResourceState InitialState, unsigned int InCreationMask, unsigned int InVisibilityMask) override final;
    virtual ComputeStatus copyBufferToReadbackBuffer(CommandList InCmdList, Resource InResource, Resource OutResource, unsigned int InBytesToCopy) override final;
    virtual ComputeStatus getResourceState(Resource resource, ResourceState& state) override final;
    virtual ComputeStatus getResourceDescription(Resource InResource, ResourceDescription &OutDesc) override final;

    virtual ComputeStatus startTrackingResource(uint32_t id, Resource resource) override final { return ComputeStatus::eOk; }
    virtual ComputeStatus stopTrackingResource(uint32_t id) override final { return ComputeStatus::eOk; }

    virtual ComputeStatus mapResource(CommandList cmdList, Resource resource, void*& data, uint32_t subResource = 0, uint64_t offset = 0, uint64_t totalBytes = UINT64_MAX) override final;
    virtual ComputeStatus unmapResource(CommandList cmdList, Resource resource, uint32_t subResource) override final;

    virtual ComputeStatus copyHostToDeviceBuffer(CommandList InCmdList, uint64_t InSize, const void* InData, Resource InUploadResource, Resource InTargetResource, unsigned long long InUploadOffset, unsigned long long InDstOffset) override final;
    virtual ComputeStatus copyHostToDeviceTexture(CommandList InCmdList, uint64_t InSize, uint64_t RowPitch, const void* InData, Resource InTargetResource, Resource& InUploadResource) override final;

    virtual ComputeStatus getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer) override final;
    
    virtual ComputeStatus clearView(CommandList InCmdList, Resource InResource, const float4 Color, const RECT * pRect, unsigned int NumRects, CLEAR_TYPE &outType) override final;

    virtual ComputeStatus setDebugName(Resource InOutResource, const char InFriendlyName[]) override final;

    virtual ComputeStatus beginPerfSection(CommandList cmdList, const char *section, unsigned int node, bool reset = false) override;
    virtual ComputeStatus endPerfSection(CommandList cmdList, const char *section, float &avgTimeMS, unsigned int node) override;

    virtual ComputeStatus setSleepMode(const ReflexOptions& consts) override final;
    virtual ComputeStatus getSleepStatus(ReflexState& settings) override final;
    virtual ComputeStatus getLatencyReport(ReflexState& settings) override final;
    virtual ComputeStatus sleep() override final;
    virtual ComputeStatus setReflexMarker(ReflexMarker marker, uint64_t frameId) override final;
    virtual ComputeStatus notifyOutOfBandCommandQueue(CommandQueue queue, OutOfBandCommandQueueType type) override final;
    virtual ComputeStatus setAsyncFrameMarker(CommandQueue queue, ReflexMarker marker, uint64_t frameId) override final;

    // Helper methods for NGX feature requirements and slIsFeatureSupported
    static ComputeStatus createInstanceAndFindPhysicalDevice(uint32_t id, chi::Instance& instance, chi::PhysicalDevice& device);
    static ComputeStatus destroyInstance(chi::Instance& instance);
    static ComputeStatus getLUIDFromDevice(chi::PhysicalDevice device, uint32_t& deviceId, LUID* OutId);
    static ComputeStatus getOpticalFlowQueueInfo(chi::PhysicalDevice physicalDevice, uint32_t& queueFamilyIndex, uint32_t& queueIndex);
    virtual ComputeStatus isNativeOpticalFlowSupported() override final;
};

}
}
