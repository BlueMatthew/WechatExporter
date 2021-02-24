//
//  MessageParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/2/22.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "MessageParser.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <json/json.h>
#include <plist/plist.h>
#include "XmlParser.h"

MessageParserBase::MessageParserBase(const ITunesDb& iTunesDb, Downloader& downloader, const Session& session, Friends& friends, Friend myself, const std::string& outputPath) : m_iTunesDb(iTunesDb), m_downloader(downloader), m_session(session), m_friends(friends), m_myself(myself), m_outputPath(outputPath)
{
}

MessageParser::MessageParser(const ITunesDb& iTunesDb, Downloader& downloader, const Session& session, Friends& friends, Friend myself, const std::string& outputPath) : MessageParserBase(iTunesDb, downloader, session, friends, myself, outputPath)
{
}

bool MessageParser::parse(MsgRecord& record, const std::string& userBase, const std::string& path, const Session& session, std::vector<TemplateValues>& tvs)
{
    TemplateValues& tv = *(tvs.emplace(tvs.end(), "msg"));

    std::string assetsDir = combinePath(m_outputPath, m_session.getOutputFileName() + "_files");
    
    tv["%%MSGID%%"] = record.msgId;
    tv["%%NAME%%"] = "";
    tv["%%TIME%%"] = fromUnixTime(record.createTime);
    tv["%%MESSAGE%%"] = "";
    
    std::string forwardedMsg;
    std::string forwardedMsgTitle;

    std::string senderId = "";
    if (session.isChatroom())
    {
        if (record.des != 0)
        {
            std::string::size_type enter = record.message.find(":\n");
            if (enter != std::string::npos && enter + 2 < record.message.size())
            {
                senderId = record.message.substr(0, enter);
                record.message = record.message.substr(enter + 2);
            }
        }
    }

#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(record.type) + ".txt"), record.message);
#endif
    
    switch (record.type)
    {
        case MSGTYPE_TEXT:  // 1
            parseText(record, tv);
            break;
        case MSGTYPE_IMAGE:  // 3
            parseImage(record, userBase, tv);
            break;
        case MSGTYPE_VOICE:  // 34
            parseVoice(record, userBase, tv);
            break;
        case MSGTYPE_VERIFYMSG: // 37
            parseVerification(record, userBase, tv);
            break;
        case MSGTYPE_POSSIBLEFRIEND:    // 40
            parsePossibleFriend(record, userBase, tv);
            break;
        case MSGTYPE_SHARECARD:  // 42
            parseCard(record, userBase, tv);
            break;
        case MSGTYPE_VIDEO: // 43
        case MSGTYPE_MICROVIDEO:    // 62
            parseVideo(record, userBase, senderId, tv);
            break;
        case MSGTYPE_EMOTICON:   // 47
            parseEmotion(record, userBase, tv);
            break;
        case MSGTYPE_LOCATION:   // 48
            parseLocation(record, tv);
            break;
        case MSGTYPE_APP:    // 49
            parseAppMsg(record, userBase, senderId, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case MSGTYPE_VOIPMSG:   // 50
            parseCall(record, tv);
            break;
        case MSGTYPE_VOIPNOTIFY:    // 52
        case MSGTYPE_VOIPINVITE:    // 53
            parseSysNotice(record, tv);
            break;
        case MSGTYPE_STATUSNOTIFY:  // 51
            parseStatusNotify(record,tv);
            break;
        case MSGTYPE_NOTICE: // 64
            parseNotice(record, tv);
            break;
        case MSGTYPE_SYSNOTICE: // 9999
            break;
        case MSGTYPE_SYS:   // 10000
            parseSystem(record, tv);
            break;
        case MSGTYPE_RECALLED:  // 10002
            parseRecall(record, tv);
            break;
        default:
#ifndef NDEBUG
            writeFile(combinePath(m_outputPath, "../dbg", "msg_unknwn_type_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
            parseText(record, tv);
            break;
    }
    
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait/" : "Portrait/";
    std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Emoji/" : "Emoji/";
    
    std::string localPortrait;
    std::string remotePortrait;
    if (session.isChatroom())
    {
        if (record.des == 0)
        {
            tv["%%ALIGNMENT%%"] = "right";
            tv["%%NAME%%"] = m_myself.getDisplayName();    // Don't show name for self
            localPortrait = portraitPath + m_myself.getLocalPortrait();
            tv["%%AVATAR%%"] = localPortrait;
            remotePortrait = m_myself.getPortrait();
        }
        else
        {
            tv["%%ALIGNMENT%%"] = "left";
            if (!senderId.empty())
            {
                std::string txtsender = session.getMemberName(md5(senderId));
                const Friend *f = m_friends.getFriendByUid(senderId);
                if (txtsender.empty() && NULL != f)
                {
                    txtsender = f->getDisplayName();
                }
                tv["%%NAME%%"] = txtsender.empty() ? senderId : txtsender;
                localPortrait = portraitPath + ((NULL != f) ? f->getLocalPortrait() : "DefaultProfileHead@2x.png");
                remotePortrait = (NULL != f) ? f->getPortrait() : "";
                tv["%%AVATAR%%"] = localPortrait;
            }
            else
            {
                tv["%%NAME%%"] = senderId;
                tv["%%AVATAR%%"] = "";
            }
        }
    }
    else
    {
        if (record.des == 0 || session.getUsrName() == m_myself.getUsrName())
        {
            tv["%%ALIGNMENT%%"] = "right";
            tv["%%NAME%%"] = m_myself.getDisplayName();
            localPortrait = portraitPath + m_myself.getLocalPortrait();
            remotePortrait = m_myself.getPortrait();
            tv["%%AVATAR%%"] = localPortrait;
        }
        else
        {
            tv["%%ALIGNMENT%%"] = "left";

            const Friend *f = m_friends.getFriend(session.getHash());
            if (NULL == f)
            {
                tv["%%NAME%%"] = session.getDisplayName();
                localPortrait = portraitPath + (session.isPortraitEmpty() ? "DefaultProfileHead@2x.png" : session.getLocalPortrait());
                remotePortrait = session.getPortrait();
                tv["%%AVATAR%%"] = localPortrait;
            }
            else
            {
                tv["%%NAME%%"] = f->getDisplayName();
                localPortrait = portraitPath + f->getLocalPortrait();
                remotePortrait = f->getPortrait();
                tv["%%AVATAR%%"] = localPortrait;
            }
        }
    }

    if ((m_options & SPO_IGNORE_AVATAR) == 0)
    {
        if (!remotePortrait.empty() && !localPortrait.empty())
        {
            
            m_downloader.addTask(remotePortrait, combinePath(m_outputPath, localPortrait), record.createTime);
        }
    }
    
    if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
    {
        tv["%%NAME%%"] = safeHTML(tv["%%NAME%%"]);
    }

    if (!forwardedMsg.empty())
    {
        // This funtion will change tvs and causes tv invalid, so we do it at last
        parseForwardedMsgs(userBase, m_outputPath, session, record, forwardedMsgTitle, forwardedMsg, tvs);
    }
    return true;
}

void MessageParser::parseText(const MsgRecord& record, TemplateValues& tv)
{
    if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
    {
        tv["%%MESSAGE%%"] = safeHTML(record.message);
    }
    else
    {
        tv["%%MESSAGE%%"] = record.message;
    }
}

void MessageParser::parseImage(const MsgRecord& record, const std::string& userBase, TemplateValues& tv)
{
    std::string vFile = combinePath(userBase, "Img", m_session.getHash(), record.msgId);
    parseImage(m_outputPath, m_session.getOutputFileName() + "_files", vFile + ".pic", "", record.msgId + ".jpg", vFile + ".pic_thum", record.msgId + "_thumb.jpg", tv);
}

void MessageParser::parseVoice(const MsgRecord& record, const std::string& userBase, TemplateValues& tv)
{
    std::string audioSrc;
    int voiceLen = -1;
    const ITunesFile* audioSrcFile = NULL;
    if ((m_options & SPO_IGNORE_AUDIO) == 0)
    {
        std::string vLenStr;
        XmlParser xmlParser(record.message);
        if (xmlParser.parseAttributeValue("/msg/voicemsg", "voicelength", vLenStr) && !vLenStr.empty())
        {
            voiceLen = std::stoi(vLenStr);
        }
        
        audioSrcFile = m_iTunesDb.findITunesFile(combinePath(userBase, "Audio", m_session.getHash(), record.msgId + ".aud"));
        if (NULL != audioSrcFile)
        {
            audioSrc = m_iTunesDb.getRealPath(*audioSrcFile);
        }
    }
    if (audioSrc.empty())
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = voiceLen == -1 ? getLocaleString("[Audio]") : formatString(getLocaleString("[Audio %s]"), getDisplayTime(voiceLen).c_str());
    }
    else
    {
        m_pcmData.clear();
        std::string assetsDir = combinePath(m_outputPath, m_session.getOutputFileName() + "_files");
        std::string mp3Path = combinePath(assetsDir, record.msgId + ".mp3");

        silkToPcm(audioSrc, m_pcmData);
        
        ensureDirectoryExisted(assetsDir);
        pcmToMp3(m_pcmData, mp3Path);
        if (audioSrcFile != NULL)
        {
            updateFileTime(mp3Path, ITunesDb::parseModifiedTime(audioSrcFile->blob));
        }

        tv.setName("audio");
        tv["%%AUDIOPATH%%"] = m_session.getOutputFileName() + "_files/" + record.msgId + ".mp3";
    }
}

void MessageParser::parseVideo(const MsgRecord& record, const std::string& userBase, std::string& senderId, TemplateValues& tv)
{
    std::map<std::string, std::string> attrs = { {"fromusername", ""}, {"cdnthumbwidth", ""}, {"cdnthumbheight", ""} };
    XmlParser xmlParser(record.message);
    if (xmlParser.parseAttributesValue("/msg/videomsg", attrs))
    {
    }
    
    if (senderId.empty())
    {
        senderId = attrs["fromusername"];
    }
    
    std::string vfile = combinePath(userBase, "Video", m_session.getHash(), record.msgId);
    parseVideo(m_outputPath, m_session.getOutputFileName() + "_files", vfile + ".mp4", record.msgId + ".mp4", vfile + ".video_thum", record.msgId + "_thum.jpg", attrs["cdnthumbwidth"], attrs["cdnthumbheight"], tv);
}

void MessageParser::parseEmotion(const MsgRecord& record, const std::string& userBase, TemplateValues& tv)
{
    std::string url;
    if ((m_options & SPO_IGNORE_EMOJI) == 0)
    {
        XmlParser xmlParser(record.message);
        if (!xmlParser.parseAttributeValue("/msg/emoji", "cdnurl", url))
        {
            url.clear();
        }
        else
        {
            if (!startsWith(url, "http") && startsWith(url, "https"))
            {
                if (!xmlParser.parseAttributeValue("/msg/emoji", "thumburl", url))
                {
                    url.clear();
                }
            }
        }
    }

    if (startsWith(url, "http") || startsWith(url, "https"))
    {
        std::string localFile = url;
        std::smatch sm2;
        static std::regex pattern47_2("\\/(\\w+?)\\/\\w*$");
        if (std::regex_search(localFile, sm2, pattern47_2))
        {
            localFile = sm2[1];
        }
        else
        {
            static int uniqueFileName = 1000000000;
            localFile = std::to_string(uniqueFileName++);
        }
        
        std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? m_session.getOutputFileName() + "_files/Emoji/" : "Emoji/";
        localFile = emojiPath + localFile + ".gif";
        ensureDirectoryExisted(m_outputPath);
        m_downloader.addTask(url, combinePath(m_outputPath, localFile), record.createTime);
        tv.setName("emoji");
        tv["%%EMOJIPATH%%"] = localFile;
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = getLocaleString("[Emoji]");
    }
}

void MessageParser::parseAppMsg(const MsgRecord& record, const std::string& userBase, std::string& senderId, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv)
{
#ifndef NDEBUG
    AppMsgInfo appMsgInfo = {record.msgId, record.type, record.message, 0};
#else
    AppMsgInfo appMsgInfo = {record.msgId, record.type, 0};
#endif
    XmlParser xmlParser(record.message, true);
    if (senderId.empty())
    {
        xmlParser.parseNodeValue("/msg/fromusername", senderId);
    }
    
    std::string appMsgTypeStr;
    if (!xmlParser.parseNodeValue("/msg/appmsg/type", appMsgTypeStr))
    {
        // Failed to parse APPMSG type
#ifndef NDEBUG
        writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_invld_" + record.msgId + ".txt"), record.message);
#endif
        tv["%%MESSAGE%%"] = getLocaleString("[Link]");
        return;
    }

    tv.setName("plainshare");
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_" + appMsgTypeStr + ".txt"), record.message);
#endif
    appMsgInfo.appMsgType = std::atoi(appMsgTypeStr.c_str());
    xmlParser.parseAttributeValue("/msg/appmsg", "appid", appMsgInfo.appId);
    if (!appMsgInfo.appId.empty())
    {
        xmlParser.parseNodeValue("/msg/appinfo/appname", appMsgInfo.appName);
        tv["%%APPNAME%%"] = appMsgInfo.appName;
        std::string vFile = combinePath(userBase, "appicon", appMsgInfo.appId + ".png");
        std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? m_session.getOutputFileName() + "_files/Portrait" : "Portrait";
        
        std::string destFileName = combinePath(m_outputPath, portraitDir, "appicon_" + appMsgInfo.appId + ".png");
        if (m_iTunesDb.copyFile(vFile, destFileName))
        {
            appMsgInfo.localAppIcon = portraitDir + "/appicon_" + appMsgInfo.appId + ".png";
            tv["%%APPICONPATH%%"] = appMsgInfo.localAppIcon;
        }
    }

    switch (appMsgInfo.appMsgType)
    {
        case APPMSGTYPE_TEXT: // 1
            parseAppMsgText(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_IMG: // 2
            parseAppMsgImage(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_AUDIO: // 3
            parseAppMsgAudio(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_VIDEO: // 4
            parseAppMsgVideo(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_URL: // 5
            parseAppMsgUrl(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_ATTACH: // 6
            parseAppMsgAttachment(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_OPEN: // 7
            parseAppMsgOpen(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_EMOJI: // 8
            parseAppMsgEmoji(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_VOICE_REMIND: // 9
            // parseAppMsgEmoji(record, xmlParser, userBase, tv);
            parseAppMsgUnknownType(appMsgInfo, xmlParser, userBase, tv);
            break;

        case APPMSGTYPE_SCAN_GOOD: // 10
            parseAppMsgUnknownType(appMsgInfo, xmlParser, userBase, tv);
            break;
            
        case APPMSGTYPE_GOOD: // 13
            parseAppMsgUnknownType(appMsgInfo, xmlParser, userBase, tv);
            break;

        case APPMSGTYPE_EMOTION: // 15
            parseAppMsgUnknownType(appMsgInfo, xmlParser, userBase, tv);
            break;

        case APPMSGTYPE_CARD_TICKET: // 16
            parseAppMsgUnknownType(appMsgInfo, xmlParser, userBase, tv);
            break;

        case APPMSGTYPE_REALTIME_LOCATION: // 17
            parseAppMsgRtLocation(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_FWD_MSG: // 19
            parseAppMsgFwdMsg(appMsgInfo, xmlParser, userBase, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case APPMSGTYPE_CHANNEL_CARD:   // 50
            parseAppMsgChannelCard(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_CHANNELS:   // 51
            parseAppMsgChannels(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_REFER:  // 57
            parseAppMsgRefer(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_TRANSFERS: // 2000
            parseAppMsgTransfer(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_RED_ENVELOPES: // 2001
            parseAppMsgRedPacket(appMsgInfo, xmlParser, userBase, tv);
            break;
        case APPMSGTYPE_READER_TYPE: // 100001
            parseAppMsgReaderType(appMsgInfo, xmlParser, userBase, tv);
            break;
        default:
            parseAppMsgUnknownType(appMsgInfo, xmlParser, userBase, tv);
            break;
    }
}

void MessageParser::parseCall(const MsgRecord& record, TemplateValues& tv)
{
    tv.setName("msg");
    tv["%%MESSAGE%%"] = getLocaleString("[Video/Audio Call]");
}

void MessageParser::parseLocation(const MsgRecord& record, TemplateValues& tv)
{
    std::map<std::string, std::string> attrs = { {"x", ""}, {"y", ""}, {"label", ""} };
    
    XmlParser xmlParser(record.message);
    if (xmlParser.parseAttributesValue("/msg/location", attrs) && !attrs["x"].empty() && !attrs["y"].empty() && !attrs["label"].empty())
    {
        tv["%%MESSAGE%%"] = formatString(getLocaleString("[Location (%s,%s) %s]"), attrs["x"].c_str(), attrs["y"].c_str(), attrs["label"].c_str());
    }
    else
    {
        tv["%%MESSAGE%%"] = getLocaleString("[Location]");
    }
    tv.setName("msg");
}

void MessageParser::parseStatusNotify(const MsgRecord& record, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    parseText(record, tv);
}

void MessageParser::parsePossibleFriend(const MsgRecord& record, const std::string& userBase, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    parseText(record, tv);
}

void MessageParser::parseVerification(const MsgRecord& record, const std::string& userBase, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    parseText(record, tv);
}

void MessageParser::parseCard(const MsgRecord& record, const std::string& userBase, TemplateValues& tv)
{
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? m_session.getOutputFileName() + "_files/Portrait" : "Portrait";
    parseCard(m_outputPath, portraitDir, record.message, tv);
}

void MessageParser::parseNotice(const MsgRecord& record, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    tv.setName("notice");

    Json::Reader reader;
    Json::Value root;
    if (reader.parse(record.message, root))
    {
        tv["%%MESSAGE%%"] = root["msgContent"].asString();
    }
}

void MessageParser::parseSysNotice(const MsgRecord& record, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    tv.setName("notice");
    std::string sysMsg = record.message;
    removeHtmlTags(sysMsg);
    tv["%%MESSAGE%%"] = sysMsg;
}

void MessageParser::parseSystem(const MsgRecord& record, TemplateValues& tv)
{
    tv.setName("notice");
    std::string sysMsg = record.message;
    removeHtmlTags(sysMsg);
    tv["%%MESSAGE%%"] = sysMsg;
}

void MessageParser::parseRecall(const MsgRecord& record, TemplateValues& tv)
{

    // Same as system
    // parseSystem(record, tv);
    tv.setName("notice");
    XmlParser xmlParser(record.message, true);
    std::string sysMsgType;
    xmlParser.parseAttributeValue("/sysmsg", "type", sysMsgType);
    if (sysMsgType == "sysmsgtemplate")
    {
        std::string plainText;
        std::string templateContent;
        std::string templateType;
        xmlParser.parseAttributeValue("/sysmsg/sysmsgtemplate/content_template", "type", templateType);
#ifndef NDEBUG
        writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + "_" + sysMsgType + ".txt"), record.message);
#endif
        if (templateType == "tmpl_type_profile")
        {
            xmlParser.parseNodeValue("/sysmsg/sysmsgtemplate/content_template/plain", plainText);
            if (plainText.empty())
            {
                xmlParser.parseNodeValue("/sysmsg/sysmsgtemplate/content_template/template", templateContent);
                WechatTemplateHandler handler(xmlParser, templateContent);
                if (xmlParser.parseWithHandler("/sysmsg/sysmsgtemplate/content_template/link_list/link", handler))
                {
                    tv["%%MESSAGE%%"] = handler.getText();
                }
            }
            else
            {
                tv["%%MESSAGE%%"] = record.message;
            }
        }
        else
        {
#ifndef NDEBUG
            writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + "_" + sysMsgType + ".txt"), record.message);
            assert(false);
#endif
        }
    }
    else
    {
#ifndef NDEBUG
        writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + "_" + sysMsgType + ".txt"), record.message);
        assert(false);
#endif
    }
    
}

////////////////////////////////

void MessageParser::parseAppMsgText(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    tv["%%MESSAGE%%"] = title.empty() ? getLocaleString("[Link]") : title;
}

void MessageParser::parseAppMsgImage(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}

void MessageParser::parseAppMsgAudio(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}

void MessageParser::parseAppMsgVideo(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}

void MessageParser::parseAppMsgEmotion(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}

void MessageParser::parseAppMsgUrl(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    std::string title;
    std::string desc;
    std::string url;
    std::string thumbUrl;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/des", desc);
    xmlParser.parseNodeValue("/msg/appmsg/url", url);
    xmlParser.parseNodeValue("/msg/appmsg/thumburl", thumbUrl);
    if (thumbUrl.empty())
    {
        thumbUrl = appMsgInfo.localAppIcon;
    }
    
    tv.setName(thumbUrl.empty() ? "plainshare" : "share");
    tv["%%SHARINGIMGPATH%%"] = thumbUrl;
    tv["%%SHARINGTITLE%%"] = title;
    tv["%%SHARINGURL%%"] = url;
    tv["%%MESSAGE%%"] = desc;
}

void MessageParser::parseAppMsgAttachment(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    std::string title;
    std::string attachFileExtName;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/appattach/fileext", attachFileExtName);
    
    std::string attachFileName = userBase + "/OpenData/" + m_session.getHash() + "/" + appMsgInfo.msgId;
    if (!attachFileExtName.empty())
    {
        attachFileName += "." + attachFileExtName;
    }
    
    std::string attachOutputFileName = appMsgInfo.msgId + "_" + title;
    parseFile(m_outputPath, m_session.getOutputFileName() + "_files", attachFileName, attachOutputFileName, title, tv);
}

void MessageParser::parseAppMsgOpen(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, "", tv);
}

void MessageParser::parseAppMsgEmoji(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}

void MessageParser::parseAppMsgRtLocation(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    tv["%%MESSAGE%%"] = getLocaleString("[Real-time Location]");
}

void MessageParser::parseAppMsgFwdMsg(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv)
{
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/recorditem", forwardedMsg);
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsgInfo.msgType) + "_app_19.txt"), forwardedMsg);
#endif
    tv.setName("msg");
    tv["%%MESSAGE%%"] = title;

    forwardedMsgTitle = title;
}

void MessageParser::parseAppMsgCard(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}
void MessageParser::parseAppMsgChannelCard(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    // Channel Card
    std::map<std::string, std::string> nodes = { {"username", ""}, {"avatar", ""}, {"nickname", ""}};
    xmlParser.parseNodesValue("/msg/appmsg/findernamecard/*", nodes);
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? m_session.getOutputFileName() + "_files/Portrait" : "Portrait";
    
    parseChannelCard(m_outputPath, portraitDir, nodes["username"], nodes["avatar"], nodes["nickname"], tv);
}

void MessageParser::parseAppMsgChannels(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsgInfo.msgType) + "_app_" + std::to_string(appMsgInfo.appMsgType) + "_" + appMsgInfo.msgId + ".txt"), appMsgInfo.message);
#endif
    // Channels SHI PIN HAO
    std::map<std::string, std::string> nodes = { {"objectId", ""}, {"nickname", ""}, {"avatar", ""}, {"desc", ""}, {"mediaCount", ""}, {"feedType", ""}, {"desc", ""}, {"username", ""}};
    xmlParser.parseNodesValue("/msg/appmsg/finderFeed/*", nodes);
    std::map<std::string, std::string> videoNodes = { {"mediaType", ""}, {"url", ""}, {"thumbUrl", ""}, {"coverUrl", ""}, {"videoPlayDuration", ""}};
    xmlParser.parseNodesValue("/msg/appmsg/finderFeed/mediaList/media/*", videoNodes);
    std::string thumbUrl;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        thumbUrl = videoNodes["thumbUrl"].empty() ? videoNodes["coverUrl"] : videoNodes["thumbUrl"];
    }
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? m_session.getOutputFileName() + "_files/Portrait" : "Portrait";
    
    tv["%%CARDNAME%%"] = nodes["nickname"];
    tv["%%CHANNELS%%"] = getLocaleString("Channels");
    tv["%%MESSAGE%%"] = nodes["desc"];
    tv["%%EXTRA_CLS%%"] = "channels";
    
    if (!thumbUrl.empty())
    {
        tv.setName("channels");
        
        std::string thumbFile = m_session.getOutputFileName() + "_files/" + appMsgInfo.msgId + ".jpg";
        tv["%%CHANNELTHUMBPATH%%"] = thumbFile;
        ensureDirectoryExisted(combinePath(m_outputPath, m_session.getOutputFileName() + "_files"));
        m_downloader.addTask(thumbUrl, combinePath(m_outputPath, thumbFile), 0);
        
        if (!nodes["avatar"].empty())
        {
            tv["%%CARDIMGPATH%%"] = portraitDir + "/" + nodes["username"] + ".jpg";
            std::string localFile = combinePath(portraitDir, nodes["username"] + ".jpg");
            ensureDirectoryExisted(portraitDir);
            m_downloader.addTask(nodes["avatar"], combinePath(m_outputPath, localFile), 0);
        }

        tv["%%CHANNELURL%%"] = videoNodes["url"];
    }
}

void MessageParser::parseAppMsgRefer(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    // Refer Message
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    std::map<std::string, std::string> nodes = { {"displayname", ""}, {"content", ""}, {"type", ""}};
    if (xmlParser.parseNodesValue("/msg/appmsg/refermsg/*", nodes))
    {
#ifndef NDEBUG
        writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsgInfo.msgType) + "_app_" + std::to_string(APPMSGTYPE_REFER) + "_ref_" + nodes["type"] + " .txt"), nodes["content"]);
#endif
        tv.setName("refermsg");
        tv["%%MESSAGE%%"] = title;
        tv["%%REFERNAME%%"] = nodes["displayname"];
        if (nodes["type"] == "43")
        {
            tv["%%REFERMSG%%"] = getLocaleString("[Video]");
        }
        else if (nodes["type"] == "1")
        {
            tv["%%REFERMSG%%"] = nodes["content"];
        }
        else if (nodes["type"] == "3")
        {
            tv["%%REFERMSG%%"] = getLocaleString("[Photo]");
        }
        else if (nodes["type"] == "49")
        {
            // APPMSG
            XmlParser subAppMsgXmlParser(nodes["content"], true);
            std::string subAppMsgTitle;
            subAppMsgXmlParser.parseNodeValue("/msg/appmsg/title", subAppMsgTitle);
            tv["%%REFERMSG%%"] = subAppMsgTitle;
        }
        else
        {
            tv["%%REFERMSG%%"] = nodes["content"];
        }
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = title;
    }
}

