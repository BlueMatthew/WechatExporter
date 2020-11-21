//
//  Exporter.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Exporter.h"
#include <json/json.h>
#include "WechatParser.h"

struct FriendDownloadHandler
{
    Downloader& downloadPool;
    std::string& userRoot;
    
    FriendDownloadHandler(Downloader& dlPool, std::string& usrRoot) : downloadPool(dlPool), userRoot(usrRoot)
    {
    }
    
    void operator()(const Friend& f)
    {
        std::string url = f.getPortraitUrl();
        if (!url.empty())
        {
            downloadPool.addTask(url, combinePath(userRoot, f.getLocalPortrait()));
        }
    }
};

Exporter::Exporter(const std::string& workDir, const std::string& backup, const std::string& output, Shell* shell, Logger* logger)
{
	m_running = false;
    m_iTunesDb = NULL;
    m_workDir = workDir;
    m_backup = backup;
    m_output = output;
    m_shell = shell;
    m_logger = logger;
    m_notifier = NULL;
    m_cancelled = false;
    m_options = 0;
}

Exporter::~Exporter()
{
    if (NULL != m_iTunesDb)
    {
        delete m_iTunesDb;
        m_iTunesDb = NULL;
    }
    m_shell = NULL;
    m_logger = NULL;
    m_notifier = NULL;
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

void Exporter::ignoreAudio(bool ignoreAudio/* = true*/)
{
    if (ignoreAudio)
        m_options |= SPO_IGNORE_AUDIO;
    else
        m_options &= ~SPO_IGNORE_AUDIO;
}

void Exporter::setOrder(bool asc/* = true*/)
{
    if (asc)
        m_options &= ~SPO_DESC;
    else
        m_options |= SPO_DESC;
}


bool Exporter::run()
{
	if (isRunning() || m_thread.joinable())
	{
		m_logger->write(getLocaleString("Previous task has not completed."));
        
		return false;
	}

    if (!m_shell->existsDirectory(m_output))
    {
		m_logger->write(formatString(getLocaleString("Can't access output directory: %s"), m_output.c_str()));
        return false;
    }
    
	m_running = true;

    std::thread th(&Exporter::runImpl, this);
	m_thread.swap(th);

	return true;
}

bool Exporter::runImpl()
{
    time_t startTime;
    std::time(&startTime);
    notifyStart();
    
	if (NULL != m_iTunesDb)
	{
		delete m_iTunesDb;
	}
	m_iTunesDb = new ITunesDb(m_backup, "Manifest.db");

	if (!m_iTunesDb->load("AppDomain-com.tencent.xin"))
	{
		m_logger->write(formatString(getLocaleString("Failed to parse the backup data of iTunes in the directory: %s"), m_backup.c_str()));
        notifyComplete();
		return false;
	}

	loadStrings();
	loadTemplates();

	m_logger->write(getLocaleString("Finding Wechat accounts..."));

	std::vector<Friend> users;
	LoginInfo2Parser loginInfo2Parser(m_iTunesDb);
	if (!loginInfo2Parser.parse(users))
	{
		m_logger->write(getLocaleString("Failed to find Wechat account."));
        notifyComplete();
		return false;
	}

	m_logger->write(formatString(getLocaleString("%d Wechat account(s) found."), (int)(users.size())));

    std::string userBody;
    
    Downloader downloader;
	for (std::vector<Friend>::iterator it = users.begin(); it != users.end(); ++it)
	{
        if (m_cancelled)
        {
            break;
        }
		fillUser(*it);
		exportUser(*it, downloader);
        
        std::string userItem = getTemplate("listitem");
        userItem = replace_all(userItem, "%%ITEMPICPATH%%", it->outputFileName + "/Portrait/" + it->getLocalPortrait());
        userItem = replace_all(userItem, "%%ITEMLINK%%", encodeUrl(it->outputFileName) + "/index.html");
        userItem = replace_all(userItem, "%%ITEMTEXT%%", safeHTML(it->DisplayName()));
        
        userBody += userItem;
	}
    
    std::string fileName = combinePath(m_output, "index.html");

    std::string html = getTemplate("listframe");
    html = replace_all(html, "%%USERNAME%%", "");
    html = replace_all(html, "%%TBODY%%", userBody);
    
    std::ofstream htmlFile;
    if (m_shell->openOutputFile(htmlFile, fileName, std::ios::out | std::ios::in | std::ios::binary | std::ios::trunc))
    {
        htmlFile.write(html.c_str(), html.size());
        htmlFile.close();
    }
    
    if (m_cancelled)
    {
        downloader.cancel();
    }
    else
    {
        int dlCount = downloader.getRunningCount();
        if (dlCount > 0)
        {
            m_logger->write(formatString(getLocaleString("Waiting for images(%d) downloading."), dlCount));
        }
    }

	downloader.finishAndWaitForExit();

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

bool Exporter::exportUser(Friend& user, Downloader& downloader)
{
    std::string uidMd5 = user.getUidHash();
    
    std::string userBase = combinePath("Documents", uidMd5);
	// Use display name first, it it can't be created, use uid hash
	std::string outputBase = combinePath(m_output, user.outputFileName);
	if (!m_shell->existsDirectory(outputBase))
	{
		if (!m_shell->makeDirectory(outputBase))
		{
			outputBase = combinePath(m_output, user.getUidHash());
			if (!m_shell->existsDirectory(outputBase))
			{
				if (!m_shell->makeDirectory(outputBase))
				{
					return false;
				}
			}
		}
	}
    
    std::string portraitPath = combinePath(outputBase, "Portrait");
    m_shell->makeDirectory(combinePath(outputBase, "Emoji"));
    m_shell->makeDirectory(portraitPath);
    
	m_logger->write(formatString(getLocaleString("Handling account: %s, Wechat Id: %s"), user.DisplayName().c_str(), user.getUsrName().c_str()));
    
	m_logger->write(getLocaleString("Reading account info."));
    
    std::string wcdbPath = m_iTunesDb->findRealPath(combinePath(userBase, "DB", "WCDB_Contact.sqlite"));
    
    Friends friends;
    
    FriendsParser friendsParser;
    friendsParser.parseWcdb(wcdbPath, friends);
    
	m_logger->write(getLocaleString("Reading chat info"));
    SessionsParser sessionsParser(m_iTunesDb, m_shell);
    std::vector<Session> sessions;
    
    sessionsParser.parse(userBase, sessions, friends);
 
	m_logger->write(formatString(getLocaleString("%d chats found."), (int)(sessions.size())));
    std::sort(sessions.begin(), sessions.end(), SessionLastMsgTimeCompare());
    
    Friend* myself = friends.getFriend(user.getUidHash());
    if (NULL == myself)
    {
		Friend& newUser = friends.addFriend(user.getUidHash());
		newUser = user;
        myself = &user;
    }
    
    friends.handleFriend(FriendDownloadHandler(downloader, portraitPath));
    
    std::string userBody;

	SessionParser sessionParser(*myself, friends, *m_iTunesDb, *m_shell, m_templates, m_localeStrings, downloader, m_cancelled);
    
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        if (m_cancelled)
        {
            break;
        }
        /*
        if (isValidFileName(it->DisplayName))
        {
            it->outputFileName = it->DisplayName;
        }
        else if (isValidFileName(it->UsrName))
        {
            it->outputFileName = it->UsrName;
        }
         */
#ifndef NDEBUG
        m_logger->write(formatString(getLocaleString("%d/%d: Handling the chat with %s"), (std::distance(sessions.begin(), it) + 1), sessions.size(), it->DisplayName.c_str()) + " uid:" + it->UsrName);
#else
        m_logger->write(formatString(getLocaleString("%d/%d: Handling the chat with %s"), (std::distance(sessions.begin(), it) + 1), sessions.size(), it->DisplayName.c_str()));
#endif
        if (it->isSubscription())
        {
            m_logger->write(formatString(getLocaleString("Skip subscription: %s"), it->DisplayName.c_str()));
            continue;
        }
        
		int count = exportSession(*myself, sessionParser, *it, userBase, outputBase);
        
        m_logger->write(formatString(getLocaleString("Succeeded handling %d messages."), count));
        
        if (count > 0)
        {
            std::string userItem = getTemplate("listitem");
            userItem = replace_all(userItem, "%%ITEMPICPATH%%", it->Portrait);
            userItem = replace_all(userItem, "%%ITEMLINK%%", encodeUrl(it->UsrName) + ".html");
            std::string displayName = it->DisplayName;
            if (displayName.empty()) displayName = it->UsrName;
            userItem = replace_all(userItem, "%%ITEMTEXT%%", safeHTML(displayName));
            
            userBody += userItem;
        }
    }
    
    std::string fileName = combinePath(outputBase, "index.html");

    std::string html = getTemplate("listframe");
    html = replace_all(html, "%%USERNAME%%", " - " + user.DisplayName());
    html = replace_all(html, "%%TBODY%%", userBody);
    
    std::ofstream htmlFile;
    if (m_shell->openOutputFile(htmlFile, fileName, std::ios::out | std::ios::in | std::ios::binary | std::ios::trunc))
    {
        htmlFile.write(html.c_str(), html.size());
        htmlFile.close();
    }

    return true;
}

int Exporter::exportSession(Friend& user, const SessionParser& sessionParser, const Session& session, const std::string& userBase, const std::string& outputBase)
{
	if (session.dbFile.empty())
	{
		return false;
	}
    std::string contents;
	int count = sessionParser.parse(userBase, outputBase, session, contents);
	if (count > 0)
	{
        std::string fileName = combinePath(outputBase, session.UsrName + ".html");

        std::string html = getTemplate("frame");
        html = replace_all(html, "%%DISPLAYNAME%%", session.DisplayName);
        html = replace_all(html, "%%BODY%%", contents);
        
        std::ofstream htmlFile;
        if (m_shell->openOutputFile(htmlFile, fileName, std::ios::out | std::ios::binary | std::ios::trunc))
        {
            htmlFile.write(html.c_str(), html.size());
            htmlFile.close();
        }
	}
    
	return count;
}

bool Exporter::fillUser(Friend& user)
{
    std::string path = combinePath("Documents", "MMappedKV", "mmsetting.archive." + user.getUsrName());
    std::string realPath = m_iTunesDb->findRealPath(path);
    
    if (realPath.empty())
    {
        return false;
    }
    
    MMKVParser parser(realPath);
    
    user.Portrait = parser.findValue("headimgurl");
    user.PortraitHD = parser.findValue("headhdimgurl");
    
    if (isValidFileName(user.DisplayName()))
    {
        user.outputFileName = user.DisplayName();
    }
    else if (isValidFileName(user.getUsrName()))
    {
        user.outputFileName = user.getUsrName();
    }
    
    return true;
}

bool Exporter::fillSession(Session& session, const Friends& friends) const
{
	if (session.DisplayName.empty())
	{
		const Friend* f = friends.getFriend(session.Hash);
		if (NULL != f)
		{
			session.DisplayName = f->DisplayName();
		}
	}

	return true;
}

bool Exporter::loadTemplates()
{
    const char* names[] = {"frame", "msg", "video", "notice", "system", "audio", "image", "card", "emoji", "share", "thumb", "listframe", "listitem"};
    for (int idx = 0; idx < sizeof(names) / sizeof(const char*); idx++)
    {
        std::string name = names[idx];
        std::string path = combinePath(m_workDir, "res", "templates", name + ".html");
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

std::string Exporter::getLocaleString(std::string key) const
{
	// std::string value = key;
	std::map<std::string, std::string>::const_iterator it = m_localeStrings.find(key);
	return it == m_localeStrings.cend() ? key : it->second;
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

void Exporter::notifyProgress(double progress)
{
    if (m_notifier)
    {
        m_notifier->onProgress(progress);
    }
}
