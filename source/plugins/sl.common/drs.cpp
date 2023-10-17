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

#include "drs.h"
#include <mutex>

#ifdef SL_WINDOWS
#define NV_WINDOWS
#endif


namespace drs
{
#ifdef NV_WINDOWS
    NvDRSSessionHandle g_hDRSSession = NULL;
    NvDRSProfileHandle g_hDRSProfile = 0;
#endif
    std::mutex g_mutex;

    bool drsInit()
    {
#ifdef NV_WINDOWS
        if (g_hDRSProfile && g_hDRSSession)
            return true;
        std::unique_lock<std::mutex> lock(g_mutex);
        if (g_hDRSProfile && g_hDRSSession)
            return true;
        if (NVAPI_OK == NvAPI_DRS_CreateSession(&g_hDRSSession))
        {
            if (NVAPI_OK == NvAPI_DRS_LoadSettings(g_hDRSSession))
            {
                if (NVAPI_OK != NvAPI_DRS_GetBaseProfile(g_hDRSSession, &g_hDRSProfile))
                {
                    g_hDRSProfile = NULL;
                }
            }
        }
        if (g_hDRSProfile && g_hDRSSession)
            return true;
        if (!g_hDRSProfile && g_hDRSSession)
        {
            NvAPI_DRS_DestroySession(g_hDRSSession);
            g_hDRSSession = NULL;
        }
#endif
        return false;
    }

    void drsShutdown()
    {
#ifdef NV_WINDOWS
        if (!g_hDRSProfile)
            return;
        std::unique_lock<std::mutex> lock(g_mutex);
        if (!g_hDRSProfile)
            return;
        NvAPI_DRS_DestroySession(g_hDRSSession);
        g_hDRSSession = NULL;
        g_hDRSProfile = NULL;
#endif
    }

    bool drsReadKeyString(NvU32 keyId, std::wstring& value)
    {
#ifdef NV_WINDOWS
        std::unique_lock<std::mutex> lock(g_mutex);
        if (!g_hDRSProfile)
        {
            return false;
        }
        NVDRS_SETTING profileSetting;
        profileSetting.version = NVDRS_SETTING_VER;
        if (NvAPI_DRS_GetSetting(g_hDRSSession, g_hDRSProfile, keyId, &profileSetting) != NVAPI_OK)
            return false;
        const char* s = (const char*)&profileSetting.binaryCurrentValue.valueData[0];
        value = std::wstring(s, s + strlen(s));
        return true;
#else
        return false;
#endif
    }

    bool drsReadKey(NvU32 keyId, NvU32& value)
    {
#ifdef NV_WINDOWS
        std::unique_lock<std::mutex> lock(g_mutex);
        if (!g_hDRSProfile)
        {
            return false;
        }
        NVDRS_SETTING profileSetting;
        profileSetting.version = NVDRS_SETTING_VER;
        if (NvAPI_DRS_GetSetting(g_hDRSSession, g_hDRSProfile, keyId, &profileSetting) != NVAPI_OK)
            return false;
        value = profileSetting.u32CurrentValue;
        return true;
#else
        return false;
#endif
    }
}
