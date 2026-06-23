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
// File: Impl/NGFX_Core.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_CORE_H
#define NGFX_CORE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include "../NGFX_CUDART_Types.h"
#include "../NGFX_CUDA_Types.h"
#include "../NGFX_D3D12_Types.h"
#include "../NGFX_GPUTrace_CUDART_Types.h"
#include "../NGFX_GPUTrace_CUDA_Types.h"
#include "../NGFX_GPUTrace_D3D12_Types.h"
#include "../NGFX_GPUTrace_OpenGL_Types.h"
#include "../NGFX_GPUTrace_Vulkan_Types.h"
#include "../NGFX_GraphicsCapture_D3D12_Types.h"
#include "../NGFX_GraphicsCapture_Vulkan_Types.h"
#include "../NGFX_OpenGL_Types.h"
#include "../NGFX_SystemProfiling_Common_Types.h"
#include "../NGFX_SystemProfiling_D3D12_Types.h"
#include "../NGFX_SystemProfiling_Vulkan_Types.h"
#include "../NGFX_Types.h"
#include "../NGFX_Vulkan_Types.h"
#include "NGFX_Defines.h"

#if defined(_WIN32)
#include <Windows.h>
#include <libloaderapi.h>
#include <shlobj_core.h>
#include <strsafe.h>
#else
#include <dirent.h>
#include <dlfcn.h>
#include <stddef.h>
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup NGFX_API_CORE NGFX API
 *
 * The NGFX API offers capabilities for programmatic control of applications integrating with Nsight
 * Graphics. It is an evoluation of the Injection API and should be preferred for all D3D12 and
 * Vulkan applications.
 *
 * @{
 */

/**
 * @cond INTERNAL_IMPLEMENTATION_DETAILS
 */
#if defined(_WIN32)
/**
 * @brief Library names for Windows platform.
 */
#define NGFX_GRAPHICS_CAPTURE_INJECTION_LIB_NAME L"ngfx-capture-injection.dll"
#define NGFX_GRAPHICS_CAPTURE_TARGET_LIB_NAME L"ngfx-capture-interception.dll"
#define NGFX_API_BOOTSTRAP_LIB_NAME L"ngfx-api-bootstrap.dll"
#define NGFX_GPU_TRACE_INJECTION_LIB_NAME L"WarpViz.Injection.dll"
#define NGFX_GPU_TRACE_TARGET_LIB_NAME L"WarpVizTarget.dll"
#define NGFX_SYSTEM_PROFILING_INJECTION_LIB_NAME L"ToolsInjection64.dll"
#define NGFX_SYSTEM_PROFILING_TARGET_LIB_NAME L"ToolsInjection64.dll"
#else
/**
 * @brief Library names for non-Windows platforms.
 */
#define NGFX_GRAPHICS_CAPTURE_INJECTION_LIB_NAME "libngfx-capture-injection.so"
#define NGFX_GRAPHICS_CAPTURE_TARGET_LIB_NAME "libngfx-capture-interception.so"
#define NGFX_API_BOOTSTRAP_LIB_NAME "libngfx-api-bootstrap.so"
#define NGFX_GPU_TRACE_INJECTION_LIB_NAME "libWarpViz.Injection.so"
#define NGFX_GPU_TRACE_TARGET_LIB_NAME "libWarpVizTarget.so"
#define NGFX_SYSTEM_PROFILING_INJECTION_LIB_NAME "libToolsInjection64.so"
#define NGFX_SYSTEM_PROFILING_TARGET_LIB_NAME "libToolsInjection64.so"
#endif

#if defined(NGFX_VERBOSE_LOGGING)
#define NGFX_LOG(fmt, ...)               \
    do                                   \
    {                                    \
        printf(fmt "\n", ##__VA_ARGS__); \
    } while (0)
#else
#define NGFX_LOG(fmt, ...) \
    do                     \
    {                      \
    } while (0)
#endif

/**
 * @brief Macro to define all CUDA function pointers.
 */
#define NGFX_FOR_EACH_CUDA_FN(_M) \
    _M(CUDA, NGFX, FrameBoundaryCUDA, NGFX_FrameBoundary_CUDA_Params*)

/**
 * @brief Macro to define all CUDART function pointers.
 */
#define NGFX_FOR_EACH_CUDART_FN(_M) \
    _M(CUDART, NGFX, FrameBoundaryCUDART, NGFX_FrameBoundary_CUDART_Params*)
/**
 * @brief Macro to define all D3D12 function pointers.
 */
#define NGFX_FOR_EACH_D3D12_FN(_M)                                        \
    _M(D3D12, NGFX, FrameBoundaryD3D12, NGFX_FrameBoundary_D3D12_Params*) \
    _M(D3D12, NGFX, DLSS_FG_PresentBoundaryD3D12, NGFX_DLSS_FG_PresentBoundary_D3D12_Params*)

/**
 * @brief Macro to define all OpenGL function pointers.
 */
#define NGFX_FOR_EACH_OPENGL_FN(_M) \
    _M(OpenGL, NGFX, FrameBoundaryOpenGL, NGFX_FrameBoundary_OpenGL_Params*)

/**
 * @brief Macro to define all Vulkan function pointers.
 */
#define NGFX_FOR_EACH_VULKAN_FN(_M)                                          \
    _M(Vulkan, NGFX, FrameBoundaryVulkan, NGFX_FrameBoundary_Vulkan_Params*) \
    _M(Vulkan, NGFX, DLSS_FG_PresentBoundaryVulkan, NGFX_DLSS_FG_PresentBoundary_Vulkan_Params*)

/**
 * @brief Macro to define all Graphics Capture function pointers.
 */
#define NGFX_FOR_EACH_GRAPHICS_CAPTURE_FN(_M)                                                                                          \
    _M(GraphicsCapture, NGFX_GraphicsCapture, InitializeCaptureActivityD3D12, NGFX_GraphicsCapture_InitializeActivity_D3D12_Params*)   \
    _M(GraphicsCapture, NGFX_GraphicsCapture, RequestCaptureD3D12, NGFX_GraphicsCapture_RequestCapture_D3D12_Params*)                  \
    _M(GraphicsCapture, NGFX_GraphicsCapture, StartCaptureD3D12, NGFX_GraphicsCapture_StartCapture_D3D12_Params*)                      \
    _M(GraphicsCapture, NGFX_GraphicsCapture, StopCaptureD3D12, NGFX_GraphicsCapture_StopCapture_D3D12_Params*)                        \
    _M(GraphicsCapture, NGFX_GraphicsCapture, InitializeCaptureActivityVulkan, NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params*) \
    _M(GraphicsCapture, NGFX_GraphicsCapture, RequestCaptureVulkan, NGFX_GraphicsCapture_RequestCapture_Vulkan_Params*)                \
    _M(GraphicsCapture, NGFX_GraphicsCapture, StartCaptureVulkan, NGFX_GraphicsCapture_StartCapture_Vulkan_Params*)                    \
    _M(GraphicsCapture, NGFX_GraphicsCapture, StopCaptureVulkan, NGFX_GraphicsCapture_StopCapture_Vulkan_Params*)

/**
 * @brief Macro to define all GPU Trace function pointers.
 */
#define NGFX_FOR_EACH_GPU_TRACE_FN(_M)                                                                          \
    _M(GPUTrace, NGFX_GPUTrace, GetStatus, NGFX_GPUTrace_GetStatus_Params*)                                     \
    _M(GPUTrace, NGFX_GPUTrace, WaitForStatus, NGFX_GPUTrace_WaitForStatus_Params*)                             \
    _M(GPUTrace, NGFX_GPUTrace, InitializeTraceActivityD3D12, NGFX_GPUTrace_InitializeActivity_D3D12_Params*)   \
    _M(GPUTrace, NGFX_GPUTrace, ActivateTraceD3D12, NGFX_GPUTrace_ActivateTrace_D3D12_Params*)                  \
    _M(GPUTrace, NGFX_GPUTrace, StartTraceD3D12, NGFX_GPUTrace_StartTrace_D3D12_Params*)                        \
    _M(GPUTrace, NGFX_GPUTrace, StopTraceD3D12, NGFX_GPUTrace_StopTrace_D3D12_Params*)                          \
    _M(GPUTrace, NGFX_GPUTrace, InitializeTraceActivityVulkan, NGFX_GPUTrace_InitializeActivity_Vulkan_Params*) \
    _M(GPUTrace, NGFX_GPUTrace, ActivateTraceVulkan, NGFX_GPUTrace_ActivateTrace_Vulkan_Params*)                \
    _M(GPUTrace, NGFX_GPUTrace, StartTraceVulkan, NGFX_GPUTrace_StartTrace_Vulkan_Params*)                      \
    _M(GPUTrace, NGFX_GPUTrace, StopTraceVulkan, NGFX_GPUTrace_StopTrace_Vulkan_Params*)

/**
 * @brief Macro to define all GPU Trace CUDA function pointers.
 */
#define NGFX_FOR_EACH_GPU_TRACE_CUDA_FN(_M)                                                                     \
    _M(GPUTrace, NGFX_GPUTrace, InitializeTraceActivityCUDA, NGFX_GPUTrace_InitializeActivity_CUDA_Params*)     \
    _M(GPUTrace, NGFX_GPUTrace, ActivateTraceCUDA, NGFX_GPUTrace_ActivateTrace_CUDA_Params*)                    \
    _M(GPUTrace, NGFX_GPUTrace, StartTraceCUDA, NGFX_GPUTrace_StartTrace_CUDA_Params*)                          \
    _M(GPUTrace, NGFX_GPUTrace, StopTraceCUDA, NGFX_GPUTrace_StopTrace_CUDA_Params*)                            \
    _M(GPUTrace, NGFX_GPUTrace, InitializeTraceActivityCUDART, NGFX_GPUTrace_InitializeActivity_CUDART_Params*) \
    _M(GPUTrace, NGFX_GPUTrace, ActivateTraceCUDART, NGFX_GPUTrace_ActivateTrace_CUDART_Params*)                \
    _M(GPUTrace, NGFX_GPUTrace, StartTraceCUDART, NGFX_GPUTrace_StartTrace_CUDART_Params*)                      \
    _M(GPUTrace, NGFX_GPUTrace, StopTraceCUDART, NGFX_GPUTrace_StopTrace_CUDART_Params*)

/**
 * @brief Macro to define all GPU Trace OpenGL function pointers.
 */
#define NGFX_FOR_EACH_GPU_TRACE_OPENGL_FN(_M)                                                                   \
    _M(GPUTrace, NGFX_GPUTrace, InitializeTraceActivityOpenGL, NGFX_GPUTrace_InitializeActivity_OpenGL_Params*) \
    _M(GPUTrace, NGFX_GPUTrace, ActivateTraceOpenGL, NGFX_GPUTrace_ActivateTrace_OpenGL_Params*)                \
    _M(GPUTrace, NGFX_GPUTrace, StartTraceOpenGL, NGFX_GPUTrace_StartTrace_OpenGL_Params*)                      \
    _M(GPUTrace, NGFX_GPUTrace, StopTraceOpenGL, NGFX_GPUTrace_StopTrace_OpenGL_Params*)



/**
 * @brief Macro to define all System profiling function pointers.
 */
#define NGFX_FOR_EACH_SYSTEMPROFILING_FN(_M)                                                                                           \
    _M(SystemProfiling, NGFX_SystemProfiling, InitializeProfilingActivityD3D12, NGFX_SystemProfiling_InitializeActivity_D3D12_Params*) \
    _M(SystemProfiling, NGFX_SystemProfiling, InitializeProfilingActivityVulkan, NGFX_SystemProfiling_InitializeActivity_Vulkan_Params*)

/**
 * @brief Macro to define a function pointer type.
 */
#define NGFX_FUNC_PTR(_CATEGORY, _PREFIX, _FUNC, _PARAM) \
    typedef NGFX_Result (*_PREFIX##_##_FUNC##_Fn)(_PARAM);

/**
 * @brief Macro to define a "not implemented" function.
 */
#define NGFX_FUNC_NOT_IMPLEMENTED(_CATEGORY, _PREFIX, _FUNC, _PARAM)            \
    NGFX_FUNCTION NGFX_Result _PREFIX##_##_FUNC##_NotImplemented(_PARAM params) \
    {                                                                           \
        (void)params;                                                           \
        return NGFX_Result_NotImplemented;                                      \
    }

/**
 * @brief Macro to declare a function table entry.
 */
#define NGFX_DECLARE_FUNC_TABLE_ENTRY(_CATEGORY, _PREFIX, _FUNC, _PARAM) \
    _PREFIX##_##_FUNC##_Fn _FUNC;

/**
 * @brief Macro to initialize a function table entry with a "not implemented" function.
 */
#define NGFX_INITIALIZE_FUNC_TABLE_ENTRY(_CATEGORY, _PREFIX, _FUNC, _PARAM) \
    &_PREFIX##_##_FUNC##_NotImplemented,

/**
 * @brief Macro to load a function table entry for a specific API category.
 */
#define NGFX_LOAD_FUNC_TABLE_ENTRY(_CATEGORY, _PREFIX, _FUNC, _PARAM) \
    g_NGFX_Globals._CATEGORY._FUNC = (_PREFIX##_##_FUNC##_Fn)NGFX_GetProc(libHandle, #_PREFIX "_" #_FUNC, (void*)&_PREFIX##_##_FUNC##_NotImplemented);

/**
 * @brief Macro to erase a function table entry for a specific API category and replace it with a "not implemented" function.
 */
#define NGFX_ERASE_FUNC_TABLE_ENTRY(_CATEGORY, _PREFIX, _FUNC, _PARAM) \
    g_NGFX_Globals._CATEGORY._FUNC = &_PREFIX##_##_FUNC##_NotImplemented;

// Function pointer types.
NGFX_FOR_EACH_CUDA_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_CUDART_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_D3D12_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_OPENGL_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_VULKAN_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_GRAPHICS_CAPTURE_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_GPU_TRACE_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_GPU_TRACE_CUDA_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_GPU_TRACE_OPENGL_FN(NGFX_FUNC_PTR);
NGFX_FOR_EACH_SYSTEMPROFILING_FN(NGFX_FUNC_PTR);

// Definition of not implemented functions.
NGFX_FOR_EACH_CUDA_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_CUDART_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_D3D12_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_OPENGL_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_VULKAN_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_GRAPHICS_CAPTURE_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_GPU_TRACE_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_GPU_TRACE_CUDA_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_GPU_TRACE_OPENGL_FN(NGFX_FUNC_NOT_IMPLEMENTED);
NGFX_FOR_EACH_SYSTEMPROFILING_FN(NGFX_FUNC_NOT_IMPLEMENTED);

/**
 * @brief Global structure containing function tables and activity states.
 */
typedef struct NGFX_Globals
{
    NGFX_LoadLibraryFnPtr loadLibraryFnPtr;    /**< Pointer to a function for loading libraries */
    NGFX_ActivityType injectedActivityType;    /**< The currently injected activity type. */
    NGFX_ActivityType initializedActivityType; /**< The currently initialized activity type. */
    NGFX_ApiType initializedApiType;           /**< The currently initialized api type. */

    struct
    {
        NGFX_FOR_EACH_CUDA_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } CUDA; /**< Function table for CUDA. */

    struct
    {
        NGFX_FOR_EACH_CUDART_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } CUDART; /**< Function table for CUDART. */

    struct
    {
        NGFX_FOR_EACH_D3D12_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } D3D12; /**< Function table for D3D12. */

    struct
    {
        NGFX_FOR_EACH_OPENGL_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } OpenGL; /**< Function table for OpenGL. */

    struct
    {
        NGFX_FOR_EACH_VULKAN_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } Vulkan; /**< Function table for Vulkan. */

    struct
    {
        NGFX_FOR_EACH_GRAPHICS_CAPTURE_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } GraphicsCapture; /**< Function table for Graphics Capture. */

    struct
    {
        NGFX_FOR_EACH_GPU_TRACE_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
        NGFX_FOR_EACH_GPU_TRACE_CUDA_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
        NGFX_FOR_EACH_GPU_TRACE_OPENGL_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } GPUTrace; /**< Function table for GPU Trace. */

    struct
    {
        NGFX_FOR_EACH_SYSTEMPROFILING_FN(NGFX_DECLARE_FUNC_TABLE_ENTRY)
    } SystemProfiling; /**< Function table for System Profiling. */
} NGFX_Globals;

/**
 * @brief Loads a library into the process.
 *
 * @param libName The name of the library.
 * @return A handle to the loaded library, or NULL on failure.
 */
NGFX_FUNCTION void* NGFX_LoadLib_UserOverrideRequired(const NGFX_PathChar* libName)
{
    (void)libName;

    fprintf(stderr, "Error: You must call NGFX_SetLibraryLoadFn to specify a function to load libraries.\n"
                    "       If your usage does not require secure loading, you may use NGFX_LoadLib_NoVerification.\n"
                    "       See the Nsight Graphics SDK user guide for more information.\n");
    abort();
}

/**
 * @brief Global instance of NGFX_Globals.
 */
NGFX_GLOBAL NGFX_Globals g_NGFX_Globals =
    {
        NGFX_LoadLib_UserOverrideRequired, // loadLibraryFnPtr
        NGFX_ActivityType_COUNT,           // injectedActivityType
        NGFX_ActivityType_COUNT,           // initializedActivityType
        NGFX_ApiType_COUNT,                // initializedApiType

        // CUDA
        {
            NGFX_FOR_EACH_CUDA_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)},

        // CUDART
        {
            NGFX_FOR_EACH_CUDART_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)},

        // D3D12
        {
            NGFX_FOR_EACH_D3D12_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)},

        // OpenGL
        {
            NGFX_FOR_EACH_OPENGL_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)},

        // Vulkan
        {
            NGFX_FOR_EACH_VULKAN_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)},

        // GraphicsCapture
        {
            NGFX_FOR_EACH_GRAPHICS_CAPTURE_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)},

        // GPUTrace
        {
            NGFX_FOR_EACH_GPU_TRACE_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)
                NGFX_FOR_EACH_GPU_TRACE_CUDA_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)
                    NGFX_FOR_EACH_GPU_TRACE_OPENGL_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)
        },

        // SystemProfiling
        {
            NGFX_FOR_EACH_SYSTEMPROFILING_FN(NGFX_INITIALIZE_FUNC_TABLE_ENTRY)},
};

