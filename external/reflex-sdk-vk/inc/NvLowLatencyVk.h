/************************************************************************************************************************************\
|*                                                                                                                                    *|
|*     Copyright © 2020 NVIDIA Corporation.  All rights reserved.                                                                     *|
|*                                                                                                                                    *|
|*  NOTICE TO USER:                                                                                                                   *|
|*                                                                                                                                    *|
|*  This software is subject to NVIDIA ownership rights under U.S. and international Copyright laws.                                  *|
|*                                                                                                                                    *|
|*  This software and the information contained herein are PROPRIETARY and CONFIDENTIAL to NVIDIA                                     *|
|*  and are being provided solely under the terms and conditions of an NVIDIA software license agreement                              *|
|*  and / or non-disclosure agreement.  Otherwise, you have no rights to use or access this software in any manner.                   *|
|*                                                                                                                                    *|
|*  If not covered by the applicable NVIDIA software license agreement:                                                               *|
|*  NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.                                            *|
|*  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.                                                           *|
|*  NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,                                                                     *|
|*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.                       *|
|*  IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,                               *|
|*  OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT,                         *|
|*  NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.            *|
|*                                                                                                                                    *|
|*  U.S. Government End Users.                                                                                                        *|
|*  This software is a "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT 1995),                                       *|
|*  consisting  of "commercial computer  software"  and "commercial computer software documentation"                                  *|
|*  as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government only as a commercial end item.     *|
|*  Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995),                                          *|
|*  all U.S. Government End Users acquire the software with only those rights set forth herein.                                       *|
|*                                                                                                                                    *|
|*  Any use of this software in individual and commercial software must include,                                                      *|
|*  in the user documentation and internal comments to the code,                                                                      *|
|*  the above Disclaimer (as applicable) and U.S. Government End Users Notice.                                                        *|
|*                                                                                                                                    *|
 \************************************************************************************************************************************/

 ///////////////////////////////////////////////////////////////////////////////
 //
 // Date: Sept 23, 2020 
 // File: NvLowLatencyVk.h
 // Version: 1.0
 //
 ///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <windows.h>

#ifdef NVLOWLATENCYVK_EXPORTS
#define NVLLVK_API __declspec(dllexport)
#else
#define NVLLVK_API __declspec(dllimport)
#endif