void MessageParser::parseAppMsgTransfer(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    tv["%%MESSAGE%%"] = getLocaleString("[Transfer]");
}

void MessageParser::parseAppMsgRedPacket(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    tv["%%MESSAGE%%"] = getLocaleString("[Red Packet]");
}

void MessageParser::parseAppMsgReaderType(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}

void MessageParser::parseAppMsgUnknownType(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsgInfo.msgType) + "_app_unknwn_" + std::to_string(appMsgInfo.appMsgType) + ".txt"), appMsgInfo.message);
#endif
    parseAppMsgDefault(appMsgInfo, xmlParser, userBase, tv);
}

void MessageParser::parseAppMsgDefault(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const std::string& userBase, TemplateValues& tv)
{
    std::map<std::string, std::string> nodes = { {"title", ""}, {"type", ""}, {"des", ""}, {"url", ""}, {"thumburl", ""}, {"recorditem", ""} };
    xmlParser.parseNodesValue("/msg/appmsg/*", nodes);
    
    if (!nodes["title"].empty() && !nodes["url"].empty())
    {
        tv.setName(nodes["thumburl"].empty() ? "plainshare" : "share");

        tv["%%SHARINGIMGPATH%%"] = nodes["thumburl"];
        tv["%%SHARINGURL%%"] = nodes["url"];
        tv["%%SHARINGTITLE%%"] = nodes["title"];
        tv["%%MESSAGE%%"] = nodes["des"];
    }
    else if (!nodes["title"].empty())
    {
        tv["%%MESSAGE%%"] = nodes["title"];
    }
    else
    {
        tv["%%MESSAGE%%"] = getLocaleString("[Link]");
    }
}


