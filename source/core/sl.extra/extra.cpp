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

#include <map>
#include <string>
#include <Windows.h>
#include <tlhelp32.h>

#include "source/core/sl.log/log.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.extra/extra.h"

namespace sl
{

namespace extra
{

namespace keyboard
{
struct Keyboard : IKeyboard
{
    virtual void registerKey(const char* name, const VirtKey& key) override final
    {
        if (m_keys.find(name) == m_keys.end())
        {
            m_keys[name] = key;
        }
        else
        {
            SL_LOG_WARN("Hot-key `%s` already registered", name);
        }
    }

    virtual bool wasKeyPressed(const char* name) override final
    {
#ifdef SL_PRODUCTION
        SL_LOG_WARN_ONCE("Keyboard manager disabled in production");
        return false;
#endif
        auto key = m_keys[name];
        // Only if we have focus, otherwise ignore keys
#ifdef SL_WINDOWS
        if (!hasFocus())
        {
            return false;
        }

        // Table of currently pressed keys with all possible combinations of modifier keys
        // indexed by the virtual key itself and the current state of each modifier key, using
        // 0 if the key is not pressed and 1 if the key is pressed: [vKey][shift][control][alt]
        static bool GKeyDown[256][2][2][2] = { false };
        if (key.m_mainKey <= 0 || key.m_mainKey > 255)
            return false;

        bool bKeyDown = ((GetAsyncKeyState(key.m_mainKey) & 0x8000) != 0) &&
            (((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) == key.m_bShift) &&
            (((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) == key.m_bControl) &&
            (((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) == key.m_bAlt);

        int shiftIndex = key.m_bShift ? 1 : 0;
        int controlIndex = key.m_bControl ? 1 : 0;
        int altIndex = key.m_bAlt ? 1 : 0;

        bool bPressed = !bKeyDown && GKeyDown[key.m_mainKey][shiftIndex][controlIndex][altIndex];
        GKeyDown[key.m_mainKey][shiftIndex][controlIndex][altIndex] = bKeyDown;
        return bPressed;
#else
        return false;
#endif
    }

    virtual const VirtKey& getKey(const char* name) override final
    {
        return m_keys[name];
    }

    virtual bool hasFocus() override final
    {
#ifdef SL_WINDOWS
        HWND wnd = GetForegroundWindow();
        DWORD pidWindow = 0;
        GetWindowThreadProcessId(wnd, &pidWindow);
        auto pidCurrent = GetCurrentProcessId();
        if (pidCurrent != pidWindow)
        {
            // Check if parent process own the foreground window
            if (!m_processEntry.dwSize)
            {
                HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                m_processEntry.dwSize = sizeof(PROCESSENTRY32);
                if (Process32First(h, &m_processEntry))
                {
                    do
                    {
                        if (m_processEntry.th32ProcessID == pidCurrent)
                        {
                            break;
                        }
                    } while (Process32Next(h, &m_processEntry));
                }
                CloseHandle(h);
            }
            return m_processEntry.th32ParentProcessID == pidWindow;
        }
#endif
        return true;
    }

    PROCESSENTRY32 m_processEntry{};
    std::map<std::string, VirtKey> m_keys;
};

IKeyboard* getInterface()
{
    static Keyboard s_keyboard;
    return &s_keyboard;
}
}

}
}
