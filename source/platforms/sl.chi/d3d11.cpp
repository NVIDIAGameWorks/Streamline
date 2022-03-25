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

#include "source/core/sl.log/log.h"
#include "source/platforms/sl.chi/d3d11.h"

namespace sl
{
namespace chi
{

D3D11 s_d3d11;
ICompute *getD3D11()
{
    return &s_d3d11;
}

std::wstring D3D11::getDebugName(Resource res)
{
    auto unknown = (IUnknown*)res;
    ID3D11Resource* pageable;
    IDXGIObject* dxgi;
    unknown->QueryInterface(&pageable);
    unknown->QueryInterface(&dxgi);
    wchar_t name[128] = {};
    std::wstring wname = L"Unknown";
    if (pageable)
    {
        UINT size = sizeof(name);
        if (FAILED(pageable->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name)))
        {
            char sname[128] = {};
            size = sizeof(sname);
            if (SUCCEEDED(pageable->GetPrivateData(WKPDID_D3DDebugObjectName, &size, sname)))
            {
                std::string tmp(sname);
                wname = std::wstring(tmp.begin(), tmp.end());
            }
        }
        else
        {
            wname = name;
        }
        pageable->Release();
    }
    else if(dxgi)
    {
        UINT size = sizeof(name);
        if (FAILED(dxgi->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name)))
        {
            char sname[128] = {};
            size = sizeof(sname);
            if (SUCCEEDED(dxgi->GetPrivateData(WKPDID_D3DDebugObjectName, &size, sname)))
            {
                std::string tmp(sname);
                wname = std::wstring(tmp.begin(), tmp.end());
            }
        }
        else
        {
            wname = name;
        }
        dxgi->Release();
    }
    return wname;
}

extern const char* getDXGIFormatStr(uint32_t format);

ComputeStatus D3D11::init(Device InDevice, param::IParameters* params)
{
    Generic::init(InDevice, params);

    m_device = (ID3D11Device*)InDevice;

    UINT NodeCount = 1;
    m_visibleNodeMask = (1 << NodeCount) - 1;

    if (NodeCount > MAX_NUM_NODES)
    {
        SL_LOG_ERROR(" too many GPU nodes");
        return eComputeStatusError;
    }

    HRESULT hr = S_OK;

    m_dbgSupportRs2RelaxedConversionRules = true;

    SL_LOG_INFO("GPU nodes %u - visible node mask %u", NodeCount, m_visibleNodeMask);
       
    {
        D3D11_SAMPLER_DESC sampDesc =
        {
            D3D11_FILTER_MIN_MAG_MIP_POINT,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            0.0f,
            1,
            D3D11_COMPARISON_NEVER,
            { 0.0f, 0.0f , 0.0f ,0.0f},
            0.0f,
            D3D11_FLOAT32_MAX,
        };
        m_device->CreateSamplerState(&sampDesc, &m_samplers[eSamplerPointClamp]);
    }

    {
        D3D11_SAMPLER_DESC sampDesc =
        {
            D3D11_FILTER_MIN_MAG_MIP_POINT,
            D3D11_TEXTURE_ADDRESS_MIRROR,
            D3D11_TEXTURE_ADDRESS_MIRROR,
            D3D11_TEXTURE_ADDRESS_MIRROR,
            0.0f,
            1,
            D3D11_COMPARISON_NEVER,
            { 0.0f, 0.0f , 0.0f ,0.0f},
            0.0f,
            D3D11_FLOAT32_MAX,
        };
        m_device->CreateSamplerState(&sampDesc, &m_samplers[eSamplerPointMirror]);
    }

    {
        D3D11_SAMPLER_DESC sampDesc =
        {
            D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            0.0f,
            1,
            D3D11_COMPARISON_NEVER,
            { 0.0f, 0.0f , 0.0f ,0.0f},
            0.0f,
            D3D11_FLOAT32_MAX,
        };
        m_device->CreateSamplerState(&sampDesc, &m_samplers[eSamplerLinearClamp]);
    }

    {
        D3D11_SAMPLER_DESC sampDesc =
        {
            D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            D3D11_TEXTURE_ADDRESS_MIRROR,
            D3D11_TEXTURE_ADDRESS_MIRROR,
            D3D11_TEXTURE_ADDRESS_MIRROR,
            0.0f,
            1,
            D3D11_COMPARISON_NEVER,
            { 0.0f, 0.0f , 0.0f ,0.0f},
            0.0f,
            D3D11_FLOAT32_MAX,
        };
        m_device->CreateSamplerState(&sampDesc, &m_samplers[eSamplerLinearMirror]);
    }

    {
        D3D11_SAMPLER_DESC sampDesc =
        {
            D3D11_FILTER_ANISOTROPIC,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            D3D11_TEXTURE_ADDRESS_CLAMP,
            0.0f,
            1,
            D3D11_COMPARISON_NEVER,
            { 0.0f, 0.0f , 0.0f ,0.0f},
            0.0f,
            D3D11_FLOAT32_MAX,
        };
        m_device->CreateSamplerState(&sampDesc, &m_samplers[eSamplerAnisoClamp]);
    }

    genericPostInit();

    return eComputeStatusOk;
}