///////////////////////////////
// Implementation

void MessageParser::parseVideo(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& srcVideo, const std::string& destVideo, const std::string& srcThumb, const std::string& destThumb, const std::string& width, const std::string& height, TemplateValues& templateValues)
{
    bool hasThumb = false;
    bool hasVideo = false;
    
    if ((m_options & SPO_IGNORE_VIDEO) == 0)
    {
        std::string fullAssertsPath = combinePath(sessionPath, sessionAssertsPath);
        ensureDirectoryExisted(fullAssertsPath);
        hasThumb = m_iTunesDb.copyFile(srcThumb, combinePath(fullAssertsPath, destThumb));
        hasVideo = m_iTunesDb.copyFile(srcVideo, combinePath(fullAssertsPath, destVideo));
    }

    if (hasVideo)
    {
        templateValues.setName("video");
        templateValues["%%THUMBPATH%%"] = hasThumb ? (sessionAssertsPath + "/" + destThumb) : "";
        templateValues["%%VIDEOPATH%%"] = sessionAssertsPath + "/" + destVideo;
        
    }
    else if (hasThumb)
    {
        templateValues.setName("thumb");
        templateValues["%%IMGTHUMBPATH%%"] = sessionAssertsPath + "/" + destThumb;
        templateValues["%%MESSAGE%%"] = getLocaleString("(Video Missed)");
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = getLocaleString("[Video]");
    }
    
    templateValues["%%VIDEOWIDTH%%"] = width;
    templateValues["%%VIDEOHEIGHT%%"] = height;
}


