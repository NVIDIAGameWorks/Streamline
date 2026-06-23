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
// File: NGFX_SystemProfiling_Common.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_SYSTEM_PROFILING_COMMON_H
#define NGFX_SYSTEM_PROFILING_COMMON_H

#include "Impl/NGFX_Core.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Initialize NGFX_SystemProfiling_InjectionSettings struct with default values.
 *
 * Note: Currently only sets version validation. Additional settings initialization
 * will be added when more injection options are supported.
 *
 * @param settings Struct to be populated
 */
NGFX_FUNCTION NGFX_Result NGFX_SystemProfiling_InjectionSettings_SetDefaults(NGFX_SystemProfiling_InjectionSettings* settings)
{
    if (!settings)
    {
        return NGFX_Result_InvalidParameter;
    }

    settings->version = NGFX_SystemProfiling_InjectionSettings_VER;

    return NGFX_Result_Success;
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_SYSTEM_PROFILING_COMMON_H