ComputeStatus D3D11::shutdown()
{
    for (auto i = 0; i < eSamplerCount; i++)
    {
        SL_SAFE_RELEASE(m_samplers[i]);
    }

    for (auto& rb : m_readbackMap)
    {
        CHI_CHECK(destroyResource(rb.second.target));
        for (int i = 0; i < SL_READBACK_QUEUE_SIZE; i++)
        {
            CHI_CHECK(destroyResource(rb.second.readback[i]));
        }
    }
    for (UINT node = 0; node < MAX_NUM_NODES; node++)
    {
#if SL_ENABLE_PERF_TIMING
        for (auto section : m_sectionPerfMap[node])
        {
            SL_SAFE_RELEASE(section.second.queryBegin);
            SL_SAFE_RELEASE(section.second.queryEnd);
            SL_SAFE_RELEASE(section.second.queryDisjoint);
        }
        m_sectionPerfMap[node].clear();
#endif
    }

    ComputeStatus Res = eComputeStatusOk;
    for (auto& k : m_kernels)
    {
        auto kernel = (KernelDataD3D11*)k.second;
        for(auto& cb : kernel->constBuffers)
        { 
            SL_SAFE_RELEASE(cb.second);
        }
        kernel->constBuffers.clear();

        SL_LOG_VERBOSE("Destroying kernel %s", kernel->name.c_str());
        SL_SAFE_RELEASE(kernel->shader);
        delete kernel;
    }

    return Generic::shutdown();
}

ComputeStatus D3D11::getPlatformType(PlatformType &OutType)
{
    OutType = ePlatformTypeD3D11;
    return eComputeStatusOk;
}

ComputeStatus D3D11::createKernel(void *blobData, unsigned int blobSize, const char* fileName, const char *entryPoint, Kernel &kernel)
{
    if (!blobData) return eComputeStatusInvalidArgument;

    size_t hash = 0;
    auto i = blobSize;
    while (i--)
    {
        hash_combine(hash, ((char*)blobData)[i]);
    }

    ComputeStatus Res = eComputeStatusOk;
    KernelDataD3D11 *data = {};
    bool missing = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_kernels.find(hash);
        missing = it == m_kernels.end();
        if (missing)
        {
            data = new KernelDataD3D11;
            data->hash = hash;
            m_kernels[hash] = data;
        }
        else
        {
            data = (KernelDataD3D11*)(*it).second;
        }
    }
    if (missing)
    {
        data->name = fileName;
        data->entryPoint = entryPoint;
        const char* blob = (const char*)blobData;
        if (blob[0] == 'D' && blob[1] == 'X' && blob[2] == 'B' && blob[3] == 'C')
        {
            data->kernelBlob.resize(blobSize);
            memcpy(data->kernelBlob.data(), blob, blobSize);
            if (FAILED(m_device->CreateComputeShader(data->kernelBlob.data(), data->kernelBlob.size(), nullptr, &data->shader)))
            {
                SL_LOG_ERROR("Failed to create shader %s:%s", fileName, entryPoint);
                return eComputeStatusError;
            }
            SL_LOG_VERBOSE("Creating DXBC kernel %s:%s hash %llu", fileName, entryPoint, hash);
        }
        else
        {
            SL_LOG_ERROR("Unsupported kernel blob");
            return eComputeStatusInvalidArgument;
        }
    }
    else
    {
        if (data->entryPoint != entryPoint || data->name != fileName)
        {
            SL_LOG_ERROR("Shader %s:%s has overlapping hash with shader %s:%s", data->name.c_str(), data->entryPoint.c_str(), fileName, entryPoint);
            return eComputeStatusError;
        }
        SL_LOG_WARN("Kernel %s:%s with hash 0x%llx already created!", fileName, entryPoint, hash);
    }
    kernel = hash;
    return Res;
}

ComputeStatus D3D11::destroyKernel(Kernel& InKernel)
{
    if (!InKernel) return eComputeStatusOk; // fine to destroy null kernels
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_kernels.find(InKernel);
    if (it == m_kernels.end())
    {
        return eComputeStatusInvalidCall;
    }
    KernelDataD3D11* data = (KernelDataD3D11*)(it->second);
    SL_LOG_VERBOSE("Destroying kernel %s", data->name.c_str());
    delete it->second;
    m_kernels.erase(it);
    InKernel = {};
    return eComputeStatusOk;
}

ComputeStatus D3D11::pushState(CommandList cmdList)
{
    auto& threadD3D11 = *(chi::D3D11ThreadContext*)m_getThreadContext();
    auto context = (ID3D11DeviceContext*)cmdList;

    context->CSGetShader(&threadD3D11.engineCS, 0, 0);
    context->CSGetSamplers(0, chi::kMaxD3D11Items, threadD3D11.engineSamplers);
    context->OMGetRenderTargets(chi::kMaxD3D11Items, threadD3D11.engineRTVs, &threadD3D11.engineDSV);
    context->CSGetShaderResources(0, chi::kMaxD3D11Items, &threadD3D11.engineSRVs[0]);
    context->CSGetUnorderedAccessViews(0, chi::kMaxD3D11Items, &threadD3D11.engineUAVs[0]);
    context->CSGetConstantBuffers(0, chi::kMaxD3D11Items, threadD3D11.engineConstBuffers);

    // Must do this to ensure RTV/SRV/UAV is not bound as previous input/output otherwise 
    // our CS passes which rely on the resources from the engine might get null input
    static ID3D11RenderTargetView* nullRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    static ID3D11UnorderedAccessView* nullUAVs[chi::kMaxD3D11Items] = {};
    static ID3D11ShaderResourceView* nullSRVs[chi::kMaxD3D11Items] = {};
    context->OMSetRenderTargets(chi::kMaxD3D11Items, nullRTVs, nullptr);
    context->CSSetUnorderedAccessViews(0, chi::kMaxD3D11Items, nullUAVs, nullptr);
    context->CSSetShaderResources(0, chi::kMaxD3D11Items, nullSRVs);

    return eComputeStatusOk;
}

