//
//  MessageParser.h
//  WechatExporter
//
//  Created by Matthew on 2021/2/22.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef MessageParser_h
#define MessageParser_h

#include <string>
#ifndef NDEBUG
#include <cassert>
#endif
#include "WechatObjects.h"
#include "Downloader.h"
#include "TaskManager.h"
#include "ITunesParser.h"
#include "FileSystem.h"
#include "XmlParser.h"
#include "Utils.h"

enum SessionParsingOption
{
    SPO_IGNORE_AVATAR = 1 << 0,
    SPO_IGNORE_AUDIO = 1 << 1,
    SPO_IGNORE_IMAGE = 1 << 2,
    SPO_IGNORE_VIDEO = 1 << 3,
    SPO_IGNORE_EMOJI = 1 << 4,
    SPO_IGNORE_FILE = 1 << 5,
    SPO_IGNORE_CARD = 1 << 6,
    SPO_IGNORE_SHARING = 1 << 7,
    SPO_IGNORE_HTML_ENC = 1 << 8,
    SPO_TEXT_MODE = 0xFFFF,
	SPO_PDF_MODE = 1 << 16,

    SPO_DESC = 1 << 20,
    SPO_ICON_IN_SESSION = 1 << 21,     // Put Head Icon and Emoji files in the folder of session
    SPO_SYNC_LOADING = 1 << 22,
    SPO_SUPPORT_FILTER = 1 << 23,
    
    SPO_INCREMENTAL_EXP = 1 << 30,
	
	SPO_END
};

struct WXMSG
{
    int createTime;
    std::string content;
    int des;
    int type;
    std::string msgId;
    int64_t msgIdValue;
};

struct WXAPPMSG
{
    const WXMSG *msg;
    int appMsgType;
    std::string appId;
    std::string appName;
    std::string localAppIcon;
};

struct WXFWDMSG
{
    const WXMSG *msg;
    std::string usrName;
    std::string displayName;
    std::string portrait;
    std::string portraitLD;
    std::string dataType;
    std::string subType;
    std::string dataId;
    std::string dataFormat;
    std::string msgTime;
    std::string srcMsgTime;
#if !defined(NDEBUG) || defined(DBG_PERF)
    std::string rawMessage;
#endif
};

class TemplateValues
{
private:
    std::string m_name;
    std::map<std::string, std::string> m_values;
    
public:
    using const_iterator = std::map<std::string, std::string>::const_iterator;

public:
    TemplateValues()
    {
    }
    TemplateValues(const std::string& name) : m_name(name)
    {
    }
    std::string getName() const
    {
        return m_name;
    }
    void setName(const std::string& name)
    {
        m_name = name;
    }
    std::string& operator[](const std::string& k)
    {
        return m_values[k];
    }
    bool hasValue(const std::string& key) const
    {
        return m_values.find(key) != m_values.cend();
    }
    
    const_iterator cbegin() const
    {
        return m_values.cbegin();
    }
    
    const_iterator cend() const
    {
        return m_values.cend();
    }
    
    void clear()
    {
        m_values.clear();
    }
    
    void clearName()
    {
        m_name.clear();
    }
};

struct WechatTemplateHandler
{
    XmlParser& m_xmlParser;
    std::string m_templateText;

    WechatTemplateHandler(XmlParser& xmlParser, const std::string& templateText) : m_xmlParser(xmlParser), m_templateText(templateText)
    {
    }
    
    std::string getText() const
    {
        return m_templateText;
    }
    