void MessageParser::parseImage(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& srcPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& templateValues)
{
    bool hasThumb = false;
    bool hasImage = false;
    if ((m_options & SPO_IGNORE_IMAGE) == 0)
    {
        hasThumb = m_iTunesDb.copyFile(srcThumb, combinePath(sessionPath, sessionAssertsPath, destThumb));
        if (!srcPre.empty())
        {
            hasImage = m_iTunesDb.copyFile(srcPre, combinePath(sessionPath, sessionAssertsPath, dest));
        }
        if (!hasImage)
        {
            hasImage = m_iTunesDb.copyFile(src, combinePath(sessionPath, sessionAssertsPath, dest));
        }
    }

    if (hasImage)
    {
        templateValues.setName("image");
        templateValues["%%IMGPATH%%"] = sessionAssertsPath + "/" + dest;
        templateValues["%%IMGTHUMBPATH%%"] = hasThumb ? (sessionAssertsPath + "/" + destThumb) : (sessionAssertsPath + "/" + dest);
    }
    else if (hasThumb)
    {
        templateValues.setName("thumb");
        templateValues["%%IMGTHUMBPATH%%"] = sessionAssertsPath + "/" + destThumb;
        templateValues["%%MESSAGE%%"] = "";
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = getLocaleString("[Photo]");
    }
}

