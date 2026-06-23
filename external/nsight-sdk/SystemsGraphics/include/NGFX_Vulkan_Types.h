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
// File: NGFX_Vulkan_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_VULKAN_TYPES_H
#define NGFX_VULKAN_TYPES_H

#include "NGFX_Types.h"

// Must match Vulkan handle definition
/**
 * @cond INTERNAL_IMPLEMENTATION_DETAILS
 */
#define NGFX_DEFINE_VULKAN_HANDLE(object) typedef struct object##_T* object;
/**
 * @endcond
 */

// Must match Vulkan struct declarations
/**
 * @cond INTERNAL_IMPLEMENTATION_DETAILS
 */
#define NGFX_DEFINE_VULKAN_STRUCT(object) \
    struct object;                        \
    typedef struct object object;
/**
 * @endcond
 */

// Forward declarations of Vulkan Handles
NGFX_DEFINE_VULKAN_HANDLE(VkBuffer);
NGFX_DEFINE_VULKAN_HANDLE(VkImage);
NGFX_DEFINE_VULKAN_HANDLE(VkQueue);
NGFX_DEFINE_VULKAN_STRUCT(VkPresentInfoKHR);

/**
 * @brief Enum representing the type of Vulkan resource.
 */
typedef enum NGFX_ResourceType_Vulkan
{
    NGFX_ResourceType_Vulkan_VkBuffer, /**< Vulkan buffer resource type. */
    NGFX_ResourceType_Vulkan_VkImage,  /**< Vulkan image resource type. */
    NGFX_ResourceType_Vulkan_COUNT,    /**< Total count of resource types. */
} NGFX_ResourceType_Vulkan;

/**
 * @brief Structure describing a Vulkan resource.
 */
typedef struct NGFX_ResourceDescription_Vulkan_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_ResourceDescription_Vulkan_VER */

    NGFX_ResourceType_Vulkan type; /**< The type of the Vulkan resource. */
    union
    {
        VkBuffer buffer; /**< Vulkan buffer resource; paired with NGFX_ResourceType_Vulkan_VkBuffer */
        VkImage image;   /**< Vulkan image resource; paired with NGFX_ResourceType_Vulkan_VkImage */
    };
} NGFX_ResourceDescription_Vulkan_V1;
typedef NGFX_ResourceDescription_Vulkan_V1 NGFX_ResourceDescription_Vulkan;
#define NGFX_ResourceDescription_Vulkan_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_ResourceDescription_Vulkan_V1, 1)
#define NGFX_ResourceDescription_Vulkan_VER NGFX_ResourceDescription_Vulkan_V1_VER

/**
 * @brief Parameters for delimiting a Vulkan frame.
 */
typedef struct NGFX_FrameBoundary_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_FrameBoundary_Vulkan_Params_VER */

    VkQueue queue;                                    /**< Vulkan queue used for the frame. */
    NGFX_ResourceDescription_Vulkan* outputResources; /**< Array of output resources; is sized to numOutputResources. Set this to NULL is unused. */
    int numOutputResources;                           /**< Number of output resources in the array. Set this to 0 if unused. */
} NGFX_FrameBoundary_Vulkan_Params_V1;
typedef NGFX_FrameBoundary_Vulkan_Params_V1 NGFX_FrameBoundary_Vulkan_Params;
#define NGFX_FrameBoundary_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_FrameBoundary_Vulkan_Params_V1, 1)
#define NGFX_FrameBoundary_Vulkan_Params_VER NGFX_FrameBoundary_Vulkan_Params_V1_VER

/**
 * @brief Parameters for delimiting a Vulkan frame.
 */
typedef struct NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_VER */

    NGFX_For_Dlss_DLSS_FG_PresentBoundaryType type; /**< Specifies the type of boundary to be created. */
    uint32_t generatedFrameIndex;                   /**< Zero based index, counting the number of generated frames so far issued for the current real frame */
    VkQueue queue;                                  /**< Vulkan queue used for the frame. */

    const VkPresentInfoKHR* pPresentInfo; /**< Application present info */
    uint32_t requestedFrameIndex;         /**< Zero based index, counting the number of application requested presents so far */
} NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_V1;
typedef NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_V1 NGFX_DLSS_FG_PresentBoundary_Vulkan_Params;
#define NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_V1, 1)
#define NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_VER NGFX_DLSS_FG_PresentBoundary_Vulkan_Params_V1_VER

#endif // NGFX_VULKAN_TYPES_H
