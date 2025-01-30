/*
* Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
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

#include "include/sl_dlss.h"
#include "source/platforms/sl.chi/compute.h"

namespace sl
{
namespace dlss
{

//! Internal shared data for DLSS
//! 
//! {9A6136D9-4E4D-4EA6-A52B-47F13F7BB766}
SL_STRUCT_BEGIN(DLSSInternalSharedData, StructType({ 0x9a6136d9, 0x4e4d, 0x4ea6, { 0xa5, 0x2b, 0x47, 0xf1, 0x3f, 0x7b, 0xb7, 0x66 } }), kStructVersion1)

    //! BACKWARDS COMPATIBILITY MUST BE PRESERVED ALWAYS - NEVER CHANGE OR MOVE OLDER MEMBERS IN THIS STRUCTURE
    //! 
    //! v1 Members
    chi::Resource cachedUpscaledOutput;

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
SL_STRUCT_END()

//! Enforcing offsets at the compile time to ensure members are not moved around
//! 
static_assert(offsetof(sl::dlss::DLSSInternalSharedData, cachedUpscaledOutput) == 32, "new elements can only be added at the end of each structure");


}
}
