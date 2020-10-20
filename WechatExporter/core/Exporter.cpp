//
//  Exporter.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Exporter.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "WechatParser.h"

Exporter::Exporter(const std::string& workDir, const std::string& backup, const std::string& output, Shell* shell, Logger* logger)
{
    m_iTunesDb = NULL;
    m_workDir = workDir;
    m_backup = backup;
    m_output = output;
    m_shell = shell;
    m_logger = logger;
}

Exporter::~Exporter()
{
    if (NULL != m_iTunesDb)
    {
        delete m_iTunesDb;
        m_iTunesDb = NULL;
    }
}

bool Exporter::run()
{
    struct stat info;
    if( stat(m_output.c_str(), &info) != 0 )
    {
        m_logger->write("不能访问输出目录：" + m_output);
        return false;
    }
    else if((info.st_mode & S_IFDIR) == 0)  // S_ISDIR() doesn't exist on my windows
    {
        m_logger->write("请选择目录：" + m_output);
    }
    
    m_iTunesDb = new ITunesDb(m_backup, "Manifest.db");
    
    if (!m_iTunesDb->load("AppDomain-com.tencent.xin"))
    {
        m_logger->write("iTunes备份目录解析出错：" + m_backup);
        return false;
    }
    
    loadTemplates();
    
    m_logger->write("查找微信登录账户");
    
    std::vector<Friend> users;
    LoginInfo2Parser loginInfo2Parser(m_iTunesDb);
    if (!loginInfo2Parser.parse(users))
    {
        m_logger->write("查找微信登录账户失败。");
        return false;
    }
    
    m_logger->write("找到" + std::to_string(users.size()) + "个账号的消息记录");
    
    for (std::vector<Friend>::iterator it = users.begin(); it != users.end(); ++it)
    {
        fillUser(*it);
        exportUser(*it);
    }
    
    m_downloadPool.finishAndWaitForExit();
    
    return true;
}

bool Exporter::exportUser(Friend& user)
{
    std::string uidMd5 = user.getUidHash();
    
    std::string userBase = combinePath("Documents", uidMd5);
    
    m_logger->write("开始处理用户: " + user.DisplayName());
    m_logger->write("读取账号信息");
    // Friend myself;
    
    std::string wcdbPath = m_iTunesDb->findRealPath(combinePath(userBase, "DB", "WCDB_Contact.sqlite"));
    
    Friends friends;
    
    FriendsParser friendsParser;
    friendsParser.parseWcdb(wcdbPath, friends);
    
    m_logger->write("读取对话信息");
    SessionsParser sessionsParser(m_iTunesDb);
    std::vector<Session> sessions;
    
    sessionsParser.parse(userBase, sessions);
    
    m_logger->write("找到" + std::to_string(sessions.size()) + "条对话。");
    std::sort(sessions.begin(), sessions.end(), SessionLastMsgTimeCompare());
    
    Friend* myself = friends.getFriend(user.getUidHash());
    if (NULL == myself)
    {
        myself = &user;
    }
    
    for (std::vector<Session>::const_iterator it = sessions.cbegin(); it != sessions.cend(); ++it)
    {
        exportSession(*myself, friends, *it);
    }
    
    /*
    if (wechat.GetUserBasics(uid, userBase, out Friend myself))
    {
        m_logger->write("微信号：" + myself.ID() + " 昵称：" + myself.DisplayName());
    }
    else
    {
        // m_logger->write("没有找到本人信息，用默认值替代，可以手动替换正确的头像文件：" + Path.Combine("res", "DefaultProfileHead@2x-Me.png").ToString());
    }
     */
    
    return true;
}

bool Exporter::exportSession(Friend& user, Friends& friends, const Session& session)
{
    // var hash = chat;
    std::string displayName = session.DisplayName;
    if (displayName.empty())
    {
        const Friend* f = friends.getFriend(session.Hash);
        if (NULL != f)
        {
            displayName = f->DisplayName();
        }
    }
    
    m_logger->write("处理与" + displayName + "的对话");
    
    // SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, Logger& logger, std::map<std::string, std::string>& templates, DownloadPool* dlPool);
    
    SessionParser sessionParser(user, friends, *m_iTunesDb, *m_shell, *m_logger, m_templates, m_downloadPool);
    
    /*
    if (endsWith(sessionId, "@chatroom"))
    {
        if (sessions.ContainsKey(displayname))
        {
            Session session = sessions[displayname];
            if (session.DisplayName != null && session.DisplayName.Length != 0)
            {
                displayname = session.DisplayName;
            }
        }
    }
     */
    
    std::string userBase = combinePath("Documents", "");
    // SessionParser sessionParser("", m_templates);
    
    // int count = sessionParser.parse(userBase, "", session, <#Friend &f#>)
    // if (.SaveHtmlRecord(conn, userBase, userSaveBase, displayname, id, myself, chat, friend, friends, out int count, out HashSet<DownloadTask> _emojidown, out lastMsgTime))
    {
        // m_logger->write("成功处理" + count + "条");
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
