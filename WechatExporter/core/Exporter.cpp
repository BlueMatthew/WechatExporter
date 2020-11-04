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
    DownloadPool& downloadPool;
    std::string& userRoot;
    
    FriendDownloadHandler(DownloadPool& dlPool, std::string& usrRoot) : downloadPool(dlPool), userRoot(usrRoot)
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
}

Exporter::~Exporter()
{
    if (NULL != m_iTunesDb)
    {
        delete m_iTunesDb;
        m_iTunesDb = NULL;
    }
}

void Exporter::setNotifier(ExportNotifier *notifier)
{
	m_notifier = notifier;
}
bool Exporter::isRunning() const
{
	return m_running;
}

void Exporter::waitForComplition()
{
	if (!isRunning())
	{
		return;
	}

	m_thread.join();
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
		m_logger->write(stringWithFormat(getLocaleString("Can't access output directory: %s"), m_output.c_str()));
        return false;
    }
    
	m_running = true;

    std::thread th(&Exporter::runImpl, this);
	m_thread.swap(th);

	return true;
}

bool Exporter::runImpl()
{
    
    notifyStart();
    
	if (NULL != m_iTunesDb)
	{
		delete m_iTunesDb;
	}
	m_iTunesDb = new ITunesDb(m_backup, "Manifest.db");

	if (!m_iTunesDb->load("AppDomain-com.tencent.xin"))
	{
		m_logger->write(stringWithFormat(getLocaleString("Failed to parse the backup data of iTunes in the directory: %s"), m_backup.c_str()));
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

	m_logger->write(stringWithFormat(getLocaleString("%d Wechat account(s) found."), (int)(users.size())));

    std::string userBody;
    
    DownloadPool downloadPool;
	for (std::vector<Friend>::iterator it = users.begin(); it != users.end(); ++it)
	{
#ifndef NDEBUG
        if (it->getUsrName() != "wxid_2gix66ls0aq611")
        {
            continue;
        }
#endif
		fillUser(*it);
		exportUser(*it, downloadPool);
        
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
    if (m_shell->openOutputFile(htmlFile, fileName, std::ios::out | std::ios::binary | std::ios::trunc))
    {
        htmlFile.write(html.c_str(), html.size());
        htmlFile.close();
    }

	downloadPool.finishAndWaitForExit();

    m_logger->write(getLocaleString("Finished."));
    
    notifyComplete();

	return true;
}

bool Exporter::exportUser(Friend& user, DownloadPool& downloadPool)
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
    
	m_logger->write(stringWithFormat(getLocaleString("Handling account: %s"), user.DisplayName().c_str()));
	m_logger->write(getLocaleString("Reading account info."));
    // Friend myself;
    
    std::string wcdbPath = m_iTunesDb->findRealPath(combinePath(userBase, "DB", "WCDB_Contact.sqlite"));
    
    Friends friends;
    
    FriendsParser friendsParser;
    friendsParser.parseWcdb(wcdbPath, friends);
    
	m_logger->write(getLocaleString("Reading chat info"));
    SessionsParser sessionsParser(m_iTunesDb, m_shell);
    std::vector<Session> sessions;
    
    sessionsParser.parse(userBase, sessions);
 
	m_logger->write(stringWithFormat(getLocaleString("%d chats found."), (int)(sessions.size())));
    std::sort(sessions.begin(), sessions.end(), SessionLastMsgTimeCompare());
    
    Friend* myself = friends.getFriend(user.getUidHash());
    if (NULL == myself)
    {
		Friend& newUser = friends.addFriend(user.getUidHash());
		newUser = user;
        myself = &user;
    }
    
    friends.handleFriend(FriendDownloadHandler(downloadPool, portraitPath));
    
    // downloadPool.addTask(user.getPortraitUrl(), combinePath(portraitPath, user.getLocalPortrait()));
    
    std::string userBody;

	m_logger->write(stringWithFormat(getLocaleString("Wechat Id: %s, Nick Name: %s"), myself->getUsrName().c_str(), myself->DisplayName().c_str()));

	SessionParser sessionParser(*myself, friends, *m_iTunesDb, *m_shell, *m_logger, m_templates, m_localeStrings, downloadPool);
    
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
#ifndef NDEBUG
        if (it->UsrName != "5424313692@chatroom")
        {
            // continue;
        }
        if (!it->DisplayName.empty())
        {
            // continue;
        }
        
        if (!it->dbFile.empty())
        {
            ;
        }
#endif
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

		exportSession(*myself, sessionParser, *it, userBase, outputBase);
        
        std::string userItem = getTemplate("listitem");
        userItem = replace_all(userItem, "%%ITEMPICPATH%%", it->Portrait);
        userItem = replace_all(userItem, "%%ITEMLINK%%", encodeUrl(it->UsrName) + ".html");
        std::string displayName = it->DisplayName;
        if (displayName.empty()) displayName = it->UsrName;
        userItem = replace_all(userItem, "%%ITEMTEXT%%", safeHTML(displayName));
        
        userBody += userItem;
    }
    
    std::string fileName = combinePath(outputBase, "index.html");

    std::string html = getTemplate("listframe");
    html = replace_all(html, "%%USERNAME%%", " - " + user.DisplayName());
    html = replace_all(html, "%%TBODY%%", userBody);
    
    std::ofstream htmlFile;
    if (m_shell->openOutputFile(htmlFile, fileName, std::ios::out | std::ios::binary | std::ios::trunc))
    {
        htmlFile.write(html.c_str(), html.size());
        htmlFile.close();
    }

    return true;
}

bool Exporter::exportSession(Friend& user, const SessionParser& sessionParser, const Session& session, const std::string& userBase, const std::string& outputBase)
{
	if (session.dbFile.empty())
	{
		return false;
	}
	// var hash = chat;
	std::string displayName = session.DisplayName;
	
	
	m_logger->write(stringWithFormat(getLocaleString("Handling the chat with %s"), displayName.c_str()));

	int count = sessionParser.parse(userBase, outputBase, session, user);
	if (count > 0)
	{
		m_logger->write(stringWithFormat(getLocaleString("Succeeded handling %d messages."), count));

		// chatList.Add(new WeChatInterface.DisplayItem() { pic = "Portrait/" + (friend != null ? friend.FindPortrait() : "DefaultProfileHead@2x.png"), text = displayname, link = id + ".html", lastMessageTime = lastMsgTime });
	}
	// else logger.AddLog("失败");
	// emojidown.UnionWith(_emojidown);

	return true;
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
