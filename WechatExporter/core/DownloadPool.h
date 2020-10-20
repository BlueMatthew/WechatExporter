//
//  DownloadPool.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#ifndef DownloadPool_h
#define DownloadPool_h

#include <stdio.h>
#include <string>
#include <queue>
#include <utility>
#include <thread>
#include <mutex>

class Task
{
protected:
    std::string m_url;
    std::string m_output;
    
public:
    Task()
    {
    }
    
    Task(const std::string &url, const std::string& output)
    {
        m_url = url;
        m_output = m_output;
    }
    
    Task& operator=(const Task& task)
    {
        if (this != &task)
        {
            m_url = task.m_url;
            m_output = task.m_output;
        }
        
        return *this;
    }
    
    size_t writeData(void *buffer, size_t size, size_t nmemb);
    void run();
};

class DownloadPool
{
protected:
    std::queue<Task> m_queue;
    std::mutex m_mtx;
    bool m_noMoreTask;
    std::vector<std::thread> m_threads;
    
public:
    DownloadPool();
    ~DownloadPool();
    
    void addTask(const std::string &url, const std::string& output);
    void setNoMoreTask();
    void run();
    
    void finishAndWaitForExit();
};

#endif /* DownloadPool_h */
