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
#include "ExportOption.h"
#include "ResManager.h"
#include "Downloader.h"
#include "TaskManager.h"
#include "ITunesParser.h"
#include "FileSystem.h"
#include "XmlParser.h"
#include "Utils.h"

class TemplateValues
{
private:
    std::map<std::string, std::string> m_values;
    std::string m_name;
    
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
    
    const std::map<std::string, std::string>& getValues() const
    {
        return m_values;
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
    static const int APPMSGTYPE_NOTE = 24;
    static const int APPMSGTYPE_CHANNEL_CARD = 50;
    static const int APPMSGTYPE_CHANNELS = 51;
    static const int APPMSGTYPE_REFER = 57;
    static const int APPMSGTYPE_PAT = 62;
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
    
    MessageParser(const ITunesDb& iTunesDb, const ITunesDb& iTunesDbShare, TaskManager& taskManager, Friends& friends, Friend myself, const ExportOption& options, const std::string& resPath, const std::string& outputPath, const ResManager& resManager);
    
    bool parse(WXMSG& msg, const Session& session, std::vector<TemplateValues>& tvs) const;
    
    bool copyPortraitIcon(const Session* session, const std::string& usrName, const std::string& portraitUrl, const std::string& portraitUrlLD, const std::string& destPath) const;
    bool copyPortraitIcon(const Session* session, const std::string& usrName, const std::string& usrNameHash, const std::string& portraitUrl, const std::string& portraitUrlLD, const std::string& destPath) const;
    bool copyPortraitIcon(const Session* session, const Friend& f, const std::string& destPath) const;
    
    std::string getError() const
    {
        return m_error;
    }
    bool hasError() const
    {
        return !m_error.empty();
    }
protected:
    
    bool parsePortrait(const WXMSG& msg, const Session& session, const std::string& senderId, TemplateValues& tv) const;
    
    bool parseText(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseImage(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseVoice(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parsePushMail(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseVideo(const WXMSG& msg, const Session& session, std::string& senderId, TemplateValues& tv) const;
    bool parseEmotion(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseAppMsg(const WXMSG& msg, const Session& session, std::string& senderId, std::string& fwdMsg, std::string& fwdMsgTitle, TemplateValues& tv) const;
    bool parseCall(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseLocation(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseStatusNotify(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parsePossibleFriend(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseVerification(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseCard(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    bool parseNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const;  // 64
    bool parseSysNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const;   // 9999
    bool parseSystem(const WXMSG& msg, const Session& session, TemplateValues& tv) const;
    
    // APP MSG
    bool parseAppMsgText(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgImage(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgAudio(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgVideo(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgEmotion(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgUrl(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgAttachment(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgOpen(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgEmoji(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgRtLocation(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgFwdMsg(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, std::string& fwdMsg, std::string& fwdMsgTitle, TemplateValues& tv) const;
    bool parseAppMsgCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgChannelCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgChannels(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgRefer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgTransfer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgRedPacket(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgPat(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgReaderType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgNote(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgUnknownType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;
    bool parseAppMsgDefault(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const;

    // FORWARDEWD MSG
    bool parseFwdMsgText(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgImage(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgVideo(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgLink(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgLocation(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgAttach(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgNestedFwdMsg(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, std::string& nestedFwdMsg, std::string& nestedFwdMsgTitle, TemplateValues& tv) const;
    bool parseFwdMsgMiniProgram(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgChannels(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    bool parseFwdMsgChannelCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const;
    
    // Implementation
    bool parseImage(const std::string& sessionPath, const std::string& sessionAssetsPath, const std::string& sessionAssetsUrlPath, const std::string& src, const std::string& srcHdOrPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& tv) const;
    bool parseVideo(const std::string& sessionPath, const std::string& sessionAssetsPath, const std::string& sessionAssetsUrlPath, const std::string& src, const std::string& srcRaw, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, const std::string& width, const std::string& height, TemplateValues& tv) const;
    bool parseFile(const std::string& sessionPath, const std::string& sessionAssetsPath, const std::string& sessionAssetsUrlPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& tv) const;
    bool parseCard(const Session& session, const std::string& sessionPath, const std::string& portraitDir, const std::string& portraitUrlDir, const std::string& cardMessage, TemplateValues& tv) const;
    bool parseChannelCard(const Session& session, const std::string& portraitDir, const std::string& portraitUrlDir, const std::string& usrName, const std::string& avatar, const std::string& avatarLD, const std::string& name, TemplateValues& tv) const;
    bool parseChannels(const std::string& msgId, const XmlParser& xmlParser, xmlNodePtr parentNode, const std::string& finderFeedXPath, const Session& session, TemplateValues& tv) const;
    bool parseForwardedMsgs(const Session& session, const WXMSG& msg, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs) const;
    
    std::string getDisplayTime(int ms) const;
    
    std::string getMemberDisplayName(const std::string& usrName, const Session& session) const;
    
    void ensureDirectoryExisted(const std::string& path) const
    {
        if (!existsDirectory(path))
        {
            makeDirectory(path);
        }
    }
    
    void ensureDefaultPortraitIconExisted(const std::string& portraitPath) const;
    
    static bool removeSupportUrl(std::string& url);
protected:
    const ITunesDb& m_iTunesDb;
    const ITunesDb& m_iTunesDbShare;
    const ResManager& m_resManager;
    TaskManager& m_taskManager;

    Friends& m_friends;
    Friend m_myself;
    ExportOption m_options;
    
    const std::string m_resPath;
    const std::string m_outputPath;
    std::string m_userBase;
    mutable std::string m_error;

protected:
#ifndef USING_ASYNC_TASK_FOR_MP3
    mutable std::vector<unsigned char> m_pcmData;  // buffer
#endif
};

#endif /* MessageParser_h */
