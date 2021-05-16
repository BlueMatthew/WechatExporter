//
//  SessionTaskManager.h
//  WechatExporter
//
//  Created by Matthew on 2021/4/20.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef TaskManager_h
#define TaskManager_h

#include <stdio.h>
#include <map>
#include <set>
#include "WechatObjects.h"
#include "AsyncExecutor.h"
#include "PdfConverter.h"
#include "Logger.h"

class TaskManager : public AsyncExecutor::Callback
{
private:
    Logger* m_logger;
    
    AsyncExecutor   *m_downloadExecutor;
#ifdef USING_ASYNC_TASK_FOR_MP3
    AsyncExecutor   *m_audioExecutor;
#endif
    AsyncExecutor   *m_pdfExecutor;
    std::map<std::string, std::string> m_downloadTasks;
    
    std::string m_userAgent;
    
    mutable std::mutex m_mutex;
    std::set<std::string> m_downloadedFiles;
    std::map<std::string, uint32_t> m_downloadingTasks;
    std::map<const Session*, uint32_t> m_sessionTaskCount;
    
    std::map<uint32_t, std::set<AsyncExecutor::Task *>> m_copyTaskQueue;
    std::map<const Session*, AsyncExecutor::Task *> m_pdfTaskQueue;
    
#ifdef USING_ASYNC_TASK_FOR_MP3
    std::queue<std::vector<unsigned char>> m_Buffers;
#endif
    
public:
    
    TaskManager(bool needPdfExecutor, Logger* logger);
    ~TaskManager();
    
    virtual void onTaskStart(const AsyncExecutor* executor, const AsyncExecutor::Task *task);
    virtual void onTaskComplete(const AsyncExecutor* executor, const AsyncExecutor::Task *task, bool succeeded);
    
    void setUserAgent(const std::string& userAgent);
    
    size_t getNumberOfQueue(std::string& queueDesc) const;
    void cancel();
    void shutdown();

    void download(const Session* session, const std::string &url, const std::string &backupUrl, const std::string& output, time_t mtime, const std::string& defaultFile = "", std::string type = "");
#ifdef USING_ASYNC_TASK_FOR_MP3
    void convertAudio(const Session* session, const std::string& pcmPath, const std::string& mp3Path, unsigned int mtime);
#endif
    void convertPdf(const Session* session, const std::string& htmlPath, const std::string& pdfPath, PdfConverter* pdfConverter);
    
private:
    
    void shutdownExecutors();
    
    inline uint32_t increaseSessionTask(const Session* session)
    {
        if (NULL != session)
        {
            std::map<const Session*, uint32_t>::iterator it = m_sessionTaskCount.find(session);
            if (it != m_sessionTaskCount.end())
            {
                it->second++;
                return it->second;
            }
        }
        
        return 0u;
    }
    
    inline uint32_t decreaseSessionTask(const Session* session)
    {
        if (NULL != session)
        {
            std::map<const Session*, uint32_t>::iterator it = m_sessionTaskCount.find(session);
            if (it != m_sessionTaskCount.end())
            {
                it->second--;
                return it->second;
            }
        }
        
        return 0u;
    }
    
    inline std::set<AsyncExecutor::Task *> dequeueCopyTasks(uint32_t taskId)
    {
        std::set<AsyncExecutor::Task *> tasks;
        std::map<uint32_t, std::set<AsyncExecutor::Task *>>::iterator it = m_copyTaskQueue.find(taskId);
        if (it != m_copyTaskQueue.end())
        {
            tasks.swap(it->second);
            m_copyTaskQueue.erase(it);
        }
        
        return tasks;
    }
    
    inline AsyncExecutor::Task* dequeuePdfTasks(const Session* session)
    {
        AsyncExecutor::Task* task = NULL;
        if (!m_pdfTaskQueue.empty())
        {
            std::map<const Session*, AsyncExecutor::Task *>::iterator it = m_pdfTaskQueue.find(session);
            if (it != m_pdfTaskQueue.end())
            {
                task = it->second;
                m_pdfTaskQueue.erase(it);
            }
        }

        return task;
    }
    
};

#endif /* SessionTaskManager_h */
