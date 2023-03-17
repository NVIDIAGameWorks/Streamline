/*
* Copyright (c) 2022 NVIDIA CORPORATION. All rights reserved
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <sys/stat.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <inttypes.h>
#include <fstream>
#include <filesystem>
#include <random>

#include "source/core/sl.extra/extra.h"

#ifdef SL_WINDOWS
#include <KnownFolders.h>
#include <ShlObj.h>
EXTERN_C IMAGE_DOS_HEADER __ImageBase; // MS linker feature
#else
#include <linux/limits.h>
#endif

namespace fs = std::filesystem;

namespace sl
{
namespace file
{

inline bool exists(const wchar_t* src)
{
    return fs::exists(src);
}

inline bool copy(const wchar_t* dst, const wchar_t* src)
{
    return fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
}

inline void write(const wchar_t* fname, const std::vector<uint8_t>& data)
{
    fs::path p(fname);
#ifdef SL_WINDOWS
    std::fstream file(fname, std::ios::binary | std::ios::out);
#else
    std::fstream file(extra::utf16ToUtf8(fname).c_str(), std::ios::binary | std::ios::out);
#endif
    file.write((char*)data.data(), data.size());
}

inline FILE* open(const wchar_t* path, const wchar_t* mode)
{
#if SL_WINDOWS    
    FILE* file = _wfsopen(path, mode, _SH_DENYNO);
#else
    FILE* file = fopen(extra::utf16ToUtf8(path).c_str(), extra::utf16ToUtf8(mode).c_str());
#endif
    if (!file)
    {
        if (errno != ENOENT)
        {
            SL_LOG_ERROR( "Unable to open file %S - error = %d", path, errno);
        }
        else
        {
            SL_LOG_ERROR( "File '%S' does not exist", path);
        }
    }
    return file;
}

inline void flush(FILE* file)
{
    fflush(file);
}

inline void close(FILE* file)
{
    fclose(file);
}

//! Attempt to read data of specified size from file.
//! Returns number of bytes read.
//! 
//! IMPORTANT: strings (char*/wchar_t*) won't be null-terminated unless the file contents
//! contain it and chunkSize includes it.  Make sure to null-terminate strings where
//! required.
inline size_t readChunk(FILE* file, void* chunk, size_t chunkSize)
{
    return fread(chunk, 1, chunkSize, file);
}

inline size_t writeChunk(FILE* file, const void* chunk, const size_t chunkSize)
{
    return fwrite(chunk, 1, chunkSize, file);
}

inline char* readLine(FILE* file, char* line, size_t maxLineSize)
{
    char* stringRead = fgets(line, int(maxLineSize), file);
    if (stringRead)
    {
        // Remove line endings (Linux and Windows)
        auto LF = '\n';
        auto CR = '\r';
        size_t end = strlen(stringRead) - 1;
        if (stringRead[end] == LF)
        {
            stringRead[end] = '\0';
            if (end > 0 && stringRead[end - 1] == CR)
            {
                stringRead[end - 1] = '\0';
            }
        }
    }
    return stringRead;
}

inline bool writeLine(FILE* file, const char* line)
{
    size_t lineLen = strlen(line);
    size_t written = fwrite(line, 1, lineLen, file);
    if (written != lineLen)
    {
        return false;
    }
    auto LF = '\n';
    int ret = fputc(LF, file);
    return ret == LF;
}

inline std::vector<uint8_t> read(const wchar_t* fname)
{
    fs::path p(fname);
    size_t file_size = fs::file_size(p);
    std::vector<uint8_t> ret_buffer(file_size);
#ifdef SL_LINUX
    std::fstream file(extra::toStr(fname), std::ios::binary | std::ios::in);
#else
    std::fstream file(fname, std::ios::binary | std::ios::in);
#endif
    file.read((char*)ret_buffer.data(), file_size);
    return ret_buffer;
}

inline const wchar_t* getTmpPath()
{
    static std::wstring g_result;
    g_result = fs::temp_directory_path().wstring();
    return g_result.c_str();
}

