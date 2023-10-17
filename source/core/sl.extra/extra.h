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

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <atomic>
#include <cmath>
#include <codecvt>
#include <locale>

#ifdef SL_WINDOWS
#define SL_IGNOREWARNING_PUSH __pragma(warning(push))
#define SL_IGNOREWARNING_POP __pragma(warning(pop))
#define SL_IGNOREWARNING(w) __pragma(warning(disable : w))
#define SL_IGNOREWARNING_WITH_PUSH(w)                    \
        SL_IGNOREWARNING_PUSH                            \
        SL_IGNOREWARNING(w)
#else
#define SL_IGNOREWARNING_PUSH _Pragma("GCC diagnostic push")
#define SL_IGNOREWARNING_POP _Pragma("GCC diagnostic pop")
#define SL_INTERNAL_IGNOREWARNING(str) _Pragma(#str)
#define SL_IGNOREWARNING(w) SL_INTERNAL_IGNOREWARNING(GCC diagnostic ignored w)
#define SL_IGNOREWARNING_WITH_PUSH(w) SL_IGNOREWARNING_PUSH SL_IGNOREWARNING(w)
#endif


namespace sl
{

namespace extra
{

// Ignore deprecated warning, c++17 does not provide proper alternative yet
SL_IGNOREWARNING_WITH_PUSH(4996)

inline std::wstring utf8ToUtf16(const char* source)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    return convert.from_bytes(source);
}

inline std::string utf16ToUtf8(const wchar_t* source)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    return convert.to_bytes(source);
}

SL_IGNOREWARNING_POP

inline std::wstring toWStr(const std::string& s)
{
    return utf8ToUtf16(s.c_str());
}

inline std::wstring toWStr(const char* s)
{
    return utf8ToUtf16(s);
}

inline std::string toStr(const std::wstring& s)
{
    return utf16ToUtf8(s.c_str());
}

inline std::string toStr(const wchar_t* s)
{
    return utf16ToUtf8(s);
}

template <typename I>
std::string toHexStr(I w, size_t hex_len = sizeof(I) << 1)
{
    constexpr const char* digits = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
        rc[i] = digits[(w >> j) & 0x0f];
    return rc;
}

