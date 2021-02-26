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
    SPO_DESC = 1 << 16,
    SPO_ICON_IN_SESSION = 1 << 17,     // Put Head Icon and Emoji files in the folder of session
    SPO_SYNC_LOADING = 1 << 18
};

struct MsgRecord
{
    int createTime;
    std::string message;
    int des;
    int type;
    std::string msgId;
};

struct ForwardMsg
{
    std::string msgId;
    std::string usrName;
    std::string displayName;
    std::string portrait;
    std::string dataType;
    std::string subType;
    std::string dataId;
    std::string dataFormat;
    std::string msgTime;
    std::string srcMsgTime;
#ifndef NDEBUG
    std::string rawMessage;
#endif
};

struct AppMsgInfo
{
    std::string msgId;
    int msgType;
#ifndef NDEBUG
    std::string message;
#endif
    int appMsgType;
    std::string appId;
    std::string appName;
    std::string localAppIcon;
};

struct FwdMsgInfo
{
    std::string msgId;
    int msgType;
#ifndef NDEBUG
    std::string message;
#endif
    int appMsgType;
    
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

class ForwardMsgsHandler
{
private:
    XmlParser& m_xmlParser;
    std::string m_msgId;
    std::vector<ForwardMsg>& m_forwardedMsgs;
public:

    ForwardMsgsHandler(XmlParser& xmlParser, const std::string& msgId, std::vector<ForwardMsg>& forwardedMsgs) : m_xmlParser(xmlParser), m_msgId(msgId), m_forwardedMsgs(forwardedMsgs)
    {
    }
    
