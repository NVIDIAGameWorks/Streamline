@echo off
setlocal

if exist .\_project\vs2019\streamline.sln (
    rem find VS 2019
    for /f "usebackq tokens=1* delims=: " %%i in (`.\tools\vswhere.exe -version [16^,17^) -requires Microsoft.VisualStudio.Workload.NativeDesktop`) do (
        if /i "%%i"=="installationPath" set VS_PATH=%%j
    )
) else (
    rem find VS 2017
    for /f "usebackq tokens=1* delims=: " %%i in (`.\tools\vswhere.exe -version [15^,16^) -requires Microsoft.VisualStudio.Workload.NativeDesktop`) do (
                if /i "%%i"=="installationPath" set VS_PATH=%%j
    )
)

echo off
set cfg=Debug
set bld=Clean,Build

:loop
IF NOT "%1"=="" (
    IF "%1"=="-debug" (
        SET cfg=Debug
        SHIFT
    )
    IF "%1"=="-release" (
        SET cfg=Release
        SHIFT
    )
    IF "%1"=="-production" (
        SET cfg=Production
        SHIFT
    )
    IF "%1"=="-profiling" (
        SET cfg=Profiling
        SHIFT
    )
    IF "%1"=="-relextdev" (
        SET cfg=RelExtDev
        SHIFT
    )
    SHIFT
    GOTO :loop
)

if not exist "%VS_PATH%" (
    echo "%VS_PATH%" not found. Is Visual Studio installed? && goto :ErrorExit
)

for /f "delims=" %%F in ('dir /b /s "%VS_PATH%\vsdevcmd.bat" 2^>nul') do set VSDEVCMD_PATH=%%F
echo ********Executing %VSDEVCMD_PATH%********
call "%VSDEVCMD_PATH%"
goto :SetVSEnvFinished

:ErrorExit
exit /b 1

:SetVSEnvFinished

if exist .\_project\vs2019\streamline.sln (
    msbuild .\_project\vs2019\streamline.sln /t:%bld% /property:Configuration=%cfg%
) else (
    msbuild .\_project\vs2017\streamline.sln /t:%bld% /property:Configuration=%cfg%
)
