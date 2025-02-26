//
//  Exporter.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Exporter.h"
#include <json/json.h>
#ifdef USING_DOWNLOADER
#include "Downloader.h"
#else
#include "AsyncTask.h"
#endif
#include "TaskManager.h"
#include "WechatParser.h"
#include "ExportContext.h"
#ifdef _WIN32
#include <winsock.h>
#endif


#define USING_NEW_TEMPLATE 1

#ifndef NDEBUG
static const size_t PAGE_SIZE = 100;
#else
static const size_t PAGE_SIZE = 1000;
#endif

Exporter::Exporter(const std::string& workDir, const std::string& backup, const std::string& output, Logger* logger, PdfConverter* pdfConverter)
{
    m_running = false;
    m_iTunesDb = NULL;
    m_iTunesDbShare = NULL;
    m_workDir = workDir;
    m_backup = backup;
    m_output = output;
    m_logger = logger;
    m_pdfConverter = pdfConverter;
    m_notifier = NULL;
    m_cancelled = false;
    m_options = 0;
    // m_filterByName = false;
    m_extName = "html";
    m_templatesName = "templates";
    m_exportContext = NULL;
}

Exporter::~Exporter()
{
    if (NULL != m_exportContext)
    {
        delete m_exportContext;
        m_exportContext = NULL;
    }
    releaseITunes();
    m_logger = NULL;
    m_notifier = NULL;
}

void Exporter::initializeExporter()
{
    // Disable Memory Stats in sqlite
    sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0);
#ifdef USING_DOWNLOADER
    Downloader::initialize();
#else
    DownloadTask::initialize();
#endif
}

void Exporter::uninitializeExporter()
{
#ifdef USING_DOWNLOADER
    Downloader::uninitialize();
#else
    DownloadTask::uninitialize();
#endif
}

void Exporter::setOptions(const ExportOption& options)
{
    m_options = options;
}

/*
void Exporter::includesSubscription()
{
    m_options.includesSubscription();
}
*/

bool Exporter::hasDebugLogs() const
{
    return m_options.isOutputtingDebugLogs();
}

bool Exporter::isSubscriptionIncluded() const
{
    return m_options.isIncludingSubscription();
}

bool Exporter::hasPreviousExporting(const std::string& outputDir, uint64_t& options, std::string& exportTime, std::string& version)
{
    std::string fileName = combinePath(outputDir, WXEXP_DATA_FOLDER, WXEXP_DATA_FILE);
    if (!existsFile(fileName))
    {
        return false;
    }
    
    ExportContext context;
    if (!loadExportContext(fileName, &context))
    {
        return false;
    }
    
    options = context.getOptions();
	version = context.getVersion();
    std::time_t ts = context.getExportTime();
    std::tm * ptm = std::localtime(&ts);
    char buffer[32];
	std::strftime(buffer, 32, "%Y-%m-%d %H:%M", ptm);
    
    exportTime = buffer;
    
    return true;
}

bool Exporter::loadExportContext(const std::string& contextFile, ExportContext *context)
{
    std::string contents = readFile(contextFile);
    if (contents.empty())
    {
        return false;
    }

    if (!context->unserialize(contents) || context->getNumberOfSessions() == 0)
    {
        return false;
    }
    
    return true;
}

void Exporter::setNotifier(ExportNotifier *notifier)
{
    m_notifier = notifier;
}

bool Exporter::isRunning() const
{
    return m_running;
}

void Exporter::cancel()
{
    m_cancelled = true;
}

void Exporter::waitForComplition()
{
    if (!isRunning())
    {
        return;
    }

    m_thread.join();
}

void Exporter::setExtName(const std::string& extName)
{
    m_extName = extName;
}

void Exporter::setTemplatesName(const std::string& templatesName)
{
    m_templatesName = templatesName;
}

void Exporter::setLanguageCode(const std::string& languageCode)
{
    m_languageCode = languageCode;
}

void Exporter::filterUsersAndSessions(const std::map<std::string, std::map<std::string, void *>>& usersAndSessions)
{
    m_usersAndSessionsFilter = usersAndSessions;
}

bool Exporter::run()
{
    if (isRunning() || m_thread.joinable())
    {
        m_logger->write(m_resManager.getLocaleString("Previous task has not completed."));
        
        return false;
    }

    if (!existsDirectory(m_output))
    {
        m_logger->write(formatString(m_resManager.getLocaleString("Can't access output directory: %s"), m_output.c_str()));
        return false;
    }
    
    m_running = true;

    std::thread th(&Exporter::runImpl, this);
    m_thread.swap(th);

    return true;
}

bool Exporter::loadUsersAndSessions()
{
    m_usersAndSessions.clear();
    
    m_resManager.initLocaleResource(m_workDir, m_languageCode);

    if (!loadITunes(false))
    {
        m_logger->write(formatString(m_resManager.getLocaleString("Failed to parse the backup data of iTunes in the directory: %s"), m_backup.c_str()));
        notifyComplete();
        return false;
    }
    m_logger->debug("ITunes Database loaded.");
    
    WechatInfoParser wechatInfoParser(m_iTunesDb);
    if (wechatInfoParser.parse(m_wechatInfo))
    {
        m_logger->write(formatString(m_resManager.getLocaleString("iTunes Version: %s, iOS Version: %s, WeChat Version: %s"), m_iTunesDb->getVersion().c_str(), m_iTunesDb->getIOSVersion().c_str(), m_wechatInfo.getShortVersion().c_str()));
    }
    
    std::vector<Friend> users;
    
    LoginInfo2Parser loginInfo2Parser(m_iTunesDb, hasDebugLogs() ? m_logger : NULL);
    if (!loginInfo2Parser.parse(users))
    {
        if (hasDebugLogs())
        {
            m_logger->debug(loginInfo2Parser.getError());
        }
        return false;
    }

    m_logger->debug("WeChat Users loaded.");
    m_usersAndSessions.reserve(users.size()); // Avoid re-allocation and causing the pointer changed
    for (std::vector<Friend>::const_iterator it = users.cbegin(); it != users.cend(); ++it)
    {
        std::vector<std::pair<Friend, std::vector<Session>>>::iterator it2 = m_usersAndSessions.emplace(m_usersAndSessions.cend(), std::pair<Friend, std::vector<Session>>(*it, std::vector<Session>()));
        Friends friends;
        loadUserFriendsAndSessions(it2->first, friends, it2->second, false);
    }

    return true;
}

void Exporter::swapUsersAndSessions(std::vector<std::pair<Friend, std::vector<Session>>>& usersAndSessions)
{
    usersAndSessions.swap(m_usersAndSessions);
}

