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
#include <atomic>
#include "Logger.h"

#ifdef USING_DOWNLOADER

class Task
{
protected:
    uint32_t m_taskId;
    std::string m_url;
    std::string m_output;
    std::string m_outputTmp;
    std::string m_error;
    std::string m_userAgent;
    time_t m_mtime;
    bool m_localCopy;
    unsigned int m_retries;
public:
    static const unsigned int MAX_RETRIES = 3;
public:
    Task() : m_taskId(0), m_localCopy(false)
    {
    }
    
    Task(uint32_t taskId, const std::string &url, const std::string& output, time_t mtime, bool localCopy = false);
    
    void setUserAgent(const std::string& userAgent)
    {
        m_userAgent = userAgent;
    }
    
    inline std::string getUrl() const
    {
        return m_url;
    }
    
    inline std::string getOutput() const
    {
        return m_output;
    }
    
    inline std::string getError() const
    {
        return m_error;
    }
    
    bool isLocalCopy() const
    {
        return m_localCopy;
    }
    
    Task& operator=(const Task& task)
    {
        if (this != &task)
        {
            m_taskId = task.m_taskId;
            m_url = task.m_url;
            m_output = task.m_output;
            m_mtime = task.m_mtime;
            m_localCopy = task.m_localCopy;
            m_retries = task.m_retries;
        }
        
        return *this;
    }
    
    size_t writeData(void *buffer, size_t size, size_t nmemb);
    bool run();
    unsigned int getRetries() const;
    
protected:
    bool downloadFile();
    bool copyFile();
};

class Downloader
{
protected:
    std::queue<Task> m_queue;
    std::queue<Task> m_copyQueue;
    std::map<std::string, std::string> m_urls;  // url => local file path for first download
    mutable std::mutex m_mtx;
    bool m_noMoreTask;
    size_t m_downloadTaskSize;    // +1 when task is added, -1 when download is completed
    std::vector<std::thread> m_threads;
    std::string m_userAgent;
    static std::atomic_uint32_t m_nextTaskId;
    
    Logger* m_logger;
    
public:
    Downloader(Logger* logger);
    ~Downloader();
    
    void setUserAgent(const std::string& userAgent);
    
    // return taskId
    uint32_t addTask(const std::string &url, const std::string& output, time_t mtime, std::string type = "");
    void setNoMoreTask();
    void run(int idx);
    
    void cancel();
    void shutdown();
    int getCount() const;
    int getRunningCount() const;

    static void initialize();
    static void uninitialize();
    
    static bool httpGet(const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers, long& httpStatus, std::vector<unsigned char>& body);
    
#ifndef NDEBUG
    std::string getStats() const;
#endif
    
protected:
    const Task& dequeue();
    
    
#ifndef NDEBUG
    std::map<std::string, uint32_t> m_statsType;
#endif
};

#endif // USING_DOWNLOADER

#endif /* Downloader_h */
