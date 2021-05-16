//
//  TaskManager.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/4/20.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "TaskManager.h"
#include "AsyncTask.h"
#include "FileSystem.h"

TaskManager::TaskManager(bool needPdfExecutor, Logger* logger) : m_logger(logger), m_downloadExecutor(NULL),
#ifdef USING_ASYNC_TASK_FOR_MP3
    m_audioExecutor(NULL),
#endif
    m_pdfExecutor(NULL)
{
    m_downloadExecutor = new AsyncExecutor(2, 4, this);
#ifdef USING_ASYNC_TASK_FOR_MP3
    m_audioExecutor = new AsyncExecutor(1, 1, this);
#endif
    // m_audioExecutor = m_downloadExecutor;
    if (needPdfExecutor)
    {
        m_pdfExecutor = new AsyncExecutor(4, 4, this);
    }
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    m_downloadExecutor->setTag("dl");
#ifdef USING_ASYNC_TASK_FOR_MP3
    m_audioExecutor->setTag("audio");
#endif
    if (m_pdfExecutor)
    {
        m_pdfExecutor->setTag("pdf");
    }
#endif
}

TaskManager::~TaskManager()
{
    shutdownExecutors();
    m_logger = NULL;
}

void TaskManager::shutdown()
{
    bool hasMoreTasks = true;
    while (1)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            hasMoreTasks = !(m_copyTaskQueue.empty() && m_pdfTaskQueue.empty());
        }
        
        if (!hasMoreTasks)
        {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    shutdownExecutors();
}

void TaskManager::cancel()
{
    std::map<uint32_t, std::set<AsyncExecutor::Task *>> copyTaskQueue;
    std::map<const Session*, AsyncExecutor::Task *> pdfTaskQueue;
    
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        copyTaskQueue.swap(m_copyTaskQueue);
        pdfTaskQueue.swap(m_pdfTaskQueue);
    }
    
    m_downloadExecutor->cancel();
#ifdef USING_ASYNC_TASK_FOR_MP3
    m_audioExecutor->cancel();
#endif
    if (NULL != m_pdfExecutor)
    {
        m_pdfExecutor->cancel();
    }
    
    for (std::map<uint32_t, std::set<AsyncExecutor::Task *>>::iterator it = copyTaskQueue.begin(); it != copyTaskQueue.end(); ++it)
    {
        for (std::set<AsyncExecutor::Task *>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2)
        {
            delete (*it2);
        }
        it->second.clear();
    }
    copyTaskQueue.clear();
    
    for (std::map<const Session*, AsyncExecutor::Task *>::iterator it = pdfTaskQueue.begin(); it != pdfTaskQueue.end(); ++it)
    {
        delete it->second;
    }
    pdfTaskQueue.clear();
}

size_t TaskManager::getNumberOfQueue(std::string& queueDesc) const
{
    size_t numberOfDownloads = m_downloadExecutor->getNumberOfQueue();
#ifdef USING_ASYNC_TASK_FOR_MP3
    size_t numberOfAudio = m_audioExecutor->getNumberOfQueue();
#endif
    size_t numberOfPdf = m_pdfTaskQueue.size();
    
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        numberOfDownloads += m_copyTaskQueue.size();
        numberOfPdf += m_pdfTaskQueue.size();
    }
    
    queueDesc = "";
    if (numberOfDownloads > 0)
    {
        queueDesc += std::to_string(numberOfDownloads) + " downloads";
    }
#ifdef USING_ASYNC_TASK_FOR_MP3
    if (numberOfAudio > 0)
    {
        if (!queueDesc.empty())
        {
            queueDesc += ", ";
        }
        queueDesc += std::to_string(numberOfAudio) + " audios";
    }
#endif
    if (numberOfPdf > 0)
    {
        if (!queueDesc.empty())
        {
            queueDesc += ", ";
        }
        queueDesc += std::to_string(numberOfPdf) + " pdf files";
    }

    return numberOfDownloads +
#ifdef USING_ASYNC_TASK_FOR_MP3
        numberOfAudio +
#endif
        numberOfPdf;
}

