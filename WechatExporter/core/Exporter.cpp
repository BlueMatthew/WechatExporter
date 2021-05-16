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
    m_loadingDataOnScroll = false; // disabled by default
    m_extName = "html";
    m_templatesName = "templates";
}

Exporter::~Exporter()
{
    releaseITunes();
    m_logger = NULL;
    m_notifier = NULL;
}

void Exporter::initializeExporter()
{
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

void Exporter::setTextMode(bool textMode/* = true*/)
{
    if (textMode)
        m_options |= SPO_TEXT_MODE;
    else
        m_options &= ~SPO_TEXT_MODE;
}

void Exporter::setPdfMode(bool pdfMode/* = true*/)
{
    setTextMode(!pdfMode);  // html mode
    if (pdfMode)
        m_options |= SPO_PDF_MODE;
    else
        m_options &= ~SPO_PDF_MODE;
}

void Exporter::setOrder(bool asc/* = true*/)
{
    if (asc)
        m_options &= ~SPO_DESC;
    else
        m_options |= SPO_DESC;
}

void Exporter::saveFilesInSessionFolder(bool flag/* = true*/)
{
    if (flag)
        m_options |= SPO_ICON_IN_SESSION;
    else
        m_options &= ~SPO_ICON_IN_SESSION;
}

void Exporter::setSyncLoading(bool syncLoading/* = true*/)
{
    if (syncLoading)
        m_options |= SPO_SYNC_LOADING;
    else
        m_options &= ~SPO_SYNC_LOADING;
}

void Exporter::setLoadingDataOnScroll(bool loadingDataOnScroll/* = true*/)
{
    m_loadingDataOnScroll = loadingDataOnScroll;
}

void Exporter::supportsFilter(bool supportsFilter/* = true*/)
{
    if (supportsFilter)
        m_options |= SPO_SUPPORT_FILTER;
    else
        m_options &= ~SPO_SUPPORT_FILTER;
}

void Exporter::setExtName(const std::string& extName)
{
    m_extName = extName;
}

void Exporter::setTemplatesName(const std::string& templatesName)
{
    m_templatesName = templatesName;
}

void Exporter::filterUsersAndSessions(const std::map<std::string, std::map<std::string, void *>>& usersAndSessions)
{
    m_usersAndSessionsFilter = usersAndSessions;
}

bool Exporter::run()
{
    if (isRunning() || m_thread.joinable())
    {
        m_logger->write(getLocaleString("Previous task has not completed."));
        
        return false;
    }

    if (!existsDirectory(m_output))
    {
        m_logger->write(formatString(getLocaleString("Can't access output directory: %s"), m_output.c_str()));
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
    
    loadStrings();
    
    if (!loadITunes(false))
    {
        m_logger->write(formatString(getLocaleString("Failed to parse the backup data of iTunes in the directory: %s"), m_backup.c_str()));
        notifyComplete();
        return false;
    }
    m_logger->debug("ITunes Database loaded.");
    
    WechatInfoParser wechatInfoParser(m_iTunesDb);
    if (wechatInfoParser.parse(m_wechatInfo))
    {
        m_logger->write(formatString(getLocaleString("iTunes Version: %s, iOS Version: %s, Wechat Version: %s"), m_iTunesDb->getVersion().c_str(), m_iTunesDb->getIOSVersion().c_str(), m_wechatInfo.getShortVersion().c_str()));
    }
    
    std::vector<Friend> users;
    LoginInfo2Parser loginInfo2Parser(m_iTunesDb);
    if (!loginInfo2Parser.parse(users))
    {
        return false;
    }

#ifndef NDEBUG
    m_logger->debug(loginInfo2Parser.getError());
#endif
    m_logger->debug("Wechat Users loaded.");
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
    
#ifndef NDEBUG
    makeDirectory(combinePath(m_output, "dbg"));
#endif
    loadStrings();
    loadTemplates();
    
    m_logger->write(formatString(getLocaleString("iTunes Backup: %s"), m_backup.c_str()));

    if (!loadITunes())
    {
        m_logger->write(formatString(getLocaleString("Failed to parse the backup data of iTunes in the directory: %s"), m_backup.c_str()));
        notifyComplete();
        return false;
    }
    m_logger->debug("ITunes Database loaded.");
    
    WechatInfoParser wechatInfoParser(m_iTunesDb);
    if (wechatInfoParser.parse(m_wechatInfo))
    {
        m_logger->write(formatString(getLocaleString("iTunes Version: %s, Wechat Version: %s"), m_iTunesDb->getVersion().c_str(), m_wechatInfo.getShortVersion().c_str()));
    }

    m_logger->write(getLocaleString("Finding Wechat accounts..."));

    std::vector<Friend> users;
    LoginInfo2Parser loginInfo2Parser(m_iTunesDb);
    if (!loginInfo2Parser.parse(users))
    {
        m_logger->write(getLocaleString("Failed to find Wechat account."));
        notifyComplete();
        return false;
    }
#ifndef NDEBUG
    m_logger->debug(loginInfo2Parser.getError());
#endif

    m_logger->write(formatString(getLocaleString("%d Wechat account(s) found."), (int)(users.size())));

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
            if (m_usersAndSessionsFilter.find(it->getUsrName()) == m_usersAndSessionsFilter.cend())
            {
                continue;
            }
        }
        
        if (!buildFileNameForUser(*it, userFileNames))
        {
            m_logger->write(formatString(getLocaleString("Can't build directory name for user: %s. Skip it."), it->getUsrName().c_str()));
            continue;
        }

        std::string userOutputPath;
        exportUser(*it, userOutputPath);
        
        std::string userItem = getTemplate("listitem");
        replaceAll(userItem, "%%ITEMPICPATH%%", userOutputPath + "/Portrait/" + it->getLocalPortrait());
        if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
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

    std::string html = getTemplate("listframe");
    replaceAll(html, "%%USERNAME%%", "");
    replaceAll(html, "%%TBODY%%", htmlBody);
    
    writeFile(fileName, html);
    
    time_t endTime = 0;
    std::time(&endTime);
    int seconds = static_cast<int>(difftime(endTime, startTime));
    std::ostringstream stream;
    
    int minutes = seconds / 60;
    int hours = minutes / 60;
    stream << std::setfill('0') << std::setw(2) << hours << ':'
        << std::setfill('0') << std::setw(2) << (minutes % 60) << ':'
        << std::setfill('0') << std::setw(2) << (seconds % 60);
    
    m_logger->write(formatString(getLocaleString((m_cancelled ? "Cancelled in %s." : "Completed in %s.")), stream.str().c_str()));
    
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
    
    if ((m_options & SPO_IGNORE_AVATAR) == 0)
    {
        std::string portraitPath = combinePath(outputBase, "Portrait");
        makeDirectory(portraitPath);
        std::string defaultPortrait = combinePath(portraitPath, "DefaultProfileHead@2x.png");
        copyFile(combinePath(m_workDir, "res", "DefaultProfileHead@2x.png"), defaultPortrait, true);
    }
    if ((m_options & SPO_ICON_IN_SESSION) == 0 && (m_options & SPO_IGNORE_EMOJI) == 0)
    {
        std::string emojiPath = combinePath(outputBase, "Emoji");
        makeDirectory(emojiPath);
    }
    
    m_logger->write(formatString(getLocaleString("Handling account: %s, Wechat Id: %s"), user.getDisplayName().c_str(), user.getUsrName().c_str()));
    
    m_logger->write(getLocaleString("Reading account info."));
    m_logger->write(getLocaleString("Reading chat info"));
    
    Friends friends;
    std::vector<Session> sessions;
    loadUserFriendsAndSessions(user, friends, sessions);
    
    m_logger->write(formatString(getLocaleString("%d chats found."), (int)(sessions.size())));
    
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
        itUser = m_usersAndSessionsFilter.find(user.getUsrName());
    }
    
    bool pdfOutput = (m_options & SPO_PDF_MODE && NULL != m_pdfConverter);
    // bool pdfOutput = (m_options & SPO_PDF_MODE && NULL != m_pdfConverter);
    if (pdfOutput)
    {
        m_pdfConverter->makeUserDirectory(userOutputPath);
    }
    
#ifdef USING_DOWNLOADER
    Downloader downloader(m_logger);
#else
    // TaskManager taskManager(pdfOutput, m_logger);
    TaskManager taskManager(false, m_logger);
#endif
#ifndef NDEBUG
    m_logger->debug("UA: " + m_wechatInfo.buildUserAgent());
#endif
    
#ifdef USING_DOWNLOADER
    downloader.setUserAgent(m_wechatInfo.buildUserAgent());
#else
    taskManager.setUserAgent(m_wechatInfo.buildUserAgent());
#endif
    
    std::function<std::string(const std::string&)> localeFunction = std::bind(&Exporter::getLocaleString, this, std::placeholders::_1);
    MessageParser msgParser(*m_iTunesDb, *m_iTunesDbShare, taskManager, friends, *myself, m_options, m_workDir, outputBase, localeFunction);
    
    if ((m_options & SPO_IGNORE_AVATAR) == 0)
    {
#ifndef NDEBUG
        m_logger->debug("Download avatar: *" + user.getPortrait() + "* => " + combinePath(outputBase, "Portrait", user.getLocalPortrait()));
#endif
        msgParser.copyPortraitIcon(NULL, user, combinePath(outputBase, "Portrait"));
        // downloader.addTask(user.getPortrait(), combinePath(outputBase, "Portrait", user.getLocalPortrait()), 0);
    }
    
    std::vector<Session *> pdfSessions;
    
    std::set<std::string> sessionFileNames;
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        if (m_cancelled)
        {
            break;
        }
        
        if (!m_usersAndSessionsFilter.empty())
        {
            std::map<std::string, void *>::const_iterator itSession = itUser->second.cend();
            if (itUser == m_usersAndSessionsFilter.cend() || (itSession = itUser->second.find(it->getUsrName())) == itUser->second.cend())
            {
                continue;
            }
            
            it->setData(itSession->second);
        }

		notifySessionStart(it->getUsrName(), it->getData(), it->getRecordCount());
        
        if (!buildFileNameForUser(*it, sessionFileNames))
        {
            m_logger->write(formatString(getLocaleString("Can't build directory name for chat: %s. Skip it."), it->getDisplayName().c_str()));
			notifySessionComplete(it->getUsrName(), it->getData(), m_cancelled);
            continue;
        }
        
        std::string sessionDisplayName = it->getDisplayName();
#ifndef NDEBUG
        m_logger->write(formatString(getLocaleString("%d/%d: Handling the chat with %s"), (std::distance(sessions.begin(), it) + 1), sessions.size(), sessionDisplayName.c_str()) + " uid:" + it->getUsrName());
#else
        m_logger->write(formatString(getLocaleString("%d/%d: Handling the chat with %s"), (std::distance(sessions.begin(), it) + 1), sessions.size(), sessionDisplayName.c_str()));
#endif
        if (it->isSubscription())
        {
            m_logger->write(formatString(getLocaleString("Skip subscription: %s"), sessionDisplayName.c_str()));
			notifySessionComplete(it->getUsrName(), it->getData(), m_cancelled);
            continue;
        }
        if ((m_options & SPO_IGNORE_AVATAR) == 0)
        {
            // Download avatar for session
            msgParser.copyPortraitIcon(&(*it), *it, combinePath(outputBase, "Portrait"));
        }
        int count = exportSession(*myself, msgParser, *it, userBase, outputBase);
        
        m_logger->write(formatString(getLocaleString("Succeeded handling %d messages."), count));

        if (count > 0)
        {
            std::string userItem = getTemplate("listitem");
            replaceAll(userItem, "%%ITEMPICPATH%%", "Portrait/" + it->getLocalPortrait());
            if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
            {
                replaceAll(userItem, "%%ITEMLINK%%", encodeUrl(it->getOutputFileName()) + "." + m_extName);
                replaceAll(userItem, "%%ITEMTEXT%%", safeHTML(sessionDisplayName));
            }
            else
            {
                replaceAll(userItem, "%%ITEMLINK%%", it->getOutputFileName() + "." + m_extName);
                replaceAll(userItem, "%%ITEMTEXT%%", sessionDisplayName);
            }
            
            userBody += userItem;
        }

		notifySessionComplete(it->getUsrName(), it->getData(), m_cancelled);
        
        if (pdfOutput)
        {
            // std::string 
            std::string htmlFileName = combinePath(outputBase, it->getOutputFileName() + "." + m_extName);
            if (existsFile(htmlFileName))
            {
                std::string pdfFileName = combinePath(m_output, "pdf", userOutputPath, it->getOutputFileName() + ".pdf");
                // taskManager.convertPdf(&(*it), htmlFileName, pdfFileName, m_pdfConverter);
                m_pdfConverter->convert(htmlFileName, pdfFileName);
            }
            // pdfSessions.push_back(&(*it));
        }
    }

    std::string html = getTemplate("listframe");
    replaceAll(html, "%%USERNAME%%", " - " + user.getDisplayName());
    replaceAll(html, "%%TBODY%%", userBody);
    
    std::string fileName = combinePath(outputBase, "index." + m_extName);
    writeFile(fileName, html);
    
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
        int dlCount = downloader.getRunningCount();
        if (dlCount > 0)
        {
            m_logger->write("Waiting for tasks: " + std::to_string(dlCount));
        }
