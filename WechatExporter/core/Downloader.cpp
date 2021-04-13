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
#include "FileSystem.h"
#ifdef _WIN32
#include <atlstr.h>
#ifndef NDEBUG
#include <cassert>
#endif
#endif

// #define FAKE_DOWNLOAD
size_t writeDataToBuffer(void *buffer, size_t size, size_t nmemb, void *user_p)
{
    std::vector<unsigned char>* body = reinterpret_cast<std::vector<unsigned char> *>(user_p);
    if (NULL != body)
    {
        size_t bytes = size * nmemb;

        unsigned char *ptr = reinterpret_cast<unsigned char *>(buffer);
        std::copy(ptr, ptr + bytes, back_inserter(*body));
        return bytes;
    }
    
    return 0;
}

bool Downloader::httpGet(const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers, long& httpStatus, std::vector<unsigned char>& body)
{
    httpStatus = 0;
    CURLcode res = CURLE_OK;
    CURL *curl = NULL;
    
    body.clear();
    
#ifndef FAKE_DOWNLOAD
    // User-Agent: WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0
    curl = curl_easy_init();
    
#ifndef NDEBUG
    struct curl_slist *host = NULL;
#endif

    struct curl_slist *chunk = NULL;
    
    for (std::vector<std::pair<std::string, std::string>>::const_iterator it = headers.cbegin(); it != headers.cend(); ++it)
    {
        if (it->first == "User-Agent")
        {
            curl_easy_setopt(curl, CURLOPT_USERAGENT, it->second.c_str());
        }
#ifndef NDEBUG
        else if (it->first == "RESOLVE")
        {
            host = curl_slist_append(host, it->second.c_str());
        }
#endif
        else
        {
            std::string header = it->first + ": " + it->second;
            chunk = curl_slist_append(chunk, header.c_str());
        }
    }
    
#ifndef NDEBUG
    if (NULL != host)
    {
        curl_easy_setopt(curl, CURLOPT_RESOLVE, host);
    }
#endif
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    if (NULL != chunk)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }
    // curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &::writeDataToBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void *>(&body));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

    res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
        // m_error = curl_easy_strerror(res);
    }
    curl_easy_cleanup(curl);
#ifndef NDEBUG
    if (NULL != host)
    {
        curl_slist_free_all(host);
    }
#endif
    if (NULL != chunk)
    {
        curl_slist_free_all(chunk);
    }
#endif // no FAKE_DOWNLOAD
    
    return res == CURLE_OK;
}

size_t writeTaskData(void *buffer, size_t size, size_t nmemb, void *user_p)
{
    Task *task = reinterpret_cast<Task *>(user_p);
    if (NULL != task)
    {
        return task->writeData(buffer, size, nmemb);
    }
    
    return 0;
}

Task::Task(const std::string &url, const std::string& output, time_t mtime, bool localCopy/* = false*/) : m_url(url), m_output(output), m_mtime(mtime), m_localCopy(localCopy), m_retries(0)
{
#ifndef NDEBUG
    if (m_output.empty())
    {
        assert(false);
    }
#endif
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

	CURLcode res = CURLE_OK;
    CURL *curl = NULL;
#ifndef NDEBUG
    std::string logPath = m_output + ".log";
    
#ifdef _WIN32
    CA2W pszW(logPath.c_str(), CP_UTF8);
    FILE* logFile = _wfopen((LPCWSTR)pszW, L"wb");
#else
    FILE* logFile = fopen(logPath.c_str(), "wb");
#endif
#endif
    
    std::string userAgent = m_userAgent.empty() ? "WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0" : m_userAgent;
    
#ifndef FAKE_DOWNLOAD
    // User-Agent: WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &::writeTaskData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
#ifndef NDEBUG
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_STDERR, logFile);
#endif

	long httpStatus = 0;
    res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
        m_error = curl_easy_strerror(res);
        if (m_retries >= MAX_RETRIES)
        {
            fprintf(stderr, "%s: %s\n", m_error.c_str(), m_url.c_str());
        }
	}
	else
	{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
	}
    curl_easy_cleanup(curl);
