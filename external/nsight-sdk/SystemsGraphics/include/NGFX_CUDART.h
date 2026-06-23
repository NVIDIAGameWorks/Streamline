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
// File: NGFX_CUDART.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_CUDART_H
#define NGFX_CUDART_H

#include "Impl/NGFX_Core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Delimits a CUDART frame.
 *
 * This function signals the boundary of a CUDART frame.
 *
 * @param params Pointer to a structure containing parameters for delimiting the frame.
 * @return NGFX_Result_Success if the operation is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_FrameBoundary_CUDART(NGFX_FrameBoundary_CUDART_Params* params)
{
    return g_NGFX_Globals.CUDART.FrameBoundaryCUDART(params);
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_CUDART_H
