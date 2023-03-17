#include <map>
#include <string>

#include "external/ngx/Include/nvsdk_ngx.h"
#include "external/ngx/Include/nvsdk_ngx_helpers.h"
#include "external/ngx/Include/nvsdk_ngx_defs.h"
#include "ngxParameters.h"

//! Should never ship NGX parameter implementation in any SL modules, always get it from NGX Core

#ifndef SL_PRODUCTION

struct NVSDK_NGX_Buffer
{
    void *GPUAllocation;
    unsigned long long SizeInBytes;
    unsigned int TileWidth;
    unsigned int TileHeight;
    unsigned int TileCount;
    NVSDK_NGX_Buffer_Format Format;
};

enum VariableType
{
    None,
    ULLong,
    Int,
    UInt,
    Float,
    Double,
    GPUAllocation,
    Void
};

struct NVSDK_NGX_Var
{   
    VariableType Type = None;
    bool Persistent = false;
    union
    {
        unsigned long long ULLong;
        int Int;
        unsigned int UInt;
        float Float;
        double Double;
        void *GPUAllocation = nullptr;
    } Value;

    void operator=(unsigned long long v) { Value.ULLong = v; }
    void operator=(unsigned int v) { Value.UInt = v; }
    void operator=(int v) { Value.Int = v; }
    void operator=(float v) { Value.Float = v; }
    void operator=(double v) { Value.Double = v; }
    void operator=(void* v) { Value.GPUAllocation = v; }

    operator unsigned long long() const { return Value.ULLong; }
    operator unsigned int() const { return Value.UInt; }
    operator int() const { return Value.Int; }
    operator float() const { return Value.Float; }
    operator double() const { return Value.Double; }
    operator void*() const { return Value.GPUAllocation; }
    operator ID3D11Resource*() const { return (ID3D11Resource*)Value.GPUAllocation; }
    operator ID3D12Resource*() const { return (ID3D12Resource*)Value.GPUAllocation; }

    inline bool IsValid() const { return Type != None; }
};

#define NVSDK_NGX_NUM_PREDEFINED_PARAMS 68

struct NVSDK_NGX_Parameter_Imp : public NVSDK_NGX_Parameter
{
    NVSDK_NGX_Var *FindVar(const char *InName)
    {
        if (!InName) return nullptr;
        NVSDK_NGX_Var *Var = nullptr;
        if (*InName == '#')
        {
            // Enum style predefined parameter (up to 65536 predefined parameters)
            // First 256 predefined parameters {#,LO,0} then {#,LO,HI,0}
            unsigned short Idx = *((const unsigned short*)(InName + 1));
            if (Idx < NVSDK_NGX_NUM_PREDEFINED_PARAMS)
            {
                Var = &m_PredefinedParams[Idx];
            }            
        }
        else
        {
            // Human readable string
            size_t h = m_Hash(InName);
            // Does it match any of our predefined params?
            auto idx = m_HashToIdx.find(h);
            if (idx != m_HashToIdx.end())
            {
                Var = &m_PredefinedParams[(*idx).second];
            }
            else
            {
                // Nope, dynamic parameter, insert empty if not duplicate
                if (m_Values.find(h) == m_Values.end())
                {
                    NVSDK_NGX_Var v = {};
                    m_Values[h] = v;
                }                
                // Our generic hash map is not returning iterator like it should
                auto it = m_Values.find(h);
                Var = &(*it).second;
                //LOGV("### %s", InName);
            }
        }
        return Var;
    }

    const NVSDK_NGX_Var *FindVar(const char *InName) const
    {
        return const_cast<NVSDK_NGX_Parameter_Imp *>(this)->FindVar(InName);
    }

    template<typename T, VariableType VT> void SetT(const char * InName, T InValue)
    {
        NVSDK_NGX_Var *Var = FindVar(InName);
        if (Var)
        {
            Var->Type = VT;
            // Don't reset to false if it was true since it might have been cached like that
            if (!Var->Persistent)
            {
                Var->Persistent = m_NewVarsPersistent;
            }            
            *Var = InValue;
        }        
    }