extern "C" {

	/* 64-bit type for compilers that support it, plus an obsolete variant */
#if defined(_WIN64)
typedef unsigned long long NvVKU64; /* 0 to 18446744073709551615 */
#else
typedef unsigned __int64   NvVKU64; /* 0 to 18446744073709551615 */
#endif

typedef unsigned long    NvVKU32; /* 0 to 4294967295 */
typedef unsigned char    NvVKU8;
typedef NvVKU8           NvVKBool;

// ====================================================
//! NvLL_VK_Status Values
//! All NvLLVk functions return one of these codes.
// ====================================================

typedef enum _NvLL_VK_Status
{
    NVLL_VK_OK = 0,      //!< Success. Request is completed.
    NVLL_VK_ERROR = -1,      //!< Generic error.
    NVLL_VK_LIBRARY_NOT_FOUND = -2,      //!< NvLLVk support library cannot be loaded.
    NVLL_VK_NO_IMPLEMENTATION = -3,      //!< Not implemented in current driver installation.
    NVLL_VK_API_NOT_INITIALIZED = -4,      //!< NvLL_VK_Initialize has not been called (successfully).
    NVLL_VK_INVALID_ARGUMENT = -5,      //!< The argument/parameter value is not valid or NULL.
    NVLL_VK_INVALID_HANDLE = -8,      //!< Invalid handle.
    NVLL_VK_INCOMPATIBLE_STRUCT_VERSION = -9,      //!< An argument's structure version is not supported.
    NVLL_VK_INVALID_POINTER = -14,     //!< An invalid pointer, usually NULL, was passed as a parameter.
    NVLL_VK_OUT_OF_MEMORY = -130,    //!< Could not allocate sufficient memory to complete the call.
    NVLL_VK_API_IN_USE = -209,    //!< An API is still being called.
    NVLL_VK_NO_VULKAN = -229,    //!< No Vulkan support.
} NvLL_VK_Status;


///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_Initialize
//
//! This function initializes the NvLLVk library (if not already initialized) but always increments the ref-counter.
//! This must be called before calling other NvLLVk functions.
//! Note: It is now mandatory to call NvLL_Initialize before calling any other NvLLVk API.
//! NvLL_VK_Unload should be called to unload the NvLLVk Library.
//!
//! SUPPORTED OS:  Windows 7 and higher
//!
//!
//! \since Release: 80
//!
//! \return      This API can return any of the error codes enumerated in NvLL_VK_Status. If there are return error codes with
//!              specific meaning for this API, they are listed below.
//! \retval      NVLL_VK_LIBRARY_NOT_FOUND  Failed to load the NvLLVk support library
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_Initialize();


///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_Unload
//
//!   DESCRIPTION: Decrements the ref-counter and when it reaches ZERO, unloads NvLLVk library.
//!                This must be called in pairs with NvLL_VK_Initialize.
//!
//! SUPPORTED OS:  Windows 7 and higher
//!
//!
//!  If the client wants unload functionality, it is recommended to always call NvLL_VK_Initialize and NvLL_VK_Unload in pairs.
//!
//!  Unloading NvLLVk library is not supported when the library is in a resource locked state.
//!  Some functions in the NvLLVk library initiates an operation or allocates certain resources
//!  and there are corresponding functions available, to complete the operation or free the
//!  allocated resources. All such function pairs are designed to prevent unloading NvLLVk library.
//!
//! \return      This API can return any of the error codes enumerated in NvLL_VK_Status. If there are return error codes with
//!              specific meaning for this API, they are listed below.
//! \retval      NVLL_VK_API_IN_USE       An API is still being called, hence cannot unload requested driver.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_Unload();


///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_InitLowLatencyDevice
//
//!   DESCRIPTION: This function has to be used to initialize a Vulkan device
//!   as a low latency device. The driver initializes a set of parameters to
//!   be used in subsequent low latency API calls for this device.
//!   The API will allocate and return a VkSemaphore (signalSemaphoreHandle) which 
//!   will be signaled based on subsequent calls to NvLL_VK_Sleep.
//!
//! \since Release: 455
//! \param [in] vkDevice                      The Vulkan device handle.
//! \param [out] signalSemaphoreHandle        Pointer to a VkSemaphore handle that is signaled in Sleep.
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in NvLL_VK_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_InitLowLatencyDevice(HANDLE vkDevice, HANDLE* signalSemaphoreHandle);


///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_DestroyLowLatencyDevice
//
//!   DESCRIPTION: This function releases the set of low latency device
//!   parameters.
//!
//! \since Release: 455
//! \param [in] vkDevice                      The Vulkan device handle.
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in NvLL_VK_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_DestroyLowLatencyDevice(HANDLE vkDevice);


typedef struct _NVLL_VK_GET_SLEEP_STATUS_PARAMS
{
    NvVKBool bLowLatencyMode;                                 //!< (OUT) Is low latency mode enabled?
} NVLL_VK_GET_SLEEP_STATUS_PARAMS;


///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_GetSleepStatus
//
//!   DESCRIPTION: This function can be used to get the latest sleep status.
//!   bLowLatencyMode indicates whether low latency mode is currently
//!   enabled in the driver.
//!   Note that it may not always reflect the previously requested sleep mode,
//!   as the feature may not be available on the platform, or the setting may have
//!   been overridden by the control panel, for example.
//!
//! \since Release: 455
//! \param [in] vkDevice                      The Vulkan device handle.
//! \param [inout] pGetSleepStatusParams      Sleep status params.
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in NvLL_VK_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_GetSleepStatus(HANDLE vkDevice, NVLL_VK_GET_SLEEP_STATUS_PARAMS* pGetSleepStatusParams);


typedef struct _NVLL_VK_SET_SLEEP_MODE_PARAMS
{
    NvVKBool bLowLatencyMode;                               //!< (IN) Low latency mode enable/disable.
    NvVKBool bLowLatencyBoost;                              //!< (IN) Request maximum GPU clock frequency regardless of workload.
    NvVKU32  minimumIntervalUs;                             //!< (IN) Minimum frame interval in microseconds. 0 = No frame rate limit.
} NVLL_VK_SET_SLEEP_MODE_PARAMS;

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_SetSleepMode
//
//!   DESCRIPTION: This function can be used to update the sleep mode dynamically.
//!   The settings are not dependent on each other, meaning low latency mode
//!   can be enabled/disabled regardless of whether minimum interval is set or
//!   not. The former is to intelligently lower latency without impacting frame
//!   rate. The latter is to limit frame rate (e.g. minimumIntervalUs = 10000
//!   limits frame rate to 100 FPS). They work well separately and/or together.
//!   Note that minimumIntervalUs usage is not limited to lowering latency, so
//!   feel free to use it to limit frame rate for menu, cut scenes, etc.
//!   The bLowLatencyBoost parameter will request for the GPU to run at max clocks
//!   even in scenarios where it is idle most of the frame and would normally try
//!   to downclock to save power. This can decrease latency in certain CPU-limited
//!   scenarios. While this function can be called as often as needed, it is not
//!   necessary or recommended to call this too frequently (e.g. every frame),
//!   as the settings persist for the target device.
//!
//! \since Release: 455
//! \param [in] vkDevice                      The Vulkan device handle.
//! \param [in] pSetSleepModeParams           Sleep mode params.
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in NvLL_VK_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_SetSleepMode(HANDLE vkDevice, NVLL_VK_SET_SLEEP_MODE_PARAMS* pSetSleepModeParams);


///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_Sleep
//
//!   DESCRIPTION: It is recommended to call this function at the very start of
//!   each frame (e.g. before input sampling). If there is a need to sleep,
//!   due to frame rate limit and/or low latency features for example,
//!   this call provides an entry point for the driver to sleep at the
//!   optimal point in time to achieve the lowest latency.
//!   It is recommended to call this function even when low latency mode is
//!   disabled and minimum interval is 0. Other features, such as the Maximum Frame
//!   Rate setting, could be enabled in the NVIDIA control panel and benefit from this.
//!   It is OK to start (or stop) using this function at any time. However,
//!   when using this function, it must be called exactly once on each frame. Each frame,
//!   the signalValue should usually be increased by 1, and this function should be called 
//!   with the new value. Then, vkWaitSemaphores (with a large timeout specified) should
//!   be called and will block until the semaphore is signaled.
//!
//! \since Release: 455
//! \param [in] vkDevice                      The Vulkan device handle.
//! \param [in] signalValue                   Value that will be signaled in signalSemaphoreHandle semaphore at Sleep.
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in NvLL_VK_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_Sleep(HANDLE vkDevice, NvVKU64 signalValue);


typedef struct _NVLL_VK_LATENCY_RESULT_PARAMS
{
    struct vkFrameReport {
        NvVKU64 frameID;
        NvVKU64 inputSampleTime;
        NvVKU64 simStartTime;
        NvVKU64 simEndTime;
        NvVKU64 renderSubmitStartTime;
        NvVKU64 renderSubmitEndTime;
        NvVKU64 presentStartTime;
        NvVKU64 presentEndTime;
        NvVKU64 driverStartTime;
        NvVKU64 driverEndTime;
        NvVKU64 osRenderQueueStartTime;
        NvVKU64 osRenderQueueEndTime;
        NvVKU64 gpuRenderStartTime;
        NvVKU64 gpuRenderEndTime;
    } frameReport[64];
} NVLL_VK_LATENCY_RESULT_PARAMS;

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_GetLatency
//
//!   DESCRIPTION: Get a latency report including the timestamps of the 
//!   application latency markers set with NvLL_VK_SetLatencyMarker as well
//!   as driver, OS queue and graphics hardware times. Requires calling
//!   NvLL_VK_SetLatencyMarker with incrementing frameID for valid results.
//!   Rendering for at least 90 frames is recommended to properly fill out the
//!   structure. The newest completed frame is at the end (element 63) and
//!   is preceded by older frames. If not enough frames are valid then all
//!   frames members are returned with all zeroes.
//!
//! \since Release: 455
//! \param [in] vkDevice                      The Vulkan device handle.
//! \param [inout] pGetLatencyParams          The latency result structure.
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in NvLL_VK_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_GetLatency(HANDLE vkDevice, NVLL_VK_LATENCY_RESULT_PARAMS* pGetLatencyResultParams);


typedef enum
{
    VK_SIMULATION_START = 0,
    VK_SIMULATION_END = 1,
    VK_RENDERSUBMIT_START = 2,
    VK_RENDERSUBMIT_END = 3,
    VK_PRESENT_START = 4,
    VK_PRESENT_END = 5,
    VK_INPUT_SAMPLE = 6,
    VK_TRIGGER_FLASH = 7,
    VK_PC_LATENCY_PING = 8,
    VK_OUT_OF_BAND_RENDERSUBMIT_START = 9,
    VK_OUT_OF_BAND_RENDERSUBMIT_END = 10,
    VK_OUT_OF_BAND_PRESENT_START = 11,
    VK_OUT_OF_BAND_PRESENT_END = 12,
} NVLL_VK_LATENCY_MARKER_TYPE;


typedef struct _NVLL_VK_LATENCY_MARKER_PARAMS
{
    NvVKU64  frameID;
    NVLL_VK_LATENCY_MARKER_TYPE markerType;
} NVLL_VK_LATENCY_MARKER_PARAMS;

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvLL_VK_SetLatencyMarker
//
//!   DESCRIPTION: Set a latency marker to be tracked by the
//!   NvLL_VK_GetLatency function. VK_SIMULATION_START must be the first marker
//!   sent in a frame, after the frame's Sleep call (if used).
//!   VK_INPUT_SAMPLE may be sent to record the moment user input was sampled and
//!   should come between VK_SIMULATION_START and VK_SIMULATION_END.
//!   VK_RENDERSUBMIT_START should come before any Vulkan API calls are made for
//!   the given frame. VK_RENDERSUBMIT_END should come at the end of the frame render
//!   work submission. VK_PRESENT_START and VK_PRESENT_END should wrap the Present call
//!   and may be used either before or after the VK_RENDERSUBMIT_END.
//!   VK_TRIGGER_FLASH tells the driver to render its flash indicator on top of the
//!   frame for latency testing, typically driven by a mouse click.
//!   The frameID can start at an abitrary value but must strictly increment from
//!   that point forward for consistent results.
//!
//! \since Release: 455
//! \param [in] vkDevice                      The Vulkan device handle.
//! \param [in] pSetLatencyMarkerParams       The latency marker structure.
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in NvLL_VK_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_SetLatencyMarker(HANDLE vkDevice, NVLL_VK_LATENCY_MARKER_PARAMS* pSetLatencyMarkerParams);


typedef enum
{
    VK_OUT_OF_BAND_QUEUE_TYPE_RENDER = 0,
    VK_OUT_OF_BAND_QUEUE_TYPE_PRESENT = 1,
} NVLL_VK_OUT_OF_BAND_QUEUE_TYPE;

///////////////////////////////////////////////////////////////////////////////
//
// FUNCTION NAME: NvAPI_Vulkan_NotifyOutOfBandVkQueue
//
//!   DESCRIPTION: Notifies the driver that this command queue runs out of band
//!                from the application's frame cadence.
//!
//! \since Release: 520
//! \param [in] vkDevice        The Vulkan device handle.
//! \param [in] queueHandle     The VkQueue
//! \param [in] queueType       The type of out of band VkQueue
//! SUPPORTED OS:  Windows 10 and higher
//!
//!
//! \return This API can return any of the error codes enumerated in #NvAPI_Status.
//!         If there are return error codes with specific meaning for this API, they are listed below.
//!
///////////////////////////////////////////////////////////////////////////////

NVLLVK_API NvLL_VK_Status NvLL_VK_NotifyOutOfBandQueue(HANDLE vkDevice, HANDLE queueHandle, NVLL_VK_OUT_OF_BAND_QUEUE_TYPE queueType);

}