bool Exporter::runImpl()
{
#if !defined(NDEBUG) || defined(DBG_PERF)
    setThreadName("exp");
#endif
    time_t startTime;
    std::time(&startTime);
    notifyStart();
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    makeDirectory(combinePath(m_output, "dbg"));
#endif
    
    if (!m_resManager.initResources(m_workDir, m_languageCode, m_templatesName))
    {
        m_logger->write(formatString("Failed to load resources in %s.", m_workDir.c_str()));

        if (hasDebugLogs())
        {
            std::string emptyTemplates = m_resManager.checkEmptyTemplates();
            if (!emptyTemplates.empty())
            {
                m_logger->write("Empty template: " + emptyTemplates);
            }
        }
    }

    m_logger->write(formatString(m_resManager.getLocaleString("iTunes Backup: %s"), m_backup.c_str()));

    if (!loadITunes())
    {
        m_logger->write(formatString(m_resManager.getLocaleString("Failed to parse the backup data of iTunes in the directory: %s"), m_backup.c_str()));
        notifyComplete();
        return false;
    }
    m_logger->debug("ITunes Database loaded.");
    
    WechatInfoParser wechatInfoParser(m_iTunesDb);
    if (wechatInfoParser.parse(m_wechatInfo))
    {
        m_logger->write(formatString(m_resManager.getLocaleString("iTunes Version: %s, WeChat Version: %s"), m_iTunesDb->getVersion().c_str(), m_wechatInfo.getShortVersion().c_str()));
    }

    m_logger->write(m_resManager.getLocaleString("Finding WeChat accounts..."));

    std::vector<Friend> users;

    LoginInfo2Parser loginInfo2Parser(m_iTunesDb, hasDebugLogs() ? m_logger : NULL);
    if (!loginInfo2Parser.parse(users))
    {
        m_logger->write(m_resManager.getLocaleString("Failed to find WeChat account."));
        if (hasDebugLogs())
        {
            m_logger->debug(loginInfo2Parser.getError());
        }
        notifyComplete();
        return false;
    }

    m_logger->write(formatString(m_resManager.getLocaleString("%d WeChat account(s) found."), (int)(users.size())));

    // if (m_options.isIncrementalExporting())
    {
        std::string path = combinePath(m_output, WXEXP_DATA_FOLDER);
        makeDirectory(path);
    }
    if (NULL == m_exportContext)
    {
        m_exportContext = new ExportContext();
    }
    uint64_t orgOptions = m_options;
    std::string contextFileName = combinePath(m_output, WXEXP_DATA_FOLDER, WXEXP_DATA_FILE);
    if ((m_options.isIncrementalExporting()) && loadExportContext(contextFileName, m_exportContext))
    {
        // Use the previous options
        m_options = m_exportContext->getOptions();
        m_options.setIncrementalExporting(true);
        m_logger->write(m_resManager.getLocaleString("Incremental Exporting"));
    }
    else
    {
        // If there is no export context, save current options
        m_exportContext->setOptions(m_options);
    }
    
    std::string htmlBody;

    std::set<std::string> userFileNames;
    for (std::vector<Friend>::iterator it = users.begin(); it != users.end(); ++it)
    {
        if (m_cancelled)
        {
            break;
        }
        
        if (!m_usersAndSessionsFilter.empty())
        {
            const std::string& nameForFilter = m_options.isFilteredByName() ? it->getDisplayName() : it->getUsrName();
            if (m_usersAndSessionsFilter.find(nameForFilter) == m_usersAndSessionsFilter.cend())
            {
                continue;
            }
        }
        
        if (!buildFileNameForUser(*it, userFileNames))
        {
            m_logger->write(formatString(m_resManager.getLocaleString("Can't build directory name for user: %s. Skip it."), it->getUsrName().c_str()));
            continue;
        }

        std::string userOutputPath;
        exportUser(*it, userOutputPath);
        
        std::string userItem = m_resManager.getTemplate("listitem");
        replaceAll(userItem, "%%ITEMPICPATH%%", userOutputPath + "/Portrait/" + it->getLocalPortrait());
        if (!m_options.isTextMode())
        {
            replaceAll(userItem, "%%ITEMLINK%%", encodeUrl(it->getOutputFileName()) + "/index." + m_extName);
            replaceAll(userItem, "%%ITEMTEXT%%", safeHTML(it->getDisplayName()));
        }
        else
        {
            replaceAll(userItem, "%%ITEMLINK%%", it->getOutputFileName() + "/index." + m_extName);
            replaceAll(userItem, "%%ITEMTEXT%%", it->getDisplayName());
        }
        
        htmlBody += userItem;
    }
    
    std::string fileName = combinePath(m_output, "index." + m_extName);

    std::string html = m_resManager.getTemplate("listframe");
    
    replaceAll(html, "%%USERNAME%%", "");
    replaceAll(html, "%%TBODY%%", htmlBody);
    
    writeFile(fileName, html);
    
    m_options = orgOptions;
    if (m_exportContext->getNumberOfSessions() > 0)
    {
        m_exportContext->refreshExportTime();
        fileName = combinePath(m_output, WXEXP_DATA_FOLDER, WXEXP_DATA_FILE);
        writeFile(fileName, m_exportContext->serialize());
    }
    
    delete m_exportContext;
    m_exportContext = NULL;
    
    time_t endTime = 0;
    std::time(&endTime);
    int seconds = static_cast<int>(difftime(endTime, startTime));
    std::ostringstream stream;
    
    int minutes = seconds / 60;
    int hours = minutes / 60;
    stream << std::setfill('0') << std::setw(2) << hours << ':'
        << std::setfill('0') << std::setw(2) << (minutes % 60) << ':'
        << std::setfill('0') << std::setw(2) << (seconds % 60);
    
    m_logger->write(formatString(m_resManager.getLocaleString((m_cancelled ? "Cancelled in %s." : "Completed in %s.")), stream.str().c_str()));
    
    notifyComplete(m_cancelled);
    
    return true;
}