    inline void Set(const char * InName, double InValue)
    {
        SetT<double,Double>(InName, InValue);        
    }
    inline void Set(const char * InName, float InValue)
    {
        SetT<float,Float>(InName, InValue);
    }
    inline void Set(const char * InName, unsigned int InValue)
    {
        SetT<unsigned int,UInt>(InName, InValue);
    }
    inline void Set(const char * InName, unsigned long long InValue)
    {
        SetT<unsigned long long,ULLong>(InName, InValue);
    }
    inline void Set(const char * InName, int InValue)
    {
        SetT<int,Int>(InName, InValue);
    }

    template<typename T> NVSDK_NGX_Result GetT(const char * InName, T *OutValue) const
    {
        const NVSDK_NGX_Var *Var = FindVar(InName);
        if (Var && Var->IsValid() && Var->Type != Void && Var->Type != GPUAllocation)
        {            
            switch (Var->Type)
            {
                case Float: *OutValue = (T)Var->operator float(); break;
                case Double: *OutValue = (T)Var->operator double(); break;
                case Int: *OutValue = (T)Var->operator int(); break;
                case ULLong: *OutValue = (T)Var->operator unsigned long long(); break;
                case UInt: *OutValue = (T)Var->operator unsigned int(); break;
                default: return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
            };            
            return NVSDK_NGX_Result_Success;
        }        
        return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
    }

    template<typename T> NVSDK_NGX_Result GetPT(const char * InName, T *OutValue) const
    {
        const NVSDK_NGX_Var *Var = FindVar(InName);
        if (Var && Var->IsValid() && (Var->Type == Void || Var->Type == GPUAllocation))
        {
            *OutValue = (T)Var->operator void *();
            return NVSDK_NGX_Result_Success;
        }
        return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
    }

    inline NVSDK_NGX_Result Get(const char * InName, unsigned long long *OutValue) const
    {
        return GetT<unsigned long long>(InName, OutValue);
    }
    inline NVSDK_NGX_Result Get(const char * InName, float *OutValue) const
    {
        return GetT<float>(InName, OutValue);
    }
    inline NVSDK_NGX_Result Get(const char * InName, double *OutValue) const
    {
        return GetT<double>(InName, OutValue);
    }
    inline NVSDK_NGX_Result Get(const char * InName, unsigned int *OutValue) const
    {
        return GetT<unsigned int>(InName, OutValue);
    }
    inline NVSDK_NGX_Result Get(const char * InName, int *OutValue) const
    {
        return GetT<int>(InName, OutValue);
    }
    
    inline void Reset()
    {        
    }    
    
    inline void SetNewParamsPersistent(bool InValue) { m_NewVarsPersistent = InValue; }

    NVSDK_NGX_Parameter_Imp();

    virtual NVSDK_NGX_Parameter_Imp *Clone() const = 0;

 protected:

    std::map<size_t, NVSDK_NGX_Var> m_Values;
    NVSDK_NGX_Var m_InvalidVar;
    std::hash<std::string> m_Hash;
    bool m_NewVarsPersistent = false;
  std::map<size_t, unsigned int> m_HashToIdx;
    NVSDK_NGX_Var m_PredefinedParams[NVSDK_NGX_NUM_PREDEFINED_PARAMS];
};


struct NVSDK_NGX_Parameter_D3D11 : public NVSDK_NGX_Parameter_Imp
{
    NVSDK_NGX_Parameter_Imp *Clone() const
    {
        NVSDK_NGX_Parameter_D3D11 *Params = new NVSDK_NGX_Parameter_D3D11();
        Params->m_Values = m_Values;
        memcpy(Params->m_PredefinedParams, m_PredefinedParams, sizeof(NVSDK_NGX_Var) * NVSDK_NGX_NUM_PREDEFINED_PARAMS);
        return Params;
    }