/**
 * @brief Retrieves a handle to a loaded library.
 *
 * @param libName The name of the library.
 * @return A handle to the library, or NULL if not found.
 */
NGFX_FUNCTION void* NGFX_GetLibHandle(const NGFX_PathChar* libName)
{
#if defined(_WIN32)
    return GetModuleHandleW(libName);
#else
    // coverity[alloc_fn] - this doesn't load the library given RTLD_NOLOAD
    return dlopen(libName, RTLD_NOW | RTLD_NOLOAD);
#endif
}

/**
 * @brief Loads a library into the process. It does not verify that the library is an
 * NVIDIA-provided one. This method may be useful for development but is not recommended for
 * production environments where security is a concern.
 *
 * @param libName The name of the library.
 * @return A handle to the loaded library, or NULL on failure.
 */
NGFX_FUNCTION void* NGFX_LoadLib_NoVerification(const NGFX_PathChar* libName)
{
#if defined(_WIN32)
    return LoadLibraryW(libName);
#else
    return dlopen(libName, RTLD_LAZY | RTLD_LOCAL);
#endif
}

/**
 * @brief Frees a library from the process.
 *
 * @param lib The library to free
 */
NGFX_FUNCTION void NGFX_FreeLib(void* lib)
{
#if defined(_WIN32)
    (void)FreeLibrary((HMODULE)lib);
#else
    (void)dlclose(lib);
#endif
}