#else
        std::string queueDesc;
        size_t dlCount = taskManager.getNumberOfQueue(queueDesc);
        if (dlCount > 0)
        {
            m_logger->write("Waiting for tasks: " + queueDesc);
        }
#endif
    }
    
#ifdef USING_DOWNLOADER
    downloader.shutdown();
#else
    taskManager.shutdown();
#endif
    
    if (!m_cancelled && !pdfSessions.empty())
    {
        // m_logger->write(getLocaleString("Convert to PDF..."));
        for (std::vector<Session *>::const_iterator it = pdfSessions.cbegin(); it != pdfSessions.cend(); ++it)
        {
            std::string htmlFileName = combinePath(outputBase, (*it)->getOutputFileName() + "." + m_extName);
            deleteFile(htmlFileName);
            deleteDirectory(combinePath(outputBase, (*it)->getOutputFileName() + "_files"));
            if (m_cancelled)
            {
                break;
            }
        }
    }
    
#ifndef NDEBUG
    // m_logger->debug(formatString("Total Downloads: %d", downloader.getCount()));
    // m_logger->debug("Download Stats: " + downloader.getStats());
#endif
    

    return true;
}

bool Exporter::loadUserFriendsAndSessions(const Friend& user, Friends& friends, std::vector<Session>& sessions, bool detailedInfo/* = true*/) const
{
    std::string uidMd5 = user.getHash();
    std::string userBase = combinePath("Documents", uidMd5);
    
    if (detailedInfo)
    {
        std::string wcdbPath = m_iTunesDb->findRealPath(combinePath(userBase, "DB", "WCDB_Contact.sqlite"));
        FriendsParser friendsParser(detailedInfo);
#ifndef NDEBUG
        friendsParser.setOutputPath(m_output);
#endif
        friendsParser.parseWcdb(wcdbPath, friends);

        m_logger->debug("Wechat Friends(" + std::to_string(friends.friends.size()) + ") for: " + user.getDisplayName() + " loaded.");
    }

    SessionsParser sessionsParser(m_iTunesDb, m_iTunesDbShare, m_wechatInfo.getCellDataVersion(), detailedInfo);
    
    sessionsParser.parse(user, sessions, friends);
 
    std::sort(sessions.begin(), sessions.end(), SessionLastMsgTimeCompare());
    
    m_logger->debug("Wechat Sessions for: " + user.getDisplayName() + " loaded.");
    return true;
}