bool Exporter::exportUser(Friend& user, std::string& userOutputPath)
{
    std::string uidMd5 = user.getHash();
    
    std::string userBase = combinePath("Documents", uidMd5);
    // Use display name first, it it can't be created, use uid hash
    userOutputPath = user.getOutputFileName();
    std::string outputBase = combinePath(m_output, userOutputPath);
    if (!existsDirectory(outputBase))
    {
        if (!makeDirectory(outputBase))
        {
            userOutputPath = user.getHash();
            outputBase = combinePath(m_output, userOutputPath);
            if (!existsDirectory(outputBase))
            {
                if (!makeDirectory(outputBase))
                {
                    return false;
                }
            }
        }
    }
    
    if (!m_options.isTextMode())
    {
        std::string portraitPath = combinePath(outputBase, "Portrait");
        makeDirectory(portraitPath);
        std::string defaultPortrait = combinePath(portraitPath, "DefaultAvatar.png");
        copyFile(combinePath(m_workDir, "res", "DefaultAvatar.png"), defaultPortrait, true);
    }
    /*
    if (false && (m_options & SPO_IGNORE_EMOJI) == 0)
    {
        std::string emojiPath = combinePath(outputBase, "Emoji");
        makeDirectory(emojiPath);
    }
     */
    
    // if (m_options.isIncrementalExporting())
    {
        std::string path = combinePath(m_output, WXEXP_DATA_FOLDER, user.getUsrName());
        makeDirectory(path);
        
        m_exportContext->prepareUserDatabase(user, m_output);
    }
    
    m_logger->write(formatString(m_resManager.getLocaleString("Handling account: %s, WeChat Id: %s"), user.getDisplayName().c_str(), user.getUsrName().c_str()));
    
    m_logger->write(m_resManager.getLocaleString("Reading account info."));
    m_logger->write(m_resManager.getLocaleString("Reading chat info"));
    
    Friends friends;
    std::vector<Session> sessions;
    loadUserFriendsAndSessions(user, friends, sessions);
    
    m_logger->write(formatString(m_resManager.getLocaleString("%d chats found."), (int)(sessions.size())));
    
    Friend* myself = friends.getFriend(user.getHash());
    if (NULL == myself)
    {
        Friend& newUser = friends.addFriend(user.getHash());
        newUser = user;
        myself = &user;
    }
    
    std::string userBody;
    
    std::map<std::string, std::map<std::string, void *>>::const_iterator itUser = m_usersAndSessionsFilter.cend();
    if (!m_usersAndSessionsFilter.empty())
    {
        std::string nameForFilter = m_options.isFilteredByName() ? user.getDisplayName() : user.getUsrName();
        itUser = m_usersAndSessionsFilter.find(nameForFilter);
    }
    
    bool pdfOutput = (m_options.isPdfMode() && NULL != m_pdfConverter);
    if (pdfOutput)
    {
        m_pdfConverter->makeUserDirectory(userOutputPath);
    }
    
#ifdef USING_DOWNLOADER
    Downloader downloader(m_logger);
#else
    TaskManager taskManager(m_logger);
#endif
#ifndef NDEBUG
    m_logger->debug("UA: " + m_wechatInfo.buildUserAgent());
#endif
    
#ifdef USING_DOWNLOADER
    downloader.setUserAgent(m_wechatInfo.buildUserAgent());
#else
    taskManager.setUserAgent(m_wechatInfo.buildUserAgent());
#endif

    MessageParser msgParser(*m_iTunesDb, *m_iTunesDbShare, taskManager, friends, *myself, m_options, m_workDir, outputBase, m_resManager);
    
    if (!m_options.isTextMode())
    {
#ifndef NDEBUG
        m_logger->debug("Download avatar: *" + user.getPortrait() + "* => " + combinePath(outputBase, "Portrait", user.getLocalPortrait()));
#endif
        msgParser.copyPortraitIcon(NULL, user, combinePath(outputBase, "Portrait"));
        // downloader.addTask(user.getPortrait(), combinePath(outputBase, "Portrait", user.getLocalPortrait()), 0);
    }
    
    // Export Contacts
    // std::map<std::string, Friend>
    // WeChat Id/Display Name/Portrait URL
    // csv
    std::string csvContents;
    std::vector<const Friend *>friendList;
    friends.toArraySortedByDisplayName(friendList);
    std::string oneQuato = "\"";
    std::string twoQuatos = "\"\"";
    std::string twoQuatos2 = "\",\"";
    for (auto it = friendList.cbegin(); it != friendList.cend(); ++it)
    {
#ifdef NDEBUG   // Release
        if ((*it)->isSubscription() || (*it)->isChatroom())
        {
            continue;
        }

        std::string wechatId = (*it)->getWxName();
        std::string displayName = (*it)->getDisplayName();
        replaceAll(displayName, oneQuato, twoQuatos);
        csvContents += oneQuato + wechatId + twoQuatos2 + displayName + twoQuatos2 + (*it)->getPortrait() + twoQuatos2 + (*it)->buildTagDesc(m_tags) + "\"\r\n";
#else
        std::string wechatId = (*it)->getWxName();
        if (wechatId == (*it)->getUsrName())
        {
            wechatId.clear();
        }
        std::string displayName = (*it)->getDisplayName();
        replaceAll(displayName, oneQuato, twoQuatos);
        csvContents += oneQuato + (*it)->getUsrName() + twoQuatos2 + wechatId + twoQuatos2 + displayName + twoQuatos2 + (*it)->buildTagDesc(m_tags) + twoQuatos2 + (*it)->getPortrait() + "\"\r\n";
#endif
    }
    std::string contactPath = combinePath(m_output, userOutputPath + "_Contacts.csv");
    writeFile(contactPath, csvContents);
    
    std::string weChatIdFormat = m_resManager.getLocaleString(" (WeChat ID: %s)");

    std::set<std::string> sessionFileNames;
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        if (m_cancelled)
        {
            break;
        }
        
        if (!m_usersAndSessionsFilter.empty() && itUser != m_usersAndSessionsFilter.cend() && !(itUser->second.empty()))
        {
            std::map<std::string, void *>::const_iterator itSession = itUser->second.cend();
            const std::string& nameForFilter = m_options.isFilteredByName() ? it->getDisplayName() : it->getUsrName();
            if ((itSession = itUser->second.find(nameForFilter)) == itUser->second.cend())
            {
                continue;
            }
            
            it->setData(itSession->second);
        }
        
        int recordCount = it->getRecordCount();
        if (m_options.isIncrementalExporting())
        {
            int64_t maxMsgId = 0;
            m_exportContext->getMaxId(user.getUsrName(), it->getUsrName(), maxMsgId);
            if (maxMsgId > 0)
            {
                recordCount = SessionParser::calcNumberOfMessages(*it, maxMsgId);
            }
        }

		notifySessionStart(it->getUsrName(), it->getData(), recordCount);
        
        if (!buildFileNameForUser(*it, sessionFileNames))
        {
            m_logger->write(formatString(m_resManager.getLocaleString("Can't build directory name for chat: %s. Skip it."), it->getDisplayName().c_str()));
			notifySessionComplete(it->getUsrName(), it->getData(), m_cancelled);
            continue;
        }
        
        std::string sessionDisplayName = it->getDisplayName();
#ifndef NDEBUG
        m_logger->write(formatString(m_resManager.getLocaleString("%d/%d: Handling the chat with %s"), (std::distance(sessions.begin(), it) + 1), sessions.size(), sessionDisplayName.c_str()) + " uid:" + it->getUsrName());
#else
        m_logger->write(formatString(m_resManager.getLocaleString("%d/%d: Handling the chat with %s"), (std::distance(sessions.begin(), it) + 1), sessions.size(), sessionDisplayName.c_str()));
#endif
        if (!isSubscriptionIncluded() && it->isSubscription())
        {
            m_logger->write(formatString(m_resManager.getLocaleString("Skip subscription: %s"), sessionDisplayName.c_str()));
			notifySessionComplete(it->getUsrName(), it->getData(), m_cancelled);
            continue;
        }
        if (!m_options.isTextMode())
        {
            // Download avatar for session
            msgParser.copyPortraitIcon(&(*it), *it, combinePath(outputBase, "Portrait"));
        }
        int count = exportSession(*myself, msgParser, *it, userBase, outputBase);
        
        m_logger->write(formatString(m_resManager.getLocaleString("Succeeded handling %d messages."), count));

        if (count > 0)
        {
            std::string userItem = m_resManager.getTemplate("listitem");
            
            std::string userItemText = sessionDisplayName;
            if (!it->isChatroom())
            {
                std::string wxName = it->isWxNameEmpty() ? "" : it->getWxName();
                if (!wxName.empty() && userItemText != wxName)
                {
                    userItemText += formatString(weChatIdFormat, wxName.c_str());
                }
            }
            
            replaceAll(userItem, "%%ITEMPICPATH%%", "Portrait/" + it->getLocalPortrait());
            if (!m_options.isTextMode())
            {
                replaceAll(userItem, "%%ITEMLINK%%", encodeUrl(it->getOutputFileName()) + "/index." + m_extName);
                replaceAll(userItem, "%%ITEMTEXT%%", safeHTML(userItemText));
            }
            else
            {
                replaceAll(userItem, "%%ITEMLINK%%", it->getOutputFileName() + "." + m_extName);
                replaceAll(userItem, "%%ITEMTEXT%%", userItemText);
            }
            
            userBody += userItem;
        }

		notifySessionComplete(it->getUsrName(), it->getData(), m_cancelled);
        
        if (pdfOutput)
        {
            // std::string 
            std::string htmlFileName = combinePath(outputBase, it->getOutputFileName(), "index." + m_extName);
            if (existsFile(htmlFileName))
            {
                std::string pdfFileName = combinePath(m_output, "pdf", userOutputPath, it->getOutputFileName() + ".pdf");
                // taskManager.convertPdf(&(*it), htmlFileName, pdfFileName, m_pdfConverter);
                m_pdfConverter->convert(htmlFileName, pdfFileName);
            }
        }
    }

    std::string html = m_resManager.getTemplate("listframe");
    replaceAll(html, "%%USERNAME%%", " - " + user.getDisplayName());
    replaceAll(html, "%%TBODY%%", userBody);
    
    std::string fileName = combinePath(outputBase, "index." + m_extName);
    writeFile(fileName, html);

    size_t dlCount = 0;
    size_t prevDlCount = 0;
    if (m_cancelled)
    {
#ifdef USING_DOWNLOADER
        downloader.cancel();
#else
        taskManager.cancel();
#endif
    }
    else
    {
#ifdef USING_DOWNLOADER
        dlCount = downloader.getRunningCount();
        prevDlCount = dlCount;
        if (dlCount > 0)
        {
            m_logger->write("Waiting for tasks: " + std::to_string(dlCount));
        }
#else
        std::string queueDesc;
        dlCount = taskManager.getNumberOfQueue(queueDesc);
        prevDlCount = dlCount;
        if (dlCount > 0)
        {
            m_logger->write("Waiting for tasks: " + queueDesc);
        }
        taskManager.shutdown();
#endif
    }

    notifyTasksStart(user.getUsrName(), static_cast<uint32_t>(dlCount));
    
