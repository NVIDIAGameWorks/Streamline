@echo off
setlocal enabledelayedexpansion

git log -1 --pretty=format:"SHA:%%x20%%h" > tmpFile 
set /p cl=<tmpFile
del tmpFile
git log -1 --pretty=format:"%%h" > tmpFile 
set /p cls=<tmpFile
del tmpFile
del gitVersion.h /f
echo #ifndef _GIT_VERSION_HEADER >> gitVersion.h
echo #define _GIT_VERSION_HEADER >> gitVersion.h
echo //! Auto generated - start >> gitVersion.h
echo #define GIT_LAST_COMMIT "%cl%" >> gitVersion.h
echo #define GIT_LAST_COMMIT_SHORT "%cls%" >> gitVersion.h
echo //! Auto generated - end >> gitVersion.h
echo #endif >> gitVersion.h
