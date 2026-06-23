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
// File: NGFX_SystemProfiling_Vulkan_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_SYSTEM_PROFILING_VULKAN_TYPES_H
#define NGFX_SYSTEM_PROFILING_VULKAN_TYPES_H

#include "NGFX_Vulkan_Types.h"

#include "NGFX_SystemProfiling_Common_Types.h"

/**
 * @brief Parameters for injecting SystemProfiling into a Vulkan application
 */
typedef struct NGFX_SystemProfiling_Inject_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_SystemProfiling_Inject_Vulkan_Params_VER */

    const NGFX_PathChar* installationPath;            /**< The path to the installation directory of the SystemProfiling library */
    NGFX_SystemProfiling_InjectionSettings* settings; /**< Pointer to structure containing settings for injection. Expected to be non-null. */
} NGFX_SystemProfiling_Inject_Vulkan_Params_V1;
typedef NGFX_SystemProfiling_Inject_Vulkan_Params_V1 NGFX_SystemProfiling_Inject_Vulkan_Params;
#define NGFX_SystemProfiling_Inject_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_SystemProfiling_Inject_Vulkan_Params_V1, 1)
#define NGFX_SystemProfiling_Inject_Vulkan_Params_VER NGFX_SystemProfiling_Inject_Vulkan_Params_V1_VER

/**
 * @brief Parameters for initializing a System profiling activity in a Vulkan application
 */
typedef struct NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_VER */
} NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_V1;
typedef NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_V1 NGFX_SystemProfiling_InitializeActivity_Vulkan_Params;
#define NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_V1, 1)
#define NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_VER NGFX_SystemProfiling_InitializeActivity_Vulkan_Params_V1_VER

/**
 * @brief Parameters for starting system profiling in a Vulkan application
 */
typedef struct NGFX_SystemProfiling_StartProfiling_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_SystemProfiling_StartProfiling_Vulkan_Params_VER */
} NGFX_SystemProfiling_StartProfiling_Vulkan_Params_V1;
typedef NGFX_SystemProfiling_StartProfiling_Vulkan_Params_V1 NGFX_SystemProfiling_StartProfiling_Vulkan_Params;
#define NGFX_SystemProfiling_StartProfiling_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_SystemProfiling_StartProfiling_Vulkan_Params_V1, 1)
#define NGFX_SystemProfiling_StartProfiling_Vulkan_Params_VER NGFX_SystemProfiling_StartProfiling_Vulkan_Params_V1_VER

/**
 * @brief Parameters for stopping system profiling in a Vulkan application
 */
typedef struct NGFX_SystemProfiling_StopProfiling_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_SystemProfiling_StopProfiling_Vulkan_Params_VER */
} NGFX_SystemProfiling_StopProfiling_Vulkan_Params_V1;
typedef NGFX_SystemProfiling_StopProfiling_Vulkan_Params_V1 NGFX_SystemProfiling_StopProfiling_Vulkan_Params;
#define NGFX_SystemProfiling_StopProfiling_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_SystemProfiling_StopProfiling_Vulkan_Params_V1, 1)
#define NGFX_SystemProfiling_StopProfiling_Vulkan_Params_VER NGFX_SystemProfiling_StopProfiling_Vulkan_Params_V1_VER

#endif // NGFX_SYSTEM_PROFILING_VULKAN_TYPES_H
