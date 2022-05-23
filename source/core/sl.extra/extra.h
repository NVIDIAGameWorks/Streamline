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

struct ScopedTasks
{
    ScopedTasks() {};
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
};

IKeyboard* getInterface();
}

struct AverageValueMeter
{
    AverageValueMeter() {};
    AverageValueMeter(const AverageValueMeter& rhs) { operator=(rhs); }
    inline AverageValueMeter& operator=(const AverageValueMeter& rhs)
    {
        useWindow = rhs.useWindow.load();
        n = rhs.n.load();
        val = rhs.val.load();
        sum = rhs.sum.load();
        mean = rhs.mean.load();
        std = rhs.std.load();
        mean_old = rhs.mean_old.load();
        m_s = rhs.m_s.load();
        median = rhs.median.load();
        window = rhs.window;
        start = rhs.start;
        return *this;
    }

    std::atomic<bool> useWindow = true;
    std::atomic<float> n = 0;
    std::atomic<float> val = 0;
    std::atomic<float> sum = 0;
    std::atomic<float> mean = 0;
    std::atomic<float> std = 0;
    std::atomic<float> mean_old = 0;
    std::atomic<float> m_s = 0;
    std::atomic<float> median = 0;
    std::vector<float> window;
    std::chrono::high_resolution_clock::time_point start = {};

    void reset()
    {
        n = 0;
        val = 0;
        sum = 0;
        mean = 0;
        std = 0;
        mean_old = 0;
        m_s = 0;
        median = 0;
        window.clear();
        start = {};
    }

    void begin()
    {
        start = std::chrono::high_resolution_clock::now();
    }

    void end()
    {
        if (start.time_since_epoch().count() > 0)
        {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float, std::milli> diff = end - start;
            add(diff.count());
        }
    }

    void timestamp()
    {
        end();
        begin();
    }

    void add(float value)
    {
        val = value;
        if (useWindow)
        {
            if (window.size() == 120)
            {
                window.erase(window.begin());
            }
            window.push_back(value);
            auto tmp = window;
            std::sort(tmp.begin(), tmp.end());
            median = tmp[tmp.size() / 2];
        }
        sum = sum + value;
        if (n == 0)
        {
            mean = 0.0f + value;
            std = -1.0f;
            mean_old.store(mean);
            m_s = 0.0;
        }
        else
        {
            mean = mean_old + (value - mean_old) / float(n + 1.0f);
            m_s = m_s + (value - mean_old) * (value - mean);
            mean_old.store(mean);
            std = sqrt(m_s / n);
        }
        n = n + 1.0f;
    }
};

struct scopedCPUTimer
{
    scopedCPUTimer(AverageValueMeter* meter)
    {
        m_meter = meter;
        meter->useWindow = false;
        start = std::chrono::high_resolution_clock::now();
    }
    ~scopedCPUTimer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> diff = end - start;
        m_meter->add(diff.count());
    }

    std::chrono::high_resolution_clock::time_point start = {};
    AverageValueMeter* m_meter = {};
};

inline void format(std::ostringstream& stream, const char* str)
{
    stream << str;
}

template <class Arg, class... Args>
inline void format(std::ostringstream& stream, const char* str, Arg&& arg, Args&&... args)
{
    const char* p = strstr(str, "{}");
    if (p)
    {
        stream.write(str, p - str);
        stream << arg;
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