    bool operator() (xmlNodeSetPtr xpathNodes)
    {
        for (int idx = 0; idx < xpathNodes->nodeNr; ++idx)
        {
            ForwardMsg fmsg = {m_msgId};
            

            // templateValues.setName("msg");
            // templateValues["%%ALIGNMENT%%"] = "left";
            
            xmlNode *cur = xpathNodes->nodeTab[idx];
            
#ifndef NDEBUG
            fmsg.rawMessage = XmlParser::getNodeOuterXml(cur);
#endif
            XmlParser::getNodeAttributeValue(cur, "datatype", fmsg.dataType);
            XmlParser::getNodeAttributeValue(cur, "dataid", fmsg.dataId);
            XmlParser::getNodeAttributeValue(cur, "subtype", fmsg.subType);
            
            xmlNodePtr childNode = xmlFirstElementChild(cur);
            bool hasDataTitle = false;
            /*
            while (NULL != childNode)
            {
                if (xmlStrcmp(childNode->name, BAD_CAST("sourcename")) == 0)
                {
                    fmsg.displayName = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("sourcetime")) == 0)
                {
                    fmsg.msgTime = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("datadesc")) == 0)
                {
                    if (!hasDataTitle)
                    {
                        fmsg.message = XmlParser::getNodeInnerText(childNode);
                    }
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("dataitemsource")) == 0)
                {
                    if (!XmlParser::getChildNodeContent(childNode, "realchatname", fmsg.usrName))
                    {
                        XmlParser::getChildNodeContent(childNode, "fromusr", fmsg.usrName);
                    }
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("srcMsgCreateTime")) == 0)
                {
                    fmsg.srcMsgTime = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("datafmt")) == 0)
                {
                    fmsg.dataFormat = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("weburlitem")) == 0)
                {
                    XmlParser::getChildNodeContent(childNode, "title", fmsg.message);
                    XmlParser::getChildNodeContent(childNode, "link", fmsg.link);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("datatitle")) == 0)
                {
                    fmsg.message = XmlParser::getNodeInnerText(childNode);
                    hasDataTitle = true;
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("recordxml")) == 0)
                {
                    xmlNodePtr nodeRecordInfo = XmlParser::getChildNode(childNode, "recordinfo");
                    if (NULL != nodeRecordInfo)
                    {
                        fmsg.nestedMsgs = XmlParser::getNodeOuterXml(nodeRecordInfo);
                    }
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("locitem")) == 0)
                {
                    fmsg.nestedMsgs = XmlParser::getNodeOuterXml(childNode);
                }
            
                childNode = childNode->next;
            }
             */
            
            m_forwardedMsgs.push_back(fmsg);
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
    
    MessageParser(const ITunesDb& iTunesDb, Downloader& downloader, Friends& friends, Friend myself, int options, const std::string& outputPath, std::function<std::string(const std::string&)>& localeFunc);
    
    bool parse(MsgRecord& record, const Session& session, std::vector<TemplateValues>& tvs);
    
protected:
    void parseText(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseImage(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseVoice(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parsePushMail(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseVideo(const MsgRecord& record, const Session& session, std::string& senderId, TemplateValues& tv);
    void parseEmotion(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseAppMsg(const MsgRecord& record, const Session& session, std::string& senderId, std::string& fwdMsg, std::string& fwdMsgTitle, TemplateValues& tv);
    void parseCall(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseLocation(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseStatusNotify(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parsePossibleFriend(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseVerification(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseCard(const MsgRecord& record, const Session& session, TemplateValues& tv);
    void parseNotice(const MsgRecord& record, const Session& session, TemplateValues& tv);  // 64
    void parseSysNotice(const MsgRecord& record, const Session& session, TemplateValues& tv);   // 9999
    void parseSystem(const MsgRecord& record, const Session& session, TemplateValues& tv);
    
    // APP MSG
    void parseAppMsgText(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgImage(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgAudio(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgVideo(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgEmotion(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgUrl(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgAttachment(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgOpen(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgEmoji(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgRtLocation(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgFwdMsg(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, std::string& fwdMsg, std::string& fwdMsgTitle, TemplateValues& tv);
    void parseAppMsgCard(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgChannelCard(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgChannels(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgRefer(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgTransfer(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgRedPacket(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgReaderType(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgUnknownType(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);
    void parseAppMsgDefault(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv);

    // FORWARDEWD MSG
    void parseFwdMsgText(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgImage(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgVideo(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgLink(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgLocation(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgAttach(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgCard(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgNestedFwdMsg(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, std::string& nestedFwdMsg, std::string& nestedFwdMsgTitle, TemplateValues& tv);
    void parseFwdMsgMiniProgram(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgChannels(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    void parseFwdMsgChannelCard(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv);
    
    // Implementation
    void parseImage(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& srcPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& templateValues);
    void parseVideo(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, const std::string& width, const std::string& height, TemplateValues& templateValues);
    void parseFile(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& templateValues);
    void parseCard(const std::string& sessionPath, const std::string& portraitDir, const std::string& cardMessage, TemplateValues& templateValues);
    void parseChannelCard(const std::string& sessionPath, const std::string& portraitDir, const std::string& usrName, const std::string& avatar, const std::string& name, TemplateValues& templateValues);
    void parseChannels(const std::string& msgId, const XmlParser& xmlParser, xmlNodePtr parentNode, const std::string& finderFeedXPath, const Session& session, TemplateValues& tv);
    bool parseForwardedMsgs(const Session& session, const MsgRecord& record, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs);
    
    std::string getDisplayTime(int ms) const;
    std::string getLocaleString(const std::string& key) const
    {
        return m_localFunction(key);
    }
    
    void ensureDirectoryExisted(const std::string& path)
    {
        if (!existsDirectory(path))
        {
            makeDirectory(path);
        }
    }

protected:
    const ITunesDb& m_iTunesDb;
    Downloader& m_downloader;

    Friends& m_friends;
    Friend m_myself;
    int m_options;
    
    const std::string m_outputPath;
    std::string m_userBase;

    std::function<std::string(const std::string&)> m_localFunction;
    
protected:
    mutable std::vector<unsigned char> m_pcmData;  // buffer
};

#endif /* MessageParser_h */
