SETLOCAL EnableDelayedExpansion

echo off

set cfg=Production
set src=%~dp0
set dest=_sdk

:loop
IF NOT "%1"=="" (
    IF "%1"=="-debug" (
        SET cfg=Debug
    )
    IF "%1"=="-release" (
        SET cfg=Release
    )
    IF "%1"=="-profiling" (
        SET cfg=Profiling
    )
    IF "%1"=="-relextdev" (
        SET cfg=RelExtDev
    )
    IF "%1"=="-dir" (
        SET dest=%2
        SHIFT
    )
    SHIFT
    GOTO :loop
)

mkdir %dest%\include
mkdir %dest%\lib\x64
mkdir %dest%\bin\x64
mkdir %dest%\doc
mkdir %dest%\symbols
mkdir %dest%\scripts

copy %src%\docs\ProgrammingGuide*.md %dest%\doc
copy %src%\docs\RTX*.* %dest%\doc
copy %src%\docs\Streamline*.pdf %dest%\doc

copy %src%\include\sl.h %dest%\include
copy %src%\include\sl_*.h %dest%\include

copy _artifacts\sl.interposer\%cfg%_x64\sl.interposer.lib %dest%\lib\x64\ /Y

del _sdk\bin\x64\*.* /F /Q

copy bin\x64\nvngx_dlss.dll %dest%\bin\x64 /Y
copy .\README.md %dest%\bin\x64 /Y

IF "%cfg%"=="Debug" (
    copy external\nrd\Lib\Debug\*.dll %dest%\bin\x64 /Y
    copy external\nrd\Lib\Debug\*.pdb %dest%\bin\x64 /Y
) ELSE (
    copy external\nrd\Lib\Release\*.dll %dest%\bin\x64 /Y
)

IF "%cfg%"=="Profiling" (
    copy external\pix\bin\WinPixEventRuntime.dll %dest%\bin\x64 /Y
)

IF  NOT "%cfg%"=="Production" (
    copy scripts\sl.*.json %dest%\bin\x64 /Y
)

IF "%cfg%"=="Production" (
    copy bin\x64\nvngx_dlssg.dll %dest%\bin\x64 /Y
    copy bin\x64\sl.dlss_g.dll %dest%\bin\x64 /Y
) ELSE (
    copy bin\x64\development\nvngx_dlssg.dll %dest%\bin\x64 /Y
    copy bin\x64\development\sl.dlss_g.dll %dest%\bin\x64 /Y
)

copy scripts\ngx_driver_*.reg %dest%\scripts /Y

copy external\reflex-sdk-vk\lib\NvLowLatencyVk.dll %dest%\bin\x64 /Y

copy _artifacts\sl.common\%cfg%_x64\sl.common.dll %dest%\bin\x64 /Y
copy _artifacts\sl.interposer\%cfg%_x64\sl.interposer.dll %dest%\bin\x64 /Y
copy _artifacts\sl.nrd\%cfg%_x64\sl.nrd.dll %dest%\bin\x64 /Y
copy _artifacts\sl.nis\%cfg%_x64\sl.nis.dll %dest%\bin\x64 /Y
copy _artifacts\sl.dlss\%cfg%_x64\sl.dlss.dll %dest%\bin\x64 /Y
copy _artifacts\sl.reflex\%cfg%_x64\sl.reflex.dll %dest%\bin\x64 /Y
IF  NOT "%cfg%"=="Production" (
    copy _artifacts\sl.imgui\%cfg%_x64\sl.imgui.dll %dest%\bin\x64 /Y
)

copy _artifacts\sl.common\%cfg%_x64\sl.common.pdb %dest%\symbols /Y
copy _artifacts\sl.interposer\%cfg%_x64\sl.interposer.pdb %dest%\symbols /Y
copy _artifacts\sl.nrd\%cfg%_x64\sl.nrd.pdb %dest%\symbols /Y
copy _artifacts\sl.nis\%cfg%_x64\sl.nis.pdb %dest%\symbols /Y
copy _artifacts\sl.dlss\%cfg%_x64\sl.dlss.pdb %dest%\symbols /Y
copy _artifacts\sl.reflex\%cfg%_x64\sl.reflex.pdb %dest%\symbols /Y
IF  NOT "%cfg%"=="Production" (
    copy _artifacts\sl.imgui\%cfg%_x64\sl.imgui.pdb %dest%\symbols /Y
)

echo Configuration:%cfg%
echo Destination:%dest%