void TaskManager::shutdownExecutors()
{
    if (NULL != m_pdfExecutor)
    {
        delete m_pdfExecutor;
        m_pdfExecutor = NULL;
    }
#ifdef USING_ASYNC_TASK_FOR_MP3
    if (NULL != m_audioExecutor && m_audioExecutor != m_downloadExecutor)
    {
        delete m_audioExecutor;
        m_audioExecutor = NULL;
    }
#endif
    if (NULL != m_downloadExecutor)
    {
        delete m_downloadExecutor;
        m_downloadExecutor = NULL;
    }
}

void TaskManager::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
}

void TaskManager::onTaskStart(const AsyncExecutor* executor, const AsyncExecutor::Task *task)
{
    if (NULL != m_logger && task->getType() != TASK_TYPE_AUDIO)
    {
        if (task->getType() == TASK_TYPE_PDF)
        {
            m_logger->write("Task Starts: " + task->getName());
        }
    }
}

void TaskManager::onTaskComplete(const AsyncExecutor* executor, const AsyncExecutor::Task *task, bool succeeded)
{
    if (NULL != m_logger)
    {
        if (!succeeded)
        {
            m_logger->write("Failed: " + task->getName());
        }
#ifndef NDEBUG
        else
        {
            if (task->getType() == TASK_TYPE_DOWNLOAD)
            {
                // m_logger->write("Task Ends: " + task->getName());
            }
            if (task->getType() == TASK_TYPE_PDF)
            {
                // m_logger->write("Task Ends: " + task->getName());
            }
        }
#endif
    }
    
    const Session* session = task->getUserData() == NULL ? NULL : reinterpret_cast<const Session *>(task->getUserData());
    if (/*executor == m_downloadExecutor && */task->getType() == TASK_TYPE_DOWNLOAD)
    {
        const DownloadTask* downloadTask = dynamic_cast<const DownloadTask *>(task);
        
        std::unique_lock<std::mutex> lock(m_mutex);
        std::map<std::string, uint32_t>::const_iterator it = m_downloadingTasks.find(downloadTask->getUrl());
#ifndef NDEBUG
        assert(it != m_downloadingTasks.cend());
#endif
        if (it != m_downloadingTasks.cend())
        {
            m_downloadingTasks.erase(it);
        }
        
        std::set<AsyncExecutor::Task *> copyTasks = dequeueCopyTasks(task->getTaskId());
        
        // check copy task
        AsyncExecutor::Task *pdfTask = NULL;
        uint32_t taskCount = decreaseSessionTask(session);
        if (0 == taskCount)
        {
            pdfTask = dequeuePdfTasks(session);
        }
        
        lock.unlock();
#ifndef NDEBUG
        if (succeeded)
        {
            assert(existsFile(downloadTask->getOutput()));
        }
#endif
        for (std::set<AsyncExecutor::Task *>::iterator it = copyTasks.begin(); it != copyTasks.end(); ++it)
        {
            m_downloadExecutor->addTask(*it);
        }
        if (NULL != pdfTask)
        {
            m_pdfExecutor->addTask(pdfTask);
        }
    }
    else if ((task->getType() == TASK_TYPE_COPY) || (task->getType() == TASK_TYPE_AUDIO))
    {
        // check copy task
        AsyncExecutor::Task *pdfTask = NULL;
        std::unique_lock<std::mutex> lock(m_mutex);
        uint32_t taskCount = decreaseSessionTask(session);
        if (0 == taskCount)
        {
            pdfTask = dequeuePdfTasks(session);
        }
        
        lock.unlock();
        if (NULL != pdfTask)
        {
            m_pdfExecutor->addTask(pdfTask);
        }
    }
    else if (executor == m_pdfExecutor)
    {
        
    }
}

