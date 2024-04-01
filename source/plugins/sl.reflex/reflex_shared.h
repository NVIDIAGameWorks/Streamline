/*
* Copyright (c) 2022-2023 NVIDIA CORPORATION. All rights reserved
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

#include "include/sl_reflex.h"
#include "include/sl_struct.h"
#include "source/plugins/sl.pcl/pcl.h"

using PFun_slReflexSetMarker = sl::Result(sl::PCLMarker marker, const sl::FrameToken& frame);

namespace sl
{

namespace reflex
{
//! Internal shared data for Reflex
//! 
//! {9FB3064E-B6B6-44D8-82D8-709472F48951}
SL_STRUCT(ReflexInternalSharedData, StructType({ 0x9fb3064e, 0xb6b6, 0x44d8, { 0x82, 0xd8, 0x70, 0x94, 0x72, 0xf4, 0x89, 0x51 } }), kStructVersion1)

    //! BACKWARDS COMPATIBILITY MUST BE PRESERVED ALWAYS - NEVER CHANGE OR MOVE OLDER MEMBERS IN THIS STRUCTURE
    //! 
    //! v1 Members
    sl::Result(*slReflexSetMarker)(sl::PCLMarker marker, const sl::FrameToken& frame);

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

//! Enforcing offsets at the compile time to ensure members are not moved around
//! 
static_assert(offsetof(sl::reflex::ReflexInternalSharedData, slReflexSetMarker) == 32, "new elements can only be added at the end of each structure");

}
}
