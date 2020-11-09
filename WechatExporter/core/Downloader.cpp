//
//  Downloader.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Downloader.h"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include "OSDef.h"

size_t writeData(void *buffer, size_t size, size_t nmemb, void *user_p)
{
    Task *task = reinterpret_cast<Task *>(user_p);
    if (NULL != task)
    {
        return task->writeData(buffer, size, nmemb);
    }
    
    return 0;
}

void Task::run()
{
    std::ofstream output(m_output, std::fstream::in | std::fstream::out | std::fstream::trunc);
    output.close();
    
    CURL *curl_handler = curl_easy_init();
    curl_easy_setopt(curl_handler, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl_handler, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, &::writeData);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, this);

    curl_easy_perform(curl_handler);
    
    curl_easy_cleanup(curl_handler);
}

size_t Task::writeData(void *buffer, size_t size, size_t nmemb)
{
    std::ofstream file;
    file.open (m_output, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::binary);
    // file.write(buffer, size);
    size_t bytesToWrite = size * nmemb;
    file.write(reinterpret_cast<const char *>(buffer), bytesToWrite);
    file.close();
    
    return bytesToWrite;
}

Downloader::Downloader()
{
    m_noMoreTask = false;
    curl_global_init(CURL_GLOBAL_ALL);
    
    for (int idx = 0; idx < 4; idx++)
    {
        m_threads.push_back(std::thread(&Downloader::run, this));
    }
    // vecOfThreads.push_back(std::thread(func));
    
}

Downloader::~Downloader()
{
    curl_global_cleanup();
}

void Downloader::addTask(const std::string &url, const std::string& output)
{
    std::string formatedPath = output;
    std::replace(formatedPath.begin(), formatedPath.end(), DIR_SEP_R, DIR_SEP);
#ifndef NDEBUG
    struct stat buffer;
    if (stat (formatedPath.c_str(), &buffer) == 0)
    {
        return;
    }
#endif

    std::string uid = url + output;
    bool existed = false;
    Task task(url, formatedPath);
    m_mtx.lock();
    if (!(existed = (m_urls.find(uid) != m_urls.cend())))
    {
        m_urls.insert(uid);
        m_queue.push(task);
    }
    m_mtx.unlock();
    
    if (existed)
    {
#ifndef NDEBUG
        printf("URL Existed: %s", url.c_str());
#endif
    }
}

void Downloader::setNoMoreTask()
{
    m_mtx.lock();
    m_noMoreTask = true;
    m_mtx.unlock();
}

void Downloader::run()
{
    while(1)
    {
        bool found = false;
        bool noMoreTask = false;
        
        Task task;
        m_mtx.lock();
        
        size_t queueSize = m_queue.size();
        if (queueSize > 0)
        {
            task = m_queue.front();
            m_queue.pop();
            found = true;
        }
        
        noMoreTask = m_noMoreTask;
        
        m_mtx.unlock();
        
        if (found)
        {
            // run task
            task.run();
            continue;
        }
        if (m_noMoreTask)
        {
            break;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void Downloader::finishAndWaitForExit()
{
    setNoMoreTask();
    for (std::vector<std::thread>::iterator it = m_threads.begin(); it != m_threads.end(); ++it)
    {
        it->join();
    }
}

int Downloader::getCount() const
{
    return static_cast<int>(m_urls.size());
}
