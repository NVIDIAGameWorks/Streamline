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
// File: NGFX_D3D12.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_D3D12_H
#define NGFX_D3D12_H

#include "Impl/NGFX_Core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Delimits a D3D12 frame and optionally provides information about the associated resources.
 *
 * This function signals the boundary of a D3D12 frame. It can optionally provide information
 * about the associated command queue and output resources that may be used by the activity
 * as a hint for resources of interest.
 *
 * @param params Pointer to a structure containing parameters for delimiting the frame.
 * @return NGFX_Result_Success if the operation is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_FrameBoundary_D3D12(NGFX_FrameBoundary_D3D12_Params* params)
{
    return g_NGFX_Globals.D3D12.FrameBoundaryD3D12(params);
}

/**
 * @brief Delimits sub-events when Streamline frame generation processes a Present call into
 * generated and real frames.
 */
NGFX_FUNCTION NGFX_Result NGFX_DLSS_FG_PresentBoundary_D3D12(NGFX_DLSS_FG_PresentBoundary_D3D12_Params* params)
{
    return g_NGFX_Globals.D3D12.DLSS_FG_PresentBoundaryD3D12(params);
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_D3D12_H
