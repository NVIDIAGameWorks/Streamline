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

#include <string>

#include "include/sl.h"
#include "include/sl_struct.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.log/log.h"

namespace sl
{

namespace internal
{

namespace shared
{

//! Status codes
//! 
enum class Status
{
    eOk,
    eInvalidRequestedData,
    eInvalidRequesterInfo,
    eCount
};

#define SL_FAILED_SHARED(r, f) sl::internal::shared::Status r = f; r != sl::internal::shared::Status::eOk

//! Inter-plugin data sharing
//! 
//! Used to share typed and version-ed data across plugin boundaries.
//! 
//! Plugins can be taken from different versions of an SL SDK but as
//! long as they check requested data's structure version they can 
//! safely access only the data that is actually provided.
//! 
//! Things to remember:
//! 
//! * When sharing data the provider plugin MUST check the version of the incoming requestedData
//!     * If incoming data is newer, version must be changed to match providers version so that requester is aware that newer bits in the structure are not valid
//!     * If incoming data is older, provider plugin must not set any newer bits and data version remains intact
//! * When accessing shared data the requester plugin MUST check the version of the provided data
//!     * If provided data is older than requested newer data bits must NOT be accessed
//! * Same applies when accessing requesterInfo (if any). Version must be checked in order to avoid accessing data which is not provided (requester is an older plugin)
//! 
//! This ensures backwards and forward compatibility. Please note, everything mentioned here is about STRUCTURE VERSIONS and not plugin versions, plugins can be mixed and matched from various SL SDKs.
//! 
//! NOTE: requesterInfo is optional and can be null. If provided, the plugin which shares the data must be aware of it (data structure existed at the compile time)
//! For example, an older plugin might not recognize new requester info in which case it will simply be ignored.
//! 
//! This API guarantees the following:
//! 
//! * It will not change - typed and versioned structures provide the flexibility we need to preserve forwards/backwards compatibility
//! * Each requester gets its own copy of the data
//! * GPU resources are managed via ICompute and IUnknown reference counting as usual
//! * Structure chaining with the `requestedData->next` is allowed but optional, if provided chained data will be treated the same way as any SL structure
//! * Thread safe access, if any shared data can be modified asynchronously on the CPU this API will provide synchronization before copy is made
//!
using PFun_GetSharedData = Status(BaseStructure* requestedData, const BaseStructure* requesterInfo);

//! Feature ids are unique hence we are generating unique parameter names
//! 
inline std::string getParameterNameForFeature(Feature feature)
{
    return extra::format("sl.param.sharedData.{}", feature);
}

} // namespace shared
} // namespace internal
} // namespace sl