ComputeStatus D3D11::popState(CommandList cmdList)
{
    auto& threadD3D11 = *(chi::D3D11ThreadContext*)m_getThreadContext();
    auto context = (ID3D11DeviceContext*)cmdList;

    context->CSSetShader(threadD3D11.engineCS, 0, 0);
    context->CSSetSamplers(0, chi::kMaxD3D11Items, threadD3D11.engineSamplers);
    context->CSSetUnorderedAccessViews(0, chi::kMaxD3D11Items, threadD3D11.engineUAVs, nullptr);
    context->OMSetRenderTargets(chi::kMaxD3D11Items, threadD3D11.engineRTVs, threadD3D11.engineDSV);
    context->CSSetShaderResources(0, chi::kMaxD3D11Items, threadD3D11.engineSRVs);
    context->CSSetConstantBuffers(0, chi::kMaxD3D11Items, threadD3D11.engineConstBuffers);

    SL_SAFE_RELEASE(threadD3D11.engineCS);
    int n = chi::kMaxD3D11Items;
    while (n--)
    {
        SL_SAFE_RELEASE(threadD3D11.engineSamplers[n]);
        SL_SAFE_RELEASE(threadD3D11.engineConstBuffers[n]);
        SL_SAFE_RELEASE(threadD3D11.engineUAVs[n]);
        SL_SAFE_RELEASE(threadD3D11.engineSRVs[n]);
    }

    threadD3D11 = {};

    return eComputeStatusOk;
}

ComputeStatus D3D11::createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[])
{
    ctx = {};
    return eComputeStatusNoImplementation;
}

ComputeStatus D3D11::destroyCommandListContext(ICommandListContext* ctx)
{
    return eComputeStatusNoImplementation;
}

ComputeStatus D3D11::createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[])
{    
    return eComputeStatusNoImplementation;
}

ComputeStatus D3D11::destroyCommandQueue(CommandQueue& queue)
{
    if (queue)
    {
        auto tmp = (IUnknown*)queue;
        SL_SAFE_RELEASE(tmp);
    }    
    return eComputeStatusOk;
}

ComputeStatus D3D11::setFullscreenState(SwapChain chain, bool fullscreen, Output out)
{
    if (!chain) return eComputeStatusInvalidArgument;
    IDXGISwapChain* swapChain = (IDXGISwapChain*)chain;
    if (FAILED(swapChain->SetFullscreenState(fullscreen, (IDXGIOutput*)out)))
    {
        SL_LOG_ERROR("Failed to set fullscreen state");
    }
    return eComputeStatusOk;
}

ComputeStatus D3D11::getRefreshRate(SwapChain chain, float& refreshRate)
{
    if (!chain) return eComputeStatusInvalidArgument;
    IDXGISwapChain* swapChain = (IDXGISwapChain*)chain;
    IDXGIOutput* dxgiOutput;
    HRESULT hr = swapChain->GetContainingOutput(&dxgiOutput);
    // if swap chain get failed to get DXGIoutput then follow the below link get the details from remarks section
    //https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getcontainingoutput
    if (SUCCEEDED(hr))
    {
        // get the descriptor for current output
        // from which associated mornitor will be fetched
        DXGI_OUTPUT_DESC outputDes{};
        hr = dxgiOutput->GetDesc(&outputDes);
        dxgiOutput->Release();
        if (SUCCEEDED(hr))
        {
            MONITORINFOEXW info;
            info.cbSize = sizeof(info);
            // get the associated monitor info
            if (GetMonitorInfoW(outputDes.Monitor, &info) != 0)
            {
                // using the CCD get the associated path and display configuration
                UINT32 requiredPaths, requiredModes;
                if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes) == ERROR_SUCCESS)
                {
                    std::vector<DISPLAYCONFIG_PATH_INFO> paths(requiredPaths);
                    std::vector<DISPLAYCONFIG_MODE_INFO> modes2(requiredModes);
                    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes2.data(), nullptr) == ERROR_SUCCESS)
                    {
                        // iterate through all the paths until find the exact source to match
                        for (auto& p : paths) {
                            DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
                            sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                            sourceName.header.size = sizeof(sourceName);
                            sourceName.header.adapterId = p.sourceInfo.adapterId;
                            sourceName.header.id = p.sourceInfo.id;
                            if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
                            {
                                // find the matched device which is associated with current device 
                                // there may be the possibility that display may be duplicated and windows may be one of them in such scenario
                                // there may be two callback because source is same target will be different
                                // as window is on both the display so either selecting either one is ok
                                if (wcscmp(info.szDevice, sourceName.viewGdiDeviceName) == 0) {
                                    // get the refresh rate
                                    UINT numerator = p.targetInfo.refreshRate.Numerator;
                                    UINT denominator = p.targetInfo.refreshRate.Denominator;
                                    double refrate = (double)numerator / (double)denominator;
                                    refreshRate = (float)refrate;
                                    return eComputeStatusOk;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    SL_LOG_ERROR("Failed to retreive refresh rate from swapchain 0x%llx", chain);
    return eComputeStatusError;
}

ComputeStatus D3D11::getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer)
{
    ID3D11Resource* tmp;
    if (FAILED(((IDXGISwapChain*)chain)->GetBuffer(index, IID_PPV_ARGS(&tmp))))
    {
        SL_LOG_ERROR("Failed to get buffer from swapchain");
        return eComputeStatusError;
    }
    buffer = tmp;
    return eComputeStatusOk;
}

ComputeStatus D3D11::bindSharedState(CommandList cmdList, UINT node)
{
    if (!cmdList) return eComputeStatusInvalidArgument;

    m_context = (ID3D11DeviceContext*)cmdList;
    return eComputeStatusOk;
}

ComputeStatus D3D11::bindKernel(const Kernel kernelToBind)
{
    if (!m_context) return eComputeStatusInvalidArgument;

    auto& ctx = m_dispatchContext.getContext();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_kernels.find(kernelToBind);
        if (it == m_kernels.end())
        {
            SL_LOG_ERROR("Trying to bind kernel which has not been created");
            return eComputeStatusInvalidCall;
        }
        ctx.kernel = (KernelDataD3D11*)(*it).second;
    }

    m_context->CSSetShader(ctx.kernel->shader, nullptr, 0);

    return eComputeStatusOk;
}

ComputeStatus D3D11::bindSampler(uint32_t pos, uint32_t base, Sampler sampler)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!m_context || !ctx.kernel || base >= 8) return eComputeStatusInvalidArgument;

    m_context->CSSetSamplers(base, 1, &m_samplers[sampler]);

    return eComputeStatusOk;
}

