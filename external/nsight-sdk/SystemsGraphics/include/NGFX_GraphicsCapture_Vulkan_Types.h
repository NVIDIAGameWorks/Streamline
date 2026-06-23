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
// File: NGFX_GraphicsCapture_Vulkan_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GRAPHICS_CAPTURE_VULKAN_TYPES_H
#define NGFX_GRAPHICS_CAPTURE_VULKAN_TYPES_H

#include "NGFX_Vulkan_Types.h"

#include "NGFX_GraphicsCapture_Common_Types.h"

/**
 * @brief Parameters for injecting GraphicsCapture into a Vulkan application
 */
typedef struct NGFX_GraphicsCapture_Inject_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GraphicsCapture_Inject_Vulkan_Params_VER */

    const NGFX_PathChar* installationPath;            /**< The path to the installation directory of the GraphicsCapture library */
    NGFX_GraphicsCapture_InjectionSettings* settings; /**< Pointer to structure containing settings for injection. Expected to be non Null. */
} NGFX_GraphicsCapture_Inject_Vulkan_Params_V1;
typedef NGFX_GraphicsCapture_Inject_Vulkan_Params_V1 NGFX_GraphicsCapture_Inject_Vulkan_Params;
#define NGFX_GraphicsCapture_Inject_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GraphicsCapture_Inject_Vulkan_Params_V1, 1)
#define NGFX_GraphicsCapture_Inject_Vulkan_Params_VER NGFX_GraphicsCapture_Inject_Vulkan_Params_V1_VER

/**
 * @brief Parameters for initializing a Graphics capture in a Vulkan application
 */
typedef struct NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_VER */
} NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_V1;
typedef NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_V1 NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params;
#define NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_V1, 1)
#define NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_VER NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_V1_VER

/**
 * @brief Parameters for requesting a Graphics capture in a Vulkan application
 */
typedef struct NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_VER */

    NGFX_GraphicsCapture_Delimiter delimiter; /**< Specifies the delimiter type that the capture state machine responds to. */
    uint32_t framesBeforeStart;               /**< Specifies the number of frames to wait before capture is started. This may be zero to being capture immediately at the next delimiter */
    uint32_t framesToCapture;                 /**< Specifies the number of frames to capture. May be from [1,60]. */
} NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_V1;
typedef NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_V1 NGFX_GraphicsCapture_RequestCapture_Vulkan_Params;
#define NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_V1, 1)
#define NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_VER NGFX_GraphicsCapture_RequestCapture_Vulkan_Params_V1_VER

/**
 * @brief Parameters for starting a Graphics capture in a Vulkan application
 */
typedef struct NGFX_GraphicsCapture_StartCapture_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GraphicsCapture_StartCapture_Vulkan_Params_VER */
} NGFX_GraphicsCapture_StartCapture_Vulkan_Params_V1;
typedef NGFX_GraphicsCapture_StartCapture_Vulkan_Params_V1 NGFX_GraphicsCapture_StartCapture_Vulkan_Params;
#define NGFX_GraphicsCapture_StartCapture_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GraphicsCapture_StartCapture_Vulkan_Params_V1, 1)
#define NGFX_GraphicsCapture_StartCapture_Vulkan_Params_VER NGFX_GraphicsCapture_StartCapture_Vulkan_Params_V1_VER

/**
 * @brief Parameters for stopping a Graphics capture in a Vulkan application
 */
typedef struct NGFX_GraphicsCapture_StopCapture_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GraphicsCapture_StopCapture_Vulkan_Params_VER */
} NGFX_GraphicsCapture_StopCapture_Vulkan_Params_V1;
typedef NGFX_GraphicsCapture_StopCapture_Vulkan_Params_V1 NGFX_GraphicsCapture_StopCapture_Vulkan_Params;
#define NGFX_GraphicsCapture_StopCapture_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GraphicsCapture_StopCapture_Vulkan_Params_V1, 1)
#define NGFX_GraphicsCapture_StopCapture_Vulkan_Params_VER NGFX_GraphicsCapture_StopCapture_Vulkan_Params_V1_VER

#endif // NGFX_GRAPHICS_CAPTURE_VULKAN_TYPES_H
