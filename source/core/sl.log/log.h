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

#ifdef SL_WINDOWS
#include <windows.h>
#else
#include <stdarg.h>
#include "include/sl.h"
#endif
#include <string>
#include <atomic>

template <typename T, size_t N>
constexpr size_t countof(T const (&)[N]) noexcept
{
    return N;
}

namespace sl
{

enum class LogLevel : uint32_t;
enum class LogType : uint32_t;

namespace log
{

#ifndef SL_WINDOWS
#define FOREGROUND_BLUE 1
#define FOREGROUND_GREEN 2
#define FOREGROUND_RED 4
#define FOREGROUND_INTENSITY 8
#endif

enum ConsoleForeground
{
    BLACK = 0,
    DARKBLUE = FOREGROUND_BLUE,
    DARKGREEN = FOREGROUND_GREEN,
    DARKCYAN = FOREGROUND_GREEN | FOREGROUND_BLUE,
    DARKRED = FOREGROUND_RED,
    DARKMAGENTA = FOREGROUND_RED | FOREGROUND_BLUE,
    DARKYELLOW = FOREGROUND_RED | FOREGROUND_GREEN,
    DARKGRAY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    GRAY = FOREGROUND_INTENSITY,
    BLUE = FOREGROUND_INTENSITY | FOREGROUND_BLUE,
    GREEN = FOREGROUND_INTENSITY | FOREGROUND_GREEN,
    CYAN = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
    RED = FOREGROUND_INTENSITY | FOREGROUND_RED,
    MAGENTA = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
    YELLOW = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
    WHITE = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
};

struct ILog
{
    virtual void logva(uint32_t level, ConsoleForeground color, const char *file, int line, const char *func, int type, const char *fmt, ...) = 0;
    virtual void enableConsole(bool flag) = 0;
    virtual LogLevel getLogLevel() const = 0;
    virtual void setLogLevel(LogLevel level) = 0;
    virtual void setLogPath(const wchar_t *path) = 0;
    virtual void setLogName(const wchar_t *name) = 0;
    virtual void setLogCallback(void* logMessageCallback) = 0;
    virtual void setLogMessageDelay(float logMessageDelayMS) = 0;
    virtual const wchar_t* getLogPath() = 0;
    virtual void shutdown() = 0;
};

ILog* getInterface();
void destroyInterface();

#define SL_RUN_ONCE                                  \
    for (static std::atomic<int> s_runAlready(false); \
         !s_runAlready.fetch_or(true);)               \

#define SL_LOG_HINT(fmt,...) sl::log::getInterface()->logva(2, sl::log::GREEN, __FILE__,__LINE__,__func__, 0,fmt,##__VA_ARGS__)
#define SL_LOG_INFO(fmt,...) sl::log::getInterface()->logva(1, sl::log::WHITE, __FILE__,__LINE__,__func__, 0,fmt,##__VA_ARGS__)
#define SL_LOG_WARN(fmt,...) sl::log::getInterface()->logva(1, sl::log::YELLOW, __FILE__,__LINE__,__func__, 1,fmt,##__VA_ARGS__)
#define SL_LOG_ERROR(fmt,...) sl::log::getInterface()->logva(1, sl::log::RED, __FILE__,__LINE__,__func__, 2,fmt,##__VA_ARGS__)
#define SL_LOG_VERBOSE(fmt,...) sl::log::getInterface()->logva(2, sl::log::WHITE, __FILE__,__LINE__,__func__,0,fmt,##__VA_ARGS__)

#define SL_LOG_HINT_ONCE(fmt,...) SL_RUN_ONCE { SL_LOG_HINT(fmt,__VA_ARGS__); }
#define SL_LOG_INFO_ONCE(fmt,...) SL_RUN_ONCE { SL_LOG_INFO(fmt,__VA_ARGS__); }
#define SL_LOG_WARN_ONCE(fmt,...) SL_RUN_ONCE { SL_LOG_WARN(fmt,__VA_ARGS__); }
#define SL_LOG_ERROR_ONCE(fmt,...) SL_RUN_ONCE { SL_LOG_ERROR(fmt,__VA_ARGS__); }
#define SL_LOG_VERBOSE_ONCE(fmt,...) SL_RUN_ONCE { SL_LOG_VERBOSE(fmt,__VA_ARGS__); }

}
}

