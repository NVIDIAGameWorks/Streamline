/*-------------------------------------------------------------------------------------
 *
 * Copyright (c) Microsoft Corporation
 *
 *-------------------------------------------------------------------------------------*/


/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0628 */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __directsr_h__
#define __directsr_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef DECLSPEC_XFGVIRT
#if defined(_CONTROL_FLOW_GUARD_XFG)
#define DECLSPEC_XFGVIRT(base, func) __declspec(xfg_virtual(base, func))
#else
#define DECLSPEC_XFGVIRT(base, func)
#endif
#endif

/* Forward Declarations */ 

#ifndef __IDSRDevice_FWD_DEFINED__
#define __IDSRDevice_FWD_DEFINED__
typedef interface IDSRDevice IDSRDevice;

#endif 	/* __IDSRDevice_FWD_DEFINED__ */


#ifndef __IDSRSuperResUpscaler_FWD_DEFINED__
#define __IDSRSuperResUpscaler_FWD_DEFINED__
typedef interface IDSRSuperResUpscaler IDSRSuperResUpscaler;

#endif 	/* __IDSRSuperResUpscaler_FWD_DEFINED__ */


#ifndef __IDSRSuperResEngine_FWD_DEFINED__
#define __IDSRSuperResEngine_FWD_DEFINED__
typedef interface IDSRSuperResEngine IDSRSuperResEngine;

#endif 	/* __IDSRSuperResEngine_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "d3d12.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_directsr_0000_0000 */
/* [local] */ 

#include <winapifamily.h>
#pragma region App Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_GAMES)
typedef 
enum DSR_SUPERRES_VARIANT_FLAGS
    {
        DSR_SUPERRES_VARIANT_FLAG_NONE	= 0,
        DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_EXPOSURE_SCALE_TEXTURE	= 0x1,
        DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_IGNORE_HISTORY_MASK	= 0x2,
        DSR_SUPERRES_VARIANT_FLAG_NATIVE	= 0x4,
        DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_REACTIVE_MASK	= 0x8,
        DSR_SUPERRES_VARIANT_FLAG_SUPPORTS_SHARPNESS	= 0x10,
        DSR_SUPERRES_VARIANT_FLAG_DISALLOWS_REGION_OFFSETS	= 0x20
    } 	DSR_SUPERRES_VARIANT_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS( DSR_SUPERRES_VARIANT_FLAGS )
typedef 
enum DSR_OPTIMIZATION_TYPE
    {
        DSR_OPTIMIZATION_TYPE_BALANCED	= 0,
        DSR_OPTIMIZATION_TYPE_HIGH_QUALITY	= ( DSR_OPTIMIZATION_TYPE_BALANCED + 1 ) ,
        DSR_OPTIMIZATION_TYPE_MAX_QUALITY	= ( DSR_OPTIMIZATION_TYPE_HIGH_QUALITY + 1 ) ,
        DSR_OPTIMIZATION_TYPE_HIGH_PERFORMANCE	= ( DSR_OPTIMIZATION_TYPE_MAX_QUALITY + 1 ) ,
        DSR_OPTIMIZATION_TYPE_MAX_PERFORMANCE	= ( DSR_OPTIMIZATION_TYPE_HIGH_PERFORMANCE + 1 ) ,
        DSR_OPTIMIZATION_TYPE_POWER_SAVING	= ( DSR_OPTIMIZATION_TYPE_MAX_PERFORMANCE + 1 ) ,
        DSR_OPTIMIZATION_TYPE_MAX_POWER_SAVING	= ( DSR_OPTIMIZATION_TYPE_POWER_SAVING + 1 ) ,
        DSR_NUM_OPTIMIZATION_TYPES	= ( DSR_OPTIMIZATION_TYPE_MAX_POWER_SAVING + 1 ) 
    } 	DSR_OPTIMIZATION_TYPE;

