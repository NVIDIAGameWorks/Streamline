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
// File: Impl/NGFX_Defines.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_DEFINES_H
#define NGFX_DEFINES_H

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

#if defined(_MSC_VER)
#if defined(__cplusplus)
#define NGFX_GLOBAL extern "C" __declspec(selectany)
#define NGFX_FUNCTION extern "C" inline
#else
#define NGFX_GLOBAL __declspec(selectany)
#define NGFX_FUNCTION __inline
#endif
#elif defined(_WIN32) || defined(__CYGWIN__)
#if defined(__cplusplus)
#define NGFX_GLOBAL __declspec(selectany)
#define NGFX_FUNCTION extern "C" inline
#else
#define NGFX_GLOBAL __declspec(selectany)
#define NGFX_FUNCTION
#endif
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__cplusplus)
#define NGFX_GLOBAL __attribute__((visibility("hidden"), weak))
#define NGFX_FUNCTION extern "C" __attribute__((visibility("hidden"))) inline
#else
#define NGFX_GLOBAL __attribute__((visibility("hidden"), weak))
#define NGFX_FUNCTION __attribute__((visibility("hidden"), weak))
#endif
#else
#define NGFX_GLOBAL
#define NGFX_FUNCTION
#endif

#if defined(__cplusplus) && (__cplusplus >= 201703)
#define NGFX_MAYBE_UNUSED [[maybe_unused]]
#else
#define NGFX_MAYBE_UNUSED
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(__i386__)
#define NGFX_VAR_NAME "windows-desktop-nomad-x86"
#define NGFX_VAR_LNAME L"windows-desktop-nomad-x86"
#elif defined(__x86_64__) || defined(_M_X64)
#define NGFX_VAR_NAME "windows-desktop-nomad-x64"
#define NGFX_VAR_LNAME L"windows-desktop-nomad-x64"
#else
#define NGFX_VAR_NAME "unknown"
#define NGFX_VAR_LNAME L"unknown"
#endif
#elif defined(__linux__)
#if defined(__i386__)
#define NGFX_VAR_NAME "linux-desktop-nomad-x86"
#define NGFX_VAR_LNAME L"linux-desktop-nomad-x86"
#elif defined(__x86_64__)
#define NGFX_VAR_NAME "linux-desktop-nomad-x64"
#define NGFX_VAR_LNAME L"linux-desktop-nomad-x64"
#elif defined(__aarch64__)
#define NGFX_VAR_NAME "linux-v4l_l4t-nomad-t210-a64"
#define NGFX_VAR_LNAME L"linux-v4l_l4t-nomad-t210-a64"
#else
#define NGFX_VAR_NAME "unknown"
#define NGFX_VAR_LNAME L"unknown"
#endif
#elif defined(__QNX__)
#if defined(__aarch64__)
#define NGFX_VAR_NAME "qnx-nomad-t210-a64"
#define NGFX_VAR_LNAME L"qnx-nomad-t210-a64"
#else
#define NGFX_VAR_NAME "unknown"
#define NGFX_VAR_LNAME L"unknown"
#endif
#else
#define NGFX_VAR_NAME "unknown"
#define NGFX_VAR_LNAME L"unknown"
#endif

/**
 * @cond INTERNAL_IMPLEMENTATION_DETAILS
 */
#define NGFX_DO_WSTR(x) L##x
#define NGFX_WSTR(x) NGFX_DO_WSTR(x)
/**
 * @endcond
 */

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#endif // NGFX_DEFINES_H