void MessageParser::parseFile(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& templateValues)
{
    bool hasFile = false;
    if ((m_options & SPO_IGNORE_FILE) == 0)
    {
        hasFile = m_iTunesDb.copyFile(src, combinePath(sessionPath, sessionAssertsPath, dest));
    }

    if (hasFile)
    {
        templateValues.setName("plainshare");
        templateValues["%%SHARINGURL%%"] = sessionAssertsPath + "/" + dest;
        templateValues["%%SHARINGTITLE%%"] = fileName;
        templateValues["%%MESSAGE%%"] = "";
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[File: %s]"), fileName.c_str());
    }
}

void MessageParser::parseCard(const std::string& sessionPath, const std::string& portraitDir, const std::string& cardMessage, TemplateValues& templateValues)
{
    // static std::regex pattern42_1("nickname ?= ?\"(.+?)\"");
    // static std::regex pattern42_2("smallheadimgurl ?= ?\"(.+?)\"");
    std::map<std::string, std::string> attrs;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        attrs = { {"nickname", ""}, {"smallheadimgurl", ""}, {"bigheadimgurl", ""}, {"username", ""} };
    }
    else
    {
        attrs = { {"nickname", ""}, {"username", ""} };
    }

    templateValues["%%CARDTYPE%%"] = getLocaleString("[Contact Card]");
    XmlParser xmlParser(cardMessage, true);
    if (xmlParser.parseAttributesValue("/msg", attrs) && !attrs["nickname"].empty())
    {
        std::string portraitUrl = attrs["bigheadimgurl"].empty() ? attrs["smallheadimgurl"] : attrs["bigheadimgurl"];
        if (!attrs["username"].empty() && !portraitUrl.empty())
        {
            templateValues.setName("card");
            templateValues["%%CARDNAME%%"] = attrs["nickname"];
            templateValues["%%CARDIMGPATH%%"] = portraitDir + "/" + attrs["username"] + ".jpg";
            std::string localfile = combinePath(portraitDir, attrs["username"] + ".jpg");
            ensureDirectoryExisted(portraitDir);
            m_downloader.addTask(portraitUrl, combinePath(sessionPath, localfile), 0);
        }
        else
        {
            templateValues.setName("msg");
            templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[Contact Card] %s"), attrs["username"].c_str());
        }
    }
    else
    {
        templateValues["%%MESSAGE%%"] = getLocaleString("[Contact Card]");
    }
    templateValues["%%EXTRA_CLS%%"] = "contact-card";
}