int Exporter::exportSession(const Friend& user, const MessageParser& msgParser, const Session& session, const std::string& userBase, const std::string& outputBase)
{
    if (session.isDbFileEmpty())
    {
        return 0;
    }
    
    std::string sessionBasePath = combinePath(outputBase, session.getOutputFileName() + "_files");
    if ((m_options & SPO_IGNORE_AVATAR) == 0)
    {
        std::string portraitPath = combinePath(sessionBasePath, "Portrait");
        makeDirectory(portraitPath);
        // std::string defaultPortrait = combinePath(portraitPath, "DefaultProfileHead@2x.png");
        // copyFile(combinePath(m_workDir, "res", "DefaultProfileHead@2x.png"), defaultPortrait, true);
    }
    if ((m_options & SPO_IGNORE_EMOJI) == 0)
    {
        makeDirectory(combinePath(sessionBasePath, "Emoji"));
    }

    std::vector<std::string> messages;
    if (session.getRecordCount() > 0)
    {
        messages.reserve(session.getRecordCount());
    }
    
    int numberOfMsgs = 0;
    SessionParser sessionParser(m_options);
    std::unique_ptr<SessionParser::MessageEnumerator> enumerator(sessionParser.buildMsgEnumerator(session));
    std::vector<TemplateValues> tvs;
    WXMSG msg;
    while (enumerator->nextMessage(msg))
    {
        tvs.clear();
        msgParser.parse(msg, session, tvs);
        exportMessage(session, tvs, messages);
        ++numberOfMsgs;
        
        notifySessionProgress(session.getUsrName(), session.getData(), numberOfMsgs, session.getRecordCount());
        if (m_cancelled)
        {
            break;
        }
    }

    if (numberOfMsgs > 0 && !messages.empty())
    {
#ifndef NDEBUG
        const size_t pageSize = 500;
#else
        const size_t pageSize = 1000;
#endif
        auto b = messages.cbegin();
        // No page for text mode
        auto e = (((m_options & (SPO_TEXT_MODE | SPO_SYNC_LOADING)) != 0) || (messages.size() <= pageSize)) ? messages.cend() : (b + pageSize);
        std::string moreMsgs = ((m_options & (SPO_TEXT_MODE | SPO_SYNC_LOADING)) != 0) ? "[]" : "[]"; // [] = empty json array
        if (e != messages.cend())
        {
            Json::Value jsonMsgs(Json::arrayValue);
            for (auto it = e; it != messages.cend(); ++it)
            {
                jsonMsgs.append(*it);
            }
            Json::FastWriter writer;
            moreMsgs = writer.write(jsonMsgs);
        }

        std::string scripts = (m_options & SPO_SYNC_LOADING) || (messages.size() <= pageSize) ? "" : getTemplate("scripts");

        std::string html = getTemplate("frame");
#ifndef NDEBUG
        replaceAll(html, "%%USRNAME%%", user.getUsrName() + " - " + user.getHash());
        replaceAll(html, "%%SESSION_USRNAME%%", session.getUsrName() + " - " + session.getHash());
#else
        replaceAll(html, "%%USRNAME%%", "");
        replaceAll(html, "%%SESSION_USRNAME%%", "");
#endif
        replaceAll(html, "%%DISPLAYNAME%%", session.getDisplayName());
        replaceAll(html, "%%JSONDATA%%", moreMsgs);
        replaceAll(html, "%%ASYNCLOADINGTYPE%%", m_loadingDataOnScroll ? "onscroll" : "initial");
        
        replaceAll(html, "%%BODY%%", join(b, e, ""));
        
        replaceAll(html, "%%LOADING_SCRIPTS%%", scripts);
        
        replaceAll(html, "%%HEADER_FILTER%%", (m_options & SPO_SUPPORT_FILTER) ? getTemplate("filter") : "");
        
        std::string fileName = combinePath(outputBase, session.getOutputFileName() + "." + m_extName);
        writeFile(fileName, html);
    }
    
    return numberOfMsgs;
}

