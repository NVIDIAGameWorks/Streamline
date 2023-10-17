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

#ifdef SL_ENABLE_EXCEPTION_HANDLING

#ifdef SL_WINDOWS

#include "include/sl.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.exception/exception.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "_artifacts/gitVersion.h"

#include <ShlObj.h>
#include <Dbghelp.h.>

namespace sl
{
namespace exception
{

struct Exception : IException
{
    virtual int writeMiniDump(LPEXCEPTION_POINTERS exceptionInfo) override final
    {
#ifndef SL_PRODUCTION
        if (IsDebuggerPresent())
        {
            __debugbreak();
        };
#endif
        // Unique id
        auto id = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        std::wstring path = L"";
        PWSTR programDataPath = NULL;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &programDataPath);
        if (SUCCEEDED(hr))
        {
            path = programDataPath + std::wstring(L"/NVIDIA/Streamline/" + file::getExecutableName() + L"/" + extra::utf8ToUtf16(std::to_string(id).c_str()));
        }
        if (!file::createDirectoryRecursively(path.c_str()))
        {
            SL_LOG_ERROR( "Failed to create folder %S", path.c_str());
            return 0;
        }

        std::wstring logSrc = log::getInterface()->getLogPath();
        logSrc += L"/sl.log";
        std::wstring logDst = path + L"/sl.log";

        path += L"/sl-sha-" + extra::utf8ToUtf16(GIT_LAST_COMMIT_SHORT) + L".dmp";
        SL_LOG_ERROR( "Exception detected - thread %u - creating mini-dump '%S'", GetCurrentThreadId(), path.c_str());
        
        // Try to create file for mini dump.
        HANDLE FileHandle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        // Write a mini dump.
        if (FileHandle)
        {
            MINIDUMP_EXCEPTION_INFORMATION DumpExceptionInfo;

            DumpExceptionInfo.ThreadId = GetCurrentThreadId();
            DumpExceptionInfo.ExceptionPointers = exceptionInfo;
            DumpExceptionInfo.ClientPointers = true;

            // Note: the 'MiniDumpWithDataSegs' flag has been removed from here by default because it can result in the size of the mini-dump growing by a very large amount.
            MINIDUMP_TYPE dumpFlags = MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpIgnoreInaccessibleMemory | MiniDumpWithHandleData | MiniDumpWithProcessThreadData | MiniDumpWithThreadInfo);
            if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), FileHandle, dumpFlags, &DumpExceptionInfo, NULL, NULL))
            {
                SL_LOG_ERROR( "Failed to create dump - %s", std::system_category().message(GetLastError()).c_str());
            }

            CloseHandle(FileHandle);
        }
        else
        {
            SL_LOG_ERROR( "Failed to create file '%S'", path.c_str());
        }
        // Flush logs here in case this also triggers an exception so we already have the dump
        log::getInterface()->shutdown();

        // Copy log file to dump location
        file::copy(logDst.c_str(), logSrc.c_str());

        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline static Exception* s_exception = {};
    HANDLE m_outHandle{};
};

IException* getInterface()
{
    if (!Exception::s_exception)
    {
        Exception::s_exception = new Exception();
    }
    return Exception::s_exception;
}

void destroyInterface()
{
    if (Exception::s_exception)
    {
        delete Exception::s_exception;
        Exception::s_exception = {};
    }
}

}
}

#endif // SL_WINDOWS
#endif // SL_ENABLE_EXCEPTION_HANDLING