    inline void Set(const char * InName, ID3D11Resource *InValue)
    {
        SetT<ID3D11Resource*,GPUAllocation>(InName, InValue);
    }
    inline void Set(const char * InName, ID3D12Resource *InValue)
    {

    }
    inline void Set(const char * InName, void *InValue)
    {
        SetT<void*,Void>(InName, InValue);
    }
    inline NVSDK_NGX_Result Get(const char * InName, ID3D11Resource **OutValue) const
    {
        return GetPT<ID3D11Resource*>(InName, OutValue);
    }
    inline NVSDK_NGX_Result Get(const char * InName, ID3D12Resource **OutValue) const
    {        
        return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
    }
    inline NVSDK_NGX_Result Get(const char * InName, void **OutValue) const
    {
        return GetPT<void*>(InName, OutValue);
    }
};

struct NVSDK_NGX_Parameter_D3D12 : public NVSDK_NGX_Parameter_Imp
{
    NVSDK_NGX_Parameter_Imp *Clone() const
    {
        NVSDK_NGX_Parameter_D3D12 *Params = new NVSDK_NGX_Parameter_D3D12();
        Params->m_Values = m_Values;
        memcpy(Params->m_PredefinedParams, m_PredefinedParams, sizeof(NVSDK_NGX_Var) * NVSDK_NGX_NUM_PREDEFINED_PARAMS);
        return Params;
    }

    inline void Set(const char * InName, ID3D12Resource *InValue)
    {
        SetT<ID3D12Resource*, GPUAllocation>(InName, InValue);
    }
    inline void Set(const char * InName, ID3D11Resource *InValue)
    {
        
    }
    inline void Set(const char * InName, void *InValue)
    {
        SetT<void*,Void>(InName, InValue);
    }

    inline NVSDK_NGX_Result Get(const char * InName, ID3D12Resource **OutValue) const
    {
        return GetPT<ID3D12Resource*>(InName, OutValue);
    }
    inline NVSDK_NGX_Result Get(const char * InName, ID3D11Resource **OutValue) const
    {
        return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
    }
    inline NVSDK_NGX_Result Get(const char * InName, void **OutValue) const
    {        
        return GetPT<void*>(InName, OutValue);
    }
};

struct NVSDK_NGX_Parameter_CUDA : public NVSDK_NGX_Parameter_Imp
{
    NVSDK_NGX_Parameter_Imp *Clone() const
    {
        NVSDK_NGX_Parameter_CUDA *Params = new NVSDK_NGX_Parameter_CUDA();
        Params->m_Values = m_Values;
        memcpy(Params->m_PredefinedParams, m_PredefinedParams, sizeof(NVSDK_NGX_Var) * NVSDK_NGX_NUM_PREDEFINED_PARAMS);
        return Params;
    }

    inline void Set(const char * InName, void *InValue)
    {
        SetT<void*,Void>(InName, InValue);
    }
    inline void Set(const char * InName, ID3D12Resource *InValue)
    {

    }
    inline void Set(const char * InName, ID3D11Resource *InValue)
    {

    }
    inline NVSDK_NGX_Result Get(const char * InName, ID3D11Resource **OutValue) const
    {
        return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
    }
    inline NVSDK_NGX_Result Get(const char * InName, ID3D12Resource **OutValue) const
    {
        return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
    }
    inline NVSDK_NGX_Result Get(const char * InName, void **OutValue) const
    {
        return GetPT<void*>(InName, OutValue);
    }
};

struct NVSDK_NGX_Parameter_VULKAN : public NVSDK_NGX_Parameter_Imp
{
  NVSDK_NGX_Parameter_Imp *Clone() const
  {
    NVSDK_NGX_Parameter_VULKAN *Params = new NVSDK_NGX_Parameter_VULKAN();
    Params->m_Values = m_Values;
    memcpy(Params->m_PredefinedParams, m_PredefinedParams, sizeof(NVSDK_NGX_Var) * NVSDK_NGX_NUM_PREDEFINED_PARAMS);
    return Params;
  }

