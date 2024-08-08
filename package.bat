:: package.bat
:: Collages Streamline components into a package for distribution and integration.
:: Usage: package.bat [args...]
::   Args:
::      -debug                Package using Debug binaries
::      -develop              Package using Develop binaries
::      -production           Package using Production binaries (also includes Develop binaries in the development/ folder)
::      -source               Include source in the final package
::      -dir [output_path]    Creates the package in output_path. Defaults to .\_sdk

@SETLOCAL EnableDelayedExpansion

@ECHO off

set cfg=UNKNOWN
set cfg_alt=None
set src=%~dp0
set dest=%~dp0\_sdk
set include_source=False
:: When packaging with source, the features dir is redundant with bin, but better reflects the original repo
set create_features_dir=False

:argloop
IF NOT "%1"=="" (
    IF "%1"=="-debug" (
        set cfg=Debug
    )
    IF "%1"=="-develop" (
        set cfg=Develop
    )
    IF "%1"=="-production" (
        set cfg=Production
        set cfg_alt=Develop
    )
    :: Deprecated build configuration names
    IF "%1"=="-release" (
        set cfg=Debug
    )
    IF "%1"=="-profiling" (
        set cfg=Develop
    )
    IF "%1"=="-relextdev" (
        set cfg=Develop
    )

    IF "%1"=="-source" (
        set include_source=True
    )
    IF "%1"=="-dir" (
        set dest=%~f2
        shift
    )
    IF "%1"=="-root" (
        set src=%~f2
        shift
    )
    shift
    goto :argloop
)

IF "%cfg%"=="UNKNOWN" (
    echo "Could not determine build type"
    echo "Use one of -debug, -develop, or -production"
    exit /b 1
)


IF EXIST %dest%\ rmdir %dest% /S /Q
mkdir %dest%

:: BINARIES AND SYMBOLS
:: copybin [copy_cfg] [copy_dest] [sym_dest]
:: Copies binaries for copy_cfg to copy_dest, and symbols to symbol_dest
:: Includes json and licenses where relevant
goto endcopybin
:copybin

set copy_cfg=%~1
set copy_dest=%~2
set sym_dest=%~3

IF EXIST %src%/features (
    set features_src=%src%\features
) ELSE (
    set features_src=%src%\bin\x64
)

:: Common Plugins
copy %src%\_artifacts\sl.common\%copy_cfg%_x64\sl.common.dll %copy_dest% /Y
copy %src%\_artifacts\sl.common\%copy_cfg%_x64\sl.common.pdb %sym_dest% /Y
copy %src%\_artifacts\sl.interposer\%copy_cfg%_x64\sl.interposer.dll %copy_dest% /Y
copy %src%\_artifacts\sl.interposer\%copy_cfg%_x64\sl.interposer.pdb %sym_dest% /Y
IF NOT "%copy_cfg%"=="Production" (
    copy %src%\_artifacts\sl.imgui\%copy_cfg%_x64\sl.imgui.dll %copy_dest% /Y
    copy %src%\_artifacts\sl.imgui\%copy_cfg%_x64\sl.imgui.pdb %sym_dest% /Y
)

:: DLSS Super Resolution
copy %src%\_artifacts\sl.dlss\%copy_cfg%_x64\sl.dlss.dll %copy_dest% /Y
copy %src%\_artifacts\sl.dlss\%copy_cfg%_x64\sl.dlss.pdb %sym_dest% /Y

copy %features_src%\nvngx_dlss.license.txt %copy_dest% /Y

IF "%copy_cfg%"=="Production" (
    copy %features_src%\nvngx_dlss.dll %copy_dest% /Y
) ELSE (
    copy %features_src%\development\nvngx_dlss.dll %copy_dest% /Y
)

:: DLSS Frame Generation
IF "%copy_cfg%"=="Production" (
    copy %features_src%\sl.dlss_g.dll %copy_dest% /Y
    copy %src%\_artifacts\sl.dlss_g\%copy_cfg%_x64\sl.dlss_g.dll %copy_dest% /Y
) ELSE (
    copy %features_src%\development\sl.dlss_g.dll %copy_dest% /Y
    copy %src%\_artifacts\sl.dlss_g\Develop_x64\sl.dlss_g.dll %copy_dest% /Y
)

IF "%copy_cfg%"=="Production" (
    copy %features_src%\nvngx_dlssg.dll %copy_dest% /Y
) ELSE (
    copy %features_src%\development\nvngx_dlssg.dll %copy_dest% /Y
)


:: DeepDVC
copy %src%\_artifacts\sl.deepdvc\%copy_cfg%_x64\sl.deepdvc.dll %copy_dest% /Y
copy %src%\_artifacts\sl.deepdvc\%copy_cfg%_x64\sl.deepdvc.pdb %sym_dest% /Y

