/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//--------------------------------------------------------------------------------------
// File: NGFX_GPUTrace_Common_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GPUTRACE_COMMON_TYPES_H
#define NGFX_GPUTRACE_COMMON_TYPES_H

#include "NGFX_Types.h"

/**
 * @brief Flags used to control the behavior of stopping a GPU trace.
 */
typedef enum NGFX_GPUTrace_StopTraceFlag
{
    NGFX_GPUTrace_StopTraceFlag_None = 0x0,                   /**< Stop tracing immediately and drain out data as future work is submitted. */
    NGFX_GPUTrace_StopTraceFlag_NextFrameBoundary = 1 << 0,   /**< Stop trace after the next frame boundary. The trace is stopped immediately if this flag is omitted. */
    NGFX_GPUTrace_StopTraceFlag_ImmediateCollection = 1 << 1, /**< Collect data immediately once the trace has been stopped. Note; this may lead to deadlocks if any queue needs to wait for unscheduled work to complete. */
} NGFX_GPUTrace_StopTraceFlag;

/**
 * @brief Alias for a combination of NGFX_GPUTrace_StopTraceFlag values.
 */
typedef NGFX_GPUTrace_StopTraceFlag NGFX_GPUTrace_StopTraceFlags;

/**
 * @brief Enum representing the status of a GPU trace.
 */
typedef enum NGFX_GPUTrace_Status
{
    NGFX_GPUTrace_Status_Inactive, /**< GPU trace is initialized, but inactive. */
    NGFX_GPUTrace_Status_Active,   /**< GPU trace is active, but not tracing. */
    NGFX_GPUTrace_Status_Tracing,  /**< GPU trace is tracing. */
    NGFX_GPUTrace_Status_Draining, /**< GPU trace is draining traced data. */
    NGFX_GPUTrace_Status_Errored,  /**< GPU trace encountered an error. */
    NGFX_GPUTrace_Status_COUNT,    /**< Total count of GPU trace statuses. */
} NGFX_GPUTrace_Status;

/**
 * @brief Parameters for waiting for a specific tool status.
 */
typedef struct NGFX_GPUTrace_GetStatus_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_WaitForStatus_Params_VER */

    NGFX_GPUTrace_Status status; /**< The current status of GPU Trace. */
} NGFX_GPUTrace_GetStatus_Params_V1;
typedef NGFX_GPUTrace_GetStatus_Params_V1 NGFX_GPUTrace_GetStatus_Params;
#define NGFX_GPUTrace_GetStatus_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_GetStatus_Params_V1, 1)
#define NGFX_GPUTrace_GetStatus_Params_VER NGFX_GPUTrace_GetStatus_Params_V1_VER

/**
 * @brief Parameters for waiting for a specific status.
 */
typedef struct NGFX_GPUTrace_WaitForStatus_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_WaitForStatus_Params_VER */

    NGFX_GPUTrace_Status status; /**< The desired status to wait for. */
    int timeoutMs;               /**< Timeout in milliseconds for waiting, a negative number will never time out. */
} NGFX_GPUTrace_WaitForStatus_Params_V1;
typedef NGFX_GPUTrace_WaitForStatus_Params_V1 NGFX_GPUTrace_WaitForStatus_Params;
#define NGFX_GPUTrace_WaitForStatus_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_WaitForStatus_Params_V1, 1)
#define NGFX_GPUTrace_WaitForStatus_Params_VER NGFX_GPUTrace_WaitForStatus_Params_V1_VER

/**
 * @brief Enum representing the type of event that triggers GPU Trace to start.
 */
typedef enum NGFX_GPUTrace_StartEvent
{
    NGFX_GPUTrace_StartEvent_Manual,   /**< GPU Trace starts after a manual trigger, which can either be a hotkey press or a button in the Nsight Graphics host.*/
    NGFX_GPUTrace_StartEvent_Frame,    /**< GPU Trace starts automatically after a specific number of frames have elapsed. */
    NGFX_GPUTrace_StartEvent_Submit,   /**< GPU Trace starts automatically after a specific number of submits have occurred. */
    NGFX_GPUTrace_StartEvent_Duration, /**< GPU Trace starts automatically after a specific duration has elapsed. */
    NGFX_GPUTrace_StartEvent_NGFXSDK,  /**< GPU Trace starts when the application calls NGFX_GPUTrace_StartTrace. */
    NGFX_GPUTrace_StartEvent_COUNT,    /**< Total count of GPU Trace start event types. */
} NGFX_GPUTrace_StartEvent;