ComputeStatus D3D11::bindConsts(uint32_t pos, uint32_t base, void *data, size_t dataSize, uint32_t instances)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!m_context || !ctx.kernel) return eComputeStatusInvalidArgument;

    auto it = ctx.kernel->constBuffers.find(base);
    if (it == ctx.kernel->constBuffers.end())
    {
        Resource buffer;
        chi::ResourceDescription desc = {};
        desc.width = extra::align((uint32_t)dataSize, 16);
        desc.height = 1;
        desc.heapType = eHeapTypeUpload;
        desc.state = ResourceState::eConstantBuffer;
        createBuffer(desc, buffer, "const buffer");
        ctx.kernel->constBuffers[base] = (ID3D11Buffer*)buffer;
    }
    auto buffer = ctx.kernel->constBuffers[base];
    D3D11_MAPPED_SUBRESOURCE bufferData = {};
    m_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &bufferData);
    if (!bufferData.pData)
    {
        SL_LOG_ERROR("Failed to map constant buffer");
        return eComputeStatusError;
    }
    memcpy(bufferData.pData, data, dataSize);
    m_context->Unmap(buffer, 0);

    m_context->CSSetConstantBuffers(base, 1, &buffer);

    return eComputeStatusOk;
}

ComputeStatus D3D11::bindTexture(uint32_t pos, uint32_t base, Resource resource, uint32_t mipOffset, uint32_t mipLevels)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!m_context || !ctx.kernel) return eComputeStatusInvalidArgument;

    ResourceDriverDataD3D11 data;
    auto res = getTextureDriverData(resource, data, mipOffset, mipLevels);

    m_context->CSSetShaderResources(base, 1, &data.SRV);

    return res;
}

ComputeStatus D3D11::bindRWTexture(uint32_t pos, uint32_t base, Resource resource, uint32_t mipOffset)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!m_context || !ctx.kernel) return eComputeStatusInvalidArgument;

    ResourceDriverDataD3D11 data;
    auto res = getSurfaceDriverData(resource, data, mipOffset);

    m_context->CSSetUnorderedAccessViews(base, 1, &data.UAV, nullptr);

    return res;
}

ComputeStatus D3D11::bindRawBuffer(uint32_t pos, uint32_t base, Resource resource)
{
    // This is still just a UAV for D3D11 so reuse the other method
    // Note that UAV creation checks for buffers and modifies view accordingly (D3D12_BUFFER_UAV_FLAG_RAW etc.)
    return bindRWTexture(pos, base, resource);
}

ComputeStatus D3D11::dispatch(unsigned int blocksX, unsigned int blocksY, unsigned int blocksZ)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!m_context || !ctx.kernel) return eComputeStatusInvalidArgument;
     
    m_context->Dispatch(blocksX, blocksY, blocksZ);

    return eComputeStatusOk;
}

ComputeStatus D3D11::getTextureDriverData(Resource res, ResourceDriverDataD3D11&data, uint32_t mipOffset, uint32_t mipLevels, Sampler sampler)
{
    ID3D11Resource *resource = (ID3D11Resource*)res;
    if (!resource) return eComputeStatusInvalidArgument;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t hash = (mipOffset << 16) | mipLevels;

    auto it = m_resourceData.find(resource);
    if (it == m_resourceData.end() || (*it).second.find(hash) == (*it).second.end())
    {
        
        ResourceDescription Desc;
        getResourceDescription(res, Desc);

        ID3D11Resource* Resource = (ID3D11Resource*)res;

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Format = getCorrectFormat((DXGI_FORMAT)Desc.nativeFormat);
        SRVDesc.Texture2D.MipLevels = Desc.mips;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        auto res = m_device->CreateShaderResourceView(Resource, &SRVDesc, &data.SRV);
        if (FAILED(res)) 
        { 
            SL_LOG_ERROR("CreateShaderResourceView failed - status %d", res); 
            return eComputeStatusError;
        }
        constexpr char SRVFriendlyName[] = "sl.compute.textureCachedSRV";
        data.SRV->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(SRVFriendlyName), SRVFriendlyName); // Narrow character type debug object name

        m_resourceData[resource][hash] = data;
    }
    else
    {
        data = (*it).second[hash];
    }

    return eComputeStatusOk;
}