IF "%copy_cfg%"=="Production" (
    copy %features_src%\nvngx_deepdvc.dll %copy_dest% /Y
) ELSE (
    copy %features_src%\development\nvngx_deepdvc.dll %copy_dest% /Y
)


:: NIS
copy %src%\_artifacts\sl.nis\%copy_cfg%_x64\sl.nis.dll %copy_dest% /Y
copy %src%\_artifacts\sl.nis\%copy_cfg%_x64\sl.nis.pdb %sym_dest% /Y

copy %features_src%\nis.license.txt %copy_dest% /Y

:: NRD
copy %src%\_artifacts\sl.nrd\%copy_cfg%_x64\sl.nrd.dll %copy_dest% /Y
copy %src%\_artifacts\sl.nrd\%copy_cfg%_x64\sl.nrd.pdb %sym_dest% /Y

copy %features_src%\NRD.license.txt %copy_dest% /Y

IF "%copy_cfg%"=="Debug" (
    copy %src%\external\nrd\Lib\Debug\*.dll %copy_dest% /Y
    copy %src%\external\nrd\Lib\Debug\*.pdb %sym_dest% /Y
) ELSE (
    copy %src%\external\nrd\Lib\Release\*.dll %copy_dest% /Y
)

:: NvPerf
copy %src%\_artifacts\sl.nvperf\%copy_cfg%_x64\sl.nvperf.dll %copy_dest% /Y
copy %src%\_artifacts\sl.nvperf\%copy_cfg%_x64\sl.nvperf.pdb %sym_dest% /Y

:: PCL
copy %src%\_artifacts\sl.pcl\%copy_cfg%_x64\sl.pcl.dll %copy_dest% /Y
copy %src%\_artifacts\sl.pcl\%copy_cfg%_x64\sl.pcl.pdb %sym_dest% /Y

:: Reflex
copy %src%\_artifacts\sl.reflex\%copy_cfg%_x64\sl.reflex.dll %copy_dest% /Y
copy %src%\_artifacts\sl.reflex\%copy_cfg%_x64\sl.reflex.pdb %sym_dest% /Y

copy %features_src%\reflex.license.txt %copy_dest% /Y

copy %src%\external\reflex-sdk-vk\lib\NvLowLatencyVk.dll %copy_dest% /Y

exit /b 0
:endcopybin

mkdir %dest%\lib\x64
mkdir %dest%\bin\x64
mkdir %dest%\symbols


IF "%cfg_alt%"=="None" (
    call:copybin %cfg% %dest%\bin\x64 %dest%\symbols
) ELSE (
    mkdir %dest%\bin\x64\development

    call:copybin %cfg% %dest%\bin\x64 NUL
    call:copybin %cfg_alt% %dest%\bin\x64\development %dest%\symbols
)

:: Interposer lib
copy %src%\_artifacts\sl.interposer\%cfg%_x64\sl.interposer.lib %dest%\lib\x64\ /Y


:: INCLUDES
mkdir %dest%\include

copy %src%\include\sl.h                %dest%\include
copy %src%\include\sl_consts.h         %dest%\include
copy %src%\include\sl_helpers.h        %dest%\include
copy %src%\include\sl_helpers_vk.h     %dest%\include
copy %src%\include\sl_hooks.h          %dest%\include
copy %src%\include\sl_matrix_helpers.h %dest%\include
copy %src%\include\sl_result.h         %dest%\include
copy %src%\include\sl_security.h       %dest%\include
copy %src%\include\sl_struct.h         %dest%\include
copy %src%\include\sl_version.h        %dest%\include

copy %src%\include\sl_deepdvc.h        %dest%\include
copy %src%\include\sl_dlss.h           %dest%\include
copy %src%\include\sl_dlss_g.h         %dest%\include
copy %src%\include\sl_nis.h            %dest%\include
copy %src%\include\sl_nrd.h            %dest%\include
copy %src%\include\sl_nvperf.h         %dest%\include
copy %src%\include\sl_pcl.h            %dest%\include
copy %src%\include\sl_reflex.h         %dest%\include
copy %src%\include\sl_template.h       %dest%\include


:: SCRIPTS
mkdir %dest%\scripts

copy %src%\scripts\sl.common.json                       %dest%\scripts
copy %src%\scripts\sl.interposer.json                   %dest%\scripts
copy %src%\scripts\sl.reflex.json                       %dest%\scripts
copy %src%\scripts\sl.imgui.json                        %dest%\scripts
copy %src%\scripts\ngx_driver_onscreenindicator.reg     %dest%\scripts
copy %src%\scripts\ngx_driver_onscreenindicator_off.reg %dest%\scripts