/**
 * @brief Enum representing the type of event that triggers GPU Trace to end.
 */
typedef enum NGFX_GPUTrace_StopEvent
{
    NGFX_GPUTrace_StopEvent_Frame,       /**< GPU Trace stops automatically after a specific number of frames have been traced. */
    NGFX_GPUTrace_StopEvent_Submit,      /**< GPU Trace stops automatically after a specific number of submits have been traced. */
    NGFX_GPUTrace_StopEvent_MaxDuration, /**< GPU Trace stops automatically after the specified maximum duration has elapsed. */
    NGFX_GPUTrace_StopEvent_NGFXSDK,     /**< GPU Trace stops when the application calls NGFX_GPUTrace_StopTrace. */
    NGFX_GPUTrace_StopEvent_COUNT,       /**< Total count of GPU Trace stop event types. */
} NGFX_GPUTrace_StopEvent;

/**
 * @brief Enum representing how GPU Trace will interact with V-Sync.
 */
typedef enum NGFX_GPUTrace_VSyncMode
{
    NGFX_GPUTrace_VSyncMode_Off,                   /**< GPU Trace will attempt to force V-Sync to be disabled. */
    NGFX_GPUTrace_VSyncMode_ApplicationControlled, /**< GPU Trace will not interfere with V-Sync. */
    NGFX_GPUTrace_VSyncMode_COUNT,                 /**< Total count of GPU Trace V-Sync modes. */
} NGFX_GPUTrace_VSyncMode;

/**
 * @brief Enum representing how GPU Trace will interact with the GPU clock.
 * Note, that clock speeds are still subject to thermal throttling which may yield different clock speeds from the target locked value.
 */
typedef enum NGFX_GPUTrace_GPUClockMode
{
    NGFX_GPUTrace_GPUClockMode_Unaltered,   /**< GPU Trace will not interfere with the GPU clock speeds. */
    NGFX_GPUTrace_GPUClockMode_LockToBase,  /**< GPU Trace will attempt to lock GPU clock speeds to the base frequency. */
    NGFX_GPUTrace_GPUClockMode_LockToBoost, /**< GPU Trace will attempt to lock GPU clock speeds to the boost frequency. */
    NGFX_GPUTrace_GPUClockMode_COUNT,       /**< Total count of GPU Trace GPU Clock modes. */
} NGFX_GPUTrace_GPUClockMode;

/**
 * @brief Enum representing where the GPU Trace will be shown in the application window.
 */
typedef enum NGFX_GPUTrace_HUDPosition
{
    NGFX_GPUTrace_HUDPosition_TopLeft,      /**< The GPU Trace HUD will be located in the top-left corner. */
    NGFX_GPUTrace_HUDPosition_TopCenter,    /**< The GPU Trace HUD will be located in the top-center. */
    NGFX_GPUTrace_HUDPosition_TopRight,     /**< The GPU Trace HUD will be located in the top-right corner. */
    NGFX_GPUTrace_HUDPosition_MiddleLeft,   /**< The GPU Trace HUD will be located in the middle-left. */
    NGFX_GPUTrace_HUDPosition_MiddleRight,  /**< The GPU Trace HUD will be located in the middle-right. */
    NGFX_GPUTrace_HUDPosition_BottomLeft,   /**< The GPU Trace HUD will be located in the bottom-left corner. */
    NGFX_GPUTrace_HUDPosition_BottomCenter, /**< The GPU Trace HUD will be located in the bottom-center. */
    NGFX_GPUTrace_HUDPosition_BottomRight,  /**< The GPU Trace HUD will be located in the bottom-right. */
    NGFX_GPUTrace_HUDPosition_Hidden,       /**< The GPU Trace HUD will be hidden. */
    NGFX_GPUTrace_HUDPosition_COUNT,        /**< Total count of GPU Trace HUD positions. */
} NGFX_GPUTrace_HUDPosition;

/**
 * @brief struct containing GPUTrace injection settings
 * Note, GPU-specific settings such as metric set, real-time shader profiler enablement,
 * and bandwidth settings can only be specified in the Nsight Graphics host application.
 */