typedef 
enum DSR_SUPERRES_CREATE_ENGINE_FLAGS
    {
        DSR_SUPERRES_CREATE_ENGINE_FLAG_NONE	= 0,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_MOTION_VECTORS_USE_TARGET_DIMENSIONS	= 0x1,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_AUTO_EXPOSURE	= 0x2,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_DRS	= 0x4,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_MOTION_VECTORS_USE_JITTER_OFFSETS	= 0x8,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_SUBRECT_OUTPUT	= 0x10,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_LINEAR_DEPTH	= 0x20,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_ENABLE_SHARPENING	= 0x40,
        DSR_SUPERRES_CREATE_ENGINE_FLAG_FORCE_LDR_COLORS	= 0x80
    } 	DSR_SUPERRES_CREATE_ENGINE_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS( DSR_SUPERRES_CREATE_ENGINE_FLAGS )
typedef 
enum DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS
    {
        DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_NONE	= 0,
        DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_RESET_HISTORY	= 0x1,
        DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS_VALID_MASK	= 0x1
    } 	DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS;

DEFINE_ENUM_FLAG_OPERATORS( DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS )
typedef struct DSR_FLOAT2
    {
    float X;
    float Y;
    } 	DSR_FLOAT2;

typedef struct DSR_SIZE
    {
    UINT Width;
    UINT Height;
    } 	DSR_SIZE;

typedef struct DSR_SUPERRES_SOURCE_SETTINGS
    {
    DSR_SIZE OptimalSize;
    DSR_SIZE MinDynamicSize;
    DSR_SIZE MaxDynamicSize;
    DXGI_FORMAT OptimalColorFormat;
    DXGI_FORMAT OptimalDepthFormat;
    } 	DSR_SUPERRES_SOURCE_SETTINGS;

typedef struct DSR_SUPERRES_CREATE_ENGINE_PARAMETERS
    {
    GUID VariantId;
    DXGI_FORMAT TargetFormat;
    DXGI_FORMAT SourceColorFormat;
    DXGI_FORMAT SourceDepthFormat;
    DXGI_FORMAT ExposureScaleFormat;
    DSR_SUPERRES_CREATE_ENGINE_FLAGS Flags;
    DSR_SIZE MaxSourceSize;
    DSR_SIZE TargetSize;
    } 	DSR_SUPERRES_CREATE_ENGINE_PARAMETERS;

typedef struct DSR_SUPERRES_VARIANT_DESC
    {
    GUID VariantId;
    CHAR VariantName[ 128 ];
    DSR_SUPERRES_VARIANT_FLAGS Flags;
    DSR_OPTIMIZATION_TYPE OptimizationRankings[ 7 ];
    DXGI_FORMAT OptimalTargetFormat;
    } 	DSR_SUPERRES_VARIANT_DESC;

typedef struct DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS
    {
    ID3D12Resource *pTargetTexture;
    D3D12_RECT TargetRegion;
    ID3D12Resource *pSourceColorTexture;
    D3D12_RECT SourceColorRegion;
    ID3D12Resource *pSourceDepthTexture;
    D3D12_RECT SourceDepthRegion;
    ID3D12Resource *pMotionVectorsTexture;
    D3D12_RECT MotionVectorsRegion;
    DSR_FLOAT2 MotionVectorScale;
    DSR_FLOAT2 CameraJitter;
    float ExposureScale;
    float PreExposure;
    float Sharpness;
    float CameraNear;
    float CameraFar;
    float CameraFovAngleVert;
    ID3D12Resource *pExposureScaleTexture;
    ID3D12Resource *pIgnoreHistoryMaskTexture;
    D3D12_RECT IgnoreHistoryMaskRegion;
    ID3D12Resource *pReactiveMaskTexture;
    D3D12_RECT ReactiveMaskRegion;
    } 	DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS;



extern RPC_IF_HANDLE __MIDL_itf_directsr_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_directsr_0000_0000_v0_0_s_ifspec;