ComputeStatus D3D11::getSurfaceDriverData(Resource res, ResourceDriverDataD3D11&data, uint32_t mipOffset)
{
    ID3D11Resource *resource = (ID3D11Resource*)res;
    if (!resource) return eComputeStatusInvalidArgument;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t hash = mipOffset << 16;

    auto it = m_resourceData.find(resource);
    if (it == m_resourceData.end() || (*it).second.find(hash) == (*it).second.end())
    {
        auto node = 0; // FIX THIS
        ResourceDescription Desc;
        getResourceDescription(res, Desc);

        ID3D11Resource* Resource = (ID3D11Resource*)res;

        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
        if (Desc.flags & (ResourceFlags::eRawOrStructuredBuffer | ResourceFlags::eConstantBuffer))
        {
            UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            UAVDesc.Buffer.FirstElement = 0;
            UAVDesc.Buffer.NumElements = Desc.width / sizeof(uint32_t);
            UAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        }
        else
        {
            if (!isSupportedFormat(getCorrectFormat((DXGI_FORMAT)Desc.nativeFormat), 0, D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE))
            {
                return eComputeStatusError;
            }

            UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            UAVDesc.Texture2D.MipSlice = mipOffset;
            UAVDesc.Format = getCorrectFormat((DXGI_FORMAT)Desc.nativeFormat);
        }
        
        auto status = m_device->CreateUnorderedAccessView(Resource, &UAVDesc, &data.UAV);
        if (FAILED(status))
        { 
            SL_LOG_ERROR("CreateShaderResourceView failed - status %d", status);
            return eComputeStatusError;
        }

        constexpr char UAVFriendlyName[] = "sl.compute.surfaceCachedUAV";
        data.UAV->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(UAVFriendlyName), UAVFriendlyName); // Narrow character type debug object name

        SL_LOG_VERBOSE("Cached UAV resource 0x%llx node %u fmt %s size (%u,%u)", res, 0, getDXGIFormatStr(Desc.nativeFormat), (UINT)Desc.width, (UINT)Desc.height);
        
        m_resourceData[resource][hash] = data;
    }
    else
    {
        data = (*it).second[hash];
    }

    return eComputeStatusOk;
}

bool D3D11::isSupportedFormat(DXGI_FORMAT format, int flag1, int flag2)
{
    HRESULT hr = {};
    {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { format, 0 };
        hr = m_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
        if (SUCCEEDED(hr) && (FormatSupport.OutFormatSupport & flag1) != 0)
        {
            return true;
        }
    }
    {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT2 FormatSupport = { format, 0 };
        hr = m_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &FormatSupport, sizeof(FormatSupport));
        if (SUCCEEDED(hr) && (FormatSupport.OutFormatSupport2 & flag2) != 0)
        {
            return true;
        }
    }
    SL_LOG_ERROR("Format %s is unsupported - hres %lu flags %d %d", getDXGIFormatStr(format), hr, flag1, flag2);
    return false;
}

ComputeStatus D3D11::createTexture2DResourceSharedImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, bool UseNativeFormat, ResourceState InitialState)
{
    ID3D11Texture2D* pResource = nullptr;

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = InOutResourceDesc.width;
    desc.Height = InOutResourceDesc.height;
    desc.MipLevels = InOutResourceDesc.mips;
    desc.ArraySize = 1;
    if (UseNativeFormat)
    {
        desc.Format = (DXGI_FORMAT)InOutResourceDesc.nativeFormat;
    }
    else
    {
        NativeFormat native;
        getNativeFormat(InOutResourceDesc.format, native);
        desc.Format = (DXGI_FORMAT)native; 
    }
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    desc.MiscFlags = 0;

    switch (InOutResourceDesc.heapType)
    {
        default: assert(0); // Fall through
        case eHeapTypeDefault:
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.CPUAccessFlags = 0;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            break;
        case eHeapTypeUpload:
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            break;
        case eHeapTypeReadback:
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.BindFlags = 0;
            break;
    }

    if (m_allocateCallback)
    {
        ResourceDesc desc = { ResourceType::eResourceTypeTex2d, &desc, 0, nullptr, nullptr };
        auto result = m_allocateCallback(&desc);
        pResource = (ID3D11Texture2D*)result.native;
    }
    else
    {
        m_device->CreateTexture2D(&desc, NULL, &pResource);
    }
    
    OutResource = pResource;
    if (!pResource)
    {
        SL_LOG_ERROR("Failed to create Tex2d");
        return eComputeStatusError;
    }
    return eComputeStatusOk;
}

