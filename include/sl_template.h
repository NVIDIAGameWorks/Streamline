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

//! Each feature must have a unique id, please see sl.h enum Feature
//! 
constexpr uint32_t eFeatureTemplate = 0xffff;

//! If your plugin does not have any constants then the code below can be removed
//! 
enum TemplateMode
{
    eOff,
    eOn
};

struct TemplateConstants
{
    TemplateMode mode = eOff;
    // Points to TemplateConstants1 or nullptr
    void* ext = {};
};

struct TemplateConstants1
{
    // New parameters included in new version of your plugin go here
    // 
    // Points to TemplateConstants2 or nullptr
    void* ext = {};
};

struct TemplateConstants2
{
    // New parameters included in new version of your plugin go here
    //     
    // Always allow for the option to extend this in the future
    void* ext{};
};

//! IMPORTANT: If your plugin does not have any settings then the code below can be removed
//! 
struct TemplateSettings
{
    // Always allow for the option to extend this in the future
    void* ext = {};
};

}
