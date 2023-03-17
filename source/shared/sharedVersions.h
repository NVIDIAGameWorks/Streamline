/*
* Copyright (c) 2022 NVIDIA CORPORATION. All rights reserved
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#pragma once

#include "include/sl_version.h"

#define SHARED_VERSION_MAJOR SL_VERSION_MAJOR
#define SHARED_VERSION_MINOR SL_VERSION_MINOR
#define SHARED_VERSION_PATCH SL_VERSION_PATCH
#if defined(SL_PRODUCTION)
#define BUILD_CONFIG_INFO "PRODUCTION"
#elif defined(SL_REL_EXT_DEV)
#define BUILD_CONFIG_INFO "DEVELOPMENT"
#elif defined(SL_DEBUG)
#define BUILD_CONFIG_INFO "DEBUG"
#elif defined(SL_RELEASE)
#define BUILD_CONFIG_INFO "RELEASE"
#elif defined(SL_PROFILING)
#define BUILD_CONFIG_INFO "PROFILING"
#else
#error "Unsupported build config"
#endif

#if defined(SL_PRODUCTION)
#define DISTRIBUTION_INFO "PRODUCTION"
#else
#define DISTRIBUTION_INFO "NOT FOR PRODUCTION"
#endif