copy %src%\scripts\sl.cmake %dest%\CMakeLists.txt


:: UTILITIES
mkdir %dest%\utils
mkdir %dest%\utils\reflex

xcopy %src%\utils\reflex %dest%\utils\reflex /S

:: DOCUMENTATION
mkdir %dest%\docs
mkdir %dest%\docs\media

copy %src%\docs\ProgrammingGuide.md              %dest%\docs
copy %src%\docs\ProgrammingGuideDeepDVC.md       %dest%\docs
copy %src%\docs\ProgrammingGuideDLSS.md          %dest%\docs
copy %src%\docs\ProgrammingGuideDLSS_G.md        %dest%\docs
copy %src%\docs\ProgrammingGuideManualHooking.md %dest%\docs
copy %src%\docs\ProgrammingGuideNIS.md           %dest%\docs
copy %src%\docs\ProgrammingGuideNRD.md           %dest%\docs
copy %src%\docs\ProgrammingGuidePCL.md           %dest%\docs
copy %src%\docs\ProgrammingGuideReflex.md        %dest%\docs

copy %src%\docs\APIChangesAndImprovements.md     %dest%\docs

copy "%src%\docs\RTX Developer Localization Strings.zip"             %dest%\docs
copy "%src%\docs\RTX UI Developer Guidelines Chinese Version.pdf"    %dest%\docs
copy "%src%\docs\RTX UI Developer Guidelines.pdf"                    %dest%\docs
copy "%src%\docs\Debugging - JSON Configs (Plugin Configs).md"       %dest%\docs
copy "%src%\docs\Debugging - NRD.md"                                 %dest%\docs
copy "%src%\docs\Debugging - NvPerf GUI.md"                          %dest%\docs
copy "%src%\docs\Debugging - SL ImGUI (Realtime Data Inspection).md" %dest%\docs

copy %src%\docs\Streamline*.pdf %dest%\docs
copy %src%\docs\media\*.* %dest%\docs\media


:: README AND LICENSES
copy %src%\README.md             %dest% /Y
copy %src%\license.txt           %dest% /Y
copy %src%\"NVIDIA Nsight Perf SDK License (28Sept2022).pdf" %dest% /Y
copy %src%\3rd-party-licenses.md %dest% /Y
copy %src%\release.txt           %dest% /Y