    bool operator() (xmlNodeSetPtr xpathNodes)
    {
        std::string linkName;
        std::string linkType;
        std::string linkHidden;
        
        for (int idx = 0; idx < xpathNodes->nodeNr; ++idx)
        {
            xmlNode *cur = xpathNodes->nodeTab[idx];
            
            XmlParser::getNodeAttributeValue(cur, "name", linkName);
            XmlParser::getNodeAttributeValue(cur, "type", linkType);
            XmlParser::getNodeAttributeValue(cur, "hidden", linkHidden);
            
            std::string linkValue;
            if (linkHidden == "1")
            {
                replaceAll(m_templateText, "$" + linkName + "$", linkValue);
                continue;
            }
            
            if (linkType == "link_plain")
            {
                XmlParser::getChildNodeContent(cur, "plain", linkValue);
            }
            else if (linkType == "link_profile")
            {
                xmlNodePtr nodeMemberList = XmlParser::getChildNode(cur, "memberlist");
                std::vector<std::string> linkValues;
                if (NULL != nodeMemberList)
                {
                    xmlNodePtr nodeMember = XmlParser::getChildNode(nodeMemberList, "member");
                    while (NULL != nodeMember)
                    {
                        XmlParser::getChildNodeContent(nodeMember, "nickname", linkValue);
                        linkValues.push_back(linkValue);
                        nodeMember = XmlParser::getNextNodeSibling(nodeMember);
                    }
                }
                std::string linkSep;
                XmlParser::getChildNodeContent(cur, "separator", linkSep);
                
                linkValue = join(linkValues, linkSep.c_str());
            }
            else if (linkType == "link_admin_explain")
            {
                XmlParser::getChildNodeContent(cur, "title", linkValue);
            }
            else if (linkType == "link_revoke_qrcode")
            {
                XmlParser::getChildNodeContent(cur, "title", linkValue);
            }
            else if (linkType == "new_link_succeed_contact")
            {
                XmlParser::getChildNodeContent(cur, "title", linkValue);
            }
            else
            {
                // Try to find value of title
                if (!XmlParser::getChildNodeContent(cur, "title", linkValue))
                {
#ifndef NDEBUG
                    assert(false);
#endif
                }
            }
            replaceAll(m_templateText, "$" + linkName + "$", linkValue);
            
        }
        
        return true;
    }
};

class MessageParser
{
public:
    static const int MSGTYPE_TEXT = 1;
    static const int MSGTYPE_IMAGE = 3;
    static const int MSGTYPE_VOICE = 34;
    static const int MSGTYPE_PUSHMAIL = 35;
    static const int MSGTYPE_VERIFYMSG = 37;
    static const int MSGTYPE_POSSIBLEFRIEND = 40;
    static const int MSGTYPE_SHARECARD = 42;
    static const int MSGTYPE_VIDEO = 43;
    static const int MSGTYPE_EMOTICON = 47;
    static const int MSGTYPE_LOCATION = 48;
    static const int MSGTYPE_APP = 49;
    static const int MSGTYPE_VOIPMSG = 50;
    static const int MSGTYPE_VOIPNOTIFY = 52;
    static const int MSGTYPE_VOIPINVITE = 53;
    
    static const int MSGTYPE_STATUSNOTIFY = 51;
    
    static const int MSGTYPE_MICROVIDEO = 62;
    
    static const int MSGTYPE_NOTICE = 64;
    
    static const int MSGTYPE_IMCARD = 66;
    
    // Transfer          = 2000, // 转账
    //   RedEnvelope       = 2001, // 红包
    //   MiniProgram       = 2002, // 小程序
    //   GroupInvite       = 2003, // 群邀请
    //  File              = 2004, // 文件消息
    
    static const int MSGTYPE_SYSNOTICE = 9999;
    static const int MSGTYPE_SYS = 10000;
    static const int MSGTYPE_RECALLED = 10002;

    static const int APPMSGTYPE_TEXT = 1;
    static const int APPMSGTYPE_IMG = 2;
    static const int APPMSGTYPE_AUDIO = 3;
    static const int APPMSGTYPE_VIDEO = 4;
    static const int APPMSGTYPE_URL = 5;
    static const int APPMSGTYPE_ATTACH = 6;
    static const int APPMSGTYPE_OPEN = 7;
    static const int APPMSGTYPE_EMOJI = 8;
    static const int APPMSGTYPE_VOICE_REMIND = 9;
    static const int APPMSGTYPE_SCAN_GOOD = 10;
    static const int APPMSGTYPE_GOOD = 13;
    static const int APPMSGTYPE_EMOTION = 15;
    static const int APPMSGTYPE_CARD_TICKET = 16;
    static const int APPMSGTYPE_REALTIME_LOCATION = 17;
    static const int APPMSGTYPE_FWD_MSG = 19;
    static const int APPMSGTYPE_CHANNEL_CARD = 50;
    static const int APPMSGTYPE_CHANNELS = 51;
    static const int APPMSGTYPE_REFER = 57;
    static const int APPMSGTYPE_TRANSFERS = 2000;
    static const int APPMSGTYPE_RED_ENVELOPES = 2001;
    static const int APPMSGTYPE_READER_TYPE = 100001;
    
    static const int FWDMSG_DATATYPE_TEXT = 1;
    static const int FWDMSG_DATATYPE_IMAGE = 2;
    static const int FWDMSG_DATATYPE_VIDEO = 4;
    static const int FWDMSG_DATATYPE_LINK = 5;
    static const int FWDMSG_DATATYPE_LOCATION = 6;
    static const int FWDMSG_DATATYPE_ATTACH = 8;
    static const int FWDMSG_DATATYPE_CARD = 16;
    static const int FWDMSG_DATATYPE_NESTED_FWD_MSG = 17;
    static const int FWDMSG_DATATYPE_MINI_PROGRAM = 19;
    static const int FWDMSG_DATATYPE_CHANNELS = 22;
    static const int FWDMSG_DATATYPE_CHANNEL_CARD = 26;
    