/**
 * @brief Loads a library into the process.
 *
 * @param libName The name of the library.
 * @return A handle to the loaded library, or NULL on failure.
 */
NGFX_FUNCTION void NGFX_SetLibraryLoadFn(const NGFX_LoadLibraryFnPtr fnPtr)
{
    g_NGFX_Globals.loadLibraryFnPtr = fnPtr;
}

/**
 * @brief Retrieves the address of a function from a library.
 *
 * @param lib The library handle.
 * @param name The name of the function.
 * @param defaultProc The default function to return if not found.
 * @return The address of the function, or defaultProc if not found.
 */
NGFX_FUNCTION void* NGFX_GetProc(void* lib, const char* name, void* defaultProc)
{
#if defined(_WIN32)
    void* proc = (void*)GetProcAddress((HMODULE)lib, name);
    if (proc)
    {
        return proc;
    }
    return defaultProc;
#else
    void* proc = dlsym(lib, name);
    if (proc)
    {
        return proc;
    }
    return defaultProc;
#endif
}

// Helper function to construct absolute library path for Win32
NGFX_FUNCTION NGFX_PathChar* NGFX_AllocAbsoluteLibPath(const NGFX_PathChar* installationPath, const NGFX_PathChar* libName)
{
#if defined(_WIN32)
    // Insert "\\target\\" NGFX_WSTR(NGFX_VAR_NAME) "\\" between installationPath and libName
    const NGFX_PathChar* targetSubdir = L"\\target\\" NGFX_VAR_LNAME L"\\";

    size_t installationPathLen = wcslen(installationPath);
    size_t targetSubdirLen = wcslen(targetSubdir);
    size_t libNameLen = wcslen(libName);
    size_t absLibStrLen = installationPathLen + targetSubdirLen + libNameLen + 1; // +1 for null terminator

    NGFX_PathChar* absLibPath = (NGFX_PathChar*)malloc(sizeof(NGFX_PathChar) * absLibStrLen);
    if (!absLibPath)
    {
        NGFX_LOG("NGFX_AllocAbsoluteLibPath: Failed to allocate memory for absolute library path");
        return NULL;
    }
    // Copy installationPath
    StringCchCopyW(absLibPath, absLibStrLen, installationPath);
    // Add target subdir
    StringCchCatW(absLibPath, absLibStrLen, targetSubdir);
    // Add libName
    StringCchCatW(absLibPath, absLibStrLen, libName);
    return absLibPath;
#else
    const NGFX_PathChar* targetSubdir = "/target/" NGFX_VAR_NAME "/";

    size_t installationPathLen = strlen(installationPath);
    size_t targetSubdirLen = strlen(targetSubdir);
    size_t libNameLen = strlen(libName);
    size_t absLibStrLen = installationPathLen + targetSubdirLen + libNameLen + 1; // +1 for null terminator

    NGFX_PathChar* absLibPath = (NGFX_PathChar*)malloc(sizeof(NGFX_PathChar) * absLibStrLen);
    if (!absLibPath)
    {
        NGFX_LOG("NGFX_AllocAbsoluteLibPath: Failed to allocate memory for absolute library path");
        return NULL;
    }
    strcpy(absLibPath, installationPath);
    strcat(absLibPath, targetSubdir);
    strcat(absLibPath, libName);
    return absLibPath;
#endif
}

// Helper functions for installation enumeration
/**
 * @brief Parse version string into major, minor, and patch components with SKU detection.
 *
 * @param versionString The version string to parse (e.g., "2025.1.0" or "Nsight Graphics Pro 2025.1.0").
 * @param major Output parameter for major version.
 * @param minor Output parameter for minor version.
 * @param patch Output parameter for patch version.
 * @param sku Output parameter for SKU type.
 */
NGFX_FUNCTION void NGFX_Do_ParseVersionAndSku(const NGFX_PathChar* versionString, uint16_t* major, uint16_t* minor, uint16_t* patch, NGFX_Injection_SKU* sku)
{
    if (!versionString)
    {
        NGFX_LOG("NGFX_Do_ParseVersionAndSku: Invalid parameter - versionString is NULL");
        return;
    }

    *major = 0;
    *minor = 0;
    *patch = 0;
    *sku = NGFX_Injection_SKU_PUBLIC;

    // Extract the version part from the string
    const NGFX_PathChar* versionPart = versionString;
#if defined(_WIN32)
    // e.g., "Nsight Graphics Pro 2025.1.0" -> "2025.1.0"
    const NGFX_PathChar* separatorPos = wcsrchr(versionString, L' ');
#else
    // e.g., "nsight-graphics-for-linux-pro-2025.1.0.0" -> "2025.1.0.0"
    //       "NVIDIA-Nsight-Graphics_Internal-2025.4" -> "2025.4"
    const NGFX_PathChar* separatorPos = strrchr(versionString, '-');
#endif
    if (separatorPos)
    {
        versionPart = separatorPos + 1; // Skip the separator
    }

    // Parse version components
#if defined(_WIN32)
    swscanf_s(versionPart, L"%hu.%hu.%hu", major, minor, patch);
#else
    sscanf(versionPart, "%hu.%hu.%hu", major, minor, patch);
#endif

    // Parse SKU from the original string
#if defined(_WIN32)
    if (wcsstr(versionString, L"Pro") != NULL)
#else
    if (strstr(versionString, "Pro") != NULL || strstr(versionString, "pro") != NULL)
#endif
    {
        *sku = NGFX_Injection_SKU_PRO;
    }
#if defined(_WIN32)
    else if (wcsstr(versionString, L"Internal") != NULL)
#else
    else if (strstr(versionString, "Internal") != NULL || strstr(versionString, "internal") != NULL)
#endif
    {
        *sku = NGFX_Injection_SKU_INTERNAL;
    }
}

/**
 * @brief Allocate and copy a path string.
 *
 * @param sourcePath The source path to copy.
 * @return Pointer to allocated path string, or NULL on failure.
 */
NGFX_FUNCTION NGFX_PathChar* NGFX_Do_AllocPathCopy(const NGFX_PathChar* sourcePath)
{
    if (!sourcePath)
    {
        NGFX_LOG("NGFX_Do_AllocPathCopy: Invalid parameter - sourcePath is NULL");
        return NULL;
    }

#if defined(_WIN32)
    size_t pathLen = wcslen(sourcePath) + 1;
    NGFX_PathChar* pathCopy = (NGFX_PathChar*)malloc(pathLen * sizeof(NGFX_PathChar));
    if (pathCopy)
    {
        wcscpy_s(pathCopy, pathLen, sourcePath);
    }
#else
    size_t pathLen = strlen(sourcePath) + 1;
    NGFX_PathChar* pathCopy = (NGFX_PathChar*)malloc(pathLen * sizeof(NGFX_PathChar));
    if (pathCopy)
    {
        strcpy(pathCopy, sourcePath);
    }
#endif
    return pathCopy;
}

/**
 * @brief Add a version to the installations array if it's not a duplicate.
 *
 * This function checks if an installation with the same path already exists in the array.
 * If not, it adds the new installation to the array.
 *
 * @param installations Array of installation info.
 * @param maxInstallations Maximum number of installations that can be stored in the array.
 * @param numInstallations Current number of installations in the array.
 * @param installationPath Path to the installation.
 * @param major Major version number.
 * @param minor Minor version number.
 * @param patch Patch version number.
 * @param sku SKU type.
 * @return true if the installation was added, false if it was a duplicate or couldn't be added.
 */