#ifdef USING_DOWNLOADER
    downloader.shutdown();
#else
    unsigned int timeout = m_cancelled ? 0 : 512;
    for (int idx = 1; ; ++idx)
    {
        if (taskManager.waitForCompltion(timeout))
        {
            break;
        }
        
        if (m_cancelled)
        {
            taskManager.cancel();
            timeout = 0;
        }
        else if ((idx % 2) == 0)
        {
            std::string queueDesc;
            size_t curDlCount = taskManager.getNumberOfQueue(queueDesc);
            if (curDlCount != prevDlCount)
            {
                notifyTasksProgress(user.getUsrName(), static_cast<uint32_t>(prevDlCount - curDlCount), static_cast<uint32_t>(dlCount));
                prevDlCount = curDlCount;
            }
        }
    }
#endif

    if (dlCount != prevDlCount)
    {
        notifyTasksProgress(user.getUsrName(), static_cast<uint32_t>(dlCount - prevDlCount), static_cast<uint32_t>(dlCount));
    }
    notifyTasksComplete(user.getUsrName(), m_cancelled);
    
#ifndef NDEBUG
    // m_logger->debug(formatString("Total Downloads: %d", downloader.getCount()));
    // m_logger->debug("Download Stats: " + downloader.getStats());
#endif

    return true;
}

bool Exporter::loadUserFriendsAndSessions(const Friend& user, Friends& friends, std::vector<Session>& sessions, bool detailedInfo/* = true*/)
{
    std::string uidMd5 = user.getHash();
    std::string userBase = combinePath("Documents", uidMd5);

    std::string wcdbPath = m_iTunesDb->findRealPath(combinePath(userBase, "DB", "WCDB_Contact.sqlite"));
    FriendsParser friendsParser(detailedInfo);
#ifndef NDEBUG
    friendsParser.setOutputPath(m_output);
#endif
    friendsParser.parseWcdb(wcdbPath, friends);

    m_logger->debug("WeChat Friends(" + std::to_string(friends.friends.size()) + ") for: " + user.getDisplayName() + " loaded.");
    
    m_tags.clear();
    if (detailedInfo)
    {
        friendsParser.parseFriendTags(m_iTunesDb, user.getHash(), m_tags);
    }

    SessionsParser sessionsParser(m_iTunesDb, m_iTunesDbShare, friends, m_wechatInfo.getCellDataVersion(), m_logger, detailedInfo);
    
    sessionsParser.parse(user, sessions);
 
    std::sort(sessions.begin(), sessions.end(), SessionLastMsgTimeCompare());
    
#ifndef NDEBUG
    int idx = 0;
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
    {
        if (it->isSubscription())
        {
            continue;
        }
        
        printf("%u : %s\t%s\r\n",it->getLastMessageTime(), it->getDisplayName().c_str(), it->getUsrName().c_str());
        idx ++;
        if (idx > 20)
        {
            // break;
        }
    }
    
    idx = 0;
#endif
    
    // m_logger->debug("WeChat Sessions for: " + user.getDisplayName() + " loaded.");
    return true;
}

bool Exporter::buildScriptFile(const std::string& fileName, std::vector<std::string>::const_iterator b, std::vector<std::string>::const_iterator e, const PageInfo& page) const
{
    std::string scripts = m_resManager.getTemplate("scripts");
    Json::Value jsonMsgs(Json::arrayValue);
    for (auto it = b; it != e; ++it)
    {
        jsonMsgs.append(*it);
    }
    Json::StreamWriterBuilder builder;
#ifndef NDEBUG
    builder["indentation"] = "\t";  // assume default for comments is None
    builder["emitUTF8"] = true;
#else
    builder["indentation"] = "";
#endif
    std::string moreMsgs = Json::writeString(builder, jsonMsgs);

    replaceAll(scripts, "%%JSON_DATA%%", moreMsgs);
    
    // std::string dataFileName = combinePath(dataPath, "msg-" + page.getFileName() + ".js");
    writeFile(fileName, scripts);
    
    return true;
}