    MessageParser(const ITunesDb& iTunesDb, const ITunesDb& iTunesDbShare, TaskManager& taskManager, Friends& friends, Friend myself, int options, const std::string& resPath, const std::string& outputPath, std::function<std::string(const std::string&)>& localeFunc);
    
    bool parse(WXMSG& msg, const Session& session, std::vector<TemplateValues>& tvs) const;
    
    bool copyPortraitIcon(const Session* session, const std::string& usrName, const std::string& portraitUrl, const std::string& portraitUrlLD, const std::string& destPath) const;
    bool copyPortraitIcon(const Session* session, const std::string& usrName, const std::string& usrNameHash, const std::string& portraitUrl, const std::string& portraitUrlLD, const std::string& destPath) const;
    bool copyPortraitIcon(const Session* session, const Friend& f, const std::string& destPath) const;
    
protected:
    
    void parsePortrait(const WXMSG& msg, const Session& session, const std::string& senderId, TemplateValues& tv) const;
    
    void parseText(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseImage(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseVoice(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parsePushMail(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseVideo(const WXMSG& msg, const Session& session, std::string& senderId, TemplateValues& tv) const;
    void parseEmotion(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseAppMsg(const WXMSG& msg, const Session& session, std::string& senderId, std::string& fwdMsg, std::string& fwdMsgTitle, TemplateValues& tv) const;
    void parseCall(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseLocation(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseStatusNotify(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parsePossibleFriend(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseVerification(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseCard(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    void parseNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const;  // 64
    void parseSysNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const;   // 9999
    void parseSystem(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    
    // APP MSG
    void parseAppMsgText(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgImage(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgAudio(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgVideo(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgEmotion(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgUrl(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgAttachment(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgOpen(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgEmoji(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgRtLocation(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgFwdMsg(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, std::string& fwdMsg, std::string& fwdMsgTitle, TemplateValues& tv) const;
    void parseAppMsgCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgChannelCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgChannels(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgRefer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgTransfer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgRedPacket(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgReaderType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgUnknownType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    void parseAppMsgDefault(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;

    // FORWARDEWD MSG
    void parseFwdMsgText(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgImage(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgVideo(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgLink(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgLocation(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgAttach(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgNestedFwdMsg(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, std::string& nestedFwdMsg, std::string& nestedFwdMsgTitle, TemplateValues& tv) const;
    void parseFwdMsgMiniProgram(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgChannels(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    void parseFwdMsgChannelCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    
    // Implementation
    void parseImage(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& srcPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& tv) const;
    void parseVideo(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, const std::string& width, const std::string& height, TemplateValues& tv) const;
    void parseFile(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& tv) const;
    void parseCard(const Session& session, const std::string& sessionPath, const std::string& portraitDir, const std::string& cardMessage, TemplateValues& tv) const;
    void parseChannelCard(const Session& session, const std::string& portraitDir, const std::string& usrName, const std::string& avatar, const std::string& avatarLD, const std::string& name, TemplateValues& tv) const;
    void parseChannels(const std::string& msgId, const XmlParser& xmlParser, xmlNodePtr parentNode, const std::string& finderFeedXPath, const Session& session, TemplateValues& tv) const;
    bool parseForwardedMsgs(const Session& session, const WXMSG& msg, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs) const;
    
    std::string getDisplayTime(int ms) const;
    std::string getLocaleString(const std::string& key) const
    {
        return m_localFunction(key);
    }
    
    void ensureDirectoryExisted(const std::string& path) const
    {
        if (!existsDirectory(path))
        {
            makeDirectory(path);
        }
    }
    
    void ensureDefaultPortraitIconExisted(const std::string& portraitPath) const;
protected:
    const ITunesDb& m_iTunesDb;
    const ITunesDb& m_iTunesDbShare;
    // Downloader& m_downloader;
    TaskManager& m_taskManager;

    Friends& m_friends;
    Friend m_myself;
    int m_options;
    
    const std::string m_resPath;
    const std::string m_outputPath;
    std::string m_userBase;

    std::function<std::string(const std::string&)> m_localFunction;
    
protected:
#ifndef USING_ASYNC_TASK_FOR_MP3
    mutable std::vector<unsigned char> m_pcmData;  // buffer
#endif
};

#endif /* MessageParser_h */
