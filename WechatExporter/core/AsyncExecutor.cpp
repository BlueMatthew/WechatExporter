//
//  AsyncExecutor.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/4/17.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "AsyncExecutor.h"
#include "Utils.h"

std::atomic_uint32_t AsyncExecutor::m_nextTaskId(1u);

uint32_t AsyncExecutor::genNextTaskId()
{
    return m_nextTaskId.fetch_add(1);
}

AsyncExecutor::Thread::Thread(AsyncExecutor *executor) : m_executor(executor),
      m_thread(new std::thread(&AsyncExecutor::Thread::ThreadFunc, this))
{
}

AsyncExecutor::Thread::~Thread()
{
    m_thread->join();
    m_thread.reset();
}

void AsyncExecutor::Thread::ThreadFunc()
{
#ifndef NDEBUG
    std::string tname = m_executor->m_tag + std::to_string(++m_executor->m_tid);
    setThreadName(tname.c_str());
#endif
    
    m_executor->ThreadFunc();
    std::unique_lock<std::mutex> lock(m_executor->m_mutex);
    m_executor->m_nthreads--;
    m_executor->m_dead_threads.push_back(this);
    if ((m_executor->m_shutdown) && (m_executor->m_nthreads == 0))
    {
        m_executor->m_shutdown_cv.notify_one();
    }
}

void AsyncExecutor::addTask(AsyncExecutor::Task* task)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Add works to the callbacks list
    m_tasks.push(task);

    // Increase pool size or notify as needed
    if (m_threads_waiting == 0 && m_nthreads < m_max_threads)
    {
        // Kick off a new thread
        m_nthreads++;
        new Thread(this);
    }
    else
    {
        m_cv.notify_one();
    }

    // Also use this chance to harvest dead threads
    if (!m_dead_threads.empty())
    {
        DestroyThreads(&m_dead_threads);
    }
}

AsyncExecutor::AsyncExecutor(int reserve_threads, int max_threads, Callback *callback) :
    m_callback(callback),
    m_shutdown(false),
    m_reserve_threads(reserve_threads),
    m_max_threads(max_threads),
    m_nthreads(0),
    m_threads_waiting(0)
{
#ifndef NDEBUG
    m_tid = 0;
#endif
    /*
    for (int i = 0; i < m_reserve_threads; i++)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_nthreads++;
        new Thread(this);
    }
     */
}

AsyncExecutor::~AsyncExecutor()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_shutdown = true;
    m_cv.notify_all();

    while (m_nthreads != 0)
    {
        m_shutdown_cv.wait(lock);
    }

    DestroyThreads(&m_dead_threads);
}

void AsyncExecutor::DestroyThreads(std::list<Thread*> *threads)
{
    for (auto it = threads->begin(); it != threads->end(); it = threads->erase(it))
    {
        delete *it;
    }
}

size_t AsyncExecutor::getNumberOfQueue() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t size = m_tasks.size();
    
    return size;
}

void AsyncExecutor::cancel()
{
    std::queue<Task *> tasks;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        tasks.swap(m_tasks);
    }
    
    while (!tasks.empty())
    {
        auto task = m_tasks.front();
        m_tasks.pop();
        delete task;
    }
}


void AsyncExecutor::ThreadFunc()
{
    for (;;)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        // Wait until work is available or we are shutting down.
        if (!m_shutdown && m_tasks.empty())
        {
            // If there are too many threads waiting, then quit this thread
            if (m_threads_waiting >= m_reserve_threads)
            {
                break;
            }

            m_threads_waiting++;
            m_cv.wait(lock);
            m_threads_waiting--;
        }
        
        // Drain callbacks before considering shutdown to ensure all work gets completed.
        if (!m_tasks.empty())
        {
            auto task = m_tasks.front();
            m_tasks.pop();
            lock.unlock();
            if (NULL != m_callback)
            {
                m_callback->onTaskStart(this, task);
            }
            bool succeeded = task->run();
            if (NULL != m_callback)
            {
                m_callback->onTaskComplete(this, task, succeeded);
            }
            delete task;
        }
        else if (m_shutdown)
        {
            break;
        }
    }
}
