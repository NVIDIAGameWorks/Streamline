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

#include <mutex>

#include "include/sl.h"
#include "include/sl_helpers.h"
#include "external/nvapi/nvapi.h"

namespace sl
{
namespace common
{

using PFunDRSInit = bool(void);
using PFunDRSShutdown = void(void);
using PFunDRSReadKeyString = bool(NvU32 keyId, std::wstring& sValue);
using PFunDRSReadKey = bool(NvU32 keyId, NvU32& uValue);

struct DRSContext
{
    PFunDRSReadKey* drsReadKey{};
    PFunDRSReadKeyString* drsReadKeyString{};
};

} // namespace common
} // namespace sl
