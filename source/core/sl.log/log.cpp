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

#include <iomanip>
#include <map>

#include "include/sl.h"
#include "include/sl_core_types.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.thread/thread.h"

#ifndef NDEBUG
#define ASSERT_ONLY_CODE 1
#endif

#if ASSERT_ONLY_CODE
#include "assert.h"
#endif

namespace sl
{
namespace log
{

#ifdef SL_WINDOWS
extern bool g_slEnableLogPreMetaDataUniqueWAR = false;

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
    std::hash<std::string> m_hash;
    std::atomic<bool> m_console = false;
    std::atomic<bool> m_pathInvalid = false;
    std::wstring m_path;
    std::wstring m_name;
    LogLevel m_logLevel = LogLevel::eVerbose;
    std::atomic<bool> m_consoleActive = false;
    FILE* m_file = {};
    PFun_LogMessageCallback* m_logMessageCallback = {};
    thread::WorkerThread* m_worker{};

    Log()
    {
        m_worker = new thread::WorkerThread(L"sl.log", THREAD_PRIORITY_BELOW_NORMAL);
    }

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

    const wchar_t* getLogPath() override { return m_path.c_str(); }

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
        m_pathInvalid = false;
    }

    void setLogName(const wchar_t *name) override
    {
        m_name = name;
    }

    const wchar_t* getLogName() override { return m_name.c_str(); }

    void setLogCallback(void* logMessageCallback) override
    {
        m_logMessageCallback = (PFun_LogMessageCallback*)logMessageCallback;
    }

    void setLogMessageDelay(float messageDelayMs) override
    {
        m_messageDelayMs = messageDelayMs;
    }

    void flush() override
    {
        if (m_worker)
        {
            m_worker->flush(UINT_MAX);
        }

        // No need to fflush here because logging lambda calls print() which always flushes
        // if (m_file)
        // {
        //     fflush(m_file);
        // }
    }

    void shutdown() override
    {
        if (m_worker)
        {
            //! IMPORTANT: During shutdown there could be a LOT of 
            //! exit logging so default timeout does not always make sense.
            m_worker->flush(UINT_MAX);
            delete m_worker;
            m_worker = nullptr;
        }
        if (m_file)
        {
            fflush(m_file);
            fclose(m_file);
            m_file = nullptr;
            m_pathInvalid = true; // prevent log file reopening
        }
        m_consoleActive = false;
        // Win32 API does not require us to close this handle
        m_outHandle = {};
    }

