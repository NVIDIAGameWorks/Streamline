@echo off
set cfg=vs2017
:loop
IF NOT "%1"=="" (
    IF "%1"=="vs2019" (
        SET cfg=vs2019
    )
    SHIFT
    GOTO :loop
)
echo Creating project files for %cfg%
call .\tools\packman\packman.cmd pull -p windows-x86_64 project.xml
call .\tools\premake5\premake5.exe %cfg% --file=.\premake.lua 