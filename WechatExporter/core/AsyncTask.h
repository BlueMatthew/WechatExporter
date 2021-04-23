//
//  AsyncTask.h
//  WechatExporter
//
//  Created by Matthew on 2021/4/20.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef AsyncTask_h
#define AsyncTask_h

#include <stdio.h>
#include "AsyncExecutor.h"
#include "PdfConverter.h"

#define TASK_TYPE_DOWNLOAD  1
#define TASK_TYPE_COPY      2
#define TASK_TYPE_MP3       3
#define TASK_TYPE_PDF       4

class DownloadTask : public AsyncExecutor::Task
{
private:
    std::string m_url;
    std::string m_output;
    std::string m_default;
    std::string m_outputTmp;
    std::string m_error;
    std::string m_userAgent;
    time_t m_mtime;
    unsigned int m_retries;
    
    std::string m_name;
    
public:
    static const unsigned int DEFAULT_RETRIES = 3;
    
    DownloadTask(const std::string &url, const std::string& output, const std::string& defaultFile, time_t mtime, const std::string& name = "");
    virtual ~DownloadTask() {}
    
    virtual int getType() const
    {
        return TASK_TYPE_DOWNLOAD;
    }
    
    virtual std::string getName() const
    {
        return m_name;
    }
    
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
    
    static bool httpGet(const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers, long& httpStatus, std::vector<unsigned char>& body);
    
    size_t writeData(void *buffer, size_t size, size_t nmemb);
    
    unsigned int getRetries() const;
    
    bool run();
    
protected:
    bool downloadFile();
};

class CopyTask : public AsyncExecutor::Task
{
public:
    CopyTask(const std::string &src, const std::string& dest, const std::string& name);
    virtual ~CopyTask() {}
    
    virtual int getType() const
    {
        return TASK_TYPE_COPY;
    }
    virtual std::string getName() const
    {
        return m_name;
    }
    
    bool run();
    
private:
    std::string m_src;
    std::string m_dest;
    std::string m_name;
};

class Mp3Task : public AsyncExecutor::Task
{
public:
    Mp3Task(const std::string &pcm, const std::string& mp3, unsigned int mtime);
    virtual ~Mp3Task() {}
    
    virtual int getType() const
    {
        return TASK_TYPE_MP3;
    }
    virtual std::string getName() const
    {
        return "Mp3: " + m_mp3;
    }
    
    bool run();
    
private:
    std::string m_pcm;
    std::string m_mp3;
    unsigned int m_mtime;
};

class PdfTask : public AsyncExecutor::Task
{
public:
    PdfTask(PdfConverter* pdfConveter, const std::string &src, const std::string& dest, const std::string& name);
    virtual ~PdfTask() {}
    
    virtual int getType() const
    {
        return TASK_TYPE_PDF;
    }
    virtual std::string getName() const
    {
        return "PDF: " + m_src + " => " + m_dest;
    }
    
    bool run();
    
private:
    PdfConverter   *m_pdfConverter;
    std::string m_src;
    std::string m_dest;
    std::string m_name;
};

#endif /* AsyncTask_h */
