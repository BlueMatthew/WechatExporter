//
//  AsyncTask.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/4/20.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "AsyncTask.h"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#ifdef _WIN32
#include <atlstr.h>
#ifndef NDEBUG
#include <cassert>
#endif
#endif
#include "FileSystem.h"
#include "Utils.h"

// #define FAKE_DOWNLOAD
size_t writeHttpDataToBuffer(void *buffer, size_t size, size_t nmemb, void *user_p)
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

void DownloadTask::initialize()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

void DownloadTask::uninitialize()
{
    curl_global_cleanup();
}

bool DownloadTask::httpGet(const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers, long& httpStatus, std::vector<unsigned char>& body)
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &::writeHttpDataToBuffer);
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

size_t writeTaskHttpData(void *buffer, size_t size, size_t nmemb, void *user_p)
{
    DownloadTask *task = reinterpret_cast<DownloadTask *>(user_p);
    if (NULL != task)
    {
        return task->writeData(buffer, size, nmemb);
    }
    
    return 0;
}

DownloadTask::DownloadTask(const std::string &url, const std::string& output, const std::string& defaultFile, time_t mtime, const std::string& name/* = ""*/) : m_url(url), m_output(output), m_default(defaultFile), m_mtime(mtime), m_retries(0), m_name(name)
{
#ifndef NDEBUG
    if (m_output.empty())
    {
        assert(false);
    }
#endif
}

unsigned int DownloadTask::getRetries() const
{
    return m_retries;
}

bool DownloadTask::run()
{
    std::string* urls[] = { &m_url, &m_urlBackup };
    
    for (int item = 0; item < sizeof(urls) / sizeof(std::string*); ++item)
    {
        if (urls[item]->empty())
        {
            continue;
        }
        for (int idx = 0; idx < DEFAULT_RETRIES; idx++)
        {
            if (downloadFile(*urls[item]))
            {
                return true;
            }
        }
        
        if (startsWith(*urls[item], "http://"))
        {
            std::string url = *urls[item];
            url.replace(0, 7, "https://");
            if (downloadFile(url))
            {
                return true;
            }
        }
    }
    
    if (!m_default.empty())
    {
        if (copyFile(m_default, m_output))
        {
            return true;
        }
        else
        {
            m_error += "\r\nFailed to copy default file: " + m_default + " => " + m_output;
        }
    }
    
    return false;
}

bool DownloadTask::downloadFile(const std::string& url)
{
    ++m_retries;
    
    m_outputTmp = m_output + ".tmp";
    deleteFile(m_outputTmp);

    CURLcode res = CURLE_OK;
    CURL *curl = NULL;
#ifndef NDEBUG
    std::string logPath = m_output + ".http.log";
    
#ifdef _WIN32
    CA2W pszW(logPath.c_str(), CP_UTF8);
    FILE* logFile = _wfopen((LPCWSTR)pszW, L"wb");
#else
    FILE* logFile = fopen(logPath.c_str(), "wb");
#endif
#endif
    
    std::string userAgent = m_userAgent.empty() ? "WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0" : m_userAgent;
    
    long httpStatus = 0;
    
#ifndef FAKE_DOWNLOAD
    // User-Agent: WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &::writeTaskHttpData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
#ifndef NDEBUG
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_STDERR, logFile);
#endif

    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        m_error = "Failed " + m_name + "\r\n";
        m_error += curl_easy_strerror(res);
        if (m_retries >= DEFAULT_RETRIES)
        {
            fprintf(stderr, "%s: %s\n", m_error.c_str(), m_url.c_str());
        }
    }
    else
    {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
        
#ifndef NDEBUG
        char *lastUrl = NULL;
        CURLcode res2 = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &lastUrl);

        if((CURLE_OK == res2) && lastUrl && m_url != lastUrl)
        {
            m_error += " \r\nRedirect: ";
            m_error += lastUrl;
        }
#endif
    }
    curl_easy_cleanup(curl);

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
        return true;
    }
#else
    return true;
#endif // no FAKE_DOWNLOAD

    if (m_error.empty())
    {
        m_error = "HTTP Status:" + std::to_string(httpStatus);
    }
    return false;
}

size_t DownloadTask::writeData(void *buffer, size_t size, size_t nmemb)
{
    size_t bytesToWrite = size * nmemb;
    if (appendFile(m_outputTmp, reinterpret_cast<const unsigned char *>(buffer), bytesToWrite))
    {
        return bytesToWrite;
    }
    return 0;
}

CopyTask::CopyTask(const std::string &src, const std::string& dest, const std::string& name) : m_src(src), m_dest(dest), m_name(name)
{
}

bool CopyTask::run()
{
    if (::copyFile(m_src, m_dest))
    {
        return true;
    }
    
    if (!existsFile(m_src))
    {
        m_error = "Failed CP: " + m_src + "(not existed)  => " + m_dest;
    }
    else
    {
        m_error = "Failed CP: " + m_src + " => " + m_dest;
#ifdef _WIN32
        DWORD lastError = ::GetLastError();
        m_error += "LastError:" + std::to_string(lastError);
#endif
    }
    
    return false;
}

Mp3Task::Mp3Task(const std::string &pcm, const std::string& mp3, unsigned int mtime) : m_pcm(pcm), m_mp3(mp3), m_mtime(mtime)
{
}

void Mp3Task::swapBuffer(std::vector<unsigned char>& buffer)
{
    m_pcmData.swap(buffer);
}

bool Mp3Task::run()
{
    std::vector<unsigned char> pcmData;
    bool isSilk = false;
    bool res = silkToPcm(m_pcm, pcmData, isSilk, &m_error) && !pcmData.empty();
    if (res)
    {
        res = pcmToMp3(pcmData, m_mp3);
    }
    else if (!isSilk)
    {
        res = amrToPcm(m_pcm, pcmData) && !pcmData.empty();
        if (res)
        {
            res = amrPcmToMp3(pcmData, m_mp3);
        }
    }
    if (res)
    {
        updateFileTime(m_mp3, m_mtime);
        return true;
    }
    /*
    if (res)
    {
        m_error = "Failed pcmToMp3: " + m_pcm + " => " + m_mp3;
    }
    else
    {
        m_error = "Failed silkTpPcm: " + m_pcm;
    }
     */
    return false;
}

PdfTask::PdfTask(PdfConverter* pdfConveter, const std::string &src, const std::string& dest, const std::string& name) : m_pdfConverter(pdfConveter), m_src(src), m_dest(dest), m_name(name)
{
}

bool PdfTask::run()
{
    return m_pdfConverter->convert(m_src, m_dest);
}
