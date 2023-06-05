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

#include "include/sl_version.h"

namespace sl
{

using Feature = uint32_t;

namespace ota
{
struct IOTA
{
    //! Reads manifest downloaded from the server
    //! and collects information about plugins
    //! which have an OTA available
    virtual bool readServerManifest() = 0;
    //! Pings server and downloads OTA config file then
    //! compares it to the local version (if any) and downloads
    //! new plugins if there is an update on the server
    virtual bool checkForOTA(Feature featureID, const Version &apiVersion, bool requestOptionalUpdates) = 0;

    //! Fetches the path to the latest plugin matching the feature ID + API
    //! Version combination.
    //!
    //! On success filePath will be populated with the path to the suitable
    //! plugin file.
    //!
    //! Return values:
    //!   TRUE - a suitable plugin was found
    //!   FALSE - otherwise
    virtual bool getOTAPluginForFeature(Feature featureID, const Version &apiVersion, std::wstring &filePath) = 0;
};

IOTA* getInterface();

}
}
