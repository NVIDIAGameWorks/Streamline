@echo off
setlocal enabledelayedexpansion

git log -1 --date=short --pretty=format:"sha:%%h%%x20'%%s'%%x20author:%%an%%x20date:%%ad" > tmpFile 
set /p ver=<tmpFile
del tmpFile
git log -1 --pretty=format:"SHA:%%x20%%h" > tmpFile 
set /p cl=<tmpFile
del tmpFile
del gitVersion.h /f
echo #ifndef _GIT_VERSION_HEADER >> gitVersion.h
echo #define _GIT_VERSION_HEADER >> gitVersion.h
echo //! Auto generated - start >> gitVersion.h
echo constexpr const char* kGitVersionString = "%ver%"; >> gitVersion.h
echo #define GIT_VERSION_STRING "%ver%" >> gitVersion.h
echo #define GIT_LAST_COMMIT "%cl%" >> gitVersion.h
echo //! Auto generated - end >> gitVersion.h
echo #endif >> gitVersion.h