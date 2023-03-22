# Streamline (SL) - Version 2.0

Streamline is an open-sourced cross-IHV solution that simplifies integration of the latest NVIDIA and other independent hardware vendors’ super resolution technologies into applications and games. This framework allows developers to easily implement one single integration and enable multiple super-resolution technologies and other graphics effects supported by the hardware vendor.

This repo contains the SDK for integrating Streamline into your application.

For a high level overview, see the [NVIDIA Developer Streamline page](https://developer.nvidia.com/rtx/streamline)

> **IMPORTANT:**
> For important changes and bug fixes included in the current release, please see the [Release Notes](release.txt)

------

## Prerequisites

#### Hardware

- GPU supporting DirectX 11 and Vulkan 1.2 or higher

#### Compiling Shaders

Users who wish to compile SL will need to compile shaders.  As a result, you will need to place slang in the .\external\slang_internal folder and xxd in the .\tools folder.  Then any *.hlsl files in the .\shaders folder will be compiled at build time.

Slang may be found at https://github.com/shader-slang/slang

xxd may be found at https://sourceforge.net/projects/xxd-for-windows/

See the `premake.lua` file for how these are used.

A upcoming release of SL may include these pre-compiled.

Additionally, if you would like to recompile the shaders for the NIS plugin, you will need to have Python 3 installed and in the path.

#### Windows

- Win10 20H1 (version 2004 - 10.0.19041) or newer
- Install latest graphics driver (**if using NVIDIA GPU it MUST be 512.15 or newer**)
- Install VS Code or VS2017/VS2019 with [SDK 10.0.19041+](https://go.microsoft.com/fwlink/?linkid=2120843)
- Install "git".
- Clone your fork to a local hard drive, make sure to use a NTFS drive on Windows (SL uses symbolic links)
- Execute `./setup.bat` with `{vs2017|vs2019}` (`vs2017` is default)

The make/solution files will be found in the generated `_project` directory.

## Building

- Execute `./build.bat` with `-{debug|release|production}` (`debug` is default) or use VS IDE and load solution from the `_project` directory

> **IMPORTANT:**
> Only use `production` builds (digitally signed by NVIDIA) when releasing your software otherwise SL plugins could be replaced with potentially malicious modules.

The build outputs can be found in the generated `_artifacts` folder. The default setting is to target x86_64 CPU architecture.

The default setting is to target x86_64 CPU architecture.

> NOTE: To build the project minimal configuration is needed. Any version of Windows 10 will do. Then
run the setup and build scripts as described here above. That's it. The specific version of Windows, NVIDIA driver,
or Vulkan are all runtime dependencies, not compile/link time dependencies. This allows SL to build on stock
virtual machines that require zero configuration. This is a beautiful thing, help us keep it that way.

## SDK Packaging

- Execute `./package.bat` with `-{debug|release|production}` (`production` is default)

The packaged SDK can be found in the generated `_sdk` folder.

## Developing

Please read [Developing.md](docs/Developing.md) to learn about our development flow.

## Architecture

Please read [Architecture.md](docs/Architecture.md) to learn about the architecture.

## Debugging

Please read [Debugging.md](docs/Debugging.md) to learn how to debug and troubleshoot issues.

## General Programming Guide

Please read [ProgrammingGuide.md](docs/ProgrammingGuide.md) to learn about the integration in games.

## Advanced Programming Guide - Manual Hooking With Lowest Overhead

Please read [ProgrammingGuideManualHooking.md](docs/ProgrammingGuideManualHooking.md) to learn about advanced SL integration in games.

## Programming Guides Per Feature:

- [DLSS Super Resolution](docs/ProgrammingGuideDLSS.md)
- [DLSS Frame Generation](docs/ProgrammingGuideDLSS_G.md)
- [NRD](docs/ProgrammingGuideNRD.md)
- [Reflex](docs/ProgrammingGuideReflex.md)
- [NIS](docs/ProgrammingGuideNIS.md)

## Sample Plugin Source Code

A sample Streamline plugin source code is located [here](source/plugins/sl.template/templateEntry.cpp)

## Sample App and Source

A sample application using Streamline may be found in [this git repo](https://github.com/NVIDIAGameWorks/Streamline_Sample)
