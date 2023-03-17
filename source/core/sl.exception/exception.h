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

#ifdef SL_ENABLE_EXCEPTION_HANDLING

#ifdef SL_WINDOWS
#include <windows.h>

namespace sl
{
namespace exception
{
#define SL_EXCEPTION_HANDLE_START __try {
#define SL_EXCEPTION_HANDLE_END } __except (sl::exception::getInterface()->writeMiniDump(GetExceptionInformation())) {}
#define SL_EXCEPTION_HANDLE_END_RETURN(R) } __except (sl::exception::getInterface()->writeMiniDump(GetExceptionInformation())) { return R;}

struct IException
{
    virtual int writeMiniDump(LPEXCEPTION_POINTERS exceptionInfo) = 0;
};

IException* getInterface();
void destroyInterface();

}
}

#endif // SL_WINDOWS

#else
#define SL_EXCEPTION_HANDLE_START
#define SL_EXCEPTION_HANDLE_END
#define SL_EXCEPTION_HANDLE_END_RETURN(R)
#endif // SL_ENABLE_EXCEPTION_HANDLING
