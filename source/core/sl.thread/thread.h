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

#include "source/core/sl.exception/exception.h"

using namespace std::chrono_literals;

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
        clear();
    }

    void clear()
    {
        for (auto& t : threads)
        {
            delete t;
        }
        for (auto& t : threadMap)
        {
            delete t.second;
        }
        threads.clear();
        threadMap.clear();
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

class WorkerThread
{
    std::mutex m_mtx;

    std::condition_variable m_cv; // work queue cv
    bool m_workAdded = false;

    std::condition_variable m_cvf; // flushing cv, no need for flag since we use a timeout for it

    std::atomic<bool> m_quit = false;
    std::atomic<bool> m_flush = false;

    size_t m_jobCount = 0;
    std::thread m_thread;
    std::vector<std::pair<bool, std::function<void(void)>>> m_work{};
    std::wstring m_name;

    void workerFunction()
    {
        while (!m_quit)
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            if (m_work.empty())
            {
                // Tell threads waiting on flush that we are done
                m_cvf.notify_all();

                // Check if there was work added while the work queue was empty. If added, don't wait. Otherwise, keep waiting until notify + work added
                m_cv.wait(lock, [this] { return m_workAdded; });
                m_workAdded = false;
            }
            else
            {
                auto [perpetual, func] = m_work.front();
                lock.unlock();
                // NOTE: No need to wrap this in the exception handler
                // since all internal workers are already executing within one.
                func();
                lock.lock();
                // Done, remove from the queue
                m_work.erase(m_work.begin());
                // Keep perpetual jobs until flush is requested
                if (!perpetual || m_flush.load())
                {
                    m_jobCount--;
                }
                else
                {
                    // Back to the queue to execute again but after other workloads (if any)
                    m_work.push_back({ perpetual, func });
                }
            }
        }
    }

public:
    WorkerThread(const WorkerThread&) = delete;

    WorkerThread(const wchar_t* name, int priority)
    {
        m_name = name;
        m_thread = std::thread(&WorkerThread::workerFunction, this);
        if (!SetThreadPriority(m_thread.native_handle(), priority))
        {
            SL_LOG_WARN("Failed to set thread priority to %d for thread '%S'", priority, name);
        }
        SetThreadDescription(m_thread.native_handle(), name);
    }

    ~WorkerThread()
    {
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            m_quit = true; // set to true so that worker thread can exit its loop
            m_workAdded = true; // set to true so that worker thread exit its wait after the notify call
        }
        m_cv.notify_all(); // wake up thread
        m_thread.join(); // block until thread exits
    }

    std::cv_status flush(uint32_t timeout = 500)
    {
        std::cv_status res = std::cv_status::no_timeout;

        // Atomic swap to true and check that it was false so we don't flush
        // multiple times from different threads.
        if (!m_flush.exchange(true))
        {
            std::unique_lock<std::mutex> lock(m_mtx);
            if (!m_work.empty())
            {
                // Wait and free the lock
                res = m_cvf.wait_for(lock, std::chrono::milliseconds(timeout));
                if (res == std::cv_status::timeout)
                {
                    SL_LOG_WARN("Worker thread '%S' timed out", m_name.c_str());
                }
            }
            m_flush = false;
        }
        return res;
    }

    size_t getJobCount()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        return m_jobCount;
    }

    bool scheduleWork(const std::function<void(void)>& func, bool perpetual = false)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_work.push_back({ perpetual, func });
        m_workAdded = true;
        m_jobCount++;

        m_cv.notify_one();

        return true;
    }
};

struct scoped_lock
{
    scoped_lock(CRITICAL_SECTION& criticalSection)
    {
        m_criticalSection = &criticalSection;
        EnterCriticalSection(m_criticalSection);
    }
    ~scoped_lock()
    {
        LeaveCriticalSection(m_criticalSection);
    }
    CRITICAL_SECTION* m_criticalSection;
};

struct LockAtomic
{
    LockAtomic() {};
    LockAtomic(std::atomic<uint32_t>* l1, std::atomic<uint32_t>* l2)
    {
        m_l1 = l1;
        m_l2 = l2;
    }

    void lock()
    {
        while (true)
        {
            m_l1->store(1, std::memory_order_seq_cst);
            if (m_l2->load(std::memory_order_seq_cst) != 0)
            {
                m_l1->store(0, std::memory_order_seq_cst);
                continue;
            }
            break;
        }
    }

    void unlock()
    {
        m_l1->store(0, std::memory_order_seq_cst);
    }

    std::atomic<uint32_t>* m_l1{};
    std::atomic<uint32_t>* m_l2{};
};

struct ScopedLockAtomic
{
    ScopedLockAtomic(std::atomic<uint32_t>* l1, std::atomic<uint32_t>* l2)
    {
        m_mutex = LockAtomic(l1, l2);
        m_mutex.lock();
    }
    ~ScopedLockAtomic()
    {
        m_mutex.unlock();
    }
    LockAtomic m_mutex;
};

}
}