ComputeStatus D3D11::createBufferResourceImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, ResourceState InitialState)
{
    ID3D11Buffer* pResource = nullptr;

    D3D11_BUFFER_DESC bufdesc;
    bufdesc.ByteWidth = InOutResourceDesc.width;
    assert(InOutResourceDesc.height == 1);
    bufdesc.StructureByteStride = 0;

    switch (InOutResourceDesc.heapType)
    {
        default: assert(0); // Fall through
        case eHeapTypeDefault:
            bufdesc.Usage = D3D11_USAGE_DEFAULT;
            bufdesc.CPUAccessFlags = 0;
            bufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
            bufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            break;
        case eHeapTypeUpload:
            bufdesc.MiscFlags = 0;
            if (InOutResourceDesc.state == ResourceState::eConstantBuffer)
            {
                bufdesc.Usage = D3D11_USAGE_DYNAMIC;
                bufdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                bufdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            }
            else
            {
                bufdesc.Usage = D3D11_USAGE_STAGING;
                bufdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
                bufdesc.BindFlags = 0;
            }
            break;
        case eHeapTypeReadback:
            bufdesc.Usage = D3D11_USAGE_STAGING;
            bufdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            bufdesc.MiscFlags = 0;
            bufdesc.BindFlags = 0;
            break;
    }

    if (m_allocateCallback)
    {
        ResourceDesc desc = { ResourceType::eResourceTypeBuffer, &bufdesc, 0, nullptr };
        auto result = m_allocateCallback(&desc);
        pResource = (ID3D11Buffer*)result.native;
    }
    else
    {
        m_device->CreateBuffer(&bufdesc, NULL, &pResource);
    }

    OutResource = pResource;
    if (!pResource)
    {
        SL_LOG_ERROR("Failed to create buffer");
        return eComputeStatusError;
    }
    return eComputeStatusOk;
}

ComputeStatus D3D11::setDebugName(Resource res, const char name[])
{
    ID3D11Resource *resource = (ID3D11Resource*)res;
    resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
    return eComputeStatusOk;
}

ComputeStatus D3D11::copyHostToDeviceBufferImpl(CommandList InCmdList, uint64_t InSize, const void *InData, Resource InUploadResource, Resource InTargetResource, unsigned long long InUploadOffset, unsigned long long InDstOffset)
{
    UINT8* StagingPtr = nullptr;

    ID3D11Resource* Resource = (ID3D11Resource*)InTargetResource;
    ID3D11Resource* Scratch = (ID3D11Resource*)InUploadResource;

    D3D11_MAPPED_SUBRESOURCE sub;
    HRESULT hr = ((ID3D11DeviceContext*)InCmdList)->Map(Scratch, 0, D3D11_MAP_WRITE, 0, &sub);
    if (hr != S_OK)
    {
        SL_LOG_ERROR("Failed to map buffer - error %lu", hr);
        return eComputeStatusError;
    }

    char* target = (char*)sub.pData + InUploadOffset;
    memcpy(target, InData, InSize);
    ((ID3D11DeviceContext*)InCmdList)->Unmap(Scratch, 0);
    D3D11_BOX Box = { (UINT)InUploadOffset, 0, 0, (UINT)InUploadOffset + (UINT)InSize, 1, 1 };
    ((ID3D11DeviceContext*)InCmdList)->CopySubresourceRegion(Resource, 0, (UINT)InDstOffset, 0, 0, Scratch, 0, &Box);

    return eComputeStatusOk;
}

ComputeStatus D3D11::writeTextureImpl(CommandList InCmdList, uint64_t InSize, uint64_t RowPitch, const void* InData, Resource InTargetResource, Resource& InUploadResource)
{
    ((ID3D11DeviceContext*)InCmdList)->UpdateSubresource((ID3D11Resource*)InTargetResource, 0, nullptr, InData, UINT(RowPitch), UINT(InSize));
    return eComputeStatusOk;
}

ComputeStatus D3D11::clearView(CommandList InCmdList, Resource InResource, const float4 Color, const RECT * pRects, unsigned int NumRects, CLEAR_TYPE &outType)
{
    outType = CLEAR_UNDEFINED;
    
    ID3D11DeviceContext1* context1;
    ((ID3D11DeviceContext*)InCmdList)->QueryInterface(IID_PPV_ARGS(&context1));
    if (context1)
    {
        ResourceDriverDataD3D11 data;
        ComputeStatus status = getSurfaceDriverData(InResource, data);
        if (status == eComputeStatusOk)
        {
            if (!data.bZBCSupported)
            {
                return eComputeStatusNotSupported;
            }
            // dx11 driver may skip the clear (ClearSkip perfstrat) if it decides that this clear is redundant. I didn't yet figure out why
            // it decides that, but calling DiscardView() prior to ClearView() disables this behaviour and works around the bug 200666776
            context1->DiscardView((ID3D11UnorderedAccessView*)data.UAV);
            context1->ClearView((ID3D11UnorderedAccessView*)data.UAV, (const FLOAT*)&Color, (const RECT*)pRects, NumRects);
        }
        return status;
    }
    return eComputeStatusError;
}

ComputeStatus D3D11::insertGPUBarrierList(CommandList InCmdList, const Resource* InResources, unsigned int InResourceCount, BarrierType InBarrierType)
{
    // Nothing to do here in d3d11
    return eComputeStatusOk;
}

ComputeStatus D3D11::insertGPUBarrier(CommandList InCmdList, Resource InResource, BarrierType InBarrierType)
{
    // Nothing to do here in d3d11
    return eComputeStatusOk;
}

ComputeStatus D3D11::transitionResourceImpl(CommandList cmdList, const ResourceTransition *transitions, uint32_t count)
{
    if (!cmdList || !transitions)
    {
        return eComputeStatusInvalidArgument;
    }
    // Nothing to do here in d3d11
    return eComputeStatusOk;
}