int Exporter::exportSession(const Friend& user, const MessageParser& msgParser, const Session& session, const std::string& userBase, const std::string& outputBase)
{
    if (session.isDbFileEmpty())
    {
        return 0;
    }
    
    std::string sessionBasePath = combinePath(outputBase, session.getOutputFileName(), "Files");
    if (!m_options.isTextMode())
    {
        std::string portraitPath = combinePath(sessionBasePath, "Portrait");
        makeDirectory(portraitPath);
        // std::string defaultPortrait = combinePath(portraitPath, "DefaultAvatar.png");
        // copyFile(combinePath(m_workDir, "res", "DefaultAvatar.png"), defaultPortrait, true);
        
        makeDirectory(combinePath(sessionBasePath, "Emoji"));
    }

    std::vector<std::string> messages;
    std::vector<std::string>::size_type numberOfExportedMsgs = 0;
    if (session.getRecordCount() > 0)
    {
        messages.reserve(session.getRecordCount());
    }
    
    int64_t maxMsgId = 0;
    m_exportContext->getMaxId(user.getUsrName(), session.getUsrName(), maxMsgId);
    if (m_options.isIncrementalExporting())
    {
        m_exportContext->prepareSessionTable(session);
    }

#if !defined(NDEBUG) || defined(DBG_PERF)
    m_logger->debug("DB: " + session.getDbFile() + " Table: Chat_" + session.getHash());
#endif
    
    int numberOfMsgs = 0;
    SessionParser sessionParser(m_options);
    std::unique_ptr<SessionParser::MessageEnumerator> enumerator(sessionParser.buildMsgEnumerator(session, maxMsgId));
    std::vector<TemplateValues> tvs;
    std::unique_ptr<Pager> pager;
    uint16_t year = 0;
    uint16_t month = 0;
    size_t msgPosOfPage = 0;
    
    std::string dataPath = combinePath(outputBase, session.getOutputFileName(), "Files", "Data");
    makeDirectory(dataPath);
    
    if (m_options.isAsyncLoading())
    {
        if (m_options.isPagerByYear())
        {
            pager.reset((Pager *)(new YearPager()));
        }
        else if (m_options.isPagerByMonth())
        {
            pager.reset((Pager *)(new YearMonthPager()));
        }
        else
        {
            pager.reset((Pager *)(new NumberPager(PAGE_SIZE)));
        }
    }
    else
    {
        pager.reset(new Pager());
    }
    
    WXMSG msg;
#if !defined(NDEBUG) || defined(DBG_PERF)
    m_logger->debug("Start exporting session");
#endif
    while (enumerator->nextMessage(msg))
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        // m_logger->debug("Export msg: " + msg.msgId);
#endif
        if (m_options.isIncrementalExporting())
        {
            m_exportContext->insertMessage(session, msg);
        }
        
        if (msg.msgIdValue > maxMsgId)
        {
            maxMsgId = msg.msgIdValue;
        }
        
        tvs.clear();
        if (!msgParser.parse(msg, session, tvs))
        {
            if (hasDebugLogs() && msgParser.hasError())
            {
                m_logger->debug(msgParser.getError());
            }
        }

        exportMessage(session, tvs, messages);
        ++numberOfMsgs;

        pager->buildNewPage(&msg, messages);
        
        notifySessionProgress(session.getUsrName(), session.getData(), numberOfMsgs, session.getRecordCount());
#if !defined(NDEBUG) || defined(DBG_PERF)
        // m_logger->debug("Finish exporting msg: " + msg.msgId);
#endif
        if (m_cancelled)
        {
            break;
        }
    }

#if !defined(NDEBUG) || defined(DBG_PERF)
    m_logger->debug("Finish exporting session");
