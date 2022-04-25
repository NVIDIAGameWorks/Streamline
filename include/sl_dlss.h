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

namespace sl
{

enum DLSSMode
{
    eDLSSModeOff,
    eDLSSModeMaxPerformance,
    eDLSSModeBalanced,
    eDLSSModeMaxQuality,
    eDLSSModeUltraPerformance,
    eDLSSModeUltraQuality,
    eDLSSModeCount
};

struct DLSSConstants
{
    //! Specifies which mode should be used
    DLSSMode mode = eDLSSModeOff;
    //! Specifies output (final) target width
    uint32_t outputWidth = INVALID_UINT;
    //! Specifies output (final) target height
    uint32_t outputHeight = INVALID_UINT;
    //! Specifies sharpening level in range [0,1]
    float sharpness = 0.0f;
    //! Specifies pre-exposure value
    float preExposure = 1.0f;
    //! Specifies exposure scale value
    float exposureScale = 1.0f;
    //! Specifies if tagged color buffers are full HDR or not (DLSS in HDR pipeline or not)
    Boolean colorBuffersHDR = Boolean::eTrue;
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

//! Returned by DLSS plugin, please see 'getFeatureSettings' API
struct DLSSSettings
{
    //! Specifies render area width
    uint32_t optimalRenderWidth;
    //! Specifies render area height
    uint32_t optimalRenderHeight;
    //! Specifies the optimal sharpness value
    float optimalSharpness;
    //! Points to DLSSSettings1 or null if not needed
    void* ext = {};
};

struct DLSSSettings1
{
    //! Specifies minimal render area width
    uint32_t renderWidthMin;
    //! Specifies minimal render area height
    uint32_t renderHeightMin;
    //! Specifies maximal render area width
    uint32_t renderWidthMax;
    //! Specifies maximal render area height
    uint32_t renderHeightMax;
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

}
