@echo off
set cfg=
:loop
IF NOT "%1"=="" (
    IF "%1"=="vs2017" (
        SET cfg=vs2017
    )
    IF "%1"=="vs2019" (
        SET cfg=vs2019
    )
    IF "%1"=="vs2022" (
        SET cfg=vs2022
    )
    SHIFT
    GOTO :loop
)

IF "%cfg%"=="" (
    IF exist .\_project\vs2022\streamline.sln (
        SET cfg=vs2022
    ) ELSE IF exist .\_project\vs2019\streamline.sln (
        SET cfg=vs2019
    ) ELSE (
        SET cfg=vs2017
    )
)

echo Creating project files for %cfg%
call .\tools\packman\packman.cmd pull -p windows-x86_64 project.xml
call .\tools\premake5\premake5.exe %cfg% --file=.\premake.lua %*