#endif

    if (maxMsgId > 0)
    {
        m_exportContext->setMaxId(user.getUsrName(), session.getUsrName(), maxMsgId);
    }
    
    if (numberOfMsgs == 0)  // no messages or no new messages for incremental exporting
    {
        return (maxMsgId > 0) ? 1 : numberOfMsgs;
    }
    
    std::string contentOfMembers;
    if (session.isChatroom())
    {
        // Export memebers
    }
    
    std::string rawMsgFileName = combinePath(m_output, WXEXP_DATA_FOLDER, session.getOwner()->getUsrName(), session.getUsrName() + ".dat");
    if (m_options.isIncrementalExporting())
    {
        m_logger->debug("Merge incremental messages.");
        mergeMessages(rawMsgFileName, messages);
    }
    
    m_logger->debug("Save messages for incremental exporting.");
    serializeMessages(rawMsgFileName, messages);
    
    // m_logger->debug("After serializeMessages.");

    if (numberOfMsgs > 0)
    {
        Json::Value jsonPages(Json::arrayValue);
        
        if (pager->hasPages())
        {
            numberOfExportedMsgs = 0;
            auto pages = pager->getPages();
            for (auto it = pages.cbegin(); it != pages.cend(); ++it)
            {
                auto b = messages.cbegin() + numberOfExportedMsgs;
                auto e = b + it->getCount();
                buildScriptFile(combinePath(dataPath, "msg-" + it->getFileName() + ".js"), b, e, *it);
                numberOfExportedMsgs += it->getCount();
                
                Json::Value jsonPage(Json::objectValue);
                jsonPage["numnberOfMsgs"] = it->getCount();
                jsonPage["text"] = it->getText();
                jsonPage["page"] = it->getPage() + 1;
                if (it->getYear() > 0)
                {
                    jsonPage["year"] = it->getYear();
                }
                if (it->getMonth() > 0)
                {
                    jsonPage["month"] = it->getMonth();
                }
                jsonPages.append(jsonPage);
            }
        }
        
        Json::StreamWriterBuilder builder;
#ifndef NDEBUG
        builder["indentation"] = "\t";  // assume default for comments is None
        builder["emitUTF8"] = true;
#else
        builder["indentation"] = "";
#endif
        std::string pageData = Json::writeString(builder, jsonPages);

        auto b = messages.cbegin();
        // No page for text mode
        auto e = (m_options.isTextMode() || m_options.isSyncLoading() || (messages.size() <= PAGE_SIZE)) ? messages.cend() : (b + PAGE_SIZE);
        
        // const size_t numberOfMessages = std::distance(e, messages.cend());
        const size_t numberOfMessages = numberOfMsgs;
        const size_t numberOfPages = (messages.size() + PAGE_SIZE - 1) / PAGE_SIZE;
        
#if USING_NEW_TEMPLATE
        std::map<std::string, std::string> values;
        
#ifndef NDEBUG
        values["%%USRNAME%%"] = user.getUsrName() + " - " + user.getHash();
        values["%%SESSION_USRNAME%%"] = session.getUsrName() + " - " + session.getHash();
#else
        values["%%USRNAME%%"] = "";
        values["%%SESSION_USRNAME%%"] = "";
#endif
        values["%%DISPLAYNAME%%"] = session.getDisplayName();
        values["%%WX_CHAT_HISTORY%%"] = m_resManager.getLocaleString("WeChat Chat History");
        values["%%ASYNC_LOADING_TYPE%%"] = m_options.getLoadingDataOnScroll() ? "onscroll" : "initial";
        
        if (m_options.isAsyncLoading())
        {
            if (m_options.hasPager())
            {
                values["%%ASYNC_LOADING_TYPE%%"] = "pager";
                if (m_options.isPagerByYear())
                {
                    values["%%PAGER_TYPE%%"] = "2";
                }
                else if (m_options.isPagerByMonth())
                {
                    values["%%PAGER_TYPE%%"] = "3";
                }
                else
                {
                    values["%%PAGER_TYPE%%"] = "1";
                }
            }
            else
            {
                values["%%ASYNC_LOADING_TYPE%%"] = "onscroll";
            }
        }
        else
        {
            values["%%ASYNC_LOADING_TYPE%%"] = "";
        }

        values["%%SIZE_OF_PAGE%%"] = std::to_string(PAGE_SIZE);
        values["%%NUMBER_OF_MSGS%%"] = std::to_string(numberOfMessages);
        values["%%NUMBER_OF_PAGES%%"] = std::to_string(numberOfPages);

        values["%%DATA_PATH%%"] = "Files/Data";
        values["%%PAGE_DATA%%"] = pageData;
        
        // m_logger->debug("Before join");
        if (!m_options.isHtmlMode() || m_options.isSyncLoading())
        {
            values["%%BODY%%"] = join(messages.cbegin(), messages.cend(), "");
        }
        // m_logger->debug("After join");
        values["%%HEADER_FILTER%%"] = m_options.isSupportingFilter() ? m_resManager.getTemplate("filter") : "";

        const std::string& html = m_resManager.buildFromTemplate("frame", values);
        m_logger->debug("After build html from template");
        
#else   // NOT USING_NEW_TEMPLATE
        
        std::string html = m_resManager.getTemplate("frame");
#if !defined(NDEBUG) || defined(DBG_PERF)
        if (html.empty())
        {
            m_logger->write("Template file is empty: frame");
        }
#endif
        
#ifndef NDEBUG
        replaceAll(html, "%%USRNAME%%", user.getUsrName() + " - " + user.getHash());
        replaceAll(html, "%%SESSION_USRNAME%%", session.getUsrName() + " - " + session.getHash());
#else
        replaceAll(html, "%%USRNAME%%", "");
        replaceAll(html, "%%SESSION_USRNAME%%", "");
#endif
        replaceAll(html, "%%DISPLAYNAME%%", session.getDisplayName());
        replaceAll(html, "%%WX_CHAT_HISTORY%%", m_resManager.getLocaleString("WeChat Chat History"));
        replaceAll(html, "%%ASYNC_LOADING_TYPE%%", m_options.getLoadingDataOnScroll() ? "onscroll" : "initial");
        
        replaceAll(html, "%%SIZE_OF_PAGE%%", std::to_string(PAGE_SIZE));
        replaceAll(html, "%%NUMBER_OF_MSGS%%", std::to_string(numberOfMessages));
        replaceAll(html, "%%NUMBER_OF_PAGES%%", std::to_string(numberOfPages));
        
        replaceAll(html, "%%DATA_PATH%%", "Files/Data");
        
        replaceAll(html, "%%PAGE_DATA%%", pageData);
        
        replaceAll(html, "%%BODY%%", join(b, e, ""));
        replaceAll(html, "%%HEADER_FILTER%%", m_options.isSupportingFilter() ? m_resManager.getTemplate("filter") : "");
        
#endif
        
        std::string fileName = combinePath(outputBase, session.getOutputFileName(), "index." + m_extName);
        if (!writeFile(fileName, html))
        {
            m_logger->write("Failed to write chat history file: " + fileName);
        }
        // m_logger->debug("Finish writing html file");
    }
    
    // m_logger->debug("Session exporting ends.");
    
    return numberOfMsgs;
}

bool Exporter::exportMessage(const Session& session, const std::vector<TemplateValues>& tvs, std::vector<std::string>& messages)
{
    std::string content;
    for (std::vector<TemplateValues>::const_iterator it = tvs.cbegin(); it != tvs.cend(); ++it)
    {
#if USING_NEW_TEMPLATE
        content.append(m_resManager.buildFromTemplate(it->getName(), it->getValues()));
#else
        content.append(buildContentFromTemplateValues(*it));
#endif
    }
    
    messages.push_back(content);
    return m_cancelled;
}

