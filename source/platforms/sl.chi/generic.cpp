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

#if defined(SL_WINDOWS)
#include <d3d12.h>
#endif // defined(SL_WINDOWS)
#define __STDC_FORMAT_MACROS 1
#include <cinttypes>
#include <utility>
#include <string.h>
#include <fstream>

struct IDXGIAdapter;
struct IDXGISwapChain;

#include "source/core/sl.log/log.h"
#include "source/core/sl.extra/extra.h"
#include "source/platforms/sl.chi/generic.h"


#include "shaders/font_cs.h"
#include "shaders/font_spv.h"

// begin font.c attribution comments
/*
 * (c) Copyright 1993, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED 
 * Permission to use, copy, modify, and distribute this software for 
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that 
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. 
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * US Government Users Restricted Rights 
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
// Code originally from https://courses.cs.washington.edu/courses/cse457/98a/tech/OpenGL/font.c
// 
// Additional modifications by NVIDIA (c) 2020
// that were required to make it compile within the Streamline library
//
// end font.c attribution comments

unsigned char GFont[][13] = {
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x36, 0x36, 0x36},
{0x00, 0x00, 0x00, 0x66, 0x66, 0xff, 0x66, 0x66, 0xff, 0x66, 0x66, 0x00, 0x00},
{0x00, 0x00, 0x18, 0x7e, 0xff, 0x1b, 0x1f, 0x7e, 0xf8, 0xd8, 0xff, 0x7e, 0x18},
{0x00, 0x00, 0x0e, 0x1b, 0xdb, 0x6e, 0x30, 0x18, 0x0c, 0x76, 0xdb, 0xd8, 0x70},
{0x00, 0x00, 0x7f, 0xc6, 0xcf, 0xd8, 0x70, 0x70, 0xd8, 0xcc, 0xcc, 0x6c, 0x38},
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x1c, 0x0c, 0x0e},
{0x00, 0x00, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c},
{0x00, 0x00, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30},
{0x00, 0x00, 0x00, 0x00, 0x99, 0x5a, 0x3c, 0xff, 0x3c, 0x5a, 0x99, 0x00, 0x00},
{0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0xff, 0xff, 0x18, 0x18, 0x18, 0x00, 0x00},
{0x00, 0x00, 0x30, 0x18, 0x1c, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x06, 0x06, 0x03, 0x03},
{0x00, 0x00, 0x3c, 0x66, 0xc3, 0xe3, 0xf3, 0xdb, 0xcf, 0xc7, 0xc3, 0x66, 0x3c},
{0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x38, 0x18},
{0x00, 0x00, 0xff, 0xc0, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0xe7, 0x7e},
{0x00, 0x00, 0x7e, 0xe7, 0x03, 0x03, 0x07, 0x7e, 0x07, 0x03, 0x03, 0xe7, 0x7e},
{0x00, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0xff, 0xcc, 0x6c, 0x3c, 0x1c, 0x0c},
{0x00, 0x00, 0x7e, 0xe7, 0x03, 0x03, 0x07, 0xfe, 0xc0, 0xc0, 0xc0, 0xc0, 0xff},
{0x00, 0x00, 0x7e, 0xe7, 0xc3, 0xc3, 0xc7, 0xfe, 0xc0, 0xc0, 0xc0, 0xe7, 0x7e},
{0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x03, 0x03, 0xff},
{0x00, 0x00, 0x7e, 0xe7, 0xc3, 0xc3, 0xe7, 0x7e, 0xe7, 0xc3, 0xc3, 0xe7, 0x7e},
{0x00, 0x00, 0x7e, 0xe7, 0x03, 0x03, 0x03, 0x7f, 0xe7, 0xc3, 0xc3, 0xe7, 0x7e},
{0x00, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x30, 0x18, 0x1c, 0x1c, 0x00, 0x00, 0x1c, 0x1c, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06},
{0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60},
{0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x18, 0x0c, 0x06, 0x03, 0xc3, 0xc3, 0x7e},
{0x00, 0x00, 0x3f, 0x60, 0xcf, 0xdb, 0xd3, 0xdd, 0xc3, 0x7e, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xc3, 0xc3, 0xc3, 0x66, 0x3c, 0x18},
{0x00, 0x00, 0xfe, 0xc7, 0xc3, 0xc3, 0xc7, 0xfe, 0xc7, 0xc3, 0xc3, 0xc7, 0xfe},
{0x00, 0x00, 0x7e, 0xe7, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xe7, 0x7e},
{0x00, 0x00, 0xfc, 0xce, 0xc7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc7, 0xce, 0xfc},
{0x00, 0x00, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xc0, 0xc0, 0xff},
{0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfc, 0xc0, 0xc0, 0xc0, 0xff},
{0x00, 0x00, 0x7e, 0xe7, 0xc3, 0xc3, 0xcf, 0xc0, 0xc0, 0xc0, 0xc0, 0xe7, 0x7e},
{0x00, 0x00, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3},
{0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e},
{0x00, 0x00, 0x7c, 0xee, 0xc6, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06},
{0x00, 0x00, 0xc3, 0xc6, 0xcc, 0xd8, 0xf0, 0xe0, 0xf0, 0xd8, 0xcc, 0xc6, 0xc3},
{0x00, 0x00, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0},
{0x00, 0x00, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xdb, 0xff, 0xff, 0xe7, 0xc3},
{0x00, 0x00, 0xc7, 0xc7, 0xcf, 0xcf, 0xdf, 0xdb, 0xfb, 0xf3, 0xf3, 0xe3, 0xe3},
{0x00, 0x00, 0x7e, 0xe7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xe7, 0x7e},
{0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xfe, 0xc7, 0xc3, 0xc3, 0xc7, 0xfe},
{0x00, 0x00, 0x3f, 0x6e, 0xdf, 0xdb, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x66, 0x3c},
{0x00, 0x00, 0xc3, 0xc6, 0xcc, 0xd8, 0xf0, 0xfe, 0xc7, 0xc3, 0xc3, 0xc7, 0xfe},
{0x00, 0x00, 0x7e, 0xe7, 0x03, 0x03, 0x07, 0x7e, 0xe0, 0xc0, 0xc0, 0xe7, 0x7e},
{0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xff},
{0x00, 0x00, 0x7e, 0xe7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3},
{0x00, 0x00, 0x18, 0x3c, 0x3c, 0x66, 0x66, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3},
{0x00, 0x00, 0xc3, 0xe7, 0xff, 0xff, 0xdb, 0xdb, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3},
{0x00, 0x00, 0xc3, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x3c, 0x3c, 0x66, 0x66, 0xc3},
{0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x3c, 0x66, 0x66, 0xc3},
{0x00, 0x00, 0xff, 0xc0, 0xc0, 0x60, 0x30, 0x7e, 0x0c, 0x06, 0x03, 0x03, 0xff},
{0x00, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c},
{0x00, 0x03, 0x03, 0x06, 0x06, 0x0c, 0x0c, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60},
{0x00, 0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c},
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x66, 0x3c, 0x18},
{0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x38, 0x30, 0x70},
{0x00, 0x00, 0x7f, 0xc3, 0xc3, 0x7f, 0x03, 0xc3, 0x7e, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0},
{0x00, 0x00, 0x7e, 0xc3, 0xc0, 0xc0, 0xc0, 0xc3, 0x7e, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x7f, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f, 0x03, 0x03, 0x03, 0x03, 0x03},
{0x00, 0x00, 0x7f, 0xc0, 0xc0, 0xfe, 0xc3, 0xc3, 0x7e, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x33, 0x1e},
{0x7e, 0xc3, 0x03, 0x03, 0x7f, 0xc3, 0xc3, 0xc3, 0x7e, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0xc0, 0xc0, 0xc0, 0xc0},
{0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00},
{0x38, 0x6c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x00},
{0x00, 0x00, 0xc6, 0xcc, 0xf8, 0xf0, 0xd8, 0xcc, 0xc6, 0xc0, 0xc0, 0xc0, 0xc0},
{0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78},
{0x00, 0x00, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb, 0xfe, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xfc, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00},
{0xc0, 0xc0, 0xc0, 0xfe, 0xc3, 0xc3, 0xc3, 0xc3, 0xfe, 0x00, 0x00, 0x00, 0x00},
{0x03, 0x03, 0x03, 0x7f, 0xc3, 0xc3, 0xc3, 0xc3, 0x7f, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xe0, 0xfe, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xfe, 0x03, 0x03, 0x7e, 0xc0, 0xc0, 0x7f, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x1c, 0x36, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x00},
{0x00, 0x00, 0x7e, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x18, 0x3c, 0x3c, 0x66, 0x66, 0xc3, 0xc3, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xc3, 0xe7, 0xff, 0xdb, 0xc3, 0xc3, 0xc3, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xc3, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0xc3, 0x00, 0x00, 0x00, 0x00},
{0xc0, 0x60, 0x60, 0x30, 0x18, 0x3c, 0x66, 0x66, 0xc3, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0xff, 0x60, 0x30, 0x18, 0x0c, 0x06, 0xff, 0x00, 0x00, 0x00, 0x00},
{0x00, 0x00, 0x0f, 0x18, 0x18, 0x18, 0x38, 0xf0, 0x38, 0x18, 0x18, 0x18, 0x0f},
{0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
{0x00, 0x00, 0xf0, 0x18, 0x18, 0x18, 0x1c, 0x0f, 0x1c, 0x18, 0x18, 0x18, 0xf0},
{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x8f, 0xf1, 0x60, 0x00, 0x00, 0x00}
};

const char *GFORMAT_STR[] = {
    "eFormatINVALID",
    "eFormatRGBA32F",
    "eFormatRGBA16F",
    "eFormatRGB32F", // Pseudo format (for typeless buffers), not supported natively by d3d/vulkan
    "eFormatRGB16F", // Pseudo format (for typeless buffers), not supported natively by d3d/vulkan
    "eFormatRG16F",
    "eFormatR16F",
    "eFormatRG32F",
    "eFormatR32F",
    "eFormatR8UN",
    "eFormatRG8UN",
    "eFormatRGB11F",
    "eFormatRGBA8UN",
    "eFormatSRGBA8UN",
    "eFormatBGRA8UN",
    "eFormatSBGRA8UN",
    "eFormatRG16UI",
    "eFormatRG16SI",
    "eFormatE5M3",
    "eFormatRGB10A2UN",
    "eFormatR8UI",
    "eFormatR16UI",
    "eFormatRG16UN",
    "eFormatR32UI",
    "eFormatRG32UI",
    "eFormatD32S32",
    "eFormatD24S8",
}; static_assert(countof(GFORMAT_STR) == sl::chi::eFormatCOUNT, "Not enough strings for eFormatCOUNT");

#define SL_TEXT_BUFFER_SIZE 16384

namespace sl
{

namespace chi
{

ComputeStatus Generic::genericPostInit()
{
    PlatformType platform;
    getPlatformType(platform);
    if (platform == ePlatformTypeVK)
    {
        CHI_CHECK(createKernel((void*)font_spv, font_spv_len, "fonts.cs", "main", m_kernelFont));
    }
    else
    {
        CHI_CHECK(createKernel((void*)font_cs, font_cs_len, "fonts.cs", "main", m_kernelFont));
    }

    auto node = 0; // FIX THIS
    CHI_CHECK(createBuffer(ResourceDescription(sizeof(GFont), 1, eFormatINVALID), m_font[node], "sl.chi.fontTexture"));
    CHI_CHECK(createBuffer(ResourceDescription(SL_TEXT_BUFFER_SIZE, 1, eFormatINVALID), m_dynamicText[node], "sl.chi.dynamicText"));
    CHI_CHECK(createBuffer(ResourceDescription(SL_TEXT_BUFFER_SIZE, 1, eFormatINVALID, eHeapTypeUpload), m_dynamicTextUpload[node], "sl.chi.dynamicTextUpload"));

    return eComputeStatusOk;
}

ComputeStatus Generic::init(Device device, param::IParameters* params)
{
    m_parameters = params;
    m_typelessDevice = device;
    return eComputeStatusOk;
}

ComputeStatus Generic::shutdown()
{
    // We go last here, all kernels are destroyed already
    m_kernelFont = {};
    for (uint32_t Node = 0; Node < MAX_NUM_NODES; Node++)
    {
        CHI_CHECK(destroyResource(m_font[Node]));
        m_font[Node] = nullptr;
        CHI_CHECK(destroyResource(m_dynamicText[Node]));
        m_dynamicText[Node] = nullptr;
        CHI_CHECK(destroyResource(m_dynamicTextUpload[Node]));
        m_dynamicTextUpload[Node] = nullptr;
    }
    CHI_CHECK(collectGarbage(UINT_MAX));
    SL_LOG_INFO("Delayed destroy resource list count %llu", m_resourcesToDestroy.size());
    m_totalAllocatedSize = 0;
    
    return eComputeStatusOk;
}

ComputeStatus Generic::onHostResourceCreated(Resource resource, const ResourceInfo& info)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // No checks here, we could have this resource with different state if MS recycles pointer
    m_resourceStateMap[resource] = info;
    return eComputeStatusOk;
}

ComputeStatus Generic::setResourceState(Resource resource, ResourceState state, uint32_t subresource)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& entry = m_resourceStateMap[resource];
    entry.desc.state = state;
    return eComputeStatusOk;
}

ComputeStatus Generic::getResourceState(Resource resource, ResourceState& state)
{
    if (!resource)
    {
        state = ResourceState::eUnknown;
        return eComputeStatusOk;
    }

    state = ResourceState::eGeneral;
    PlatformType platform;
    getPlatformType(platform);
    if (platform == ePlatformTypeD3D11)
    {
        return eComputeStatusOk;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_resourceStateMap.find(resource);
    if (it != m_resourceStateMap.end())
    {
        state = (*it).second.desc.state;
        return eComputeStatusOk;
    }
    SL_LOG_ERROR("resource 0x%llx does not have a state", resource);
    return eComputeStatusInvalidArgument;
}

ComputeStatus Generic::transitionResources(CommandList cmdList, const ResourceTransition* transitions, uint32_t count, extra::ScopedTasks* scopedTasks)
{
    if (!cmdList)
    {
        return eComputeStatusInvalidArgument;
    }
    if (!transitions || count == 0) return eComputeStatusOk;

    std::vector<ResourceTransition> transitionList;
    for (uint32_t i = 0; i < count; i++)
    {
        auto tr = transitions[i];
        if (!tr.resource || (tr.from & tr.to) != 0)
        {
            continue;
        }
        if (tr.from != ResourceState::eUnknown)
        {
            if (std::find(transitionList.begin(), transitionList.end(), tr) == transitionList.end())
            {
                transitionList.push_back(tr);
            }
            continue;
        }
        
        getResourceState(transitions[i].resource, tr.from);
        setResourceState(transitions[i].resource, tr.to);
    }

    if (transitionList.empty()) return eComputeStatusOk;

    if (scopedTasks)
    {
        auto lambda = [this, cmdList, transitionList](void) -> void
        {
            std::vector<ResourceTransition> revTransitionList;
            for (auto &tr : transitionList)
            {
                if (chi::ResourceState(tr.from & tr.to) & chi::ResourceState::eStorageRW)
                {
                    // to and from states are UAV which means we need to insert barrier on scope exit to make sure writes are done
                    insertGPUBarrier(cmdList, tr.resource);
                }
                revTransitionList.push_back(ResourceTransition(tr.resource, tr.from, tr.to));
            }
            transitionResources(cmdList, revTransitionList.data(), (uint32_t)revTransitionList.size());
        };
        scopedTasks->tasks.push_back(lambda);
    }

    return transitionResourceImpl(cmdList, transitionList.data(), (uint32_t)transitionList.size());
}

ComputeStatus Generic::createBuffer(const ResourceDescription &CreateResourceDesc, Resource &OutResource, const char InFriendlyName[])
{
    ResourceDescription ResourceDesc = CreateResourceDesc;
    ResourceDesc.flags |= ResourceFlags::eRawOrStructuredBuffer | ResourceFlags::eConstantBuffer;

    {
        ++m_allocCount;
        uint64_t currentSize = ResourceDesc.width;
        m_totalAllocatedSize += currentSize;
        SL_LOG_VERBOSE("Buffer (%s), m_allocCount=%d, currentSize %.1lf MB, totalSize %.1lf MB", InFriendlyName, m_allocCount.load(), (double)currentSize / (1024 * 1024), (double)m_totalAllocatedSize.load() / (1024 * 1024));
    }

    CHI_CHECK(createBufferResourceImpl(ResourceDesc, OutResource, ResourceDesc.state));
    
    setDebugName(OutResource, InFriendlyName);
    setResourceState(OutResource, ResourceDesc.state);
    
    return eComputeStatusOk;
}

ComputeStatus Generic::createTexture2D(const ResourceDescription &CreateResourceDesc, Resource &OutResource, const char InFriendlyName[])
{
    return createTexture2DResourceShared(CreateResourceDesc, OutResource, CreateResourceDesc.format == eFormatINVALID, InFriendlyName);
}

ComputeStatus Generic::createTexture2DResourceShared(const ResourceDescription & CreateResourceDesc, Resource &OutResource, bool UseNativeFormat, const char InFriendlyName[])
{
    ResourceDescription resourceDesc = CreateResourceDesc;
    if (resourceDesc.flags & (ResourceFlags::eRawOrStructuredBuffer | ResourceFlags::eConstantBuffer))
    {
        SL_LOG_ERROR("Creating tex2d with buffer flags");
        return eComputeStatusError;
    }

    if (!(resourceDesc.state & ResourceState::ePresent))
    {
        resourceDesc.flags |= ResourceFlags::eShaderResourceStorage;
    }

    {
        ++m_allocCount;
        if (resourceDesc.format == eFormatINVALID && resourceDesc.nativeFormat != NativeFormatUnknown)
        {
            getFormat(resourceDesc.nativeFormat, resourceDesc.format);
        }
        uint64_t currentSize = (resourceDesc.format != eFormatINVALID) ? resourceDesc.width * resourceDesc.height * getFormatBytesPerPixel(resourceDesc.format) : 0;
        m_totalAllocatedSize += currentSize;        
        SL_LOG_VERBOSE("Tex2d (%s:%u:%u:%s), m_allocCount=%d, currentSize %.1lf MB, totalSize %.1lf MB", InFriendlyName, resourceDesc.width, resourceDesc.height, GFORMAT_STR[resourceDesc.format], m_allocCount.load(), (double)currentSize / (1024 * 1024), (double)m_totalAllocatedSize.load() / (1024 * 1024));
    }

    CHI_CHECK(createTexture2DResourceSharedImpl(resourceDesc, OutResource, UseNativeFormat, resourceDesc.state));
    
    setResourceState(OutResource, resourceDesc.state);
    setDebugName(OutResource, InFriendlyName);
    
    return eComputeStatusOk;
}

ComputeStatus Generic::destroy(std::function<void(void)> task)
{
    // Delayed destroy for safety
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_destroyWithLambdas.push_back({ task, m_finishedFrame });
    }
    SL_LOG_VERBOSE("Scheduled to destroy lambda task - frame %u", m_finishedFrame.load());
    return eComputeStatusOk;
}

ComputeStatus Generic::destroyResource(Resource& resource)
{
    if (!resource) return eComputeStatusOk; // OK to release null resource

    ResourceDescription desc;
    auto validResource = getResourceDescription(resource, desc) == eComputeStatusOk;

    if (m_releaseCallback && validResource)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_resourceStateMap.erase(resource);
        }
        sl::Resource res = { (desc.flags & ResourceFlags::eRawOrStructuredBuffer) != 0 ? ResourceType::eResourceTypeBuffer : ResourceType::eResourceTypeTex2d, (void*)resource, nullptr, nullptr, nullptr };
        m_releaseCallback(&res, m_typelessDevice);
    }
    else
    {
        // Delayed destroy for safety
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            TimestampedResource rest = { resource, m_finishedFrame };
            if (std::find(m_resourcesToDestroy.begin(), m_resourcesToDestroy.end(), rest) == m_resourcesToDestroy.end())
            {
                m_resourcesToDestroy.push_back(rest);
                auto name = getDebugName(resource);
                SL_LOG_VERBOSE("Scheduled to destroy 0x%llx(%S) - frame %u", resource, name.c_str(), m_finishedFrame.load());
            }
        }
    }
    resource = {};
    return eComputeStatusOk;
}

ComputeStatus Generic::collectGarbage(uint32_t finishedFrame)
{    
    if (finishedFrame != UINT_MAX)
    {
        m_finishedFrame.store(finishedFrame);
    }

    TimestampedLambdaList destroyWithLambdas;
    TimestampedResourceList resourcesToDestroy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        destroyWithLambdas = m_destroyWithLambdas;
        resourcesToDestroy = m_resourcesToDestroy;
    }

    {
        auto it = destroyWithLambdas.begin();
        while (it != destroyWithLambdas.end())
        {
            auto& tres = (*it);
            // Run lambda is
            if (finishedFrame > tres.frame + 3)
            {
                SL_LOG_VERBOSE("Calling destroy lambda - scheduled at frame %u - finished frame %u - forced %s", tres.frame, m_finishedFrame.load(), finishedFrame != UINT_MAX ? "no" : "yes");
                tres.task();
                it = destroyWithLambdas.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    {
        auto it = resourcesToDestroy.begin();
        while (it != resourcesToDestroy.end())
        {
            auto& tres = (*it);
            // Release resources dumped more than few frames ago
            if (finishedFrame > tres.frame + 3)
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_resourceStateMap.erase(tres.resource);
                }
                auto name = getDebugName(tres.resource);
                SL_LOG_VERBOSE("Destroying 0x%llx(%S) - scheduled at frame %u - finished frame %u - forced %s", tres.resource, name.c_str(), tres.frame, m_finishedFrame.load(), finishedFrame != UINT_MAX ? "no" : "yes");
                destroyResourceDeferredImpl(tres.resource);
                it = resourcesToDestroy.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_resourcesToDestroy = resourcesToDestroy;
        m_destroyWithLambdas = destroyWithLambdas;
    }
    return eComputeStatusOk;
}

ComputeStatus Generic::insertGPUBarrierList(CommandList cmdList, const Resource* InResources, unsigned int InResourceCount, BarrierType InBarrierType)
{
    if (InBarrierType == BarrierType::eBarrierTypeUAV)
    {
        std::vector< D3D12_RESOURCE_BARRIER> Barriers;
        for (unsigned int i = 0; i < InResourceCount; i++)
        {
            const Resource& Res = InResources[i];
            ComputeStatus Status = insertGPUBarrier(cmdList, Res, InBarrierType);
            if (eComputeStatusOk != Status)
                return Status;
        }
    }
    else
    {
        assert(false);
        return eComputeStatusNotSupported;
    }
    return eComputeStatusOk;
}

ComputeStatus Generic::renderText(CommandList cmdList, int x, int y, const char *text, const ResourceArea &out, const float4& color, int reverseX, int reverseY)
{
    if (!cmdList || !text) return eComputeStatusInvalidPointer;
    
    unsigned int textSize = (unsigned int)strlen(text);
    if (textSize >= SL_TEXT_BUFFER_SIZE) return eComputeStatusInvalidArgument;

    auto node = 0; // FIX THIS

    uint32_t index = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        static bool firstRun = true;
        if (firstRun)
        {
            firstRun = false;
            // Copy font data at the back so text can start from the front
            CHI_CHECK(copyHostToDeviceBuffer(cmdList, sizeof(GFont), GFont, m_dynamicTextUpload[node], m_font[node], SL_TEXT_BUFFER_SIZE - sizeof(GFont), 0));
        }
        if (m_textIndex[node] + textSize >= SL_TEXT_BUFFER_SIZE)
        {
            m_textIndex[node] = 0;
        }
        index = m_textIndex[node];
        CHI_CHECK(copyHostToDeviceBuffer(cmdList, textSize, (const void*)text, m_dynamicTextUpload[node], m_dynamicText[node], index, index));
        m_textIndex[node] = extra::align((m_textIndex[node] + textSize) % SL_TEXT_BUFFER_SIZE, 4);
    }

    CHI_CHECK(bindSharedState(cmdList));
    CHI_CHECK(bindKernel(m_kernelFont));

    struct CBData
    {
        float4 color;
        uint32_t text_offset;
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
        uint32_t surf_offset_x;
        uint32_t surf_offset_y;
        int  reverse_x;
        int  reverse_y;
    };
    CBData cb;
    cb.color = color;
    cb.x = x;
    cb.y = y;
    cb.width = out.dimensions.width;
    cb.height = out.dimensions.height;
    cb.surf_offset_x = out.base.x;
    cb.surf_offset_y = out.base.y;
    cb.reverse_x = reverseX;
    cb.reverse_y = reverseY;
    cb.text_offset = index;
    CHI_CHECK(bindRawBuffer(0, 0, m_dynamicText[node]));
    CHI_CHECK(bindRawBuffer(1, 1, m_font[node]));
    CHI_CHECK(bindRWTexture(2, 2, out.resource));
    CHI_CHECK(bindConsts(3, 0, &cb, sizeof(CBData), 64));
    CHI_CHECK(dispatch(textSize, 1, 1));

    return eComputeStatusOk;
}

size_t Generic::getFormatBytesPerPixel(Format InFormat)
{
    static constexpr size_t BYTES_PER_PIXEL_TABLE[] = {
        1,                      // eFormatINVALID - aka unknown - used for buffers
        4 * sizeof(float),      // eFormatRGBA32F,
        4 * sizeof(uint16_t),   // eFormatRGBA16F,
        3 * sizeof(float),      // eFormatRGB32F, 
        3 * sizeof(uint16_t),   // eFormatRGB16F,
        2 * sizeof(uint16_t),   // eFormatRG16F,
        1 * sizeof(uint16_t),   // eFormatR16F,
        2 * sizeof(float),      // eFormatRG32F,
        1 * sizeof(float),      // eFormatR32F,
        1,                      // eFormatR8UN,
        2,                      // eFormatRG8UN,
        sizeof(uint32_t),       // eFormatRGB11F,
        sizeof(uint32_t),       // eFormatRGBA8UN,
        sizeof(uint32_t),       // eFormatSRGBA8UN,
        sizeof(uint32_t),       // eFormatBGRA8UN,
        sizeof(uint32_t),       // eFormatSBGRA8UN,
        2 * sizeof(uint16_t),   // eFormatRG16UI,
        2 * sizeof(uint16_t),   // eFormatRG16SI,
        1 * sizeof(uint8_t),    // eFormatE5M3,
        sizeof(uint32_t),       // eFormatRGB10A2UN,
        1,                      // eFormatR8UI,
        2,                      // eFormatR16UI,
        4,                      // eFormatRG16UN,
        4,                      // eFormatR32UI,
        8,                      // eFormatRG32UI,
        8,                      // eFormatD32S32,
        4,                      // eFormatD24S8,
    }; static_assert(countof(BYTES_PER_PIXEL_TABLE) == eFormatCOUNT, "Not enough numbers for eFormatCOUNT");

    return BYTES_PER_PIXEL_TABLE[InFormat];
}

uint64_t Generic::getResourceSize(Resource res)
{
    ResourceDescription resourceDesc;
    if (getResourceDescription(res, resourceDesc) != eComputeStatusOk)
    {
        return 0;
    }
    Format format = resourceDesc.format;
    if (format == eFormatINVALID && resourceDesc.nativeFormat != NativeFormatUnknown)
    {
        getFormat(resourceDesc.nativeFormat, format);
        if (format == eFormatINVALID)
        {
            SL_LOG_ERROR("Don't know the size for resource 0x%llx format %u native %u", res, resourceDesc.format, resourceDesc.nativeFormat);
        }
    }
    return resourceDesc.width * resourceDesc.height * getFormatBytesPerPixel(format);
}

ComputeStatus Generic::getNativeFormat(Format format, NativeFormat& native)
{
    native = DXGI_FORMAT_UNKNOWN;
    switch (format)
    {
        case eFormatR8UN: native = DXGI_FORMAT_R8_UNORM; break;
        case eFormatRG8UN: native = DXGI_FORMAT_R8G8_UNORM; break;
        case eFormatRGB10A2UN: native = DXGI_FORMAT_R10G10B10A2_UNORM; break;
        case eFormatRGBA8UN: native = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case eFormatBGRA8UN: native = DXGI_FORMAT_B8G8R8A8_UNORM; break;
        case eFormatRGBA32F: native = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
        case eFormatRGB32F:  native = DXGI_FORMAT_R32G32B32_FLOAT; break;
        case eFormatRGBA16F: native = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
        case eFormatRGB11F: native = DXGI_FORMAT_R11G11B10_FLOAT; break;
        case eFormatRG16F: native = DXGI_FORMAT_R16G16_FLOAT; break;
        case eFormatRG16UI: native = DXGI_FORMAT_R16G16_UINT; break;
        case eFormatRG16SI: native = DXGI_FORMAT_R16G16_SINT; break;
        case eFormatRG32F: native = DXGI_FORMAT_R32G32_FLOAT; break;
        case eFormatR16F: native = DXGI_FORMAT_R16_FLOAT; break;
        case eFormatR32F: native = DXGI_FORMAT_R32_FLOAT; break;
        case eFormatR8UI: native = DXGI_FORMAT_R8_UINT; break;
        case eFormatR16UI: native = DXGI_FORMAT_R16_UINT; break;
        case eFormatRG16UN: native = DXGI_FORMAT_R16G16_UNORM; break;
        case eFormatR32UI: native = DXGI_FORMAT_R32_UINT; break;
        case eFormatRG32UI: native = DXGI_FORMAT_R32G32_UINT; break;
        case eFormatD24S8: native = DXGI_FORMAT_R24G8_TYPELESS; break;
        case eFormatD32S32: native = DXGI_FORMAT_R32G8X24_TYPELESS; break;
        case eFormatSBGRA8UN: native = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; break;
        case eFormatSRGBA8UN: native = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
        case eFormatE5M3: // fall through
        default: assert(false);
    }
    return eComputeStatusOk;
};

 ComputeStatus Generic::getFormat(NativeFormat nativeFmt, Format &format)
{
    format = eFormatINVALID;

    DXGI_FORMAT DXGIFmt = static_cast<DXGI_FORMAT>(nativeFmt);

    switch (DXGIFmt)
    {
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            format = eFormatSBGRA8UN; break;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
            format = eFormatBGRA8UN; break;
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_TYPELESS:
            format = eFormatR8UN; break;
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_TYPELESS:
            format = eFormatRG8UN; break;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            format = eFormatRGB10A2UN; break;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            format = eFormatSRGBA8UN; break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            format = eFormatRGBA8UN; break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            format = eFormatRGBA32F; break;
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_TYPELESS:
            format = eFormatRGB32F; break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            format = eFormatRGBA16F; break;
        case DXGI_FORMAT_R11G11B10_FLOAT:
            format = eFormatRGB11F; break;
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_TYPELESS:
            format = eFormatRG16F; break;
        case DXGI_FORMAT_R16G16_UINT:
            format = eFormatRG16UI; break;
        case DXGI_FORMAT_R16G16_SINT:
            format = eFormatRG16SI; break;
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_TYPELESS:
            format = eFormatRG32F; break;
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_TYPELESS:
            format = eFormatR16F; break;
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS:
            format = eFormatR32F; break;
        case DXGI_FORMAT_R8_UINT:
            format = eFormatR8UI; break;
        case DXGI_FORMAT_R16_UINT:
            format = eFormatR16UI; break;
        case DXGI_FORMAT_R16G16_UNORM:
            format = eFormatRG16UN; break;
        case DXGI_FORMAT_R32_UINT:
            format = eFormatR32UI; break;
        case DXGI_FORMAT_R32G32_UINT:
            format = eFormatRG32UI; break;
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            format = eFormatD24S8; break;
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            format = eFormatD32S32;
    }

    return eComputeStatusOk;
};

bool Generic::savePFM(const std::string &path, const char* srcBuffer, const int width, const int height)
{
    auto fpath = path;
    fpath += ".pfm";

    const int totalBytes = width * height * sizeof(float) * 3;

    // Setup header
    const std::string header = "PF\n" + std::to_string(width) + " " + std::to_string(height) + "\n-1.0\n";
    const size_t headerSize = header.length();
    const char* headerArr = header.c_str();

    std::ofstream binWriter(fpath.c_str(), std::ios::binary);

    if (!binWriter)
    {
        SL_LOG_ERROR("Failed to open %s", fpath.c_str());
        binWriter.close();
        return false;
    }

    binWriter.write(header.c_str(), headerSize);
    binWriter.write(srcBuffer, totalBytes);
    binWriter.close();
    return true;
}

}
}