bool Exporter::exportMessage(const Session& session, const std::vector<TemplateValues>& tvs, std::vector<std::string>& messages)
{
    std::string content;
    for (std::vector<TemplateValues>::const_iterator it = tvs.cbegin(); it != tvs.cend(); ++it)
    {
        content.append(buildContentFromTemplateValues(*it));
    }
    
    messages.push_back(content);
    return m_cancelled;
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

bool Exporter::fillSession(Session& session, const Friends& friends) const
{
    if (session.isDisplayNameEmpty())
    {
        const Friend* f = friends.getFriend(session.getHash());
        if (NULL != f && !f->isDisplayNameEmpty())
        {
            session.setDisplayName(f->getDisplayName());
        }
    }

    return true;
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
    
    m_iTunesDb = new ITunesDb(m_backup, "Manifest.db");
    if (!detailedInfo)
    {
        std::function<bool(const char*, int)> fn = std::bind(&Exporter::filterITunesFile, this, std::placeholders::_1, std::placeholders::_2);
        m_iTunesDb->setLoadingFilter(fn);
    }
    if (!m_iTunesDb->load("AppDomain-com.tencent.xin", !detailedInfo))
    {
        return false;
    }
    m_iTunesDbShare = new ITunesDb(m_backup, "Manifest.db");
    
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

bool Exporter::loadTemplates()
{
    const char* names[] = {"frame", "msg", "video", "notice", "system", "audio", "image", "card", "emoji", "plainshare", "share", "thumb", "listframe", "listitem", "scripts", "filter", "refermsg", "channels"};
    for (int idx = 0; idx < sizeof(names) / sizeof(const char*); idx++)
    {
        std::string name = names[idx];
        std::string path = combinePath(m_workDir, "res", m_templatesName, name + ".html");
        m_templates[name] = readFile(path);
    }
    return true;
}

bool Exporter::loadStrings()
{
    m_localeStrings.clear();

    std::string path = combinePath(m_workDir, "res", "locale.txt");

    Json::Reader reader;
    Json::Value value;
    if (reader.parse(readFile(path), value))
    {
        int sz = value.size();
        for (int idx = 0; idx < sz; ++idx)
        {
            std::string k = value[idx]["key"].asString();
            std::string v = value[idx]["value"].asString();
            if (m_localeStrings.find(k) != m_localeStrings.cend())
            {
                // return false;
            }
            m_localeStrings[k] = v;
        }
    }

    return true;
}

std::string Exporter::getTemplate(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = m_templates.find(key);
    return (it == m_templates.cend()) ? "" : it->second;
}

std::string Exporter::getLocaleString(const std::string& key) const
{
    // std::string value = key;
    std::map<std::string, std::string>::const_iterator it = m_localeStrings.find(key);
    return it == m_localeStrings.cend() ? key : it->second;
}

std::string Exporter::buildContentFromTemplateValues(const TemplateValues& tv) const
{
#if !defined(NDEBUG) && defined(SAMPLING_TMPL)
    std::string alignment = "";
#endif
    std::string content = getTemplate(tv.getName());
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