bool Exporter::exportPageToFile(const Friend& user, const Session& session, const std::vector<TemplateValues>& tvs, std::vector<std::string>& messages, const PageInfo& pageInfo, const std::string& outputBase)
{
    if (messages.empty())
    {
        return true;
    }
    
    std::string dataPath = combinePath(outputBase, session.getOutputFileName(), "Files", "Data");
    makeDirectory(dataPath);
    
    std::string rawMsgFileName = combinePath(m_output, WXEXP_DATA_FOLDER, session.getOwner()->getUsrName(), session.getUsrName() + ".dat");
    if (m_options.isIncrementalExporting())
    {
        m_logger->debug("Merge incremental messages.");
        mergeMessages(rawMsgFileName, messages);
    }
    
    m_logger->debug("Save messages for incremental exporting.");
    serializeMessages(rawMsgFileName, messages);
    
    // m_logger->debug("After serializeMessages.");

    // auto b = messages.cbegin();
    // No page for text mode
    // auto e = (((m_options & (SPO_TEXT_MODE | SPO_SYNC_LOADING)) != 0) || (messages.size() <= PAGE_SIZE)) ? messages.cend() : (b + PAGE_SIZE);
    
    // const size_t numberOfMessages = std::distance(e, messages.cend());
    // const size_t numberOfPages = (numberOfMessages + PAGE_SIZE - 1) / PAGE_SIZE;
    
#if USING_NEW_TEMPLATE
    std::map<std::string, std::string> values;
    
#ifndef NDEBUG
    values["%%USRNAME%%"] = user.getUsrName() + " - " + user.getHash();
    values["%%SESSION_USRNAME%%"] = session.getUsrName() + " - " + session.getHash();
#else
    values["%%USRNAME%%"] = "";
    values["%%SESSION_USRNAME%%"] = "";
#endif
    values["%%DISPLAYNAME%%"] = session.getDisplayName();
    values["%%WX_CHAT_HISTORY%%"] = m_resManager.getLocaleString("WeChat Chat History");
    values["%%ASYNC_LOADING_TYPE%%"] = m_options.getLoadingDataOnScroll() ? "onscroll" : "initial";
    
    values["%%SIZE_OF_PAGE%%"] = std::to_string(PAGE_SIZE);
    // values["%%NUMBER_OF_MSGS%%"] = std::to_string(numberOfMessages);
    // values["%%NUMBER_OF_PAGES%%"] = std::to_string(numberOfPages);

    values["%%DATA_PATH%%"] = "Files/Data";

    // m_logger->debug("Before join");
    values["%%BODY%%"] = join(messages.cbegin(), messages.cend(), "");
    // m_logger->debug("After join");
    values["%%HEADER_FILTER%%"] = m_options.isSupportingFilter() ? m_resManager.getTemplate("filter") : "";

    const std::string& html = m_resManager.buildFromTemplate("frame", values);
    m_logger->debug("After build html from template");
#else   // NOT USING_NEW_TEMPLATE
    std::string html = m_resManager.getTemplate("frame");
#if !defined(NDEBUG) || defined(DBG_PERF)
    if (html.empty())
    {
        m_logger->write("Template file is empty: frame");
    }
#endif
#ifndef NDEBUG
    replaceAll(html, "%%USRNAME%%", user.getUsrName() + " - " + user.getHash());
    replaceAll(html, "%%SESSION_USRNAME%%", session.getUsrName() + " - " + session.getHash());
#else
    replaceAll(html, "%%USRNAME%%", "");
    replaceAll(html, "%%SESSION_USRNAME%%", "");
#endif
    replaceAll(html, "%%DISPLAYNAME%%", session.getDisplayName());
    replaceAll(html, "%%WX_CHAT_HISTORY%%", m_resManager.getLocaleString("WeChat Chat History"));
    replaceAll(html, "%%ASYNC_LOADING_TYPE%%", m_options.getLoadingDataOnScroll() ? "onscroll" : "initial");
    
    replaceAll(html, "%%SIZE_OF_PAGE%%", std::to_string(PAGE_SIZE));
    // replaceAll(html, "%%NUMBER_OF_MSGS%%", std::to_string(numberOfMessages));
    // replaceAll(html, "%%NUMBER_OF_PAGES%%", std::to_string(numberOfPages));
    
    replaceAll(html, "%%DATA_PATH%%", "Files/Data");
    
    replaceAll(html, "%%BODY%%", join(messages.cbegin(), messages.cend(), ""));
    replaceAll(html, "%%HEADER_FILTER%%", m_options.isSupportingFilter() ? m_resManager.getTemplate("filter") : "");
    
#endif

    std::string outputFileName = "index";
    if (pageInfo.getPage() > 0)
    {
        outputFileName += "-" + std::to_string(pageInfo.getPage());
    }
    outputFileName += m_extName;
    
    std::string fullFileName = combinePath(outputBase, session. getOutputFileName(), outputFileName);
    if (!writeFile(fullFileName, html))
    {
        m_logger->write("Failed to write chat history file: " + fullFileName);
    }
    // m_logger->debug("Finish writing html file");
    
    // if ((m_options & SPO_SYNC_LOADING) == 0 && numberOfPages > 0)
    {
        // makeDirectory(dataPath);

        // for (size_t page = 0; page < numberOfPages; ++page)
        {
            // b = e;
            std::string scripts = m_resManager.getTemplate("scripts");
            // e = (page == (numberOfPages - 1)) ? messages.cend() : (b + PAGE_SIZE);
            Json::Value jsonMsgs(Json::arrayValue);
            for (auto it = messages.cbegin(); it != messages.cend(); ++it)
            {
                jsonMsgs.append(*it);
            }
            Json::StreamWriterBuilder builder;
#ifndef NDEBUG
            builder["indentation"] = "\t";  // assume default for comments is None
            builder["emitUTF8"] = true;
#else
            builder["indentation"] = "";
#endif
            std::string moreMsgs = Json::writeString(builder, jsonMsgs);

            replaceAll(scripts, "%%JSON_DATA%%", moreMsgs);
            
            fullFileName = combinePath(dataPath, "msg-" + std::to_string(pageInfo.getPage() + 1) + ".js");
            writeFile(fullFileName, scripts);
        }
    }
    
    return true;
}

void Exporter::serializeMessages(const std::string& fileName, const std::vector<std::string>& messages)
{
    File file;
    size_t byteWriten = 0;
    if (file.open(fileName, false))
    {
        uint32_t size = htonl(static_cast<uint32_t>(messages.size()));
        file.write(reinterpret_cast<const unsigned char *>(&size), sizeof(size), byteWriten);
        for (std::vector<std::string>::const_iterator it = messages.cbegin(); it != messages.cend(); ++it)
        {
            size = htonl(static_cast<uint32_t>(it->size()));
            file.write(reinterpret_cast<const unsigned char *>(&size), sizeof(size), byteWriten);
            // appendFile(fileName, reinterpret_cast<const unsigned char *>(&size), sizeof(size));
            file.write(reinterpret_cast<const unsigned char *>(it->c_str()), it->size(), byteWriten);
        }
        file.close();
    }
}

void Exporter::unserializeMessages(const std::string& fileName, std::vector<std::string>& messages)
{
    std::vector<unsigned char> data;
    if (!readFile(fileName, data))
    {
        messages.clear();
        return;
    }
    
    size_t dataSize = data.size();
    if (dataSize < sizeof(uint32_t))
    {
        return;
    }
    
    size_t offset = 0;
    uint32_t itemSize = 0;
    memcpy(&itemSize, &data[offset], sizeof(uint32_t));
    offset += sizeof(uint32_t);
    itemSize = ntohl(itemSize);
    
    messages.clear();
    messages.reserve(itemSize);
    
    uint32_t sizeOfString = 0;
    for (uint32_t idx = 0; idx < itemSize; ++idx)
    {
        if (offset + sizeof(uint32_t) > dataSize)
        {
            break;
        }
        memcpy(&sizeOfString, &data[offset], sizeof(uint32_t));
        offset += sizeof(uint32_t);
        sizeOfString = ntohl(sizeOfString);
        
        if (offset + sizeOfString > dataSize)
        {
            break;
        }
        
        messages.emplace_back(reinterpret_cast<const char *>(&data[offset]), sizeOfString);
        offset += sizeOfString;
    }
}

void Exporter::mergeMessages(const std::string& fileName, std::vector<std::string>& messages)
{
    std::vector<std::string> orgMessages;
    unserializeMessages(fileName, orgMessages);
    
    // std::string contents = readFile(fileName);
    if (!m_options.isDesc())
    {
        // ASC
        messages.swap(orgMessages);
    }
    
    messages.reserve(messages.size() + orgMessages.size());
    messages.insert(messages.cend(), orgMessages.cbegin(), orgMessages.cend());
}

