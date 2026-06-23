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
// File: NGFX_SystemProfiling_Common_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_SYSTEM_PROFILING_COMMON_TYPES_H
#define NGFX_SYSTEM_PROFILING_COMMON_TYPES_H

#include "NGFX_D3D12_Types.h"
#include "NGFX_Vulkan_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief struct containing SystemProfiling injection settings
 *
 * Note: Currently only contains version validation. Additional injection settings
 * and bootstrap loading support will be added in the future.
 */
typedef struct NGFX_SystemProfiling_InjectionSettings_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_SystemProfiling_InjectionSettings_VER */
} NGFX_SystemProfiling_InjectionSettings_V1;
typedef NGFX_SystemProfiling_InjectionSettings_V1 NGFX_SystemProfiling_InjectionSettings;
#define NGFX_SystemProfiling_InjectionSettings_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_SystemProfiling_InjectionSettings_V1, 1)
#define NGFX_SystemProfiling_InjectionSettings_VER NGFX_SystemProfiling_InjectionSettings_V1_VER

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_SYSTEM_PROFILING_COMMON_TYPES_H