void TaskManager::download(const Session* session, const std::string &url, const std::string &backupUrl, const std::string& output, time_t mtime, const std::string& defaultFile/* = ""*/, std::string type/* = ""*/)
{
#ifndef NDEBUG
    if (url == "")
    {
        assert(!"url shouldn't be empty");
    }
	if (output.find(DIR_SEP_R) != std::string::npos)
	{
		assert(!"Directory is invalid");
	}
#endif
    
    std::map<std::string, std::string>::iterator it = m_downloadTasks.find(url);
    if (it != m_downloadTasks.end() && it->second == output)
    {
        // Existed and same output path, skip it
        return;
    }
    
    bool downloadFile = false;
    uint32_t taskId = AsyncExecutor::genNextTaskId();
    AsyncExecutor::Task *task = NULL;
    if (it != m_downloadTasks.end())
    {
        // Existed and different output path, copy it
        task = new CopyTask(it->second, output, "CP: " + url + " => " + output + " <= " + it->second);
    }
    else
    {
        DownloadTask* downloadTask = new DownloadTask(url, output, defaultFile, mtime, "DL: " + url + " => " + output);
        downloadTask->setUserAgent(m_userAgent);
        task = downloadTask;
        downloadFile = true;
        m_downloadTasks.insert(std::pair<std::string, std::string>(url, output));
    }
    task->setTaskId(taskId);
    task->setUserData(reinterpret_cast<const void *>(session));
    
    std::unique_lock<std::mutex> lock(m_mutex);
    if (NULL != session)
    {
        std::map<const Session*, uint32_t>::iterator it2 = m_sessionTaskCount.find(session);
        if (it2 == m_sessionTaskCount.end())
        {
            m_sessionTaskCount.insert(std::pair<const Session*, uint32_t>(session, 1u));
        }
        else
        {
            it2->second++;
        }
    }
    
    if (downloadFile)
    {
        m_downloadingTasks.insert(std::pair<std::string, uint32_t>(url, taskId));
    }
    else
    {
        std::map<std::string, uint32_t>::const_iterator it3 = m_downloadingTasks.find(url);
        if (it3 != m_downloadingTasks.cend())
        {
            // Downloading is running, add it into waiting queue
            std::map<uint32_t, std::set<AsyncExecutor::Task *>>::iterator it4 = m_copyTaskQueue.find(it3->second);
            if (it4 == m_copyTaskQueue.end())
            {
                it4 = m_copyTaskQueue.insert(it4, std::pair<uint32_t, std::set<AsyncExecutor::Task *>>(it3->second, std::set<AsyncExecutor::Task *>()));
            }
            it4->second.insert(task);
            task = NULL;
        }
    }

    lock.unlock();
    if (NULL != task)
    {
        m_downloadExecutor->addTask(task);
    }
}

#ifdef USING_ASYNC_TASK_FOR_MP3
void TaskManager::convertAudio(const Session* session, const std::string& pcmPath, const std::string& mp3Path, unsigned int mtime)
{
    if (NULL == session)
    {
        return;
    }
    
    Mp3Task *task = new Mp3Task(pcmPath, mp3Path, mtime);
    task->setTaskId(AsyncExecutor::genNextTaskId());
    task->setUserData(reinterpret_cast<const void *>(session));
    
    std::unique_lock<std::mutex> lock(m_mutex);
    std::map<const Session*, uint32_t>::iterator it = m_sessionTaskCount.find(session);
    if (it == m_sessionTaskCount.end())
    {
        m_sessionTaskCount.insert(std::pair<const Session*, uint32_t>(session, 1u));
    }
    else
    {
        it->second++;
    }
    
    lock.unlock();
    
    m_audioExecutor->addTask(task);
}
#endif

void TaskManager::convertPdf(const Session* session, const std::string& htmlPath, const std::string& pdfPath, PdfConverter* pdfConverter)
{
    if (NULL == session)
    {
        return;
    }
    
    PdfTask *task = new PdfTask(pdfConverter, htmlPath, pdfPath, "PDF: " + htmlPath + " => " + pdfPath);
    task->setTaskId(AsyncExecutor::genNextTaskId());
    task->setUserData(reinterpret_cast<const void *>(session));
    
    std::unique_lock<std::mutex> lock(m_mutex);
    std::map<const Session*, uint32_t>::iterator it = m_sessionTaskCount.find(session);
    if (it != m_sessionTaskCount.end() && it->second != 0)
    {
        m_pdfTaskQueue.insert(std::pair<const Session*, AsyncExecutor::Task *>(session, task));
    }
    else
    {
        lock.unlock();
        m_pdfExecutor->addTask(task);
    }
    
    
}