inline constexpr uint32_t align(uint32_t size, uint32_t alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

//! If value is null it will remove the environment variable
inline bool setEnvVar(const char* varName, const char* value)
{
    bool result;
#if SL_WINDOWS
    result = (SetEnvironmentVariableA(varName, value) != 0);
#else
    if (value)
    {
        result = (setenv(varName, value, /*overwrite=*/1) == 0);
    }
    else
    {
        result = (unsetenv(varName) == 0);
    }
#endif
    return result;
}

struct ScopedTasks
{
    ScopedTasks() {};
    ScopedTasks(std::function<void(void)> funIn, std::function<void(void)> funOut) { funIn();  tasks.push_back(funOut); }
    ScopedTasks(std::function<void(void)> fun) { tasks.push_back(fun); }
    ~ScopedTasks()
    {
        for (auto& task : tasks)
        {
            task();
        }
    }
    std::vector<std::function<void(void)>> tasks;
};

namespace keyboard
{
struct VirtKey
{
    VirtKey(int mainKey = 0, bool bShift = false, bool bControl = false, bool bAlt = false)
        : m_mainKey(mainKey)
        , m_bShift(bShift)
        , m_bControl(bControl)
        , m_bAlt(bAlt)
    {}

    std::string asStr() const
    {
        std::string s;
        if (m_bControl) s += "ctrl+";
        if (m_bShift) s += "shift+";
        if (m_bAlt) s += "alt+";
        if (m_mainKey)
        {
            s += (char)m_mainKey;
        }
        else
        {
            s = "unassigned";
        }
        return s;
    }

    // Main key press for the binding
    int m_mainKey = 0;

    // Modifier keys required to match to activate the virtual key binding
    // True means that the corresponding modifier key must be pressed for the virtual key to be considered pressed,
    // and False means that the corresponding modifier key may not be pressed for the virtual key to be considered pressed.
    bool m_bShift = false;
    bool m_bControl = false;
    bool m_bAlt = false;
};

struct IKeyboard
{
    virtual void registerKey(const char* name, const VirtKey& key) = 0;
    virtual bool wasKeyPressed(const char* name) = 0;
    virtual const VirtKey& getKey(const char* name) = 0;
    virtual bool hasFocus() = 0;
};

IKeyboard* getInterface();
}

constexpr size_t kAverageMeterWindowSize = 120;

//! IMPORTANT: Mainly not thread safe for performance reasons
//! 
//! Only selected "get" methods use atomics.
struct AverageValueMeter
{
    AverageValueMeter()
    {
#ifdef SL_WINDOWS
        QueryPerformanceFrequency(&frequency);
#endif
    };

    AverageValueMeter(const AverageValueMeter& rhs) { operator=(rhs); }

    inline AverageValueMeter& operator=(const AverageValueMeter& rhs)
    {
        n = rhs.n.load();
        val = rhs.val.load();
        sum = rhs.sum;
        memcpy(window, rhs.window, sizeof(double) * kAverageMeterWindowSize);
#ifdef SL_WINDOWS
        frequency = rhs.frequency;
        startTime = rhs.startTime;
        elapsedUs = rhs.elapsedUs;
#endif
        return *this;
    }

    //! NOT thread safe
    void reset()
    {
        n = 0;
        val = 0;
        sum = 0;
        mean = 0;
        memset(window, 0, sizeof(double) * kAverageMeterWindowSize);
#ifdef SL_WINDOWS
        startTime = {};
        elapsedUs = {};
#endif
    }

    //! NOT thread safe
    void begin()
    {
#ifdef SL_WINDOWS
        QueryPerformanceCounter(&startTime);
#endif
    }

    //! NOT thread safe
    void end()
    {
#ifdef SL_WINDOWS
        if (startTime.QuadPart > 0)
        {
            LARGE_INTEGER endTime{};
            QueryPerformanceCounter(&endTime);
            elapsedUs.QuadPart = endTime.QuadPart - startTime.QuadPart;
            elapsedUs.QuadPart *= 1000000;
            elapsedUs.QuadPart /= frequency.QuadPart;
            auto elapsedMs = elapsedUs.QuadPart / 1000.0;
            add(elapsedMs);
        }
#endif
    }

    //! NOT thread safe
    void timestamp()
    {
        end();
        begin();
    }

    //! NOT thread safe
    int64_t timeFromLastTimestampUs()
    {
#ifdef SL_WINDOWS
        if (startTime.QuadPart > 0)
        {
            LARGE_INTEGER endTime{};
            QueryPerformanceCounter(&endTime);
            elapsedUs.QuadPart = endTime.QuadPart - startTime.QuadPart;
            elapsedUs.QuadPart *= 1000000;
            elapsedUs.QuadPart /= frequency.QuadPart;
        }
        return elapsedUs.QuadPart;
#else
        return 0;
#endif
    }

    //! Performance sensitive code, can be called
    //! thousands of times in CPU taxing loops hence
    //! avoiding using std vectors as much as possible.
    //! 
    //! NOT thread safe
    void add(double value)
    {
        val = value;
        sum = sum + value;
        auto i = n.load() % kAverageMeterWindowSize;
        if (n >= kAverageMeterWindowSize)
        {
            sum = sum - window[i];
        }
        window[i] = value;
        n++;
        mean = sum / double(std::min(n.load(), kAverageMeterWindowSize));
    }

    //! NOT thread safe
    double getMedian()
    {
        double median = 0;
        if (n > 0)
        {
            std::vector<double> tmp(window, window + std::min(n.load(),kAverageMeterWindowSize));
            std::sort(tmp.begin(), tmp.end());
            median = tmp[tmp.size() / 2];
        }
        return median;
    }

    //! NOT thread safe
    inline int64_t getElapsedTimeUs() const
    {
#ifdef SL_WINDOWS
        return elapsedUs.QuadPart;
#else
        return 0;
#endif
    }

    //! Thread safe
    //! 
    inline double getMean() const { return mean.load(); }
    inline double getValue() const { return val.load(); }
    inline uint64_t getNumSamples() const { return n.load(); }

private:
    std::atomic<double> val = 0;
    std::atomic<double> mean = 0;
    std::atomic<uint64_t> n = 0;

    double sum{};
    double window[kAverageMeterWindowSize];

#ifdef SL_WINDOWS
    LARGE_INTEGER frequency{};
    LARGE_INTEGER startTime{};
    LARGE_INTEGER elapsedUs{};
#endif
};

struct ScopedCPUTimer
{
    ScopedCPUTimer(AverageValueMeter* meter)
    {
        m_meter = meter;
        meter->begin();
    }
    ~ScopedCPUTimer()
    {
        m_meter->end();
    }

    AverageValueMeter* m_meter{};
};

inline void format(std::ostringstream& stream, const char* str)
{
    stream << str;
}

template <class Arg, class... Args>
inline void format(std::ostringstream& stream, const char* str, Arg&& arg, Args&&... args)
{
    auto p = strstr(str, "{}");
    if (p)
    {
        stream.write(str, p - str);
        auto p1 = strstr(p + 2, "%x");
        if (p1 == p + 2)
        {
            stream << std::hex;
        }
        stream << arg;
        if (p1 == p + 2)
        {
            stream << std::dec;
            p += 2;
        }
        format(stream, p + 2, std::forward<Args>(args)...);
    }
    else
    {
        stream << str;
    }
 }

/**
 * Formats a string similar to the {fmt} library (https://fmt.dev), but header-only and without requiring an external
 * library be included
 *
 * NOTE: This is not intended to be a full replacement for {fmt}. Only '{}' is supported (i.e. no non-positional
 * support). And any type can be formatted, but must be streamable (i.e. have an appropriate operator<<)
 *
 * Example: format("{}, {} and {}: {}", "Peter", "Paul", "Mary", 42) would produce the string "Peter, Paul and Mary: 42"
 * @param str The format string. Use '{}' to indicate where the next parameter would be inserted.
 * @returns The formatted string
 */
template <class... Args>
inline std::string format(const char* str, Args&&... args)
{
    std::ostringstream stream;
    stream.precision(2);
    stream << std::fixed;
    extra::format(stream, str, std::forward<Args>(args)...);
    return stream.str();
}

}
}