// Required when using symlinks
inline std::string getRealPath(const char* filename)
{
#ifdef SL_WINDOWS
    auto file = CreateFile(filename,   // file to open
        GENERIC_READ,          // open for reading
        FILE_SHARE_READ,       // share for reading
        NULL,                  // default security
        OPEN_EXISTING,         // existing file only
        FILE_ATTRIBUTE_NORMAL, // normal file
        NULL);                 // no attr. template  
    char buffer[MAX_PATH] = {};
    GetFinalPathNameByHandleA(file, buffer, MAX_PATH, FILE_NAME_OPENED);
    CloseHandle(file);
#else
    char buffer[PATH_MAX] = {};
    ::realpath(filename, buffer);
#endif
    return std::string(buffer);
}


inline time_t getModTime(const std::string& pathAbs)
{
    std::string path = getRealPath(pathAbs.c_str());
    auto t = fs::last_write_time(path);
    return t.time_since_epoch().count();
}

inline const wchar_t* getCurrentDirectoryPath()
{
    static std::wstring s_path;
    s_path = fs::current_path().wstring();
    return s_path.c_str();
}

inline bool setCurrentDirectoryPath(const wchar_t* path)
{
    std::error_code ec;
    fs::current_path(path, ec);
    return !ec;
}

inline std::string removeExtension(const std::string& filename)
{
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

inline bool remove(const wchar_t* path)
{
    bool success = false;
#if SL_WINDOWS    
    SHFILEOPSTRUCTW fileOperation;
    fileOperation.wFunc = FO_DELETE;
    fileOperation.pFrom = path;
    fileOperation.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;

    int result = ::SHFileOperationW(&fileOperation);
    if (result != 0)
    {
        SL_LOG_ERROR( "Failed to delete file '%S' (error code: %d)", path, result);
    }
    else
    {
        success = true;
    }

#else
    success = remove(path) == 0;
#endif

    return success;
}

inline bool move(const wchar_t* from, const wchar_t* to)
{
#if SL_WINDOWS
    if (!MoveFileW(from, to))
    {
        SL_LOG_ERROR( "File move failed: '%S' -> '%S' (error code %" PRIu32 ")", from, to, GetLastError());
        return false;
    }
    return true;
#else
    if (rename(extra::toStr(from).c_str(), extra::toStr(to).c_str()) < 0)
    {
        SL_LOG_ERROR("File move failed: '%S' -> '%S' (%s)", from, to, strerror(errno));
        return false;
    }
    return true;
#endif
}

inline bool createDirectoryRecursively(const wchar_t* path)
{
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec)
    {
        SL_LOG_ERROR( "createDirectoryRecursively failed with %s", ec.message().c_str());
        return false;
    }
    return true;
}

inline std::wstring getModulePath()
{
    std::wstring res;
#ifdef SL_WINDOWS
    wchar_t modulePath[MAX_PATH] = { 0 };
    GetModuleFileNameW((HINSTANCE)&__ImageBase, modulePath, MAX_PATH);
    fs::path dllPath(modulePath);
    dllPath.remove_filename();
    res = dllPath.c_str();
#endif
    return res;
}

inline std::wstring getExecutablePath()
{
#ifdef SL_LINUX
    char exePath[PATH_MAX] = {};
    readlink("/proc/self/exe", exePath, sizeof(exePath));
    return extra::toWStr(exePath);
#else
    WCHAR pathAbsW[MAX_PATH] = {};
    GetModuleFileNameW(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::wstring searchPathW = pathAbsW;
    searchPathW.erase(searchPathW.rfind('\\'));
    return searchPathW + L"\\";
#endif
}

inline std::wstring getExecutableName()
{
#ifdef SL_LINUX
    char exePath[PATH_MAX] = {};
    readlink("/proc/self/exe", exePath, sizeof(exePath));
    return extra::toWStr(exePath);
#else
    WCHAR pathAbsW[MAX_PATH] = {};
    GetModuleFileNameW(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::wstring searchPathW = pathAbsW;
    searchPathW = searchPathW.substr(searchPathW.rfind('\\') + 1);
    searchPathW.erase(searchPathW.rfind('.'));
    return searchPathW;
#endif
}

inline bool isRelativePath(const std::wstring& path)
{
    return fs::path(path).is_relative();
}

class scoped_dir_change
{
public:
    scoped_dir_change(const wchar_t* newCurrentDir)
    {
        dir = getCurrentDirectoryPath();
        setCurrentDirectoryPath(newCurrentDir);
    }

    ~scoped_dir_change()
    {
        setCurrentDirectoryPath(dir.c_str());
    }

private:
    std::wstring dir;
};

}
}