NGFX_FUNCTION bool NGFX_Do_AddVersion(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations, const NGFX_PathChar* installationPath, uint16_t major, uint16_t minor, uint16_t patch, NGFX_Injection_SKU sku)
{
    if (!installations || !numInstallations || !installationPath || *numInstallations >= maxInstallations)
    {
        NGFX_LOG("NGFX_Do_AddVersion: Invalid parameter or installations array is full");
        return false;
    }

    // Check for duplicates by comparing installation paths
    for (uint32_t i = 0; i < *numInstallations; ++i)
    {
        if (installations[i].installationPath)
        {
#if defined(_WIN32)
            if (wcscmp(installations[i].installationPath, installationPath) == 0)
#else
            if (strcmp(installations[i].installationPath, installationPath) == 0)
#endif
            {
                NGFX_LOG("NGFX_Do_AddVersion: Duplicate installation path found, skipping");
                return false; // Duplicate found
            }
        }
    }

    // Allocate and copy the installation path
    NGFX_PathChar* pathCopy = NGFX_Do_AllocPathCopy(installationPath);
    if (!pathCopy)
    {
        NGFX_LOG("NGFX_Do_AddVersion: Failed to allocate memory for installation path copy");
        return false;
    }

    // Store the installation info
    installations[*numInstallations].installationPath = pathCopy;
    installations[*numInstallations].versionMajor = major;
    installations[*numInstallations].versionMinor = minor;
    installations[*numInstallations].versionPatch = patch;
    installations[*numInstallations].sku = sku;

    (*numInstallations)++;
    return true;
}

/**
 * @brief Loads the injection library for a specified activity type.
 *
 * @note Only supported on Windows.
 *
 * This function loads the appropriate injection library based on the provided activity type
 * and installation path. It sets the necessary directory for the library and validates the
 * environment before loading the library.
 *
 * @param activityType The type of activity for which the injection library is to be loaded.
 * @param installationPath The path to the installation directory of the injection library.
 * @param settings Optional pointer to a Ngfx_GraphicsCapture_Settings, Ngfx_GPUTrace_Settings, or Ngfx_SystemProfiling_Settings structure depending on the activity type.
 * @return NGFX_Result indicating success or failure of the operation.
 */
NGFX_FUNCTION NGFX_Result NGFX_LoadInjectionLib(NGFX_ActivityType activityType, const NGFX_PathChar* installationPath, void* settings, void** loadedLib)
{
    if (!loadedLib)
    {
        NGFX_LOG("NGFX_LoadInjectionLib: Invalid parameter - loadedLib is NULL");
        return NGFX_Result_InvalidParameter;
    }
    if (!installationPath)
    {
        NGFX_LOG("NGFX_LoadInjectionLib: Invalid parameter - installationPath is NULL");
        return NGFX_Result_InvalidParameter;
    }

    *loadedLib = NULL;

    switch (activityType)
    {
    case NGFX_ActivityType_GraphicsCapture:
    case NGFX_ActivityType_GPUTrace:
        break;
    // System Profiling is not currently supported for bootstrap-based loading
    case NGFX_ActivityType_SystemProfiling:
    case NGFX_ActivityType_COUNT:
    default:
        NGFX_LOG("NGFX_LoadInjectionLib: Activity type %d not implemented or invalid", activityType);
        return NGFX_Result_NotImplemented;
    }

    NGFX_PathChar* absBootstrapLib = NGFX_AllocAbsoluteLibPath(installationPath, NGFX_API_BOOTSTRAP_LIB_NAME);

    void* loadedBootstrapLib = (*g_NGFX_Globals.loadLibraryFnPtr)(absBootstrapLib);
    free(absBootstrapLib);
    if (!loadedBootstrapLib)
    {
        NGFX_LOG("NGFX_LoadInjectionLib: Failed to load bootstrap library");
        return NGFX_Result_LibNotFound;
    }

    typedef NGFX_Result (*NGFX_InjectionSettings_ValidateAndSet_Fn)(NGFX_ActivityType activityType, void* settings);
    NGFX_InjectionSettings_ValidateAndSet_Fn ValidateAndSet =
        (NGFX_InjectionSettings_ValidateAndSet_Fn)NGFX_GetProc(
            loadedBootstrapLib,
            "NGFX_InjectionSettings_ValidateAndSet",
            NULL);

    if (!ValidateAndSet)
    {
        NGFX_LOG("NGFX_LoadInjectionLib: Failed to find NGFX_InjectionSettings_ValidateAndSet function");
        NGFX_FreeLib(loadedBootstrapLib);
        return NGFX_Result_InvalidLib;
    }

    NGFX_Result validateResult = ValidateAndSet(activityType, settings);
    if (validateResult != NGFX_Result_Success)
    {
        NGFX_LOG("NGFX_LoadInjectionLib: Settings validation failed with result %d for activity type %d", validateResult, activityType);
        NGFX_FreeLib(loadedBootstrapLib);
        return validateResult;
    }

    NGFX_FreeLib(loadedBootstrapLib);

    NGFX_PathChar* absGraphicsCaptureInjectionLib = NULL;
    NGFX_PathChar* absGraphicsCaptureTargetLib = NULL;
    NGFX_PathChar* absGPUTraceInjectionLib = NULL;
    NGFX_PathChar* absGPUTraceTargetLib = NULL;
    // Suppress unused warnings
    (void)absGraphicsCaptureInjectionLib;
    (void)absGraphicsCaptureTargetLib;
    (void)absGPUTraceInjectionLib;
    (void)absGPUTraceTargetLib;

    void* loadedInjectionLib = NULL;
    void* loadedTargetLib = NULL;
    switch (activityType)
    {
    case NGFX_ActivityType_GraphicsCapture:
        absGraphicsCaptureInjectionLib = NGFX_AllocAbsoluteLibPath(installationPath, NGFX_GRAPHICS_CAPTURE_INJECTION_LIB_NAME);
        loadedInjectionLib = (*g_NGFX_Globals.loadLibraryFnPtr)(absGraphicsCaptureInjectionLib);
        if (loadedInjectionLib)
        {
            absGraphicsCaptureTargetLib = NGFX_AllocAbsoluteLibPath(installationPath, NGFX_GRAPHICS_CAPTURE_TARGET_LIB_NAME);
            loadedTargetLib = (*g_NGFX_Globals.loadLibraryFnPtr)(absGraphicsCaptureTargetLib);
        }
        break;
    case NGFX_ActivityType_GPUTrace:
        absGPUTraceInjectionLib = NGFX_AllocAbsoluteLibPath(installationPath, NGFX_GPU_TRACE_INJECTION_LIB_NAME);
        loadedInjectionLib = (*g_NGFX_Globals.loadLibraryFnPtr)(absGPUTraceInjectionLib);
        if (loadedInjectionLib)
        {
            absGPUTraceTargetLib = NGFX_AllocAbsoluteLibPath(installationPath, NGFX_GPU_TRACE_TARGET_LIB_NAME);
            loadedTargetLib = (*g_NGFX_Globals.loadLibraryFnPtr)(absGPUTraceTargetLib);
        }
        break;
// System Profiling is not currently supported for bootstrap-based loading
#if !defined(__COVERITY__)
    case NGFX_ActivityType_SystemProfiling:
#endif
#ifndef __COVERITY__
    case NGFX_ActivityType_COUNT:
    default:
        break;
#endif
    }

    free(absGraphicsCaptureInjectionLib);
    free(absGraphicsCaptureTargetLib);
    free(absGPUTraceInjectionLib);
    free(absGPUTraceTargetLib);

    if (!loadedInjectionLib)
    {
        NGFX_LOG("NGFX_LoadInjectionLib: Failed to load injection library for activity type %d", activityType);
        return NGFX_Result_LibNotFound;
    }

    if (!loadedTargetLib)
    {
        NGFX_LOG("NGFX_LoadInjectionLib: Failed to load target library for activity type %d", activityType);
        // coverity[leaked_storage] - we do not attempt to unload, as error is unexpected
        return NGFX_Result_LibNotFound;
    }

    *loadedLib = loadedInjectionLib;
    // coverity[leaked_storage] - the libraries are expected to remain open for the duration of this process
    return NGFX_Result_Success;
}

/**
 * @brief Retrieves the handle of the activity library.
 *
 * This function retrieves the handle of the activity library based on the specified activity type
 * and whether it is an injection library or not. It sets the activity type and returns the library handle.
 *
 * @param activityType Pointer to the activity type to be set.
 * @param injectionLib Boolean indicating whether to load the injection library or not.
 * @param libHandle Pointer to a variable that will receive the library handle.
 * @return NGFX_Result indicating success or failure of the operation.
 */
