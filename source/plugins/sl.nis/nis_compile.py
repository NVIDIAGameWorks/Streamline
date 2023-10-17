#!/usr/bin/python

# Copyright (c) 2022 NVIDIA CORPORATION. All rights reserved
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import subprocess
import shutil
import argparse
import os.path

def hasCompiler(compiler):
    return shutil.which(compiler) is not None

def outputFilename(scalerMode, viewportSupport, hdrMode):
    mode = "Scaler" if scalerMode else "Sharpen"
    return f"NIS_{mode}_V{viewportSupport}_H{hdrMode}"

def appendToHeader(shadersFolder, outputHeader, shaderName, extension):
    print(f"{shaderName}.{extension}")
    try:
        bytecode = []
        with open(os.path.join(shadersFolder, shaderName) + "." + extension, "rb") as f:
            bytecode = bytearray(f.read())
        size = len(bytecode)
        str = f"unsigned char {shaderName}_{extension}[] = {{\n"
        i = 0
        while i < size:
            j = 0
            str += "  "
            while i < size and j < 16:
                str += f"0x{bytecode[i]:02x}"
                if i < size - 1 and j < 15:
                    str += ", "
                if j == 15:
                    str += ","
                i += 1
                j += 1
            str += "\n"

        str += f"}};\n"
        str += f"unsigned int {shaderName}_{extension}_len = {size};\n\n"
        with open(outputHeader, "a") as f:
            f.write(str)
    except IOError:
        print('Error Opening Compiled Shader!')
    return

def GetOptimalArguments(arch, isUpscaling):
    blockWidth = 32
    blockHeight = 24
    threadGroupSize = 256
    if arch == 'NVIDIA_Generic':
        blockHeight = 24 if isUpscaling else 32
        threadGroupSize = 128
    if arch == 'NVIDIA_Generic_fp16':
        blockHeight = 32
        threadGroupSize = 128
    if arch == 'AMD_Generic':
        threadGroupSize = 256
    if arch == 'Intel_Generic':
        threadGroupSize = 256
    return blockWidth, blockHeight, threadGroupSize


# generatePermutation uses slang to compile NIS shader permutations
def generatePermutations(slangcPath, dxcPath, inputFolder, outputFolder):
    compiler = os.path.join(slangcPath, "slangc")
    inputShader = os.path.join(inputFolder, 'NIS_Main.hlsl')
    outputHeader = os.path.join(outputFolder, 'NIS_shaders.h')
    if os.path.exists(outputHeader):
        os.remove(outputHeader)

    shaderTypes = ['cs', 'cs6', 'spv']
    for upscale in range(2):
        for viewport in range(2):
            for hdr in range(3):
                for st in shaderTypes:
                    shaderName = outputFilename(upscale, viewport, hdr)
                    fullName = os.path.join(outputFolder, shaderName) + "." + st
                    options = ""
                    if st == 'cs':
                        arch = 'NVIDIA_Generic'
                        blockWidth, blockHeight, threadGroupSize = GetOptimalArguments(arch, upscale)
                        target = 'dxbc'
                        profile = 'sm_5_0'
                        hlsl_6_2 = 0
                        use_half_precision = 0
                        use_vk_bindings = 0
                        options += f" -target {target}"
                        options += f" -D NIS_UNROLL_INNER= "
                    if st == 'spv':
                        arch = 'NVIDIA_Generic_fp16'
                        blockWidth, blockHeight, threadGroupSize = GetOptimalArguments(arch, upscale)
                        target = "spirv"
                        hlsl_6_2 = 1
                        profile = 'sm_6_2'
                        use_vk_bindings = 1
                        use_half_precision = 1
                        options += f" -target {target}"
                    if st == 'cs6':
                        if dxcPath != None:
                            arch = 'NVIDIA_Generic_fp16'
                            blockWidth, blockHeight, threadGroupSize = GetOptimalArguments(arch, upscale)
                            target = 'dxil'
                            profile = 'sm_6_2'
                            hlsl_6_2 = 1
                            use_vk_bindings = 0
                            use_half_precision = 1
                            options += f' -dxc-path "{dxcPath}" '
                        else:
                            arch = 'NVIDIA_Generic'
                            blockWidth, blockHeight, threadGroupSize = GetOptimalArguments(arch, upscale)
                            target = 'dxbc'
                            profile = 'sm_5_0'
                            hlsl_6_2 = 0
                            use_half_precision = 0
                            use_vk_bindings = 0
                            options += f" -target {target}"
                            options += f" -D NIS_UNROLL_INNER= "

                    options += f" -D NIS_SCALER={upscale}"
                    options += f" -D NIS_GLSL=0"
                    options += f" -D NIS_BLOCK_WIDTH={blockWidth}"
                    options += f" -D NIS_BLOCK_HEIGHT={blockHeight}"
                    options += f" -D NIS_THREAD_GROUP_SIZE={threadGroupSize}"
                    options += f" -D NIS_VIEWPORT_SUPPORT={viewport}"
                    options += f" -D NIS_HDR_MODE={hdr}"
                    options += f" -D NIS_HLSL_6_2={hlsl_6_2}"
                    options += f" -D NIS_DXC={use_vk_bindings}"
                    options += f" -D NIS_USE_HALF_PRECISION={use_half_precision}"
                    options += f" -entry main -stage compute -profile {profile} -O3 -o {fullName} {inputShader}"
                    try:
                        return_code = subprocess.run(compiler+" "+options)
                    except:
                        print("Compiler not found. Please provide ")
                        return
                    if os.path.exists(fullName):
                        appendToHeader(outputFolder, outputHeader, shaderName, st)
                        os.remove(fullName)
    print("\nOutput header file : " + outputHeader)
    return

if __name__ == '__main__':
    print("-----------------------------------------")
    print("NVIDIA Image Scaling SKD Header Generator")
    print("-----------------------------------------")
    print("This script uses slang and dxc. For more information visit: ")
    print("https://github.com/shader-slang/slang")
    print("https://github.com/microsoft/DirectXShaderCompiler")
    print("\n")
    parser = argparse.ArgumentParser(description='Compiles shader permutations and generates header files')
    parser.add_argument('--slangc', nargs='?', required=True, help='Shader compiler. Especify a path to slangc')
    parser.add_argument('--dxc_path', nargs='?', help='DXC Shader compiler. Especify a path to dxc')
    parser.add_argument('--input_path', nargs='?', help='NIS shader path. Path to NIS_main.hlsl')
    parser.add_argument('--output_path', nargs='?', help='NIS compiled header output path')
    args = parser.parse_args()
    print("\n")
    if args.slangc == None and not hasCompiler("slangc"):
        print("slangc not found")
        exit(1)
    if args.input_path == None:
        args.input_path = "NIS"
    if args.output_path == None:
        args.output_path = "."
    print("---------- Shader header generation -----------")
    print("slangc : ", args.slangc)
    print("dxc_path : ", args.dxc_path)
    print("input_path : ", args.input_path)
    print("output_path : ", args.output_path)
    print("-----------------------------------------------")
    if args.dxc_path == None:
        print("Warning: dxc-path not specified. hlsl 5.0 will be used instead of 6.2")

    generatePermutations(args.slangc, args.dxc_path , args.input_path, args.output_path)
