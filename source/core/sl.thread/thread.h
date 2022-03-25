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

#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <map>

namespace sl
{
namespace thread
{

template<typename T>
struct ThreadContext
{
    ThreadContext() 
    {
        // Space for 64K thread ids.
        // This does not waste too much memory
        threads.resize(65536);
    };

    ~ThreadContext()
    {
        for (auto &t : threads)
        {
            delete t;
        }
        for (auto &t : threadMap)
        {
            delete t.second;
        }
    }

    T &getContext()
    {
        // Accessing thread context via thread id
        // This is super fast since there are no sync points
        // but can result in huge memory consumption if
        // OS assigns some really big id (it is 32bit value)
        // As long as thread ids are 16bit we are good.
        // If we hit a title where thread ids are crazy high we 
        // need to switch to classic mode with mutex lock.
        auto id = GetCurrentThreadId();
        if (!useThreadMap && id > 65536)
        {
            useThreadMap = true; 
            SL_LOG_WARN("Thread id over 65536 detected, switching to thread map");
        }
        // Atomic check
        if (useThreadMap)
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = threadMap.find(id);
            if (it == threadMap.end())
            {
                T* context = new T;
                // If we switched to this later in the game copy our context if any
                if (threads.size() > (size_t)id && threads[id])
                {
                    *context = *threads[id];
                }
                threadMap[id] = context;
            }
            return *threadMap[id];
        }

        // Each thread has different id so no need to sync here
        if (!threads[id])
        {
            threads[id] = new T();
            threadCount++;
            SL_LOG_HINT("detected new thread %u - total threads %u", id, threadCount.load());
        }
        return *threads[id];
    }

protected:

    std::atomic<bool> useThreadMap = false;
    std::mutex mutex = {};
    std::vector<T*> threads = {};
    std::map<DWORD, T*> threadMap = {};
    std::atomic<uint32_t> threadCount = {};
};

}
}