NGFX_FUNCTION NGFX_Result NGFX_GetActivityLibHandle(NGFX_ActivityType* activityType, bool injectionLib, void** libHandle)
{
    if (!activityType || !libHandle)
    {
        NGFX_LOG("NGFX_GetActivityLibHandle: Invalid parameter - activityType or libHandle is NULL");
        return NGFX_Result_InvalidParameter;
    }

    for (int i = 0; i < NGFX_ActivityType_COUNT; ++i)
    {
        switch (i)
        {
        case NGFX_ActivityType_GraphicsCapture:
            if (injectionLib)
            {
                *libHandle = NGFX_GetLibHandle(NGFX_GRAPHICS_CAPTURE_INJECTION_LIB_NAME);
            }
            else
            {
                *libHandle = NGFX_GetLibHandle(NGFX_GRAPHICS_CAPTURE_TARGET_LIB_NAME);
            }
            break;
        case NGFX_ActivityType_GPUTrace:
            if (injectionLib)
            {
                *libHandle = NGFX_GetLibHandle(NGFX_GPU_TRACE_INJECTION_LIB_NAME);
            }
            else
            {
                *libHandle = NGFX_GetLibHandle(NGFX_GPU_TRACE_TARGET_LIB_NAME);
            }
            break;
        case NGFX_ActivityType_SystemProfiling:
            if (injectionLib)
            {
                *libHandle = NGFX_GetLibHandle(NGFX_SYSTEM_PROFILING_INJECTION_LIB_NAME);
            }
            else
            {
                *libHandle = NGFX_GetLibHandle(NGFX_SYSTEM_PROFILING_TARGET_LIB_NAME);
            }
            break;
#ifndef __COVERITY__
        case NGFX_ActivityType_COUNT:
        default:
            NGFX_LOG("NGFX_GetActivityLibHandle: Invalid activity type");
            return NGFX_Result_InvalidParameter;
#endif
        }

        if (*libHandle)
        {
            if (*activityType == (NGFX_ActivityType)i)
            {
                return NGFX_Result_Success;
            }
            else
            {
                NGFX_LOG("NGFX_GetActivityLibHandle: Different activity already injected (expected: %d, found: %d)", *activityType, i);
                *activityType = (NGFX_ActivityType)i;
                return NGFX_Result_DifferentActivityInjected;
            }
        }
    }

    NGFX_LOG("NGFX_GetActivityLibHandle: Activity library not found");
    return NGFX_Result_LibNotFound;
}

/**
 * @brief Checks if an activity is injected.
 *
 * This function checks if the specified activity type is currently injected into the process.
 *
 * @param activityType The type of activity to check for injection.
 * @param injected Pointer to a boolean that will be set to true if the activity is injected, false otherwise.
 * @return NGFX_Result indicating success or failure of the operation.
 */
NGFX_FUNCTION NGFX_Result NGFX_IsActivityInjected(NGFX_ActivityType activityType, bool* injected)
{
    if (!injected)
    {
        NGFX_LOG("NGFX_IsActivityInjected: Invalid parameter - injected is NULL");
        return NGFX_Result_InvalidParameter;
    }

    if (g_NGFX_Globals.injectedActivityType == activityType)
    {
        return NGFX_Result_Success;
    }

    void* libHandle;
    // coverity[alloc_arg] NGFX_GetActivityLibHandle does not add refs or allocate
    NGFX_Result result = NGFX_GetActivityLibHandle(&activityType, true, &libHandle);
    if (result == NGFX_Result_Success)
    {
        *injected = true;
        g_NGFX_Globals.injectedActivityType = activityType;
        return NGFX_Result_Success;
    }
    else if (result == NGFX_Result_LibNotFound)
    {
        *injected = false;
        return NGFX_Result_Success;
    }

    return result;
}

/**
 * @brief Injects an activity into the process.
 *
 * @note Only supported on Windows.
 *
 * This function injects the specified activity type into the process by loading the appropriate
 * injection library. It checks if the activity is already injected and loads the library if not.
 *
 * @param activityType The type of activity to inject.
 * @param installationPath The path to the installation directory of the injection library.
 * @param settings Optional pointer to a Ngfx_GraphicsCapture_Settings, Ngfx_GPUTrace_Settings, or Ngfx_SystemProfiling_Settings structure depending on the activity type.
 * @return NGFX_Result indicating success or failure of the operation.
 */
NGFX_FUNCTION NGFX_Result NGFX_InjectActivity(NGFX_ActivityType activityType, const NGFX_PathChar* installationPath, void* settings)
{
    bool isActivityInjected;
    NGFX_Result result = NGFX_IsActivityInjected(activityType, &isActivityInjected);
    if (result != NGFX_Result_Success)
    {
        NGFX_LOG("NGFX_InjectActivity: Failed to check if activity is injected, result: %d", result);
        return result;
    }

    if (isActivityInjected)
    {
        return NGFX_Result_Success;
    }

    void* libHandle = NULL;
    result = NGFX_LoadInjectionLib(activityType, installationPath, settings, &libHandle);
    if (result != NGFX_Result_Success)
    {
        NGFX_LOG("NGFX_InjectActivity: Failed to load injection library, result: %d", result);
        return result;
    }

    if (!libHandle)
    {
        NGFX_LOG("NGFX_InjectActivity: Injection library handle is NULL after loading");
        return NGFX_Result_LibNotFound;
    }

    // coverity[leaked_storage] - loading the injection lib is expected
    return NGFX_Result_Success;
}

/**
 * @brief Checks if an activity is initialized.
 *
 * This function checks if the specified activity type is currently initialized.
 *
 * @param activityType The type of activity to check for initialization.
 * @param initialized Pointer to a boolean that will be set to true if the activity is initialized, false otherwise.
 * @return NGFX_Result indicating success or failure of the operation.
 */
NGFX_FUNCTION NGFX_Result NGFX_IsActivityInitialized(NGFX_ActivityType activityType, bool* initialized)
{
    if (!initialized)
    {
        NGFX_LOG("NGFX_IsActivityInitialized: Invalid parameter - initialized is NULL");
        return NGFX_Result_InvalidParameter;
    }

    if (g_NGFX_Globals.initializedActivityType == NGFX_ActivityType_COUNT)
    {
        *initialized = false;
        return NGFX_Result_Success;
    }
    else if (g_NGFX_Globals.initializedActivityType != activityType)
    {
        NGFX_LOG("NGFX_IsActivityInitialized: Different activity already initialized (requested: %d, initialized: %d)", activityType, g_NGFX_Globals.initializedActivityType);
        *initialized = false;
        return NGFX_Result_DifferentActivityInjected;
    }

    *initialized = true;
    return NGFX_Result_Success;
}

/**
 * @brief Initializes the activity for the specified activity type.
 *
 * This function initializes the activity for the specified activity type by loading the appropriate
 * target library and setting up the function pointers. It checks if the activity is already initialized
 * and returns an error if it is.
 *
 * NOTE: This function is an internal implementation detail and should not be called directly.
 *
 * @param activityType The type of activity to initialize.
 * @param apiType The api for the specified activity to initialize.
 * @param initializeActivityParams The parameters associated with the activity initialization.
 * @return NGFX_Result indicating success or failure of the operation.
 */