  inline void Set(const char * InName, void *InValue)
  {
    SetT<void*, Void>(InName, InValue);
  }
  inline void Set(const char * InName, ID3D12Resource *InValue)
  {

  }
  inline void Set(const char * InName, ID3D11Resource *InValue)
  {

  }
  inline NVSDK_NGX_Result Get(const char * InName, ID3D11Resource **OutValue) const
  {
    return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
  }
  inline NVSDK_NGX_Result Get(const char * InName, ID3D12Resource **OutValue) const
  {
    return NVSDK_NGX_Result_FAIL_UnsupportedParameter;
  }
  inline NVSDK_NGX_Result Get(const char * InName, void **OutValue) const
  {
    return GetPT<void*>(InName, OutValue);
  }
};

const char *PredefinedParams[NVSDK_NGX_NUM_PREDEFINED_PARAMS] =
{
    "Denoiser.Available", // NVSDK_NGX_Parameter_Denoiser_Available
    "SuperSampling.Available", // NVSDK_NGX_Parameter_SuperSampling_Available
    "InPainting.Available", // NVSDK_NGX_Parameter_InPainting_Available
    "ImageSuperResolution.Available", // NVSDK_NGX_Parameter_ImageSuperResolution_Available
    "SlowMotion.Available", // NVSDK_NGX_Parameter_SlowMotion_Available
    "VideoSuperResolution.Available", // NVSDK_NGX_Parameter_VideoSuperResolution_Available
    "Colorize.Available", // NVSDK_NGX_Parameter_Colorize_Available
    "StyleTransfer.Available", // NVSDK_NGX_Parameter_StyleTransfer_Available
    "VideoDenoiser.Available", // NVSDK_NGX_Parameter_VideoDenoiser_Available
    "ImageSignalProcessing.Available", // NVSDK_NGX_Parameter_ImageSignalProcessing_Available    
    "ImageSuperResolution.ScaleFactor.2.1", // NVSDK_NGX_Parameter_ImageSuperResolution_ScaleFactor_2_1
    "ImageSuperResolution.ScaleFactor.3.1", // NVSDK_NGX_Parameter_ImageSuperResolution_ScaleFactor_3_1
    "ImageSuperResolution.ScaleFactor.3.2", // NVSDK_NGX_Parameter_ImageSuperResolution_ScaleFactor_3_2
    "ImageSuperResolution.ScaleFactor.4.3", // NVSDK_NGX_Parameter_ImageSuperResolution_ScaleFactor_4_3
    "NumFrames", // NVSDK_NGX_Parameter_NumFrames
    "Scale", // NVSDK_NGX_Parameter_Scale
    "Width", // NVSDK_NGX_Parameter_Width
    "Height", // NVSDK_NGX_Parameter_Height
    "OutWidth", // NVSDK_NGX_Parameter_OutWidth
    "OutHeight", // NVSDK_NGX_Parameter_OutHeight
    "Sharpness", // NVSDK_NGX_Parameter_Sharpness
    "Scratch", // NVSDK_NGX_Parameter_Scratch
    "Scratch.SizeInBytes", // NVSDK_NGX_Parameter_Scratch_SizeInBytes
    "Hint.HDR", // NVSDK_NGX_Parameter_Hint_HDR
    "Input1", // NVSDK_NGX_Parameter_Input1
    "Input1.Format", // NVSDK_NGX_Parameter_Input1_Format
    "Input1.SizeInBytes", // NVSDK_NGX_Parameter_Input1_SizeInBytes
    "Input2", // NVSDK_NGX_Parameter_Input2
    "Input2.Format", // NVSDK_NGX_Parameter_Input2_Format
    "Input2.SizeInBytes", // NVSDK_NGX_Parameter_Input2_SizeInBytes
    "Color", // NVSDK_NGX_Parameter_Color
    "Color.Format", // NVSDK_NGX_Parameter_Color_Format
    "Color.SizeInBytes", // NVSDK_NGX_Parameter_Color_SizeInBytes
    "Albedo", // NVSDK_NGX_Parameter_Albedo
    "Output", // NVSDK_NGX_Parameter_Output
    "Output.Format", // NVSDK_NGX_Parameter_Output_Format
    "Output.SizeInBytes", // NVSDK_NGX_Parameter_Output_SizeInBytes
    "Reset", // NVSDK_NGX_Parameter_Reset
    "BlendFactor", // NVSDK_NGX_Parameter_BlendFactor
    "MotionVectors", // NVSDK_NGX_Parameter_MotionVectors
    "Rect.X", // NVSDK_NGX_Parameter_Rect_X
    "Rect.Y", // NVSDK_NGX_Parameter_Rect_Y
    "Rect.W", // NVSDK_NGX_Parameter_Rect_W
    "Rect.H", // NVSDK_NGX_Parameter_Rect_H
    "MV.Scale.X", // NVSDK_NGX_Parameter_MV_Scale_X
    "MV.Scale.Y", // NVSDK_NGX_Parameter_MV_Scale_Y
    "Model", // NVSDK_NGX_Parameter_Model
    "Format", // NVSDK_NGX_Parameter_Format
    "SizeInBytes", // NVSDK_NGX_Parameter_SizeInBytes
    "ResourceAllocCallback", // NVSDK_NGX_Parameter_ResourceAllocCallback
    "BufferAllocCallback", // NVSDK_NGX_Parameter_BufferAllocCallback
    "Tex2DAllocCallback", // NVSDK_NGX_Parameter_Tex2DAllocCallback
    "ResourceReleaseCallback", // NVSDK_NGX_Parameter_ResourceReleaseCallback
    "CreationNodeMask", // NVSDK_NGX_Parameter_CreationNodeMask
    "VisibilityNodeMask", // NVSDK_NGX_Parameter_VisibilityNodeMask
    "PreviousOutput", // NVSDK_NGX_Parameter_PreviousOutput'
    "MV.Offset.X", // NVSDK_NGX_Parameter_MV_Offset_X
    "MV.Offset.Y", // NVSDK_NGX_Parameter_MV_Offset_Y 
    "Hint.UseFireflySwatter", // NVSDK_NGX_Parameter_Hint_UseFireflySwatter "Hint.UseFireflySwatter"
    "ResourceWidth", //NVSDK_NGX_Parameter_Resource_Width
    "ResourceHeight", //NVSDK_NGX_Parameter_Resource_Height
    "Depth", //NVSDK_NGX_Parameter_Resource_Depth
    "DLSSOptimalSettingsCallback", //NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback
    "PerfQualityValue", // NVSDK_NGX_Parameter_PerfQualityValue
    "RTXValue", // NVSDK_NGX_Parameter_RTXValue
    "DLSSMode", // NVSDK_NGX_Parameter_DLSSMode    
    "DeepResolve.Available", // NVSDK_NGX_Parameter_DeepResolve_Available
    "DepthInverted", // NVSDK_NGX_Parameter_DepthInverted

    // IMPORTANT: NEW PARAMETERS MUST GO HERE
};

NVSDK_NGX_Parameter_Imp::NVSDK_NGX_Parameter_Imp()
{
    for (unsigned int i = 0; i < NVSDK_NGX_NUM_PREDEFINED_PARAMS; i++)
    {
        m_HashToIdx[m_Hash((const char*)PredefinedParams[i])] = i;
    }
}

NVSDK_NGX_Parameter *getNGXParameters()
{
    static auto params = new NVSDK_NGX_Parameter_D3D12();
    return params;
} 

#endif