#endif // no FAKE_DOWNLOAD

#ifndef NDEBUG
    if (NULL != logFile)
    {
        fclose(logFile);
    }
#endif
    
    if (res == CURLE_OK && httpStatus == 200)
    {
        ::moveFile(m_outputTmp, m_output);
        if (m_mtime > 0)
        {
            updateFileTime(m_output, m_mtime);
        }
#ifndef NDEBUG
        ::deleteFile(logPath);
#endif
    }

    return res == CURLE_OK;
}

bool Task::copyFile()
{
	return ::copyFile(m_url, m_output);
}

size_t Task::writeData(void *buffer, size_t size, size_t nmemb)
{
	size_t bytesToWrite = size * nmemb;
	if (appendFile(m_outputTmp, reinterpret_cast<const unsigned char *>(buffer), bytesToWrite))
	{
		return bytesToWrite;
	}
	return 0;
}

Downloader::Downloader(Logger* logger) : m_logger(logger)
{
    m_noMoreTask = false;
    m_downloadTaskSize = 0;
}

Downloader::~Downloader()
{
}

void Downloader::initialize()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

void Downloader::uninitialize()
{
    curl_global_cleanup();
}

void Downloader::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
}

void Downloader::addTask(const std::string &url, const std::string& output, time_t mtime, std::string type/* = ""*/)
{
#ifndef NDEBUG
    if (url == "/0" || url.empty() || output.empty())
    {
        int aa = 0;
    }
    
    if (!startsWith(url, "http://") && !startsWith(url, "https://") && !startsWith(url, "file://"))
    {
        assert(false);
    }
#endif
    m_mtx.lock();
    if (m_threads.empty())
    {
        for (int idx = 0; idx < 4; idx++)
        {
            m_threads.push_back(std::thread(&Downloader::run, this, idx));
        }
    }
    m_mtx.unlock();
    
    std::string formatedPath = output;
    std::replace(formatedPath.begin(), formatedPath.end(), DIR_SEP_R, DIR_SEP);
    std::string uid = url + output;
    bool existed = false;
    
    m_mtx.lock();
    if (startsWith(url, "file://"))
    {
        Task task(url.substr(7), formatedPath, mtime, true);
        m_copyQueue.push(task);
    }
    else
    {
        std::map<std::string, std::string>::const_iterator it = m_urls.find(url);
        existed = (it != m_urls.cend());
        
        if (!existed)
        {
            m_urls[url] = output;
            Task task(url, formatedPath, mtime);
            task.setUserAgent(m_userAgent);
            m_queue.push(task);
            m_downloadTaskSize++;
#ifndef NDEBUG
            std::string key = type.empty() ? "unkownd" : type;
            std::map<std::string, uint32_t>::iterator it = m_statsType.find(key);
            if (it == m_statsType.end())
            {
                m_statsType.insert(std::pair<std::string, uint32_t>(key, 1));
            }
            else
            {
                ++(it->second);
            }
#endif
        }
        else if (output != it->second)
        {
            Task task(it->second, formatedPath, mtime, true);
            m_copyQueue.push(task);
        }
    }

    m_mtx.unlock();
    
    if (existed)
    {
#ifndef NDEBUG
        // printf("URL Existed: %s\r\n", url.c_str());
#endif
    }
}

#ifndef NDEBUG
std::string Downloader::getStats() const
{
    std::string stats;
    
    for (std::map<std::string, uint32_t>::const_iterator it = m_statsType.cbegin(); it != m_statsType.cend(); ++it)
    {
        stats.append(it->first + ":" + std::to_string(it->second) + " ");
    }
    
    return stats;
}
#endif

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
                
                if (!succeeded/* && task.getRetries() >= Task::MAX_RETRIES*/)
                {
                    std::string log = "Failed Download(" + std::to_string(task.getRetries()) + "): " + task.getUrl() + " => " + task.getOutput() + " ERR:" + task.getError();
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
    m_downloadTaskSize -= m_queue.size();
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