NGFX_FUNCTION NGFX_Result NGFX_Do_InitializeActivity(NGFX_ActivityType activityType, NGFX_ApiType apiType, void* initializeActivityParams)
{
    if (!initializeActivityParams)
    {
        NGFX_LOG("NGFX_Do_InitializeActivity: Invalid parameter - initializeActivityParams is NULL");
        return NGFX_Result_InvalidParameter;
    }

    if (g_NGFX_Globals.initializedActivityType == activityType && g_NGFX_Globals.initializedApiType == apiType)
    {
        return NGFX_Result_Success;
    }
    else if (g_NGFX_Globals.initializedActivityType != NGFX_ActivityType_COUNT || g_NGFX_Globals.initializedApiType != NGFX_ApiType_COUNT)
    {
        NGFX_LOG("NGFX_Do_InitializeActivity: Different activity/API already initialized (activity: %d, API: %d)", g_NGFX_Globals.initializedActivityType, g_NGFX_Globals.initializedApiType);
        return NGFX_Result_DifferentActivityInjected;
    }

    bool isActivityInjected;
    NGFX_Result result = NGFX_IsActivityInjected(activityType, &isActivityInjected);
    if (result != NGFX_Result_Success)
    {
        NGFX_LOG("NGFX_Do_InitializeActivity: Failed to check if activity is injected, result: %d", result);
        return result;
    }

    if (!isActivityInjected)
    {
        NGFX_LOG("NGFX_Do_InitializeActivity: Activity type %d is not injected", activityType);
        return NGFX_Result_InvalidState;
    }

    void* libHandle = NULL;
    result = NGFX_GetActivityLibHandle(&activityType, false, &libHandle);
    if (result != NGFX_Result_Success || !libHandle)
    {
        NGFX_LOG("NGFX_Do_InitializeActivity: Failed to get library handle for activity type %d", activityType);
        return NGFX_Result_LibNotFound;
    }

    NGFX_FOR_EACH_CUDA_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_CUDART_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_D3D12_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_OPENGL_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_VULKAN_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_GRAPHICS_CAPTURE_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_GPU_TRACE_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_GPU_TRACE_OPENGL_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_GPU_TRACE_CUDA_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);
    NGFX_FOR_EACH_SYSTEMPROFILING_FN(NGFX_LOAD_FUNC_TABLE_ENTRY);

    switch (activityType)
    {
    case NGFX_ActivityType_GraphicsCapture:
        switch (apiType)
        {
        case NGFX_ApiType_CUDA:
            NGFX_LOG("NGFX_Do_InitializeActivity: CUDA API not supported for GraphicsCapture");
            result = NGFX_Result_InvalidParameter;
            break;
        case NGFX_ApiType_CUDART:
            NGFX_LOG("NGFX_Do_InitializeActivity: CUDART API not supported for GraphicsCapture");
            result = NGFX_Result_InvalidParameter;
            break;
        case NGFX_ApiType_D3D12:
            result = g_NGFX_Globals.GraphicsCapture.InitializeCaptureActivityD3D12((NGFX_GraphicsCapture_InitializeActivity_D3D12_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_OpenGL:
            NGFX_LOG("NGFX_Do_InitializeActivity: OpenGL API not supported for GraphicsCapture");
            result = NGFX_Result_InvalidParameter;
            break;
        case NGFX_ApiType_Vulkan:
            result = g_NGFX_Globals.GraphicsCapture.InitializeCaptureActivityVulkan((NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_COUNT:
        default:
            result = NGFX_Result_InvalidParameter;
            break;
        }
        break;
    case NGFX_ActivityType_GPUTrace:
        switch (apiType)
        {
        case NGFX_ApiType_CUDA:
            result = g_NGFX_Globals.GPUTrace.InitializeTraceActivityCUDA((NGFX_GPUTrace_InitializeActivity_CUDA_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_CUDART:
            result = g_NGFX_Globals.GPUTrace.InitializeTraceActivityCUDART((NGFX_GPUTrace_InitializeActivity_CUDART_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_D3D12:
            result = g_NGFX_Globals.GPUTrace.InitializeTraceActivityD3D12((NGFX_GPUTrace_InitializeActivity_D3D12_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_OpenGL:
            result = g_NGFX_Globals.GPUTrace.InitializeTraceActivityOpenGL((NGFX_GPUTrace_InitializeActivity_OpenGL_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_Vulkan:
            result = g_NGFX_Globals.GPUTrace.InitializeTraceActivityVulkan((NGFX_GPUTrace_InitializeActivity_Vulkan_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_COUNT:
        default:
            result = NGFX_Result_InvalidParameter;
            break;
        }
        break;
    case NGFX_ActivityType_SystemProfiling:
        switch (apiType)
        {
        case NGFX_ApiType_D3D12:
            result = g_NGFX_Globals.SystemProfiling.InitializeProfilingActivityD3D12((NGFX_SystemProfiling_InitializeActivity_D3D12_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_Vulkan:
            result = g_NGFX_Globals.SystemProfiling.InitializeProfilingActivityVulkan((NGFX_SystemProfiling_InitializeActivity_Vulkan_Params*)initializeActivityParams);
            break;
        case NGFX_ApiType_CUDA:
        case NGFX_ApiType_CUDART:
        case NGFX_ApiType_OpenGL:
        case NGFX_ApiType_COUNT:
        default:
            result = NGFX_Result_InvalidParameter;
            break;
        }
        break;
#ifndef __COVERITY__
    case NGFX_ActivityType_COUNT:
    default:
        NGFX_LOG("NGFX_Do_InitializeActivity: Invalid activity type %d", activityType);
        // coverity[leaked_storage] The library is expected to remain open for this application lifetime, even on failure
        return NGFX_Result_InvalidParameter;
        break;
#endif
    }

    if (result != NGFX_Result_Success)
    {
        NGFX_LOG("NGFX_Do_InitializeActivity: Activity initialization failed with result %d (activity: %d, API: %d)", result, activityType, apiType);
        NGFX_FOR_EACH_CUDA_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_CUDART_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_D3D12_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_OPENGL_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_VULKAN_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_GRAPHICS_CAPTURE_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_GPU_TRACE_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_GPU_TRACE_CUDA_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_GPU_TRACE_OPENGL_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);
        NGFX_FOR_EACH_SYSTEMPROFILING_FN(NGFX_ERASE_FUNC_TABLE_ENTRY);

        // coverity[leaked_storage] The library is expected to remain open for this application lifetime, even on failure
        return result;
    }

    g_NGFX_Globals.initializedActivityType = activityType;
    g_NGFX_Globals.initializedApiType = apiType;

    // coverity[leaked_storage] The library is expected to remain open for this application lifetime
    return NGFX_Result_Success;
}

/**
 * @endcond
 */

/**
 * END defgroup NGFX_API_CORE
 * @}
 */

#if defined(_WIN32)
/**
 * @brief Enumerates all Nsight Graphics installations found in the registry.
 *
 * This function searches the Windows registry for installed Nsight Graphics versions
 * and populates the provided array with installation information.
 *
 * @param installations Array to be populated with installation information.
 * @param maxInstallations Maximum number of installations that can be stored in the array.
 * @param numInstallations Pointer to the current number of installations in the array.
 *                         The function will append new installations starting from this position.
 *                         On return, this will be updated to reflect the total number of installations.
 * @return NGFX_Result indicating success or failure of the operation.
 *                     Note that even if the function indicates failure populated installations are valid.
 */
NGFX_FUNCTION NGFX_Result NGFX_Do_EnumerateInstallationsFromRegistry(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations)
{
    if (!installations || !numInstallations || maxInstallations == 0)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Invalid parameter");
        return NGFX_Result_InvalidParameter;
    }

    // This is the most reliable, structured key to enumerate all installed versions.
    const wchar_t* parentKeyString = L"SOFTWARE\\WOW6432Node\\NVIDIA Corporation\\Nsight VS Integration";

    // We must read the 64-bit registry, regardless of the bitness of the usage
    const DWORD samDesired = KEY_READ | KEY_WOW64_64KEY;

    HKEY hKeyParent = NULL;
    LONG err = RegOpenKeyExW(HKEY_LOCAL_MACHINE, parentKeyString, 0, samDesired, &hKeyParent);
    if (err != ERROR_SUCCESS)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to open registry key, error: %ld", err);
        return NGFX_Result_LibNotFound;
    }

    DWORD numSubkeys = 0;
    DWORD maxSubkeyLength = 0; // Note: This does NOT include the terminating null character.

    err = RegQueryInfoKeyW(hKeyParent, NULL, NULL, NULL, &numSubkeys, &maxSubkeyLength, NULL, NULL, NULL, NULL, NULL, NULL);
    if (err != ERROR_SUCCESS)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to query registry key info, error: %ld", err);
        RegCloseKey(hKeyParent);
        return NGFX_Result_LibNotFound;
    }

    // Process each subkey (version)
    for (DWORD i = 0; i < numSubkeys && *numInstallations < maxInstallations; ++i)
    {
        wchar_t* subkeyName = (NGFX_PathChar*)malloc((maxSubkeyLength + 1) * sizeof(NGFX_PathChar));
        if (!subkeyName)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to allocate memory for subkey name");
            continue;
        }

        err = RegEnumKeyW(hKeyParent, i, subkeyName, maxSubkeyLength + 1);
        if (err != ERROR_SUCCESS)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to enumerate registry subkey, error: %ld", err);
            free(subkeyName);
            continue;
        }

        // Construct child key path
        wchar_t childKey[512];
        StringCchPrintfW(childKey, sizeof(childKey) / sizeof(wchar_t), L"%s\\%s", parentKeyString, subkeyName);

        HKEY hKeyChild = NULL;
        err = RegOpenKeyExW(HKEY_LOCAL_MACHINE, childKey, 0, samDesired, &hKeyChild);
        if (err != ERROR_SUCCESS)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to open child registry key, error: %ld", err);
            free(subkeyName);
            continue;
        }

        DWORD bufferSize = 0;
        err = RegQueryValueExW(hKeyChild, L"PluginPath", NULL, NULL, NULL, &bufferSize);
        if (err != ERROR_SUCCESS)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to query PluginPath size, error: %ld", err);
            RegCloseKey(hKeyChild);
            free(subkeyName);
            continue;
        }

        wchar_t* buf = (wchar_t*)malloc(bufferSize);
        if (!buf)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to allocate memory for PluginPath buffer");
            RegCloseKey(hKeyChild);
            free(subkeyName);
            continue;
        }

        err = RegQueryValueExW(hKeyChild, L"PluginPath", NULL, NULL, (LPBYTE)buf, &bufferSize);
        RegCloseKey(hKeyChild);
        if (err != ERROR_SUCCESS)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: Failed to read PluginPath value, error: %ld", err);
            free(buf);
            free(subkeyName);
            continue;
        }

        // Find the position of "\\host\\" and truncate there
        wchar_t* hostPos = wcsstr(buf, L"\\host\\");
        if (!hostPos)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromRegistry: PluginPath does not contain \\host\\ marker");
            free(buf);
            free(subkeyName);
            continue;
        }
        *hostPos = L'\0';

        // Parse version and SKU using helper function
        uint16_t major = 0, minor = 0, patch = 0;
        NGFX_Injection_SKU sku = NGFX_Injection_SKU_UNKNOWN;
        NGFX_Do_ParseVersionAndSku(subkeyName, &major, &minor, &patch, &sku);

        // Add installation using helper function
        bool added = NGFX_Do_AddVersion(installations, maxInstallations, numInstallations, buf, major, minor, patch, sku);
        (void)added;

        // Free the original buffer since we've copied what we need
        free(buf);

        // Free the subkey name since we no longer need it
        free(subkeyName);
    }

    RegCloseKey(hKeyParent);
    return NGFX_Result_Success;
}
#endif

#if defined(_WIN32)
/**
 * @brief Enumerates all Nsight Graphics installations found in Program Files.
 *
 * This function searches the Program Files directory for installed Nsight Graphics versions
 * and populates the provided array with installation information.
 *
 * @param installations Array to be populated with installation information.
 * @param maxInstallations Maximum number of installations that can be stored in the array.
 * @param numInstallations Pointer to the current number of installations in the array.
 *                         The function will append new installations starting from this position.
 *                         On return, this will be updated to reflect the total number of installations.
 * @return NGFX_Result indicating success or failure of the operation.
 *                     Note that even if the function indicates failure populated installations are valid.
 */
NGFX_FUNCTION NGFX_Result NGFX_Do_EnumerateInstallationsFromProgramFiles(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations)
{
    if (!installations || !numInstallations || maxInstallations == 0)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromProgramFiles: Invalid parameter");
        return NGFX_Result_InvalidParameter;
    }

    // Gather results discovered from searching program files
    //
    // Note that this will not catch installations that aren't installed to program files,
    // and so there is a separate registry check in NGFX_Do_EnumerateInstallationsFromRegistry.
    {
        // Use environment variable instead of SHGetKnownFolderPath to avoid GUID/KNOWNFOLDERID type issues
        wchar_t programFilesPath[MAX_PATH];
        DWORD result = GetEnvironmentVariableW(L"ProgramFiles", programFilesPath, MAX_PATH);
        if (result == 0)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromProgramFiles: Failed to get ProgramFiles environment variable");
            return NGFX_Result_LibNotFound;
        }

        // Build the NVIDIA folder path
        wchar_t nvidiaFolder[MAX_PATH];
        wcscpy_s(nvidiaFolder, MAX_PATH, programFilesPath);
        wcscat_s(nvidiaFolder, MAX_PATH, L"\\NVIDIA Corporation\\");

        // Build the wildcard path for Nsight Graphics folders
        wchar_t nsightFolderWildcard[MAX_PATH];
        wcscpy_s(nsightFolderWildcard, MAX_PATH, nvidiaFolder);
        wcscat_s(nsightFolderWildcard, MAX_PATH, L"Nsight Graphics*");

        // Search for subfolders matching an expected pattern
        WIN32_FIND_DATAW findData = {0};
        HANDLE hCurrentNsightFolder = FindFirstFileW(nsightFolderWildcard, &findData);
        if (hCurrentNsightFolder != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (*numInstallations >= maxInstallations)
                {
                    break;
                }

                // Build the full installation path
                wchar_t installationPath[MAX_PATH];
                wcscpy_s(installationPath, MAX_PATH, nvidiaFolder);
                wcscat_s(installationPath, MAX_PATH, findData.cFileName);

                // Parse version and SKU using helper function
                uint16_t major = 0, minor = 0, patch = 0;
                NGFX_Injection_SKU sku = NGFX_Injection_SKU_UNKNOWN;
                NGFX_Do_ParseVersionAndSku(findData.cFileName, &major, &minor, &patch, &sku);

                // Add installation using helper function
                bool added = NGFX_Do_AddVersion(installations, maxInstallations, numInstallations, installationPath, major, minor, patch, sku);
                (void)added;
            } while (FindNextFileW(hCurrentNsightFolder, &findData));
            FindClose(hCurrentNsightFolder);
        }
    }
    return NGFX_Result_Success;
}
#endif

#if !defined(_WIN32)
/**
 * @brief Enumerates all Nsight Graphics installations found in the Debian packages management system.
 *
 * This function searches the Debian packages management system for installed Nsight Graphics versions
 * and populates the provided array with installation information.
 *
 * @param installations Array to be populated with installation information.
 * @param maxInstallations Maximum number of installations that can be stored in the array.
 * @param numInstallations Pointer to the current number of installations in the array.
 *                         The function will append new installations starting from this position.
 *                         On return, this will be updated to reflect the total number of installations.
 * @return NGFX_Result indicating success or failure of the operation.
 *                     Note that even if the function indicates failure populated installations are valid.
 */
NGFX_FUNCTION NGFX_Result NGFX_Do_EnumerateInstallationsFromDebian(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations)
{
    if (!installations || !numInstallations || maxInstallations == 0)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromDebian: Invalid parameter");
        return NGFX_Result_InvalidParameter;
    }

    if (system("which dpkg-query > /dev/null 2>&1") != 0)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromDebian: dpkg-query not found");
        return NGFX_Result_LibNotFound;
    }

    FILE* fileQueryPkgs = popen("dpkg-query -W -f '${db:Status-Abbrev} ${Package}\n' 'nsight-graphics*'", "r");
    if (!fileQueryPkgs)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromDebian: Failed to execute dpkg-query");
        return NGFX_Result_LibNotFound;
    }

    char packageLine[256];
    while (fgets(packageLine, sizeof(packageLine), fileQueryPkgs))
    {
        if (*numInstallations >= maxInstallations)
        {
            break;
        }

        // Parse the line e.g. "ii  nsight-graphics-for-linux-internal-2025.4.0.0" so
        // we confirm it's installed and extract the package name.
        if (strncmp(packageLine, "ii", 2) != 0)
        {
            continue;
        }
        const char* packageName = strrchr(packageLine, ' ') + 1;
        *strrchr(packageLine, '\n') = '\0';

        // Query the host path of the installation so we get the installation path.
        char command[256];
        snprintf(command, sizeof(command), "dpkg-query -L %s | grep %s/host", packageName, packageName);
        FILE* fileQueryHostPath = popen(command, "r");
        if (!fileQueryHostPath)
        {
            NGFX_LOG("NGFX_Do_EnumerateInstallationsFromDebian: Failed to execute dpkg-query for package host path");
            continue;
        }
        char installationPath[PATH_MAX];
        fgets(installationPath, sizeof(installationPath), fileQueryHostPath);
        *strstr(installationPath, "/host") = '\0';
        pclose(fileQueryHostPath);

        // Parse version and SKU using helper function
        uint16_t major = 0, minor = 0, patch = 0;
        NGFX_Injection_SKU sku = NGFX_Injection_SKU_UNKNOWN;
        NGFX_Do_ParseVersionAndSku(packageName, &major, &minor, &patch, &sku);

        // Add installation using helper function
        bool added = NGFX_Do_AddVersion(installations, maxInstallations, numInstallations, installationPath, major, minor, patch, sku);
        (void)added;
    }
    pclose(fileQueryPkgs);

    return NGFX_Result_Success;
}
#endif

