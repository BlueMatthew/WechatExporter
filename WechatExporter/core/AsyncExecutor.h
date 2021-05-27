//
//  AsyncExecutor.h
//  WechatExporter
//
//  Created by Matthew on 2021/4/17.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef AsyncExecutor_h
#define AsyncExecutor_h

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <list>

class AsyncExecutor
{
public:
    
    class Task
    {
    public:
        virtual bool run() = 0;
        virtual int getType() const = 0;
        virtual std::string getName() const
        {
            return "";
        }
        virtual bool hasError() const
        {
            return false;
        }
        virtual std::string getError() const
        {
            return "";
        }

        Task() : m_taskId(0u), m_userData(NULL)
        {
        }
        virtual ~Task() {}
        
        uint32_t getTaskId() const
        {
            return m_taskId;
        }
        
        void setTaskId(uint32_t taskId)
        {
            m_taskId = taskId;
        }
        
        const void* getUserData() const
        {
            return m_userData;
        }
        
        void setUserData(const void* userData)
        {
            m_userData = userData;
        }
        
    private:
        uint32_t        m_taskId;
        const void*     m_userData;
    };
    
    class Callback
    {
    public:
        virtual void onTaskStart(const AsyncExecutor* executor, const Task *task) = 0;
        virtual void onTaskComplete(const AsyncExecutor* executor, const Task *task, bool succeeded) = 0;
        virtual ~Callback() {}
    };
    
protected:
    class Thread
    {
    public:
        Thread(AsyncExecutor* executor);
        ~Thread();
    private:
        AsyncExecutor* m_executor;
        std::unique_ptr<std::thread> m_thread;
        void ThreadFunc();
        
    };

public:
    explicit AsyncExecutor(int reserve_threads, int max_threads, Callback *callback);
    ~AsyncExecutor();

    static uint32_t genNextTaskId();
    void addTask(Task *task);
    
    size_t getNumberOfQueue() const;
    void shutdown();
    void cancel();
    // true: completed, false: timeout
    bool waitForCompltion(unsigned int ms);
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    void setTag(const std::string& tag)
    {
        m_tag = tag;
    }
#endif

protected:
    
    Callback* m_callback;
#if !defined(NDEBUG) || defined(DBG_PERF)
    std::string m_tag;
    uint32_t m_tid;
#endif
    
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_shutdown_cv;
    bool m_shutdown;
    std::queue<Task *> m_tasks;
    int m_reserve_threads;
    int m_max_threads;
    int m_nthreads;

    int m_threads_waiting;
    std::list<Thread*> m_dead_threads;
    
    static std::atomic_uint32_t m_nextTaskId;

    void ThreadFunc();
    static void DestroyThreads(std::list<Thread*>* m_threads);
    
};

#endif /* AsyncExecutor_h */