:: SOURCE
IF "%include_source%"=="True" (
    mkdir %dest%\shaders
    mkdir %dest%\source
    mkdir %dest%\source\core
    mkdir %dest%\source\platforms
    mkdir %dest%\source\shared
    mkdir %dest%\source\plugins
    mkdir %dest%\tools
    mkdir %dest%\tools\packman
    mkdir %dest%\_artifacts
    mkdir %dest%\_artifacts\shaders

    :: Tools
    copy %src%\tools\build_shader.bat %dest%\tools
    copy %src%\tools\gitVersion.bat   %dest%\tools
    copy %src%\tools\vswhere.exe      %dest%\tools
    copy %src%\tools\bin2cheader.ps1  %dest%\tools
    xcopy %src%\tools\packman         %dest%\tools\packman /S

    :: Additional scripts
    copy %src%\scripts\_manifest.lua %dest%\scripts
    copy %src%\scripts\_preload.lua  %dest%\scripts
    copy %src%\scripts\sl.cmake      %dest%\scripts

    :: Build scripts
    copy %src%\premake.lua   %dest%
    copy %src%\build.bat     %dest%
    copy %src%\build_all.bat %dest%
    copy %src%\setup.bat     %dest%
    copy %src%\project.xml   %dest%
    copy %src%\package.bat   %dest%

    :: Common Source
    xcopy %src%\source\core      %dest%\source\core /S
    xcopy %src%\source\platforms %dest%\source\platforms /S
    xcopy %src%\source\shared    %dest%\source\shared /S

    :: Plugins
    mkdir %dest%\source\plugins\sl.common
    xcopy %src%\source\plugins\sl.common   %dest%\source\plugins\sl.common   /S
    mkdir %dest%\source\plugins\sl.deepdvc
    xcopy %src%\source\plugins\sl.deepdvc  %dest%\source\plugins\sl.deepdvc  /S
    mkdir %dest%\source\plugins\sl.dlss
    xcopy %src%\source\plugins\sl.dlss     %dest%\source\plugins\sl.dlss     /S
    mkdir %dest%\source\plugins\sl.imgui
    xcopy %src%\source\plugins\sl.imgui    %dest%\source\plugins\sl.imgui    /S
    mkdir %dest%\source\plugins\sl.nis
    xcopy %src%\source\plugins\sl.nis      %dest%\source\plugins\sl.nis      /S
    mkdir %dest%\source\plugins\sl.nrd
    xcopy %src%\source\plugins\sl.nrd      %dest%\source\plugins\sl.nrd      /S
    mkdir %dest%\source\plugins\sl.pcl
    xcopy %src%\source\plugins\sl.pcl      %dest%\source\plugins\sl.pcl      /S
    mkdir %dest%\source\plugins\sl.reflex
    xcopy %src%\source\plugins\sl.reflex   %dest%\source\plugins\sl.reflex   /S
    mkdir %dest%\source\plugins\sl.template
    xcopy %src%\source\plugins\sl.template %dest%\source\plugins\sl.template /S

    :: External Dependencies
    mkdir %dest%\external
    mkdir %dest%\external\json
    mkdir %dest%\external\json\include
    mkdir %dest%\external\ngx-sdk
    mkdir %dest%\external\ngx-sdk\include
    mkdir %dest%\external\ngx-sdk\lib

    xcopy %src%\external\json\include                          %dest%\external\json\include /S
    copy %src%\external\json\LICENSE.MIT                       %dest%\external\json
    copy %src%\external\json\nlohmann_json.natvis              %dest%\external\json
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_defs.h       %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_helpers.h    %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_helpers_vk.h %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_params.h     %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx.h            %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_vk.h         %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_defs_deepdvc.h       %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_helpers_deepdvc.h    %dest%\external\ngx-sdk\include
    copy %src%\external\ngx-sdk\include\nvsdk_ngx_helpers_deepdvc_vk.h %dest%\external\ngx-sdk\include
    xcopy %src%\external\ngx-sdk\lib                           %dest%\external\ngx-sdk\lib /S

    :: Shader Source
    copy %src%\shaders\copy.hlsl                       %dest%\shaders
    copy %src%\shaders\copy_to_buffer.hlsl             %dest%\shaders
    copy %src%\shaders\copy_to_buffer_cs.h             %dest%\shaders
    copy %src%\shaders\copy_to_buffer_spv.h            %dest%\shaders
    copy %src%\shaders\mvec.hlsl                       %dest%\shaders
    copy %src%\shaders\nrd_pack.hlsl                   %dest%\shaders
    copy %src%\shaders\nrd_prep.hlsl                   %dest%\shaders
    copy %src%\shaders\vulkan_clear_image_view.comp    %dest%\shaders
    copy %src%\shaders\vulkan_clear_image_view_spirv.h %dest%\shaders

    :: Compiled Shaders
    copy %src%\_artifacts\shaders\copy_cs.h            %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\copy_spv.h           %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\copy_to_buffer_cs.h  %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\copy_to_buffer_spv.h %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\mvec_cs.h            %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\mvec_spv.h           %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\nrd_pack_cs.h        %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\nrd_pack_spv.h       %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\nrd_prep_cs.h        %dest%\_artifacts\shaders
    copy %src%\_artifacts\shaders\nrd_prep_spv.h       %dest%\_artifacts\shaders

    :: Feature DLLs
    IF "%create_features_dir%"=="True" (
        mkdir %dest%\features
        mkdir %dest%\features\development

        copy %src%\features\nvngx_dlss.dll %dest%\features
        copy %src%\features\development\nvngx_dlss.dll %dest%\features\development

        copy %src%\features\nvngx_dlssg.dll %dest%\features
        copy %src%\features\development\nvngx_dlssg.dll %dest%\features\development


        copy %src%\features\nvngx_deepdvc.dll %dest%\features
        copy %src%\features\development\nvngx_deepdvc.dll %dest%\features\development

        :: Feature Licenses
        copy %src%\features\nvngx_dlss.license.txt %dest%\features\nvngx_dlss.license.txt
        copy %src%\features\nis.license.txt %dest%\features\nis.license.txt
        copy %src%\features\NRD.license.txt %dest%\features\NRD.license.txt
        copy %src%\features\reflex.license.txt %dest%\features\reflex.license.txt
    )

    :: DLSS-G Plugin
    mkdir %dest%\_artifacts\sl.dlss_g
    mkdir %dest%\_artifacts\sl.dlss_g\Production_x64
    mkdir %dest%\_artifacts\sl.dlss_g\Develop_x64
    copy %src%\_artifacts\sl.dlss_g\Production_x64\sl.dlss_g.dll %dest%\_artifacts\sl.dlss_g\Production_x64
    copy %src%\_artifacts\sl.dlss_g\Develop_x64\sl.dlss_g.dll %dest%\_artifacts\sl.dlss_g\Develop_x64

)