typedef struct NGFX_GPUTrace_InjectionSettings_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_InjectionSettings_VER */

    NGFX_GPUTrace_StartEvent startEvent; /**< The type of event that triggers the start of GPU Trace. */
    union
    {
        uint32_t frameCount;           /**< The number of frames that need to elapse after which GPU Trace is started when NGFX_GPUTrace_StartEvent_Frame is specified. */
        uint32_t submitCount;          /**< The number of submits that need to occur after which GPU Trace is started when NGFX_GPUTrace_StartEvent_Submit is specified. */
        uint32_t durationMs;           /**< The duration in milliseconds after which GPU Trace is started when NGFX_GPUTrace_StartEvent_Duration is specified. */
    } startParams;                     /**< Parameters for the start event. */
    NGFX_GPUTrace_StopEvent stopEvent; /**< The type of event that triggers the end of GPU Trace. */
    union
    {
        uint32_t frameCount;  /**< The number of frames to trace when NGFX_GPUTrace_StopEvent_Frame is specified. */
        uint32_t submitCount; /**< The number of submits to trace when NGFX_GPUTrace_StopEvent_Submit is specified. */
    } stopParams;             /**< Parameters for the stop event. */
    uint32_t maxDurationMs;   /**< The maximum duration in milliseconds for which GPU Trace will run, this is a limiter used by all NGFX_GPUTrace_StopEvent types. */

    bool collectScreenshot;              /**< If true, GPU Trace will collect a screenshot for present-based applications. */
    bool timeEveryAction;                /**< If true, individual actions will be timed separately instead of being coalesced with adjacent actions of the same kind. Enabling this option will introduce additional overhead. */
    bool traceShaderBindings;            /**< If true, GPU Trace will intercept and trace information about pipeline and shader bindings. */
    bool collectShaderInfo;              /**< If true, GPU Trace will collect shader information used by the Real-Time Shader Profiler. */
    bool collectExternalShaderDebugInfo; /**< If true, GPU Trace will attempt to resolve external debug information for shaders that don't have it embedded. */
    bool enableNVTXranges;               /**< If true, NVTX ranges can be used to create markers around submits. Enabling this may introduce additional overhead. */

    uint32_t eventBufferSizeKB; /**< The amount of memory in kB that GPU Trace will allocate per API device to keep track of GPU events. */
    uint32_t timestampCount;    /**< The amount of timestamps that GPU Trace will allocate per API device to keep track of GPU events, each timestamp is 32 bytes in size. */

    bool recompileCachedVulkanShaderModules;        /**< If true, any pipelines that reference shader modules through identifiers from VK_EXT_shader_module_identifier will be recompiled.
                                                         Disabling this setting while using VK_EXT_shader_module_identifier may lead to shader source correlation being absent for shaders modules using identifiers.*/
    bool disableD3D12DebugLayer;                    /**< If true, the D3D12 debug layer will be forcefully disabled. */
    bool forceD3D12BackgroundOptimizations;         /**< If true, the D3D12 driver will perform background optimizations of shaders even when it detects that the additional CPU overhead will impact the application framerate. */
    uint32_t maxInternalVKSCCommandBuffersPerQueue; /**< Maximum number of internal command buffers GPU Trace can allocate per VulkanSC application queue. */
    uint32_t maxInternalVKSCCommandBuffersMemoryKB; /**< Maximum amount of memory in kB that GPU Trace can allocate for each internal VulkanSC command buffer. */

    NGFX_GPUTrace_VSyncMode vsyncMode;       /**< Specifies how GPU Trace will interact with V-Sync. */
    NGFX_GPUTrace_GPUClockMode gpuClockMode; /**< Specifies how GPU Trace will interact with the clock speeds of the GPU. */
    bool forceRepaint;                       /**< If true, forces the application to continuously redraw the window, as opposed to only redrawing on change. */

    NGFX_GPUTrace_HUDPosition hudPosition; /**< The position of the GPU Trace HUD in the application window. */
} NGFX_GPUTrace_InjectionSettings_V1;
typedef NGFX_GPUTrace_InjectionSettings_V1 NGFX_GPUTrace_InjectionSettings;
#define NGFX_GPUTrace_InjectionSettings_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_InjectionSettings_V1, 1)
#define NGFX_GPUTrace_InjectionSettings_VER NGFX_GPUTrace_InjectionSettings_V1_VER

#endif // NGFX_GPUTRACE_COMMON_TYPES_H