ComputeStatus D3D11::copyResource(CommandList cmdList, Resource dstResource, Resource srcResource)
{
    if (!cmdList || !dstResource || !srcResource) return eComputeStatusInvalidArgument;
    auto context = (ID3D11DeviceContext*)cmdList;
    context->CopyResource((ID3D11Resource*)dstResource, (ID3D11Resource*)srcResource);
    return eComputeStatusOk;
}

ComputeStatus D3D11::cloneResource(Resource resource, Resource &clone, const char friendlyName[], ResourceState initialState, unsigned int creationMask, unsigned int visibilityMask)
{
    if (!resource) return eComputeStatusInvalidArgument;

    ID3D11Resource* res = nullptr;
    HRESULT hr = S_OK;
    ResourceDescription desc;
    if (getResourceDescription(resource, desc) != eComputeStatusOk)
    {
        return eComputeStatusError;
    }
    if (desc.flags & (ResourceFlags::eRawOrStructuredBuffer | ResourceFlags::eConstantBuffer))
    {
        auto buffer = (ID3D11Buffer*)resource;
        D3D11_BUFFER_DESC desc1;
        buffer->GetDesc(&desc1);
        if (m_allocateCallback)
        {
            ResourceDesc desc = { ResourceType::eResourceTypeBuffer, &desc1, (uint32_t)initialState, nullptr };
            auto result = m_allocateCallback(&desc);
            res = (ID3D11Resource*)result.native;
        }
        else
        {
            hr = m_device->CreateBuffer(&desc1, nullptr, &buffer);
        }
        res = buffer;
    }
    else
    {
        auto tex2d = (ID3D11Texture2D*)resource;
        D3D11_TEXTURE2D_DESC desc1;
        tex2d->GetDesc(&desc1);
        if (m_allocateCallback)
        {
            ResourceDesc desc = { ResourceType::eResourceTypeTex2d, &desc1, (uint32_t)initialState, nullptr };
            auto result = m_allocateCallback(&desc);
            res = (ID3D11Resource*)result.native;
        }
        else
        {
            hr = m_device->CreateTexture2D(&desc1, nullptr, &tex2d);
        }
        res = tex2d;
    }
    
    if (hr != S_OK || !res)
    {
        SL_LOG_ERROR("Unable to clone resource");
        return eComputeStatusError;
    }

    auto currentSize = getResourceSize(res);

    SL_LOG_VERBOSE("Cloning 0x%llx (%s:%u:%u:%s), m_allocCount=%d, currentSize %.1lf MB, totalSize %.1lf MB", res, friendlyName, desc.width, desc.height, getDXGIFormatStr(desc.format), m_allocCount.load(), (double)currentSize / (1024 * 1024), (double)m_totalAllocatedSize.load() / (1024 * 1024));

    clone = res;

    return eComputeStatusOk;
}

ComputeStatus D3D11::copyBufferToReadbackBuffer(CommandList InCmdList, Resource InResource, Resource OutResource, unsigned int InBytesToCopy) 
{
    ID3D11Resource *InD3dResource = (ID3D11Resource*)InResource;
    ID3D11Resource *OutD3dResource = (ID3D11Resource*)OutResource;
    
    ID3D11DeviceContext* DeviceContext = reinterpret_cast<ID3D11DeviceContext*>(InCmdList);
    ID3D11Resource* ReadbackResource = reinterpret_cast<ID3D11Resource*>(OutResource);

    D3D11_BOX SrcBox = { 0, 0, 0, InBytesToCopy, 1, 1 };
    DeviceContext->CopySubresourceRegion(ReadbackResource, 0, 0, 0, 0, (ID3D11Resource*)InResource, 0, &SrcBox);

    return eComputeStatusOk;
}

ComputeStatus D3D11::getResourceDescription(Resource resource, ResourceDescription &outDesc)
{
    if (!resource) return eComputeStatusInvalidArgument;

    // First make sure this is not an DXGI or some other resource
    auto unknown = (IUnknown*)resource;
    ID3D11Resource* pageable;
    unknown->QueryInterface(&pageable);
    if (!pageable)
    {
        return eComputeStatusError;
    }

    D3D11_RESOURCE_DIMENSION dim;
    pageable->GetType(&dim);

    outDesc = {};
    outDesc.format = eFormatINVALID;

    ID3D11Texture2D* tex2d = nullptr;
    ID3D11Buffer* buffer = nullptr;
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
    {
        tex2d = (ID3D11Texture2D*)resource;
        D3D11_TEXTURE2D_DESC desc;
        tex2d->GetDesc(&desc);

        if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
        {
            outDesc.flags |= ResourceFlags::eShaderResourceStorage;
        }

        outDesc.width = (UINT)desc.Width;
        outDesc.height = desc.Height;
        outDesc.nativeFormat = desc.Format;
        outDesc.mips = desc.MipLevels;
        outDesc.flags |= ResourceFlags::eShaderResource;
    }
    else if (dim == D3D11_RESOURCE_DIMENSION_BUFFER)
    {
        buffer = (ID3D11Buffer*)resource;
        D3D11_BUFFER_DESC desc;
        buffer->GetDesc(&desc);

        if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
        {
            outDesc.flags |= ResourceFlags::eShaderResourceStorage;
        }

        outDesc.width = (UINT)desc.ByteWidth;
        outDesc.height = 1;
        outDesc.nativeFormat = DXGI_FORMAT_UNKNOWN;
        outDesc.flags |= ResourceFlags::eRawOrStructuredBuffer | ResourceFlags::eConstantBuffer;
    }
    else
    {
        SL_LOG_ERROR("Unknown resource");
    }

    pageable->Release();

    return eComputeStatusOk;
}

