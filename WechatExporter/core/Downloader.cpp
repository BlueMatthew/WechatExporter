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
#include "Utils.h"

size_t writeData(void *buffer, size_t size, size_t nmemb, void *user_p)
{
    Task *task = reinterpret_cast<Task *>(user_p);
    if (NULL != task)
    {
        return task->writeData(buffer, size, nmemb);
    }
    
    return 0;
}

unsigned int Task::getRetries() const
{
    return m_retries;
}

bool Task::run()
{
    return m_localCopy ? copyFile() : downloadFile();
}

bool Task::downloadFile()
{
    ++m_retries;
    
    m_outputTmp = m_output + ".tmp";
    remove(utf8ToLocalAnsi(m_outputTmp).c_str());

	CURLcode res;

    std::string userAgent = m_userAgent.empty() ? "WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0" : m_userAgent;
    
    // User-Agent: WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &::writeData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
#ifndef NDEBUG
    std::string logPath = m_output + ".log";
    FILE* logFile = fopen(logPath.c_str(), "wb");
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_STDERR, logFile);
#endif

    res = curl_easy_perform(curl);
	if (res == CURLE_OK)
    {
		remove(utf8ToLocalAnsi(m_output).c_str());
        rename(utf8ToLocalAnsi(m_outputTmp).c_str(), utf8ToLocalAnsi(m_output).c_str());
    }
    else
	{
        m_error = curl_easy_strerror(res);
        if (m_retries >= MAX_RETRIES)
        {
            fprintf(stderr, "%s: %s\n", m_error.c_str(), m_url.c_str());
        }
	}
    
    curl_easy_cleanup(curl);
    
    if (m_mtime > 0)
    {
        updateFileTime(m_output, m_mtime);
    }
#ifndef NDEBUG
    if (NULL != logFile)
    {
        fclose(logFile);
    }
#endif
    
    return res == CURLE_OK;
}

bool Task::copyFile()
{
    std::ifstream  src(utf8ToLocalAnsi(m_url), std::ios::binary);
    std::ofstream  dst(utf8ToLocalAnsi(m_output), std::ios::binary);

    dst << src.rdbuf();
    
    return true;
}

size_t Task::writeData(void *buffer, size_t size, size_t nmemb)
{
    std::ofstream file;
    file.open (utf8ToLocalAnsi(m_outputTmp), std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::binary);
	size_t bytesToWrite = size * nmemb;
	if (file.is_open())
	{
		file.write(reinterpret_cast<const char *>(buffer), bytesToWrite);
		file.close();
	}

    return bytesToWrite;
}

Downloader::Downloader(Logger* logger) : m_logger(logger)
{
    m_noMoreTask = false;
    m_downloadTaskSize = 0;
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    for (int idx = 0; idx < 4; idx++)
    {
        m_threads.push_back(std::thread(&Downloader::run, this, idx));
    }
}

Downloader::~Downloader()
{
    curl_global_cleanup();
}

void Downloader::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
}

void Downloader::addTask(const std::string &url, const std::string& output, time_t mtime)
{
#ifndef NDEBUG
    if (url == "/0")
    {
        int aa = 0;
    }
#endif
    std::string formatedPath = output;
    std::replace(formatedPath.begin(), formatedPath.end(), DIR_SEP_R, DIR_SEP);
    std::string uid = url + output;
    bool existed = false;
    
    m_mtx.lock();
    std::map<std::string, std::string>::const_iterator it = m_urls.find(url);
    existed = (it != m_urls.cend());
    if (!existed)
    {
        m_urls[url] = output;
        Task task(url, formatedPath, mtime);
        task.setUserAgent(m_userAgent);
        m_queue.push(task);
        m_downloadTaskSize++;
    }
    else if (output != it->second)
    {
        Task task(it->second, formatedPath, mtime, true);
        m_copyQueue.push(task);
    }
    m_mtx.unlock();
    
    if (existed)
    {
#ifndef NDEBUG
        // printf("URL Existed: %s\r\n", url.c_str());
#endif
    }
}

void Downloader::setNoMoreTask()
{
    m_mtx.lock();
    m_noMoreTask = true;
    m_mtx.unlock();
}

void Downloader::run(int idx)
{
    while(1)
    {
        bool found = false;
        bool noMoreTask = false;
        
        Task task;
        m_mtx.lock();
        
        noMoreTask = m_noMoreTask && m_downloadTaskSize == 0;
        
        size_t queueSize = m_queue.size();
        if (queueSize > 0)
        {
            task = m_queue.front();
            m_queue.pop();
            found = true;
        }
        else if (noMoreTask)
        {
            if (!m_copyQueue.empty())
            {
                task = m_copyQueue.front();
                m_copyQueue.pop();
                found = true;
            }
        }

        m_mtx.unlock();
        
        if (found)
        {
            // run task
#ifndef NDEBUG
            if (!task.isLocalCopy())
            {
                std::string log = "Start downloading: " + task.getUrl() + " (" + std::to_string(task.getRetries() + 1) + ")";
                // m_logger->debug(log);
            }
#endif
            bool succeeded = task.run();
            if (!task.isLocalCopy())
            {
                m_mtx.lock();
                // unsigned int dlTaskSizeChanging = 0;
                if (!succeeded && task.getRetries() < Task::MAX_RETRIES)
                {
                    // Retry to download it
                    m_queue.push(task);
                }
                if (succeeded || task.getRetries() >= Task::MAX_RETRIES)
                {
                    m_downloadTaskSize--;
                }
                m_mtx.unlock();
#ifndef NDEBUG
                std::string log = "Downloading ";
                log += succeeded ? "Succeeded" : "Failed";
                log += ":" + task.getUrl() + " => " + task.getOutput() + " (" + std::to_string(task.getRetries() + 1) + ")";
                // m_logger->debug(log);
#endif
                
                if (!succeeded && task.getRetries() >= Task::MAX_RETRIES)
                {
                    std::string log = "Failed Download: " + task.getUrl() + " => " + task.getOutput() + " ERR:" + task.getError();
                    m_logger->write(log);
                }
            }
            
            continue;
        }
        if (noMoreTask)
        {
            break;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}

void Downloader::cancel()
{
    std::queue<Task> empty;
	std::queue<Task> empty2;
    m_mtx.lock();
    m_queue.swap(empty);
	m_copyQueue.swap(empty2);
    m_mtx.unlock();
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

int Downloader::getRunningCount() const
{
    size_t count  = 0;
    m_mtx.lock();
    count = m_queue.size();
    m_mtx.unlock();
    
    return static_cast<int>(count);
}