void MessageParser::parseChannelCard(const std::string& sessionPath, const std::string& portraitDir, const std::string& usrName, const std::string& avatar, const std::string& name, TemplateValues& templateValues)
{
    bool hasImg = false;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        hasImg = (!usrName.empty() && !avatar.empty());
    }
    templateValues["%%CARDTYPE%%"] = getLocaleString("[Channel Card]");
    if (!name.empty())
    {
        if (hasImg)
        {
            templateValues.setName("card");
            templateValues["%%CARDNAME%%"] = name;
            templateValues["%%CARDIMGPATH%%"] = portraitDir + "/" + usrName + ".jpg";
            std::string localfile = combinePath(portraitDir, usrName + ".jpg");
            ensureDirectoryExisted(portraitDir);
            m_downloader.addTask(avatar, combinePath(sessionPath, localfile), 0);
        }
        else
        {
            templateValues.setName("msg");
            templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[Channel Card] %s"), name.c_str());
        }
    }
    else
    {
        templateValues["%%MESSAGE%%"] = getLocaleString("[Channel Card]");
    }
    templateValues["%%EXTRA_CLS%%"] = "channel-card";
}

bool MessageParser::parseForwardedMsgs(const std::string& userBase, const std::string& outputPath, const Session& session, const MsgRecord& record, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs)
{
    XmlParser xmlParser(message);
    std::vector<ForwardMsg> forwardedMsgs;
    ForwardMsgsHandler handler(xmlParser, record.msgId, forwardedMsgs);
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait/" : "Portrait/";
    std::string localPortrait;
    std::string remotePortrait;
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& beginTv = tvs.back();
    beginTv["%%MESSAGE%%"] = formatString(getLocaleString("<< %s"), title.c_str());
    beginTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg

    if (xmlParser.parseWithHandler("/recordinfo/datalist/dataitem", handler))
    {
        for (std::vector<ForwardMsg>::const_iterator it = forwardedMsgs.begin(); it != forwardedMsgs.end(); ++it)
        {
            TemplateValues& tv = *(tvs.emplace(tvs.end(), "msg"));
            tv["%%ALIGNMENT%%"] = "left";
            tv["%%EXTRA_CLS%%"] = "fmsg";   // forwarded msg
            // 1: message
            // 2: image
            // 4: video
            // 5: link
            // 6: location
            // 8: File
            // 16: Card
            // 17: Nested Forwarded Messages
            // 19: mini program
            // 22: Channels
            
            if (it->dataType == "1")
            {
                tv["%%MESSAGE%%"] = replaceAll(replaceAll(replaceAll(it->message, "\r\n", "<br />"), "\r", "<br />"), "\n", "<br />");
            }
            else if (it->dataType == "2")
            {
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + record.msgId + "/" + it->dataId;
                parseImage(outputPath, session.getOutputFileName() + "_files/" + record.msgId, vfile + fileExtName, vfile + fileExtName + "_pre3", it->dataId + ".jpg", vfile + ".record_thumb", it->dataId + "_thumb.jpg", tv);
            }
            else if (it->dataType == "3")
            {
                tv["%%MESSAGE%%"] = it->message;
            }
            else if (it->dataType == "4")
            {
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + record.msgId + "/" + it->dataId;
                parseVideo(outputPath, session.getOutputFileName() + "_files/" + record.msgId, vfile + fileExtName, it->dataId + fileExtName, vfile + ".record_thumb", it->dataId + "_thumb.jpg", "", "", tv);
                
            }
            else if (it->dataType == "5")
            {
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + record.msgId + "/" + it->dataId + ".record_thumb";
                std::string dest = session.getOutputFileName() + "_files/" + record.msgId + "/" + it->dataId + "_thumb.jpg";
                bool hasThumb = false;
                if ((m_options & SPO_IGNORE_SHARING) == 0)
                {
                    hasThumb = m_iTunesDb.copyFile(vfile, combinePath(outputPath, dest));
                }
                
                if (!(it->link.empty()))
                {
                    tv.setName(hasThumb ? "share" : "plainshare");

                    tv["%%SHARINGIMGPATH%%"] = dest;
                    tv["%%SHARINGURL%%"] = it->link;
                    tv["%%SHARINGTITLE%%"] = it->message;
                    // tv["%%MESSAGE%%"] = nodes["des"];
                }
                else
                {
                    tv["%%MESSAGE%%"] = it->message;
                }
            }
            else if (it->dataType == "6")
            {
                // Location
                std::map<std::string, std::string> attrs = { {"poiname", ""}, {"lng", ""}, {"lat", ""}, {"label", ""} };
                
                XmlParser xmlParser(it->nestedMsgs);
                if (xmlParser.parseNodesValue("/locitem/*", attrs) && !attrs["lat"].empty() && !attrs["lng"].empty() && !attrs["poiname"].empty())
                {
                    tv["%%MESSAGE%%"] = formatString(getLocaleString("[Location (%s,%s) %s]"), attrs["lat"].c_str(), attrs["lng"].c_str(), attrs["poiname"].c_str());
                }
                else
                {
                    tv["%%MESSAGE%%"] = getLocaleString("[Location]");
                }
                tv.setName("msg");
            }
            else if (it->dataType == "8")
            {
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + record.msgId + "/" + it->dataId;
                parseFile(outputPath, session.getOutputFileName() + "_files/" + record.msgId, vfile + fileExtName, it->dataId + fileExtName, it->message, tv);
            }
            else if (it->dataType == "16")
            {
                // Card
                std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
                parseCard(outputPath, portraitDir, it->message, tv);
            }
            else if (it->dataType == "17")
            {
                // parseForwardedMsgs(userBase, outputPath, session, record, title, it->message, tvs);
                tv["%%MESSAGE%%"] = it->message;
            }
            else if (it->dataType == "19")
            {
                // Mini Program
                tv["%%MESSAGE%%"] = it->message;
            }
            else if (it->dataType == "22")
            {
                // Channels
                
                tv["%%MESSAGE%%"] = it->message;
            }
            else
            {
#ifndef NDEBUG
                writeFile(combinePath(outputPath, "../dbg", "fwdmsg_unknwn_" + it->dataType + ".txt"), it->message);
#endif
                tv["%%MESSAGE%%"] = it->message;
            }
            
            tv["%%NAME%%"] = it->displayName;
            tv["%%MSGID%%"] = record.msgId + "_" + it->dataId;
            tv["%%TIME%%"] = it->srcMsgTime.empty() ? it->msgTime : fromUnixTime(static_cast<unsigned int>(std::atoi(it->srcMsgTime.c_str())));
            
            localPortrait = portraitPath + (it->protrait.empty() ? "DefaultProfileHead@2x.png" : session.getLocalPortrait());
            remotePortrait = it->protrait;
            tv["%%AVATAR%%"] = localPortrait;
            if (!it->usrName.empty() && it->protrait.empty())
            {
                const Friend *f = (m_myself.getUsrName() == it->usrName) ? &m_myself : m_friends.getFriendByUid(it->usrName);
                std::string localPortrait = portraitPath + ((NULL != f) ? f->getLocalPortrait() : "DefaultProfileHead@2x.png");
                remotePortrait = (NULL != f) ? f->getPortrait() : "";
                
                tv["%%AVATAR%%"] = localPortrait;
                
                if ((m_options & SPO_IGNORE_AVATAR) == 0)
                {
                    if (!remotePortrait.empty() && !localPortrait.empty())
                    {
                        m_downloader.addTask(remotePortrait, combinePath(outputPath, localPortrait), record.createTime);
                    }
                }
            }
            
            if ((it->dataType == "17") && !it->nestedMsgs.empty())
            {
                parseForwardedMsgs(userBase, outputPath, session, record, it->message, it->nestedMsgs, tvs);
            }
        }
        
    }
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& endTv = tvs.back();
    endTv["%%MESSAGE%%"] = formatString(getLocaleString("%s Ends >>"), title.c_str());
    endTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg
    
    return true;
}

/////////////////////////////


/////////////////////////

std::string MessageParser::getDisplayTime(int ms) const
{
    if (ms < 1000) return "1\'";
    return std::to_string(std::round((double)ms)) + "\"";
}
