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
// File: NGFX_D3D12_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_D3D12_TYPES_H
#define NGFX_D3D12_TYPES_H

#include "NGFX_Types.h"

// Must match D3D12 object declarations
/**
 * @cond INTERNAL_IMPLEMENTATION_DETAILS
 */
#define NGFX_DEFINE_D3D12_OBJECT(object) \
    struct object;                       \
    typedef struct object object;
/**
 * @endcond
 */

// Forward declarations of D3D12 objects
NGFX_DEFINE_D3D12_OBJECT(ID3D12CommandQueue);
NGFX_DEFINE_D3D12_OBJECT(ID3D12Resource);
NGFX_DEFINE_D3D12_OBJECT(DXGI_PRESENT_PARAMETERS);

/**
 * @brief Enum representing the type of D3D12 resources accepted by the API.
 */
typedef enum NGFX_ResourceType_D3D12
{
    NGFX_ResourceType_D3D12_Resource, /**< Identifies a resource of type ID3D12Resource. */
    NGFX_ResourceType_D3D12_COUNT,    /**< Total count of resource types. */
} NGFX_ResourceType_D3D12;

/**
 * @brief Structure describing a D3D12 resource.
 */
typedef struct NGFX_ResourceDescription_D3D12_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_ResourceDescription_D3D12_VER */

    NGFX_ResourceType_D3D12 type; /**< Specifies the type of resource within the union. */
    union
    {
        ID3D12Resource* resource; /**< An ID3D12Resource; paired with NGFX_ResourceType_D3D12_Resource. */
    };
} NGFX_ResourceDescription_D3D12_V1;
typedef NGFX_ResourceDescription_D3D12_V1 NGFX_ResourceDescription_D3D12;
#define NGFX_ResourceDescription_D3D12_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_ResourceDescription_D3D12_V1, 1)
#define NGFX_ResourceDescription_D3D12_VER NGFX_ResourceDescription_D3D12_V1_VER

/**
 * @brief Parameters for delimiting a D3D12 frame.
 *
 * Optionally provides information on the associated command queue and output resources
 * that may be used by the activity as a hint for resources of interest.
 */
typedef struct NGFX_FrameBoundary_D3D12_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_FrameBoundary_D3D12_Params_VER */

    ID3D12CommandQueue* commandQueue;                /**< The command queue associated with the end of frame operation. */
    NGFX_ResourceDescription_D3D12* outputResources; /**< Array of output resources of interest. */
    int numOutputResources;                          /**< Number of output resources in the array. */
} NGFX_FrameBoundary_D3D12_Params_V1;
typedef NGFX_FrameBoundary_D3D12_Params_V1 NGFX_FrameBoundary_D3D12_Params;
#define NGFX_FrameBoundary_D3D12_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_FrameBoundary_D3D12_Params_V1, 1)
#define NGFX_FrameBoundary_D3D12_Params_VER NGFX_FrameBoundary_D3D12_Params_V1_VER

/**
 * @brief Parameters for delimiting a D3D12 frame.
 *
 * Optionally provides information on the associated command queue and output resources
 * that may be used by the activity as a hint for resources of interest.
 */
typedef struct NGFX_DLSS_FG_PresentBoundary_D3D12_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_FrameBoundary_D3D12_Params_VER */

    NGFX_For_Dlss_DLSS_FG_PresentBoundaryType type; /**< Specifies the type of boundary to be created. */
    uint32_t generatedFrameIndex;                   /**< Zero based index, counting the number of generated frames so far issued for the current real frame */
    ID3D12CommandQueue* queue;                      /**< The command queue associated with the end of frame operation. */

    bool isPresent1;                                   /**< Identify application present type (Present or Present1) */
    uint32_t syncInterval;                             /**< Identify application sync interval */
    uint32_t presentFlags;                             /**< Identify application present flags */
    const DXGI_PRESENT_PARAMETERS* pPresentParameters; /**< Identify Present parameters */
    uint32_t requestedFrameIndex;                      /**< Zero based index, counting the number of application requested presents so far */

} NGFX_DLSS_FG_PresentBoundary_D3D12_Params_V1;
typedef NGFX_DLSS_FG_PresentBoundary_D3D12_Params_V1 NGFX_DLSS_FG_PresentBoundary_D3D12_Params;
#define NGFX_DLSS_FG_PresentBoundary_D3D12_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_DLSS_FG_PresentBoundary_D3D12_Params_V1, 1)
#define NGFX_DLSS_FG_PresentBoundary_D3D12_Params_VER NGFX_DLSS_FG_PresentBoundary_D3D12_Params_V1_VER


#endif // NGFX_D3D12_TYPES_H