    void print(ConsoleForeground color, const std::string &logMessage)
    {
        // Set attribute for newly written text
        if (m_consoleActive)
        {
            SetConsoleTextAttribute(m_outHandle, color);
            DWORD OutChars;
            WriteConsoleA(m_outHandle, logMessage.c_str(), (DWORD)logMessage.length(), &OutChars, nullptr);
            if (color != sl::log::WHITE)
            {
                SetConsoleTextAttribute(m_outHandle, sl::log::WHITE);
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

    void logva(uint32_t level, ConsoleForeground color, const char *_file, int line, const char *_func, int type, bool isMetaDataUnique, const char *_fmt,...) override
    {
        if (level > (uint32_t)m_logLevel)
        {
            // Higher level than requested, bail out
            return;
        }

        std::string file(_file);
        std::string func(_func);
        std::string fmt(_fmt);
        std::string msg;

        // Incoming message can be un-formatted if provided by 3rd party like NGX
        bool formatted = fmt.empty() || fmt.back() != '\n';
        if (formatted)
        {
            bool errorDetected = false;
            va_list args;

            // Make sure va_end is called before early out!
            va_start(args, _fmt);

            // Determine the required size of the formatted message (without null-terminator)
            int msgSize = _vscprintf(_fmt, args);
            if (msgSize >= 1)
            {
                // Account for the null terminator char
                msgSize++;
                msg.resize(msgSize);

                // Format the message
                msgSize = vsnprintf(msg.data(), msg.size(), _fmt, args);

                if (msgSize <= 0)
                {
                    // invalid character in the string or any other error
                    errorDetected = true;
                }
                // vsnprintf adds '0' at the end of the string. if we don't remove it,
                // the '+=' won't work correctly on that string
                while (msg.size() && *msg.rbegin() == '\0')
                {
                    msg.pop_back();
                }
            }
            else
            {
                // _fmt is bad or empty log
                errorDetected = true;
            }
            va_end(args);

            if (errorDetected)
            {
                // Something went wrong during `_vscprintf` or `vsprintf_s`
                return;
            }
        }

#if ASSERT_ONLY_CODE
        if ((LogType)type == LogType::eError && IsDebuggerPresent())
        {
            // Use log message and originating file/line number for assert
            _wassert(std::wstring(msg.begin(), msg.end()).c_str(), std::wstring(file.begin(), file.end()).c_str(), line);
        }
#endif

        // This thread ID
        auto tid = std::this_thread::get_id();
        auto logLambda = [this, tid, msg, level, color, file, line, func, type, fmt, formatted, isMetaDataUnique]()->void
        {
            if (m_console && !m_consoleActive)
            {
                startConsole();
                m_consoleActive = isConsoleActive();
            }
            std::string completeLogMessage;

            // Today's time
            auto t = std::time(nullptr);
            tm time = {};
            localtime_s(&time, &t);

            if (!m_file && !m_path.empty() && !m_pathInvalid)
            {
                // Allow other process to read log file
                auto path = m_path + L"\\" + m_name;
                m_file = _wfsopen(path.c_str(), L"wt", _SH_DENYWR);
                if (!m_file)
                {
                    m_pathInvalid = true;
                    std::wstring tmp = L"[streamline][error]log.cpp:125[logva] Failed to open log file " + path + L"\n";
                    completeLogMessage = extra::toStr(tmp);
                    print(RED, completeLogMessage);
                }
                else
                {
                    std::stringstream dateTimeOss{};
                    dateTimeOss << std::put_time(&time, "on %d.%m.%Y at %H-%M-%S");
                    std::wstring tmp = L"[streamline][info]log.cpp:131[logva] Log file " + path + L" opened " + extra::toWStr(dateTimeOss.str()) + L"\n";
                    completeLogMessage = extra::toStr(tmp);
                    print(WHITE, completeLogMessage);
                }
            }

            std::string message;
            if (formatted)
            {
                message = msg;
            }
            else
            {
                message = fmt;
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
            
            // Filename
            std::string f(file);
            // file is constexpr so always valid and always will have at least one '\'
            f = f.substr(f.rfind('\\') + 1);

            // Log type
            std::string prefix[] = { "info","warn","error" };
            static_assert(countof(prefix) == (size_t)LogType::eCount);
            
            // Metadata that makes a log message unique
            std::ostringstream oss_logSourceMetdata;
            oss_logSourceMetdata << "[tid:" << tid << "]" << "[" << sl::extra::getPrettyTimestamp() + "]";

            // Put it all together in the message header
            std::ostringstream oss;
            oss << std::put_time(&time, "[%H-%M-%S]") << "[streamline][" << prefix[type] << "]" << oss_logSourceMetdata.str() << f << ":" << line << "[" << func << "]";
            
            // Actual message will get appended a bit later in this func
            completeLogMessage = oss.str();

            // Safety in case map grows too big like 10K unique messages (which is highly unlikely ever to happen but ...)
            // However if verbose logging is on allow all messages
            if (m_logLevel != LogLevel::eVerbose)
            {
                if (m_logTimes.size() > 10000)
                {
                    m_logTimes.clear();
                }

                std::string messageHashPrefix = "";
                if (isMetaDataUnique)
                {
                    // We consider source metadata(e.g., thread id, granular timestamp) to make a log message unique in this case
                    // e.g.: logging "Hello!" from 2 different threads is considered logging 2 different messages
                    messageHashPrefix = oss_logSourceMetdata.str();
                }

                auto id = m_hash(messageHashPrefix + message);
                auto lastLogTime = m_logTimes[id];
                if (lastLogTime.time_since_epoch().count() > 0)
                {
                    // Already logged before, make sure not to spam the log
                    std::chrono::duration<float, std::milli> diff = std::chrono::system_clock::now() - lastLogTime;
                    if (diff.count() < m_messageDelayMs)
                    {
                        // Show frequent messages every 'messageDelayMs'
                        return;
                    }
                }
                m_logTimes[id] = std::chrono::system_clock::now();
            }

            completeLogMessage += ' ' + message;

            if (formatted)
            {
                completeLogMessage += '\n';
            }            

            print(color, completeLogMessage);

            if (m_logMessageCallback)
            {
                m_logMessageCallback((LogType)type, completeLogMessage.c_str());
            }
        };
        m_worker->scheduleWork(logLambda);
    }

    void startConsole()
    {
        if (!isConsoleActive() || !m_outHandle)
        {
            AllocConsole();
            SetConsoleTitleA("Streamline");
            moveWindowToAnotherMonitor(GetConsoleWindow(), 0);
            m_outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        }
    }

    bool isConsoleActive()
    {
        HWND consoleWnd = GetConsoleWindow();
        return consoleWnd != NULL;
    }
    
    float m_messageDelayMs = 5000.0f;

    std::map<size_t, std::chrono::time_point<std::chrono::system_clock>> m_logTimes{};

    inline static Log* s_log = {};
    HANDLE m_outHandle{};
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
