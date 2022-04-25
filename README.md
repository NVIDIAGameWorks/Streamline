# Streamline (SL) - Version 1.0.2
This is where all the components of SL are being developed.

## Prerequisites
#### Hardware
- GPU supporting DirectX 11

> **IMPORTANT:**
> Current version of SDK does not support Vulkan properly, this is still work in progress
#### Windows
- Install latest driver
- Install VS Code (recommended) or VS2017 with [SDK 10.17763+](https://go.microsoft.com/fwlink/?LinkID=2023014)
- Install "git".
- Install Python 3
- Clone your fork to a local hard drive, make sure to use a NTFS drive on Windows (SL uses symbolic links)
- Execute `./setup.bat` with `{vs2017|vs2019}` (`vs2017` is default)

The make/solution files will be found in the generated `_project` directory. 

## Building
- Open Visual Studio command prompt
- Run `msbuild _project\{vs2017|vs2019}\streamline.sln /t:Clean,Build /property:Configuration=Production`
> **IMPORTANT:**
> Only use `production` builds when releasing your software otherwise SL plugins could be replaced with potentially malicious modules.

The build output can be found in the generated `_artifacts` folder 

The default setting is to target x86_64 CPU architecture.

> NOTE: To build the project minimal configuration is needed. Any version of Windows 10 will do. Then
run the setup and build scripts as described here above. That's it. The specific version of Windows, NVIDIA driver,
or Vulkan are all runtime dependencies, not compile/link time dependencies. This allows SL to build on stock
virtual machines that require zero configuration. This is a beautiful thing, help us keep it that way.

## Programming Guide
Please read [ProgrammingGuide.md](docs/ProgrammingGuide.md) to learn about the integration in games.
## Programming Guide - DLSS
Please read [ProgrammingGuideDLSS.md](docs/ProgrammingGuideDLSS.md) to learn about DLSS specific integration in games.
## Programming Guide - NRD
Please read [ProgrammingGuideNRD.md](docs/ProgrammingGuideNRD.md) to learn about NRD specific integration in games.

## Sample Plugin Source Code
A sample Streamline plugin source code is located [here](source/plugins/sl.template/templateEntry.cpp)
## Sample App and Source
A sample application using Streamline may be found in [this git repo](https://github.com/NVIDIAGameWorks/Streamline_Sample)