#if !defined(_WIN32)
/**
 * @brief Enumerates all Nsight Graphics installations found in default destination.
 *
 * This function searches the default .run installation location of Nsight Graphics versions
 * and populates the provided array with installation information.
 *
 * @param installations Array to be populated with installation information.
 * @param maxInstallations Maximum number of installations that can be stored in the array.
 * @param numInstallations Pointer to the current number of installations in the array.
 *                         The function will append new installations starting from this position.
 *                         On return, this will be updated to reflect the total number of installations.
 * @return NGFX_Result indicating success or failure of the operation.
 *                     Note that even if the function indicates failure populated installations are valid.
 */
NGFX_FUNCTION NGFX_Result NGFX_Do_EnumerateInstallationsFromDefaultDestination(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations)
{
    if (!installations || !numInstallations || maxInstallations == 0)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromDefaultDestination: Invalid parameter");
        return NGFX_Result_InvalidParameter;
    }

    const char* homeDir = getenv("HOME");
    const char* baseDir = homeDir;
    if (!homeDir || strcmp(homeDir, "") == 0)
    {
        baseDir = "/usr/local";
    }

    char defaultDestinationPath[256];
    strcpy(defaultDestinationPath, baseDir);
    strcat(defaultDestinationPath, "/nvidia");

    DIR* dir = opendir(defaultDestinationPath);
    if (!dir)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromDefaultDestination: Failed to open directory: %s", defaultDestinationPath);
        return NGFX_Result_LibNotFound;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (*numInstallations >= maxInstallations)
        {
            break;
        }

        if (strncmp(entry->d_name, "NVIDIA-Nsight-Graphics", 22) != 0)
        {
            continue;
        }

        // Get the full installation path
        char installationPath[PATH_MAX];
        strcpy(installationPath, defaultDestinationPath);
        strcat(installationPath, "/");
        strcat(installationPath, entry->d_name);

        // Parse version and SKU using helper function
        uint16_t major = 0, minor = 0, patch = 0;
        NGFX_Injection_SKU sku = NGFX_Injection_SKU_UNKNOWN;
        NGFX_Do_ParseVersionAndSku(installationPath, &major, &minor, &patch, &sku);

        // Add installation using helper function
        bool added = NGFX_Do_AddVersion(installations, maxInstallations, numInstallations, installationPath, major, minor, patch, sku);
        (void)added;
    }
    closedir(dir);

    return NGFX_Result_Success;
}
#endif

