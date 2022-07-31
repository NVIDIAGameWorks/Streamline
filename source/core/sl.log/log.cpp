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

#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "include/sl.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"

namespace sl
{
namespace log
{

#ifdef SL_WINDOWS

HMONITOR g_otherMonitor = {};

BOOL MyInfoEnumProc(
    HMONITOR monitor,
    HDC unnamedParam2,
    LPRECT unnamedParam3,
    LPARAM unnamedParam4
)
{
    if (monitor != MonitorFromWindow(GetConsoleWindow(), MONITOR_DEFAULTTONEAREST))
    {
        g_otherMonitor = monitor;
    }
    return TRUE;
}

void moveWindowToAnotherMonitor(HWND hwnd, UINT flags)
{
    RECT prc;
    GetWindowRect(hwnd, &prc);
    
    MONITORINFO mi;
    RECT        rc;
    int         w = 2 * (prc.right - prc.left);
    int         h = 2 * (prc.bottom - prc.top);

    EnumDisplayMonitors(NULL, NULL, MyInfoEnumProc, 0);

    if (g_otherMonitor)
    {
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(g_otherMonitor, &mi);

        //if (flags & MONITOR_WORKAREA)
        rc = mi.rcWork;
        //else
        //    rc = mi.rcMonitor;

        prc.left = rc.left + (rc.right - rc.left - w) / 2;
        prc.top = rc.top + (rc.bottom - rc.top - h) / 2;
        prc.right = prc.left + w;
        prc.bottom = prc.top + h;

        SetWindowPos(hwnd, NULL, prc.left, prc.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

struct Log : ILog
{
    bool m_console = false;
    bool m_pathInvalid = false;
    std::wstring m_path;
    std::wstring m_name;
    LogLevel m_logLevel = eLogLevelVerbose;
    bool m_consoleActive = false;
    FILE* m_file = {};
    pfunLogMessageCallback* m_logMessageCallback = {};

    void enableConsole(bool flag) override
    {
        m_console = flag;
    }
    
    LogLevel getLogLevel() const override
    {
        return m_logLevel;
    }

    void setLogLevel(LogLevel level) override
    {
        m_logLevel = level;
    }

    void setLogPath(const wchar_t *path) override
    {
        if (m_file)
        {
            fflush(m_file);
            fclose(m_file);
            m_file = nullptr;
        }
        // Passing nullptr will disable logging to a file
        m_path = path ? path : L"";
    }

    void setLogName(const wchar_t *name) override
    {
        m_name = name;
    }

    void setLogCallback(void* logMessageCallback) override
    {
        m_logMessageCallback = (pfunLogMessageCallback*)logMessageCallback;
    }

    void shutdown() override
    {
        if (m_file)
        {
            fflush(m_file);
            fclose(m_file);
            m_file = nullptr;
            m_pathInvalid = true; // prevent log file reopening
        }
    }

    void print(ConsoleForeground color, const std::string &logMessage)
    {
        // Set attribute for newly written text
        if (m_consoleActive)
        {
            auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(handle, color);
            DWORD OutChars;
            WriteConsoleA(handle, logMessage.c_str(), (DWORD)logMessage.length(), &OutChars, nullptr);
            if (color != sl::log::WHITE)
            {
                SetConsoleTextAttribute(handle, sl::log::WHITE);
            }
        }

        // Only output to VS debugger if host is not handling it
        if (!m_logMessageCallback)
        {
            OutputDebugStringA(logMessage.c_str());
        }

        if (m_file)
        {
            fputs(logMessage.c_str(), m_file);
            fflush(m_file);
        }
    }

    void logva(int level, ConsoleForeground color, const char *file, int line, const char *func, int type, const char *fmt, ...) override
    {
        if (level > m_logLevel)
        {
            // Higher level than requested, bail out
            return;
        }
        if (m_console && !m_consoleActive)
        {
            startConsole();
            m_consoleActive = isConsoleActive();
        }
        std::string logMessage;
        if (!m_file && !m_path.empty() && !m_pathInvalid)
        {
            // Allow other process to read log file
            auto path = m_path + L"\\" + m_name;
            m_file = _wfsopen(path.c_str(), L"wt", _SH_DENYWR);
            if (!m_file)
            {
                m_pathInvalid = true;
                std::wstring tmp = L"[streamline][error]log.cpp:125[logva] Failed to open log file " + path + L"\n";
                logMessage = extra::toStr(tmp);
                print(RED, logMessage);
            }
            else
            {
                std::wstring tmp = L"[streamline][info]log.cpp:131[logva] Log file " + path + L" opened\n";
                logMessage = extra::toStr(tmp);
                print(WHITE, logMessage);
            }
        }
        std::string message;
        message.resize(16 * 1024);

        std::string f(file);
        // file is constexpr so always valid and always will have at least one '\'
        f = file + f.rfind('\\') + 1;
        std::string prefix[] = { "info","warn","error" };
        static_assert(countof(prefix) == eLogTypeCount);
        auto t = std::time(nullptr);
        tm time = {};
        localtime_s(&time, &t);
        std::ostringstream oss;
        oss << std::put_time(&time, "[%d.%m.%Y %H-%M-%S]");
        logMessage = oss.str() + "[streamline][" + prefix[type] + "]" + f + ":" + std::to_string(line) + "[" + std::string(func) + "] ";

        va_list args;
        va_start(args, fmt);
        auto size = vsprintf_s(message.data(), message.size(), fmt, args);
        if (size <= 0) return;
        message.resize(size);
        bool crlf = message.back() == '\n';
        if (crlf)
        {
            // Message coming from 3rd party (NGX) so remove the time stamp
            auto p = message.find("]");
            if (p != std::string::npos)
            {
                p = message.find("]", p + 1);
                if (p != std::string::npos)
                {
                    message = message.substr(p + 1);
                }
            }
        }
        va_end(args);
        logMessage += message;
        
        if (!crlf)
        {
            logMessage += '\n';
        }

        print(color, logMessage);

        if (m_logMessageCallback)
        {
            m_logMessageCallback((LogType)type, logMessage.c_str());
        }
    }

    void startConsole()
    {
        if (!isConsoleActive())
        {
            AllocConsole();
            SetConsoleTitleA("Streamline");
            moveWindowToAnotherMonitor(GetConsoleWindow(), 0);
        }
    }

    bool isConsoleActive()
    {
        HWND consoleWnd = GetConsoleWindow();
        return consoleWnd != NULL;
    }

    inline static Log* s_log = {};
};

ILog* getInterface()
{
    if (!Log::s_log)
    {
        Log::s_log = new Log();
    }
    return Log::s_log;
}

void destroyInterface()
{
    if (Log::s_log)
    {
        Log::s_log->shutdown();
        delete Log::s_log;
        Log::s_log = {};
    }
}

#endif // SL_WINDOWS

}
}