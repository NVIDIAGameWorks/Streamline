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

#include <stdint.h>
#include <string.h>

namespace sl
{

//! GUID
struct StructType
{
    unsigned long  data1;
    unsigned short data2;
    unsigned short data3;
    unsigned char  data4[8];

    inline bool operator==(const StructType& rhs) const { return memcmp(this, &rhs, sizeof(this)) == 0; }
    inline bool operator!=(const StructType& rhs) const { return memcmp(this, &rhs, sizeof(this)) != 0; }
};

//! SL is using typed and versioned structures which can be chained or not.
//! 
//! -------- Example using chaining --------
//! 
//! SL_STRUCT(S1, GUID1, kStructVersion1)
//! {
//!   A
//!   B
//!   C
//! }
//! 
//! SL_STRUCT(S1, GUID1, kStructVersion2)
//! {
//!   D
//!   E
//! }
//! 
//! S1 s1;
//! slFunction(s1) // old code
//! 
//! S2 s2;
//! s1->next = &s2; // new code
//! slFunction(s1)
//! 
//! -------- Example NOT using chaining -------- 
//! 
//! SL_STRUCT(S1, GUID1, kStructVersion1)
//! {
//!   A
//!   B
//!   C
//! }
//! 
//! SL_STRUCT(S1, GUID1, kStructVersion2)
//! {
//!   A
//!   B
//!   C
//!   D
//!   E
//! }
//! 
//! S1 s1;
//! slFunction(s1) // old and new code

constexpr uint32_t kStructVersion1 = 1;

struct BaseStructure
{
    BaseStructure() = delete;
    BaseStructure(StructType t, uint32_t v) : structType(t), structVersion(v) {};
    BaseStructure* next{};
    StructType structType{};
    size_t structVersion;
};

#define SL_STRUCT(name, guid, version)                                      \
struct name : public BaseStructure                                          \
{                                                                           \
    name##() : BaseStructure(guid, version){}                               \
    constexpr static StructType s_structType = guid;                        \

#define SL_STRUCT_PROTECTED(name, guid, version)                            \
struct name : public BaseStructure                                          \
{                                                                           \
protected:                                                                  \
    name##() : BaseStructure(guid, version){}                               \
public:                                                                     \
    constexpr static StructType s_structType = guid;                        \

} // namespace sl
