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
#include <map>
#include <utility>
#include <thread>
#include <mutex>

class Task
{
protected:
    std::string m_url;
    std::string m_output;
    time_t m_mtime;
    bool m_localCopy;
    
public:
    Task() : m_localCopy(false)
    {
    }
    
    Task(const std::string &url, const std::string& output, time_t mtime, bool localCopy = false) : m_url(url), m_output(output), m_mtime(mtime), m_localCopy(localCopy)
    {
    }
    
    bool isLocalCopy() const
    {
        return m_localCopy;
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
    
protected:
    void downloadFile();
    void copyFile();
};

class Downloader
{
protected:
    std::queue<Task> m_queue;
    std::queue<Task> m_copyQueue;
    std::map<std::string, std::string> m_urls;  // url => local file path for first download
    mutable std::mutex m_mtx;
    bool m_noMoreTask;
    std::vector<std::thread> m_threads;
    
public:
    Downloader();
    ~Downloader();
    
    void addTask(const std::string &url, const std::string& output, time_t mtime);
    void setNoMoreTask();
    void run(int idx);
    
    void cancel();
    void finishAndWaitForExit();
    int getCount() const;
    int getRunningCount() const;
    
protected:
    const Task& dequeue();
};

#endif /* Downloader_h */
