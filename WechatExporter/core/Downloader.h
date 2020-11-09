//
//  Downloader.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#ifndef Downloader_h
#define Downloader_h

#include <stdio.h>
#include <string>
#include <queue>
#include <set>
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
        m_output = output;
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

class Downloader
{
protected:
    std::queue<Task> m_queue;
    std::set<std::string> m_urls;
    std::mutex m_mtx;
    bool m_noMoreTask;
    std::vector<std::thread> m_threads;
    
public:
    Downloader();
    ~Downloader();
    
    void addTask(const std::string &url, const std::string& output);
    void setNoMoreTask();
    void run();
    
    void finishAndWaitForExit();
    int getCount() const;
};

#endif /* Downloader_h */