bool Exporter::buildFileNameForUser(Friend& user, std::set<std::string>& existingFileNames)
{
    std::string names[] = {user.getDisplayName(), user.getUsrName(), user.getHash()};
    
    bool succeeded = false;
    for (int idx = 0; idx < 3; ++idx)
    {
        std::string outputFileName = removeInvalidCharsForFileName(names[idx]);
        if (isValidFileName(outputFileName))
        {
            if ( existingFileNames.find(outputFileName) != existingFileNames.cend())
            {
                int idx = 1;
                while (idx++)
                {
                    if (existingFileNames.find(outputFileName + "_" + std::to_string(idx)) == existingFileNames.cend())
                    {
                        outputFileName += "_" + std::to_string(idx);
                        break;
                    }
                }
            }
            user.setOutputFileName(outputFileName);
            existingFileNames.insert(outputFileName);
            succeeded = true;
            break;
        }
    }
    
    return succeeded;
}

void Exporter::releaseITunes()
{
    if (NULL != m_iTunesDb)
    {
        delete m_iTunesDb;
        m_iTunesDb = NULL;
    }
    if (NULL != m_iTunesDbShare)
    {
        delete m_iTunesDbShare;
        m_iTunesDbShare = NULL;
    }
}

bool Exporter::loadITunes(bool detailedInfo/* = true*/)
{
    releaseITunes();
    
#ifndef USING_DECODED_ITUNESBACKUP
    m_iTunesDb = new ITunesDb(m_backup, "Manifest.db");
#else
    m_iTunesDb = new DecodedWechatITunesDb(m_backup, "Manifest.db");
#endif
    if (!detailedInfo)
    {
        std::function<bool(const char*, int)> fn = std::bind(&Exporter::filterITunesFile, this, std::placeholders::_1, std::placeholders::_2);
        m_iTunesDb->setLoadingFilter(fn);
    }
    if (!m_iTunesDb->load("AppDomain-com.tencent.xin", !detailedInfo))
    {
        return false;
    }
#ifndef USING_DECODED_ITUNESBACKUP
    m_iTunesDbShare = new ITunesDb(m_backup, "Manifest.db");
#else
    m_iTunesDbShare = new DecodedWechatITunesDb(m_backup, "Manifest.db");
#endif
    
    if (!m_iTunesDbShare->load("AppDomainGroup-group.com.tencent.xin"))
    {
        // Optional
        // return false;
    }
    
    return true;
}

std::string Exporter::getITunesVersion() const
{
    return NULL != m_iTunesDb ? m_iTunesDb->getVersion() : "";
}

std::string Exporter::getIOSVersion() const
{
    return NULL != m_iTunesDb ? m_iTunesDb->getIOSVersion() : "";
}

std::string Exporter::getWechatVersion() const
{
    return m_wechatInfo.getVersion();
}

std::string Exporter::buildContentFromTemplateValues(const TemplateValues& tv) const
{
#if !defined(NDEBUG) && defined(SAMPLING_TMPL)
    std::string alignment = "";
#endif
    std::string content = m_resManager.getTemplate(tv.getName());
    for (TemplateValues::const_iterator it = tv.cbegin(); it != tv.cend(); ++it)
    {
        if (startsWith(it->first, "%"))
        {
            replaceAll(content, it->first, it->second);
        }
#if !defined(NDEBUG) && defined(SAMPLING_TMPL)
        if (it->first == "%%ALIGNMENT%%")
        {
            alignment = it->second;
        }
#endif
    }
    
    std::string::size_type pos = 0;

    while ((pos = content.find("%%", pos)) != std::string::npos)
    {
        std::string::size_type posEnd = content.find("%%", pos + 2);
        if (posEnd == std::string::npos)
        {
            break;
        }
        
        content.erase(pos, posEnd + 2 - pos);
    }
    
#if !defined(NDEBUG) && defined(SAMPLING_TMPL)
    std::string fileName = "sample_" + tv.getName() + alignment + ".html";
    writeFile(combinePath(m_output, "dbg", fileName), content);
#endif
    
    return content;
}

void Exporter::notifyStart()
{
    if (m_notifier)
    {
        m_notifier->onStart();
    }
}

void Exporter::notifyComplete(bool cancelled/* = false*/)
{
    if (m_notifier)
    {
        m_notifier->onComplete(cancelled);
    }
}

void Exporter::notifyProgress(uint32_t numberOfMessages, uint32_t numberOfTotalMessages)
{
    if (m_notifier)
    {
        m_notifier->onProgress(numberOfMessages, numberOfTotalMessages);
    }
}

void Exporter::notifySessionStart(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfTotalMessages)
{
    if (m_notifier)
    {
        m_notifier->onSessionStart(sessionUsrName, sessionData, numberOfTotalMessages);
    }
}

void Exporter::notifySessionComplete(const std::string& sessionUsrName, void * sessionData, bool cancelled/* = false*/)
{
    if (m_notifier)
    {
        m_notifier->onSessionComplete(sessionUsrName, sessionData, cancelled);
    }
}

void Exporter::notifySessionProgress(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfMessages, uint32_t numberOfTotalMessages)
{
    if (m_notifier)
    {
        m_notifier->onSessionProgress(sessionUsrName, sessionData, numberOfMessages, numberOfTotalMessages);
    }
}

void Exporter::notifyTasksStart(const std::string& usrName, uint32_t numberOfTotalTasks)
{
    if (m_notifier)
    {
        m_notifier->onTasksStart(usrName, numberOfTotalTasks);
    }
}

void Exporter::notifyTasksComplete(const std::string& usrName, bool cancelled/* = false*/)
{
    if (m_notifier)
    {
        m_notifier->onTasksComplete(usrName, cancelled);
    }
}

void Exporter::notifyTasksProgress(const std::string& usrName, uint32_t numberOfCompletedTasks, uint32_t numberOfTotalTasks)
{
    if (m_notifier)
    {
        m_notifier->onTasksProgress(usrName, numberOfCompletedTasks, numberOfTotalTasks);
    }
}

bool Exporter::filterITunesFile(const char *file, int flags) const
{
    if (startsWith(file, "Documents/MMappedKV/"))
    {
        return startsWith(file, "mmsetting", 20);
    }
    
    if (std::strncmp(file, "Documents/MapDocument/", 22) == 0 ||
        std::strncmp(file, "Library/WebKit/", 15) == 0)
    {
        return false;
    }
    
    const char *str = std::strchr(file, '/');
    if (str != NULL)
    {
        str = std::strchr(str + 1, '/');
        if (str != NULL)
        {
            if (std::strncmp(str, "/Audio/", 7) == 0 ||
                std::strncmp(str, "/Img/", 5) == 0 ||
                std::strncmp(str, "/OpenData/", 10) == 0 ||
                std::strncmp(str, "/Video/", 7) == 0 ||
                std::strncmp(str, "/appicon/", 9) == 0 ||
                std::strncmp(str, "/translate/", 11) == 0 ||
                std::strncmp(str, "/Brand/", 7) == 0 ||
                std::strncmp(str, "/Pattern_v3/", 12) == 0 ||
                std::strncmp(str, "/WCPay/", 7) == 0)
            {
                return false;
            }
        }
    }
    
    return true;
}