ComputeStatus D3D11::getLUIDFromDevice(NVSDK_NGX_LUID *OutId)
{
    return eComputeStatusError;
}

ComputeStatus D3D11::beginPerfSection(CommandList cmdList, const char *key, unsigned int node, bool reset)
{
#ifndef SL_PRODUCTION
    PerfData* data = {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto section = m_sectionPerfMap[node].find(key);
        if (section == m_sectionPerfMap[node].end())
        {
            m_sectionPerfMap[node][key] = {};
            section = m_sectionPerfMap[node].find(key);
        }
        data = &(*section).second;
    }
    
    if (reset)
    {
        data->values.clear();
    }
    
    if (!data->queryBegin)
    {
        D3D11_QUERY_DESC timestamp_query_desc;
        timestamp_query_desc.Query = D3D11_QUERY_TIMESTAMP;
        timestamp_query_desc.MiscFlags = 0;

        D3D11_QUERY_DESC timestamp_disjoint_query_desc;
        timestamp_disjoint_query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        timestamp_disjoint_query_desc.MiscFlags = 0;

        m_device->CreateQuery(&timestamp_query_desc, &data->queryBegin);
        m_device->CreateQuery(&timestamp_query_desc, &data->queryEnd);
        m_device->CreateQuery(&timestamp_disjoint_query_desc, &data->queryDisjoint);
    }
    
    auto context = (ID3D11DeviceContext*)cmdList;
    context->Begin(data->queryDisjoint);
    context->End(data->queryBegin);
#endif
    return eComputeStatusOk;
}

ComputeStatus D3D11::endPerfSection(CommandList cmdList, const char* key, float &avgTimeMS, unsigned int node)
{
#ifndef SL_PRODUCTION
    PerfData* data = {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto section = m_sectionPerfMap[node].find(key);
        if (section == m_sectionPerfMap[node].end())
        {
            return eComputeStatusError;
        }
        data = &(*section).second;
    }
    auto context = (ID3D11DeviceContext*)cmdList;
    context->End(data->queryEnd);
    context->End(data->queryDisjoint);

    UINT64 beginTimeStamp = 0, endTimeStamp = 0;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampData = { 0 };

    while (context->GetData(data->queryDisjoint, &timestampData, sizeof(timestampData), 0))
    {
        Sleep(0);
    }

    while (context->GetData(data->queryBegin, &beginTimeStamp, sizeof(beginTimeStamp), 0) == S_FALSE)
    {
        Sleep(0);
    }

    while (context->GetData(data->queryEnd, &endTimeStamp, sizeof(endTimeStamp), 0) == S_FALSE)
    {
        Sleep(0);
    }

    if (!timestampData.Disjoint)
    {
        float delta = (float)((endTimeStamp - beginTimeStamp) / (double)timestampData.Frequency * 1000);
        // Average over last N executions
        if (data->values.size() == 100)
        {
            data->accumulatedTimeMS -= data->values.front();
            data->values.erase(data->values.begin());
        }
        data->accumulatedTimeMS += delta;
        data->values.push_back(delta);
    }

    avgTimeMS = data->values.size() ? data->accumulatedTimeMS / (float)data->values.size() : 0;
#else
    avgTimeMS = 0;
#endif
    return eComputeStatusOk;
}

#if SL_ENABLE_PERF_TIMING
ComputeStatus D3D11::beginProfiling(CommandList cmdList, unsigned int Metadata, const void *pData, unsigned int Size)
{
    return eComputeStatusError;
}

ComputeStatus D3D11::endProfiling(CommandList cmdList)
{
    return eComputeStatusError;
}
#endif

ComputeStatus D3D11::dumpResource(CommandList cmdList, Resource src, const char *path)
{
    return eComputeStatusError;
}

void D3D11::destroyResourceDeferredImpl(const Resource resource)
{   
    auto unknown = (IUnknown*)resource;
    ID3D11Resource* pageable;
    unknown->QueryInterface(&pageable);
    uint64_t currentSize = 0;
    if (pageable)
    {
        pageable->Release();
        if (m_allocCount && m_totalAllocatedSize)
        {
            m_allocCount--;
            currentSize = getResourceSize(resource);
            if (m_totalAllocatedSize >= currentSize)
            {
                m_totalAllocatedSize -= currentSize;
            }
        }
    }
    auto name = getDebugName((ID3D11Resource*)resource);
    auto ref = ((IUnknown*)resource)->Release();
    SL_LOG_VERBOSE("Releasing resource 0x%llx (%S) ref count %u - currentSize %.2f - totalSize %.2f", resource, name.c_str(), ref, currentSize / (1024.0f * 1024.0f), m_totalAllocatedSize / (1024.0f * 1024.0f));
}

DXGI_FORMAT D3D11::getCorrectFormat(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_D16_UNORM: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules);
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_D32_FLOAT: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules); // fall through
    case DXGI_FORMAT_R32_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R32G32_TYPELESS:
        return DXGI_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R16G16_TYPELESS:
        return DXGI_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16_TYPELESS:
        return DXGI_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules); // fall through
    case DXGI_FORMAT_R24G8_TYPELESS:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules); // fall through
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:
        return Format;
    };
}


}
}