#if defined(_WIN32)
/**
 * @brief Enumerates Nsight Graphics installations from the NSIGHT_GRAPHICS_DIR environment variable.
 *
 * This function checks for build locations specified via the NSIGHT_GRAPHICS_DIR environment variable
 * and populates the provided array with installation information. Builds specified this way take precedence.
 *
 * @param installations Array to be populated with installation information.
 * @param maxInstallations Maximum number of installations that can be stored in the array.
 * @param numInstallations Pointer to the current number of installations in the array.
 *                         The function will append new installations starting from this position.
 *                         On return, this will be updated to reflect the total number of installations.
 * @return NGFX_Result indicating success or failure of the operation.
 *                     Note that even if the function indicates failure populated installations are valid.
 */
NGFX_FUNCTION NGFX_Result NGFX_Do_EnumerateInstallationsFromEnv(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations)
{
    if (!installations || !numInstallations || maxInstallations == 0)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromEnv: Invalid parameter");
        return NGFX_Result_InvalidParameter;
    }

    wchar_t developmentInstallationPath[MAX_PATH];
    DWORD result = GetEnvironmentVariableW(L"NSIGHT_GRAPHICS_DIR", developmentInstallationPath, MAX_PATH);
    if (result > 0 && result < MAX_PATH && *numInstallations < maxInstallations)
    {

        NGFX_Injection_SKU sku = NGFX_Injection_SKU_PUBLIC;
        sku = NGFX_Injection_SKU_INTERNAL;

        // Add installation using helper function
        bool added = NGFX_Do_AddVersion(installations, maxInstallations, numInstallations, developmentInstallationPath, UINT16_MAX, UINT16_MAX, UINT16_MAX, sku);
        (void)added;
    }
    return NGFX_Result_Success;
}
#else
NGFX_FUNCTION NGFX_Result NGFX_Do_EnumerateInstallationsFromEnv(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations)
{
    if (!installations || !numInstallations || maxInstallations == 0)
    {
        NGFX_LOG("NGFX_Do_EnumerateInstallationsFromEnv: Invalid parameter");
        return NGFX_Result_InvalidParameter;
    }

    const char* developmentInstallationPath = getenv("NSIGHT_GRAPHICS_DIR");
    if (developmentInstallationPath && *developmentInstallationPath && *numInstallations < maxInstallations)
    {
        NGFX_Injection_SKU sku = NGFX_Injection_SKU_PUBLIC;
        sku = NGFX_Injection_SKU_INTERNAL;

        // Use the cross-platform helper
        bool added = NGFX_Do_AddVersion(installations, maxInstallations, numInstallations, developmentInstallationPath, UINT16_MAX, UINT16_MAX, UINT16_MAX, sku);
        (void)added;
    }
    return NGFX_Result_Success;
}
#endif

/**
 * @brief Comparison function for sorting installations by version.
 *
 * This function compares two NGFX_InstallationInfo structures by their version numbers.
 * Versions are compared in order: major, minor, patch. Higher versions come first.
 *
 * @param a Pointer to first installation info.
 * @param b Pointer to second installation info.
 * @return Negative if a > b, positive if a < b, zero if equal.
 */
NGFX_FUNCTION int NGFX_Do_CompareInstallationsByVersion(const void* a, const void* b)
{
    if (!a || !b)
    {
        NGFX_LOG("NGFX_Do_CompareInstallationsByVersion: Invalid parameter - a or b is NULL");
        return 0;
    }

    const NGFX_InstallationInfo* infoA = (const NGFX_InstallationInfo*)a;
    const NGFX_InstallationInfo* infoB = (const NGFX_InstallationInfo*)b;

    // Compare major version first
    if (infoA->versionMajor != infoB->versionMajor)
    {
        return (int)infoB->versionMajor - (int)infoA->versionMajor; // Higher versions first
    }

    // Compare minor version if major is equal
    if (infoA->versionMinor != infoB->versionMinor)
    {
        return (int)infoB->versionMinor - (int)infoA->versionMinor; // Higher versions first
    }

    // Compare patch version if minor is equal
    if (infoA->versionPatch != infoB->versionPatch)
    {
        return (int)infoB->versionPatch - (int)infoA->versionPatch; // Higher versions first
    }

    // Versions are equal
    return 0;
}

/**
 * @brief Enumerates all Nsight Graphics installations on the system.
 *
 * This function combines results from multiple sources to find all Nsight Graphics installations:
 * - Windows registry entries
 * - Program Files directory search
 * - NSIGHT_GRAPHICS_DIR environment variable
 *
 * The function populates the provided array with installation information from all sources.
 * Duplicate installations are not filtered out - the caller should handle deduplication if needed.
 * Call NGFX_FreeInstallations when done with the installations, even if this function indicates failure.
 *
 * @param installations Array to be populated with installation information.
 * @param maxInstallations Maximum number of installations that can be stored in the array.
 * @param numInstallations Pointer to receive the number of installations found.
 * @return NGFX_Result indicating success or failure of the operation.
 *                     Note that even if the function indicates failure populated installations are valid.
 */
NGFX_FUNCTION NGFX_Result NGFX_EnumerateInstallations(NGFX_InstallationInfo* installations, uint32_t maxInstallations, uint32_t* numInstallations)
{
    if (!installations || !numInstallations || maxInstallations == 0)
    {
        NGFX_LOG("NGFX_EnumerateInstallations: Invalid parameter");
        return NGFX_Result_InvalidParameter;
    }

    *numInstallations = 0;

    for (uint32_t i = 0; i < maxInstallations; ++i)
    {
        memset(&installations[i], 0, sizeof(NGFX_InstallationInfo));
    }

    NGFX_Result envResult = NGFX_Do_EnumerateInstallationsFromEnv(installations, maxInstallations, numInstallations);
    // Continue even if environment variable check fails

#if defined(_WIN32)
    NGFX_Result registryResult = NGFX_Do_EnumerateInstallationsFromRegistry(installations, maxInstallations, numInstallations);
    // Continue even if registry fails

    NGFX_Result programFilesResult = NGFX_Do_EnumerateInstallationsFromProgramFiles(installations, maxInstallations, numInstallations);
    // Continue even if Program Files search fails

    // Return failure only if all sub-functions failed
    if (registryResult != NGFX_Result_Success && programFilesResult != NGFX_Result_Success && envResult != NGFX_Result_Success)
    {
        NGFX_LOG("NGFX_EnumerateInstallations: All enumeration methods failed (registry: %d, programFiles: %d, env: %d)", registryResult, programFilesResult, envResult);
        return NGFX_Result_LibNotFound;
    }
#else
    NGFX_Result debianResult = NGFX_Do_EnumerateInstallationsFromDebian(installations, maxInstallations, numInstallations);
    // Continue even if debian fails

    NGFX_Result defaultDestResult = NGFX_Do_EnumerateInstallationsFromDefaultDestination(installations, maxInstallations, numInstallations);
    // Continue even if default destination search fails

    // Return failure only if all sub-functions failed
    if (debianResult != NGFX_Result_Success && defaultDestResult != NGFX_Result_Success && envResult != NGFX_Result_Success)
    {
        NGFX_LOG("NGFX_EnumerateInstallations: All enumeration methods failed (debian: %d, defaultDest: %d, env: %d)", debianResult, defaultDestResult, envResult);
        return NGFX_Result_LibNotFound;
    }
#endif

    // Sort installations by version (highest version first)
    if (*numInstallations > 0)
    {
        qsort(installations, *numInstallations, sizeof(NGFX_InstallationInfo), NGFX_Do_CompareInstallationsByVersion);
    }

    // Return success if we found any installations
    if (*numInstallations == 0)
    {
        NGFX_LOG("NGFX_EnumerateInstallations: No installations found");
        return NGFX_Result_LibNotFound;
    }
    return NGFX_Result_Success;
}

/**
 * @brief Frees memory allocated by NGFX_EnumerateInstallations.
 *
 * @param installations Array of installation info to free.
 * @param numInstallations Number of installations in the array.
 */
NGFX_FUNCTION void NGFX_FreeInstallations(NGFX_InstallationInfo* installations, uint32_t numInstallations)
{
    if (!installations)
    {
        return;
    }

    for (uint32_t i = 0; i < numInstallations; ++i)
    {
        if (installations[i].installationPath)
        {
            free(installations[i].installationPath);
        }
    }
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_CORE_H
