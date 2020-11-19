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

void Task::run()
{
	remove(utf8ToLocalAnsi(m_output).c_str());
    
	CURLcode res;
	char errbuf[CURL_ERROR_SIZE] = { 0 };

    // User-Agent: WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0
    CURL *curl_handler = curl_easy_init();
    curl_easy_setopt(curl_handler, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl_handler, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEFUNCTION, &::writeData);
    curl_easy_setopt(curl_handler, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(curl_handler, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl_handler, CURLOPT_SSL_VERIFYHOST, 0);

    res = curl_easy_perform(curl_handler);
	if (res != CURLE_OK)
	{
		size_t len = strlen(errbuf);
		fprintf(stderr, "\nlibcurl: (%d) ", res);
		if (len)
			fprintf(stderr, "%s: %s%s", errbuf, m_url.c_str(),
			((errbuf[len - 1] != '\n') ? "\n" : ""));
		else
			fprintf(stderr, "%s: %s\n", curl_easy_strerror(res), m_url.c_str());
	}
    
    curl_easy_cleanup(curl_handler);
}

size_t Task::writeData(void *buffer, size_t size, size_t nmemb)
{
    std::ofstream file;
    file.open (utf8ToLocalAnsi(m_output), std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::binary);
	size_t bytesToWrite = size * nmemb;
	if (file.is_open())
	{
		file.write(reinterpret_cast<const char *>(buffer), bytesToWrite);
		file.close();
	}

    return bytesToWrite;
}

Downloader::Downloader()
{
    m_noMoreTask = false;
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

void Downloader::addTask(const std::string &url, const std::string& output)
{
    std::string formatedPath = output;
    std::replace(formatedPath.begin(), formatedPath.end(), DIR_SEP_R, DIR_SEP);

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

void Downloader::cancel()
{
    std::queue<Task> empty;
    m_mtx.lock();
    m_queue.swap(empty);
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