#ifndef __IDSRDevice_INTERFACE_DEFINED__
#define __IDSRDevice_INTERFACE_DEFINED__

/* interface IDSRDevice */
/* [unique][local][object][uuid] */ 


EXTERN_C const IID IID_IDSRDevice;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("994659a7-31ad-4912-9414-159f16630306")
    IDSRDevice : public IUnknown
    {
    public:
        virtual UINT STDMETHODCALLTYPE GetNumSuperResVariants( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSuperResVariantDesc( 
            UINT VariantIndex,
            _Out_  DSR_SUPERRES_VARIANT_DESC *pVariantDesc) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE QuerySuperResSourceSettings( 
            UINT VariantIndex,
            DSR_SIZE TargetSize,
            DXGI_FORMAT TargetFormat,
            DSR_OPTIMIZATION_TYPE OptimizationType,
            DSR_SUPERRES_CREATE_ENGINE_FLAGS CreateFlags,
            _Out_  DSR_SUPERRES_SOURCE_SETTINGS *pSourceSettings) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateSuperResEngine( 
            _In_  const DSR_SUPERRES_CREATE_ENGINE_PARAMETERS *pCreateParams,
            _In_  REFIID iid,
            _COM_Outptr_opt_  void **ppEngine) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IDSRDeviceVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDSRDevice * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDSRDevice * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDSRDevice * This);
        
        DECLSPEC_XFGVIRT(IDSRDevice, GetNumSuperResVariants)
        UINT ( STDMETHODCALLTYPE *GetNumSuperResVariants )( 
            IDSRDevice * This);
        
        DECLSPEC_XFGVIRT(IDSRDevice, GetSuperResVariantDesc)
        HRESULT ( STDMETHODCALLTYPE *GetSuperResVariantDesc )( 
            IDSRDevice * This,
            UINT VariantIndex,
            _Out_  DSR_SUPERRES_VARIANT_DESC *pVariantDesc);
        
        DECLSPEC_XFGVIRT(IDSRDevice, QuerySuperResSourceSettings)
        HRESULT ( STDMETHODCALLTYPE *QuerySuperResSourceSettings )( 
            IDSRDevice * This,
            UINT VariantIndex,
            DSR_SIZE TargetSize,
            DXGI_FORMAT TargetFormat,
            DSR_OPTIMIZATION_TYPE OptimizationType,
            DSR_SUPERRES_CREATE_ENGINE_FLAGS CreateFlags,
            _Out_  DSR_SUPERRES_SOURCE_SETTINGS *pSourceSettings);
        
        DECLSPEC_XFGVIRT(IDSRDevice, CreateSuperResEngine)
        HRESULT ( STDMETHODCALLTYPE *CreateSuperResEngine )( 
            IDSRDevice * This,
            _In_  const DSR_SUPERRES_CREATE_ENGINE_PARAMETERS *pCreateParams,
            _In_  REFIID iid,
            _COM_Outptr_opt_  void **ppEngine);
        
        END_INTERFACE
    } IDSRDeviceVtbl;

    interface IDSRDevice
    {
        CONST_VTBL struct IDSRDeviceVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IDSRDevice_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IDSRDevice_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IDSRDevice_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IDSRDevice_GetNumSuperResVariants(This)	\
    ( (This)->lpVtbl -> GetNumSuperResVariants(This) ) 

#define IDSRDevice_GetSuperResVariantDesc(This,VariantIndex,pVariantDesc)	\
    ( (This)->lpVtbl -> GetSuperResVariantDesc(This,VariantIndex,pVariantDesc) ) 

#define IDSRDevice_QuerySuperResSourceSettings(This,VariantIndex,TargetSize,TargetFormat,OptimizationType,CreateFlags,pSourceSettings)	\
    ( (This)->lpVtbl -> QuerySuperResSourceSettings(This,VariantIndex,TargetSize,TargetFormat,OptimizationType,CreateFlags,pSourceSettings) ) 

#define IDSRDevice_CreateSuperResEngine(This,pCreateParams,iid,ppEngine)	\
    ( (This)->lpVtbl -> CreateSuperResEngine(This,pCreateParams,iid,ppEngine) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IDSRDevice_INTERFACE_DEFINED__ */


#ifndef __IDSRSuperResUpscaler_INTERFACE_DEFINED__
#define __IDSRSuperResUpscaler_INTERFACE_DEFINED__

/* interface IDSRSuperResUpscaler */
/* [unique][local][object][uuid] */ 


EXTERN_C const IID IID_IDSRSuperResUpscaler;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2adc388c-1b5a-4a87-9377-64822e489c12")
    IDSRSuperResUpscaler : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Execute( 
            _In_  const DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS *pExecuteParams,
            float TimeDeltaInSeconds,
            DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS Flags) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Evict( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE MakeResident( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IDSRSuperResUpscalerVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDSRSuperResUpscaler * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDSRSuperResUpscaler * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDSRSuperResUpscaler * This);
        
        DECLSPEC_XFGVIRT(IDSRSuperResUpscaler, Execute)
        HRESULT ( STDMETHODCALLTYPE *Execute )( 
            IDSRSuperResUpscaler * This,
            _In_  const DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS *pExecuteParams,
            float TimeDeltaInSeconds,
            DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS Flags);
        
        DECLSPEC_XFGVIRT(IDSRSuperResUpscaler, Evict)
        HRESULT ( STDMETHODCALLTYPE *Evict )( 
            IDSRSuperResUpscaler * This);
        
        DECLSPEC_XFGVIRT(IDSRSuperResUpscaler, MakeResident)
        HRESULT ( STDMETHODCALLTYPE *MakeResident )( 
            IDSRSuperResUpscaler * This);
        
        END_INTERFACE
    } IDSRSuperResUpscalerVtbl;

    interface IDSRSuperResUpscaler
    {
        CONST_VTBL struct IDSRSuperResUpscalerVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IDSRSuperResUpscaler_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IDSRSuperResUpscaler_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IDSRSuperResUpscaler_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IDSRSuperResUpscaler_Execute(This,pExecuteParams,TimeDeltaInSeconds,Flags)	\
    ( (This)->lpVtbl -> Execute(This,pExecuteParams,TimeDeltaInSeconds,Flags) ) 

#define IDSRSuperResUpscaler_Evict(This)	\
    ( (This)->lpVtbl -> Evict(This) ) 

#define IDSRSuperResUpscaler_MakeResident(This)	\
    ( (This)->lpVtbl -> MakeResident(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IDSRSuperResUpscaler_INTERFACE_DEFINED__ */


#ifndef __IDSRSuperResEngine_INTERFACE_DEFINED__
#define __IDSRSuperResEngine_INTERFACE_DEFINED__

/* interface IDSRSuperResEngine */
/* [unique][local][object][uuid] */ 


EXTERN_C const IID IID_IDSRSuperResEngine;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4bfd72e2-2767-4800-bcf4-cedc0d07ea5a")
    IDSRSuperResEngine : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetOptimalJitterPattern( 
            DSR_SIZE SourceSize,
            DSR_SIZE TargetSize,
            _Inout_  UINT *pSize,
            _Out_opt_  DSR_FLOAT2 *pPattern) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE CreateUpscaler( 
            _In_  ID3D12CommandQueue *pCommandQueue,
            _In_  REFIID __MIDL__IDSRSuperResEngine0000,
            _COM_Outptr_  void **ppUpscaler) = 0;
        
        virtual IDSRDevice *STDMETHODCALLTYPE GetDevice( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IDSRSuperResEngineVtbl
    {
        BEGIN_INTERFACE
        
        DECLSPEC_XFGVIRT(IUnknown, QueryInterface)
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDSRSuperResEngine * This,
            REFIID riid,
            _COM_Outptr_  void **ppvObject);
        
        DECLSPEC_XFGVIRT(IUnknown, AddRef)
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDSRSuperResEngine * This);
        
        DECLSPEC_XFGVIRT(IUnknown, Release)
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDSRSuperResEngine * This);
        
        DECLSPEC_XFGVIRT(IDSRSuperResEngine, GetOptimalJitterPattern)
        HRESULT ( STDMETHODCALLTYPE *GetOptimalJitterPattern )( 
            IDSRSuperResEngine * This,
            DSR_SIZE SourceSize,
            DSR_SIZE TargetSize,
            _Inout_  UINT *pSize,
            _Out_opt_  DSR_FLOAT2 *pPattern);
        
        DECLSPEC_XFGVIRT(IDSRSuperResEngine, CreateUpscaler)
        HRESULT ( STDMETHODCALLTYPE *CreateUpscaler )( 
            IDSRSuperResEngine * This,
            _In_  ID3D12CommandQueue *pCommandQueue,
            _In_  REFIID __MIDL__IDSRSuperResEngine0000,
            _COM_Outptr_  void **ppUpscaler);
        
        DECLSPEC_XFGVIRT(IDSRSuperResEngine, GetDevice)
        IDSRDevice *( STDMETHODCALLTYPE *GetDevice )( 
            IDSRSuperResEngine * This);
        
        END_INTERFACE
    } IDSRSuperResEngineVtbl;

    interface IDSRSuperResEngine
    {
        CONST_VTBL struct IDSRSuperResEngineVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IDSRSuperResEngine_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IDSRSuperResEngine_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IDSRSuperResEngine_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IDSRSuperResEngine_GetOptimalJitterPattern(This,SourceSize,TargetSize,pSize,pPattern)	\
    ( (This)->lpVtbl -> GetOptimalJitterPattern(This,SourceSize,TargetSize,pSize,pPattern) ) 

#define IDSRSuperResEngine_CreateUpscaler(This,pCommandQueue,__MIDL__IDSRSuperResEngine0000,ppUpscaler)	\
    ( (This)->lpVtbl -> CreateUpscaler(This,pCommandQueue,__MIDL__IDSRSuperResEngine0000,ppUpscaler) ) 

#define IDSRSuperResEngine_GetDevice(This)	\
    ( (This)->lpVtbl -> GetDevice(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IDSRSuperResEngine_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_directsr_0000_0003 */
/* [local] */ 

typedef 
enum DSR_EX_VERSION
    {
        DSR_EX_VERSION_NONE	= 0,
        DSR_EX_VERSION_1_0	= ( DSR_EX_VERSION_NONE + 1 ) 
    } 	DSR_EX_VERSION;

typedef void * DSRExSuperResEngineHandle;
typedef void * DSRExSuperResUpscalerHandle;

typedef HRESULT (WINAPI *FNDSRExGetVersionedFunctionTable)(
    DSR_EX_VERSION Version,
    void *pFunctionTable,
    UINT TableSizeInBytes);

typedef UINT (WINAPI *FNDSRExSuperResGetNumVariants)(
    ID3D12Device *pDevice);

typedef HRESULT (WINAPI *FNDSRExSuperResEnumVariant)(
    UINT VariantIndex,
    ID3D12Device *pDevice,
    struct DSR_SUPERRES_VARIANT_DESC *pVariantDesc);

typedef HRESULT (WINAPI *FNDSRExSuperResQuerySourceSettings)(
    UINT VariantIndex,
    ID3D12Device *pDevice,
    const DSR_SIZE &TargetSize,
    DXGI_FORMAT TargetFormat,
    DSR_OPTIMIZATION_TYPE OptimizationType,
    DSR_SUPERRES_CREATE_ENGINE_FLAGS CreateFlags,
    DSR_SUPERRES_SOURCE_SETTINGS *pSourceSettings);

typedef HRESULT (WINAPI *FNDSRExSuperResCreateEngine)(
    UINT VariantIndex,
    ID3D12Device* pDevice,
    const struct DSR_SUPERRES_CREATE_ENGINE_PARAMETERS* pCreateParams,
    DSRExSuperResEngineHandle* pSREngineHandle);

typedef HRESULT (WINAPI *FNDSRExSuperResDestroyEngine)(DSRExSuperResEngineHandle SREngine);

typedef HRESULT (WINAPI *FNDSRExSuperResCreateUpscaler)(
    DSRExSuperResEngineHandle EngineHandle,
    ID3D12CommandQueue *pCommandQueue,
    DSRExSuperResUpscalerHandle* pSRUpscalerHandle);

typedef HRESULT (WINAPI *FNDSRExSuperResDestroyUpscaler)(DSRExSuperResUpscalerHandle SRUpscaler);

typedef HRESULT (WINAPI *FNDSRExSuperResGetOptimalJitterPattern)(
    DSRExSuperResEngineHandle SREngineHandle,
    const DSR_SIZE &SourceSize,
    const DSR_SIZE &TargetSize,
    UINT *pSize,
    DSR_FLOAT2 *pPattern);

typedef HRESULT (WINAPI *FNDSRExSuperResExecuteUpscaler)(
    DSRExSuperResUpscalerHandle SRUpscalerHandle,
    const DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS *pParams,
    float TimeDeltaInSeconds,
    DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS Flags);

typedef HRESULT (WINAPI *FNDSRExSuperResUpscalerEvict)(
    DSRExSuperResUpscalerHandle SRUpscalerHandle);

typedef HRESULT (WINAPI *FNDSRExSuperResUpscalerMakeResident)(
    DSRExSuperResUpscalerHandle SRUpscalerHandle);

typedef struct DSR_EX_FUNCTION_TABLE_1_0
{
    FNDSRExSuperResGetNumVariants pfnDSRExSuperResGetNumVariants;
    FNDSRExSuperResEnumVariant pfnDSRExSuperResEnumVariant;
    FNDSRExSuperResQuerySourceSettings pfnDSRExSuperResQuerySourceSettings;
    FNDSRExSuperResCreateEngine pfnDSRExSuperResCreateEngine;
    FNDSRExSuperResDestroyEngine pfnDSRExSuperResDestroyEngine;
    FNDSRExSuperResCreateUpscaler pfnDSRExSuperResCreateUpscaler;
    FNDSRExSuperResDestroyUpscaler pfnDSRExSuperResDestroyUpscaler;
    FNDSRExSuperResGetOptimalJitterPattern pfnDSRExSuperResGetOptimalJitterPattern;
    FNDSRExSuperResExecuteUpscaler pfnDSRExSuperResExecuteUpscaler;
    FNDSRExSuperResUpscalerEvict pfnDSRExSuperResUpscalerEvict;
    FNDSRExSuperResUpscalerMakeResident pfnDSRExSuperResUpscalerMakeResident;
} DSR_EX_FUNCTION_TABLE_1_0;

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_GAMES) */
#pragma endregion
DEFINE_GUID(IID_IDSRDevice,0x994659a7,0x31ad,0x4912,0x94,0x14,0x15,0x9f,0x16,0x63,0x03,0x06);
DEFINE_GUID(IID_IDSRSuperResUpscaler,0x2adc388c,0x1b5a,0x4a87,0x93,0x77,0x64,0x82,0x2e,0x48,0x9c,0x12);
DEFINE_GUID(IID_IDSRSuperResEngine,0x4bfd72e2,0x2767,0x4800,0xbc,0xf4,0xce,0xdc,0x0d,0x07,0xea,0x5a);


extern RPC_IF_HANDLE __MIDL_itf_directsr_0000_0003_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_directsr_0000_0003_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


