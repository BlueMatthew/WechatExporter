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

MessageParser::MessageParser(const ITunesDb& iTunesDb, const ITunesDb& iTunesDbShare, Downloader& downloader, Friends& friends, Friend myself, int options, const std::string& resPath, const std::string& outputPath, std::function<std::string(const std::string&)>& localeFunc) : m_iTunesDb(iTunesDb), m_iTunesDbShare(iTunesDbShare), m_downloader(downloader), m_friends(friends), m_myself(myself), m_options(options), m_resPath(resPath), m_outputPath(outputPath)
{
    m_userBase = "Documents/" + m_myself.getHash();
    m_localFunction = std::move(localeFunc);
}

bool MessageParser::parse(MsgRecord& record, const Session& session, std::vector<TemplateValues>& tvs)
{
    TemplateValues& tv = *(tvs.emplace(tvs.end(), "msg"));

    std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName() + "_files");
    
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
            parseText(record, session, tv);
            break;
        case MSGTYPE_IMAGE:  // 3
            parseImage(record, session, tv);
            break;
        case MSGTYPE_VOICE:  // 34
            parseVoice(record, session, tv);
            break;
        case MSGTYPE_PUSHMAIL:  // 35
            parsePushMail(record, session, tv);
            break;
        case MSGTYPE_VERIFYMSG: // 37
            parseVerification(record, session, tv);
            break;
        case MSGTYPE_POSSIBLEFRIEND:    // 40
            parsePossibleFriend(record, session, tv);
            break;
        case MSGTYPE_SHARECARD:  // 42
        case MSGTYPE_IMCARD: // 66
            parseCard(record, session, tv);
            break;
        case MSGTYPE_VIDEO: // 43
        case MSGTYPE_MICROVIDEO:    // 62
            parseVideo(record, session, senderId, tv);
            break;
        case MSGTYPE_EMOTICON:   // 47
            parseEmotion(record, session, tv);
            break;
        case MSGTYPE_LOCATION:   // 48
            parseLocation(record, session, tv);
            break;
        case MSGTYPE_APP:    // 49
            parseAppMsg(record, session, senderId, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case MSGTYPE_VOIPMSG:   // 50
            parseCall(record, session, tv);
            break;
        case MSGTYPE_VOIPNOTIFY:    // 52
        case MSGTYPE_VOIPINVITE:    // 53
            parseSysNotice(record, session, tv);
            break;
        case MSGTYPE_STATUSNOTIFY:  // 51
            parseStatusNotify(record, session, tv);
            break;
        case MSGTYPE_NOTICE: // 64
            parseNotice(record, session, tv);
            break;
        case MSGTYPE_SYSNOTICE: // 9999
            parseNotice(record, session, tv);
            break;
        case MSGTYPE_SYS:   // 10000
        case MSGTYPE_RECALLED:  // 10002
            parseSystem(record, session, tv);
            break;
        default:
#ifndef NDEBUG
            writeFile(combinePath(m_outputPath, "../dbg", "msg_unknwn_type_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
            parseText(record, session, tv);
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
            // copyPortraitIcon(senderId, remotePortrait, combinePath(m_outputPath, portraitPath));
            
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
        parseForwardedMsgs(session, record, forwardedMsgTitle, forwardedMsg, tvs);
    }
    return true;
}

void MessageParser::parseText(const MsgRecord& record, const Session& session, TemplateValues& tv)
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

void MessageParser::parseImage(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
    std::string vFile = combinePath(m_userBase, "Img", session.getHash(), record.msgId);
    parseImage(m_outputPath, session.getOutputFileName() + "_files", vFile + ".pic", "", record.msgId + ".jpg", vFile + ".pic_thum", record.msgId + "_thumb.jpg", tv);
}

void MessageParser::parseVoice(const MsgRecord& record, const Session& session, TemplateValues& tv)
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
        
        audioSrcFile = m_iTunesDb.findITunesFile(combinePath(m_userBase, "Audio", session.getHash(), record.msgId + ".aud"));
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
        std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName() + "_files");
        std::string mp3Path = combinePath(assetsDir, record.msgId + ".mp3");

        silkToPcm(audioSrc, m_pcmData);
        
        ensureDirectoryExisted(assetsDir);
        pcmToMp3(m_pcmData, mp3Path);
        if (audioSrcFile != NULL)
        {
            updateFileTime(mp3Path, ITunesDb::parseModifiedTime(audioSrcFile->blob));
        }

        tv.setName("audio");
        tv["%%AUDIOPATH%%"] = session.getOutputFileName() + "_files/" + record.msgId + ".mp3";
    }
}

void MessageParser::parsePushMail(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
    std::string subject;
    std::string digest;
    XmlParser xmlParser(record.message);
    xmlParser.parseNodeValue("/msg/pushmail/content/subject", subject);
    xmlParser.parseNodeValue("/msg/pushmail/content/digest", digest);
    
    tv.setName("plainshare");

    tv["%%SHARINGURL%%"] = "##";
    tv["%%SHARINGTITLE%%"] = subject;
    tv["%%MESSAGE%%"] = digest;
}

void MessageParser::parseVideo(const MsgRecord& record, const Session& session, std::string& senderId, TemplateValues& tv)
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
    
    std::string vfile = combinePath(m_userBase, "Video", session.getHash(), record.msgId);
    parseVideo(m_outputPath, session.getOutputFileName() + "_files", vfile + ".mp4", record.msgId + ".mp4", vfile + ".video_thum", record.msgId + "_thum.jpg", attrs["cdnthumbwidth"], attrs["cdnthumbheight"], tv);
}

void MessageParser::parseEmotion(const MsgRecord& record, const Session& session, TemplateValues& tv)
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
        
        std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Emoji/" : "Emoji/";
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

void MessageParser::parseAppMsg(const MsgRecord& record, const Session& session, std::string& senderId, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv)
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
        std::string vFile = combinePath(m_userBase, "appicon", appMsgInfo.appId + ".png");
        std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";

        if (m_iTunesDb.copyFile(vFile, combinePath(m_outputPath, portraitDir), "appicon_" + appMsgInfo.appId + ".png"))
        {
            appMsgInfo.localAppIcon = portraitDir + "/appicon_" + appMsgInfo.appId + ".png";
            tv["%%APPICONPATH%%"] = appMsgInfo.localAppIcon;
        }
    }

    switch (appMsgInfo.appMsgType)
    {
        case APPMSGTYPE_TEXT: // 1
            parseAppMsgText(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_IMG: // 2
            parseAppMsgImage(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_AUDIO: // 3
            parseAppMsgAudio(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_VIDEO: // 4
            parseAppMsgVideo(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_URL: // 5
            parseAppMsgUrl(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_ATTACH: // 6
            parseAppMsgAttachment(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_OPEN: // 7
            parseAppMsgOpen(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_EMOJI: // 8
            parseAppMsgEmoji(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_VOICE_REMIND: // 9
            parseAppMsgUnknownType(appMsgInfo, xmlParser, session, tv);
            break;

        case APPMSGTYPE_SCAN_GOOD: // 10
            parseAppMsgUnknownType(appMsgInfo, xmlParser, session, tv);
            break;
            
        case APPMSGTYPE_GOOD: // 13
            parseAppMsgUnknownType(appMsgInfo, xmlParser, session, tv);
            break;

        case APPMSGTYPE_EMOTION: // 15
            parseAppMsgUnknownType(appMsgInfo, xmlParser, session, tv);
            break;

        case APPMSGTYPE_CARD_TICKET: // 16
            parseAppMsgUnknownType(appMsgInfo, xmlParser, session, tv);
            break;

        case APPMSGTYPE_REALTIME_LOCATION: // 17
            parseAppMsgRtLocation(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_FWD_MSG: // 19
            parseAppMsgFwdMsg(appMsgInfo, xmlParser, session, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case APPMSGTYPE_CHANNEL_CARD:   // 50
            parseAppMsgChannelCard(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_CHANNELS:   // 51
            parseAppMsgChannels(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_REFER:  // 57
            parseAppMsgRefer(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_TRANSFERS: // 2000
            parseAppMsgTransfer(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_RED_ENVELOPES: // 2001
            parseAppMsgRedPacket(appMsgInfo, xmlParser, session, tv);
            break;
        case APPMSGTYPE_READER_TYPE: // 100001
            parseAppMsgReaderType(appMsgInfo, xmlParser, session, tv);
            break;
        default:
            parseAppMsgUnknownType(appMsgInfo, xmlParser, session, tv);
            break;
    }
}

void MessageParser::parseCall(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
    tv.setName("msg");
    tv["%%MESSAGE%%"] = getLocaleString("[Video/Audio Call]");
}

void MessageParser::parseLocation(const MsgRecord& record, const Session& session, TemplateValues& tv)
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

void MessageParser::parseStatusNotify(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    parseText(record, session, tv);
}

void MessageParser::parsePossibleFriend(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    parseText(record, session, tv);
}

void MessageParser::parseVerification(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    parseText(record, session, tv);
}

void MessageParser::parseCard(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    parseCard(m_outputPath, portraitDir, record.message, tv);
}

void MessageParser::parseNotice(const MsgRecord& record, const Session& session, TemplateValues& tv)
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

void MessageParser::parseSysNotice(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + record.msgId + ".txt"), record.message);
#endif
    tv.setName("notice");
    std::string sysMsg = record.message;
    removeHtmlTags(sysMsg);
    tv["%%MESSAGE%%"] = sysMsg;
}

void MessageParser::parseSystem(const MsgRecord& record, const Session& session, TemplateValues& tv)
{
    tv.setName("notice");
    if (startsWith(record.message, "<sysmsg"))
    {
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
            if (startsWith(templateType, "tmpl_type_profile") || templateType == "tmpl_type_admin_explain" || templateType == "new_tmpl_type_succeed_contact")
            {
                // tmpl_type_profilewithrevokeqrcode
                // tmpl_type_profilewithrevoke
                // tmpl_type_profile
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
        else if (sysMsgType == "editrevokecontent")
        {
            std::string content;
            xmlParser.parseNodeValue("/sysmsg/" + sysMsgType + "/text", content);
            tv["%%MESSAGE%%"] = content;
        }
        else
        {
            // Try to find plain first
            std::string plainText;
            if (xmlParser.parseNodeValue("/sysmsg/" + sysMsgType + "/plain", plainText) && !plainText.empty())
            {
                tv["%%MESSAGE%%"] = plainText;
            }
            else
            {
#ifndef NDEBUG
                writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + "_" + sysMsgType + ".txt"), record.message);
                assert(false);
#endif
            }
        }
    }
    else
    {
#ifndef NDEBUG
        if (startsWith(record.message, "<") && !startsWith(record.message, "<img"))
        {
            writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(record.type) + "_unkwn_fmt_" + record.msgId + ".txt"), record.message);
            assert(false);
        }
#endif
        // Plain Text
        std::string sysMsg = record.message;
        removeHtmlTags(sysMsg);
        tv["%%MESSAGE%%"] = sysMsg;
    }
}

////////////////////////////////

void MessageParser::parseAppMsgText(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    tv["%%MESSAGE%%"] = title.empty() ? getLocaleString("[Link]") : title;
}

void MessageParser::parseAppMsgImage(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgAudio(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgVideo(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgEmotion(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgUrl(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
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

void MessageParser::parseAppMsgAttachment(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    std::string title;
    std::string attachFileExtName;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/appattach/fileext", attachFileExtName);
    
    std::string attachFileName = m_userBase + "/OpenData/" + session.getHash() + "/" + appMsgInfo.msgId;
    if (!attachFileExtName.empty())
    {
        attachFileName += "." + attachFileExtName;
    }
    
    std::string attachOutputFileName = appMsgInfo.msgId + "_" + title;
    parseFile(m_outputPath, session.getOutputFileName() + "_files", attachFileName, attachOutputFileName, title, tv);
}

void MessageParser::parseAppMsgOpen(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgEmoji(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    // Can't parse the detail info of emoji as the url is encrypted
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgRtLocation(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    tv["%%MESSAGE%%"] = getLocaleString("[Real-time Location]");
}

void MessageParser::parseAppMsgFwdMsg(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv)
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

void MessageParser::parseAppMsgCard(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgChannelCard(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    // Channel Card
    std::map<std::string, std::string> nodes = { {"username", ""}, {"avatar", ""}, {"nickname", ""}};
    xmlParser.parseNodesValue("/msg/appmsg/findernamecard/*", nodes);
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    
    parseChannelCard(m_outputPath, portraitDir, nodes["username"], nodes["avatar"], nodes["nickname"], tv);
}

void MessageParser::parseAppMsgChannels(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsgInfo.msgType) + "_app_" + std::to_string(appMsgInfo.appMsgType) + "_" + appMsgInfo.msgId + ".txt"), appMsgInfo.message);
#endif
    
    parseChannels(appMsgInfo.msgId, xmlParser, NULL, "/msg/appmsg/finderFeed", session, tv);
}

void MessageParser::parseAppMsgRefer(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
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

void MessageParser::parseAppMsgTransfer(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    tv["%%MESSAGE%%"] = getLocaleString("[Transfer]");
}

void MessageParser::parseAppMsgRedPacket(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    tv["%%MESSAGE%%"] = getLocaleString("[Red Packet]");
}

void MessageParser::parseAppMsgReaderType(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgUnknownType(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsgInfo.msgType) + "_app_unknwn_" + std::to_string(appMsgInfo.appMsgType) + ".txt"), appMsgInfo.message);
#endif
    parseAppMsgDefault(appMsgInfo, xmlParser, session, tv);
}

void MessageParser::parseAppMsgDefault(const AppMsgInfo& appMsgInfo, const XmlParser& xmlParser, const Session& session, TemplateValues& tv)
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

////////////////////////////////

void MessageParser::parseFwdMsgText(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNode *itemNode, const Session& session, TemplateValues& tv)
{
    std::string message;
    xmlParser.getChildNodeContent(itemNode, "datadesc", message);
    static std::vector<std::pair<std::string, std::string>> replaces = { {"\r\n", "<br />"}, {"\r", "<br />"}, {"\n", "<br />"}};
    replaceAll(message, replaces);
    tv["%%MESSAGE%%"] = message;
}

void MessageParser::parseFwdMsgImage(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNode *itemNode, const Session& session, TemplateValues& tv)
{
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msgId + "/" + fwdMsg.dataId;
    parseImage(m_outputPath, session.getOutputFileName() + "_files/" + fwdMsg.msgId, vfile + fileExtName, vfile + fileExtName + "_pre3", fwdMsg.dataId + ".jpg", vfile + ".record_thumb", fwdMsg.dataId + "_thumb.jpg", tv);
}

void MessageParser::parseFwdMsgVideo(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msgId + "/" + fwdMsg.dataId;
    parseVideo(m_outputPath, session.getOutputFileName() + "_files/" + fwdMsg.msgId, vfile + fileExtName, fwdMsg.dataId + fileExtName, vfile + ".record_thumb", fwdMsg.dataId + "_thumb.jpg", "", "", tv);
                    
}

void MessageParser::parseFwdMsgLink(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    std::string link;
    std::string title;
    std::string thumbUrl;
    std::string message;
    xmlNodePtr urlItemNode = xmlParser.getChildNode(itemNode, "weburlitem");
    if (NULL != urlItemNode)
    {
        XmlParser::getChildNodeContent(urlItemNode, "title", title);
        XmlParser::getChildNodeContent(urlItemNode, "link", link);
        XmlParser::getChildNodeContent(urlItemNode, "thumburl", thumbUrl);
        XmlParser::getChildNodeContent(urlItemNode, "desc", message);
    }
    
    
    bool hasThumb = false;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msgId + "/" + fwdMsg.dataId + ".record_thumb";
        hasThumb = m_iTunesDb.copyFile(vfile, combinePath(m_outputPath, session.getOutputFileName() + "_files", fwdMsg.msgId), fwdMsg.dataId + "_thumb.jpg");
    }
    
    if (!(link.empty()))
    {
        tv.setName(hasThumb ? "share" : "plainshare");

        tv["%%SHARINGIMGPATH%%"] = session.getOutputFileName() + "_files/" + fwdMsg.msgId + "/" + fwdMsg.dataId + "_thumb.jpg";
        tv["%%SHARINGURL%%"] = link;
        tv["%%SHARINGTITLE%%"] = title;
        tv["%%MESSAGE%%"] = message;
    }
    else
    {
        tv["%%MESSAGE%%"] = title;
    }
}

void MessageParser::parseFwdMsgLocation(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    std::string label;
    std::string message;
    std::string lng;
    std::string lat;
    xmlNodePtr locItemNode = xmlParser.getChildNode(itemNode, "locitem");
    if (NULL != locItemNode)
    {
        XmlParser::getChildNodeContent(locItemNode, "label", label);
        XmlParser::getChildNodeContent(locItemNode, "poiname", message);
        XmlParser::getChildNodeContent(locItemNode, "lat", lat);
        XmlParser::getChildNodeContent(locItemNode, "lng", lng);
    }

    if (!lat.empty() && !lng.empty() && !message.empty())
    {
        tv["%%MESSAGE%%"] = formatString(getLocaleString("[Location (%s,%s) %s]"), lat.c_str(), lng.c_str(), message.c_str());
    }
    else
    {
        tv["%%MESSAGE%%"] = getLocaleString("[Location]");
    }
    tv.setName("msg");
}

void MessageParser::parseFwdMsgAttach(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    std::string message;
    xmlParser.getChildNodeContent(itemNode, "datatitle", message);
    
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msgId + "/" + fwdMsg.dataId;
    parseFile(m_outputPath, session.getOutputFileName() + "_files/" + fwdMsg.msgId, vfile + fileExtName, fwdMsg.dataId + fileExtName, message, tv);
}

void MessageParser::parseFwdMsgCard(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    std::string cardContent;
    xmlParser.getChildNodeContent(itemNode, "datadesc", cardContent);
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    parseCard(m_outputPath, portraitDir, cardContent, tv);
}

void MessageParser::MessageParser::parseFwdMsgNestedFwdMsg(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, std::string& nestedFwdMsg, std::string& nestedFwdMsgTitle, TemplateValues& tv)
{
    xmlParser.getChildNodeContent(itemNode, "datadesc", nestedFwdMsgTitle);
    xmlNodePtr nodeRecordInfo = XmlParser::getChildNode(itemNode, "recordinfo");
    if (NULL != nodeRecordInfo)
    {
        nestedFwdMsg = XmlParser::getNodeOuterXml(nodeRecordInfo);
    }
    
    tv["%%MESSAGE%%"] = nestedFwdMsgTitle;
}

void MessageParser::MessageParser::parseFwdMsgMiniProgram(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    std::string title;
    xmlParser.getChildNodeContent(itemNode, "datatitle", title);
    tv["%%MESSAGE%%"] = title;
}

void MessageParser::MessageParser::parseFwdMsgChannels(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    parseChannels(fwdMsg.msgId, xmlParser, itemNode, "./finderFeed", session, tv);
}

void MessageParser::MessageParser::parseFwdMsgChannelCard(const ForwardMsg& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv)
{
    std::string usrName;
    std::string avatar;
    std::string nickName;
    xmlNodePtr cardItemNode = xmlParser.getChildNode(itemNode, "finderShareNameCard");
    if (NULL != cardItemNode)
    {
        XmlParser::getChildNodeContent(cardItemNode, "username", usrName);
        XmlParser::getChildNodeContent(cardItemNode, "avatar", avatar);
        XmlParser::getChildNodeContent(cardItemNode, "nickname", nickName);
    }
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    parseChannelCard(m_outputPath, portraitDir, usrName, avatar, nickName, tv);
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
        std::string fullAssertsPath = combinePath(sessionPath, sessionAssertsPath);
        hasThumb = m_iTunesDb.copyFile(srcThumb, fullAssertsPath, destThumb);
        if (!srcPre.empty())
        {
            hasImage = m_iTunesDb.copyFile(srcPre, fullAssertsPath, dest);
        }
        if (!hasImage)
        {
            hasImage = m_iTunesDb.copyFile(src, fullAssertsPath, dest);
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
        else if (!attrs["nickname"].empty())
        {
            templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[Contact Card] %s"), attrs["nickname"].c_str());
        }
        else
        {
            templateValues["%%MESSAGE%%"] = getLocaleString("[Contact Card]");
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

void MessageParser::parseChannels(const std::string& msgId, const XmlParser& xmlParser, xmlNodePtr parentNode, const std::string& finderFeedXPath, const Session& session, TemplateValues& tv)
{
    // Channels SHI PIN HAO
    std::map<std::string, std::string> nodes = { {"objectId", ""}, {"nickname", ""}, {"avatar", ""}, {"desc", ""}, {"mediaCount", ""}, {"feedType", ""}, {"desc", ""}, {"username", ""}};
    std::map<std::string, std::string> videoNodes = { {"mediaType", ""}, {"url", ""}, {"thumbUrl", ""}, {"coverUrl", ""}, {"videoPlayDuration", ""}};
    
    if (NULL == parentNode)
    {
        xmlParser.parseNodesValue(finderFeedXPath + "/*", nodes);
        xmlParser.parseNodesValue(finderFeedXPath + "/mediaList/media/*", videoNodes);
    }
    else
    {
        xmlParser.parseChildNodesValue(parentNode, finderFeedXPath + "/*", nodes);
        xmlParser.parseChildNodesValue(parentNode, finderFeedXPath + "/mediaList/media/*", videoNodes);
    }
#ifndef NDEBUG
    assert(nodes["mediaCount"] == "1");
#endif
    std::string thumbUrl;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        thumbUrl = videoNodes["thumbUrl"].empty() ? videoNodes["coverUrl"] : videoNodes["thumbUrl"];
    }
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    
    tv["%%CARDNAME%%"] = nodes["nickname"];
    tv["%%CHANNELS%%"] = getLocaleString("Channels");
    tv["%%MESSAGE%%"] = nodes["desc"];
    tv["%%EXTRA_CLS%%"] = "channels";
    
    if (!thumbUrl.empty())
    {
        tv.setName("channels");
        
        std::string thumbFile = session.getOutputFileName() + "_files/" + msgId + ".jpg";
        tv["%%CHANNELTHUMBPATH%%"] = thumbFile;
        ensureDirectoryExisted(combinePath(m_outputPath, session.getOutputFileName() + "_files"));
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

bool MessageParser::parseForwardedMsgs(const Session& session, const MsgRecord& record, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs)
{
    XmlParser xmlParser(message);
    std::vector<ForwardMsg> forwardedMsgs;
    ForwardMsgsHandler handler(xmlParser, record.msgId, forwardedMsgs);
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait/" : "Portrait/";
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& beginTv = tvs.back();
    beginTv["%%MESSAGE%%"] = formatString(getLocaleString("<< %s"), title.c_str());
    beginTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg
    
    XmlParser::XPathEnumerator enumerator(xmlParser, "/recordinfo/datalist/dataitem");
    while (enumerator.hasNext())
    {
        xmlNodePtr node = enumerator.nextNode();
        if (NULL != node)
        {
            ForwardMsg fmsg = { record.msgId };

            XmlParser::getNodeAttributeValue(node, "datatype", fmsg.dataType);
            XmlParser::getNodeAttributeValue(node, "dataid", fmsg.dataId);
            XmlParser::getNodeAttributeValue(node, "subtype", fmsg.subType);
            
            XmlParser::getChildNodeContent(node, "sourcename", fmsg.displayName);
            XmlParser::getChildNodeContent(node, "sourcetime", fmsg.msgTime);
            XmlParser::getChildNodeContent(node, "srcMsgCreateTime", fmsg.srcMsgTime);
            XmlParser::getChildNodeContent(node, "datafmt", fmsg.dataFormat);
            xmlNodePtr srcNode = XmlParser::getChildNode(node, "dataitemsource");
            if (NULL != srcNode)
            {
                if (!XmlParser::getChildNodeContent(srcNode, "realchatname", fmsg.usrName))
                {
                    XmlParser::getChildNodeContent(srcNode, "fromusr", fmsg.usrName);
                }
            }
            
#ifndef NDEBUG
            fmsg.rawMessage = xmlParser.getNodeOuterXml(node);
            writeFile(combinePath(m_outputPath, "../dbg", "fwdmsg_" + fmsg.dataType + ".txt"), fmsg.rawMessage);
#endif
            TemplateValues& tv = *(tvs.emplace(tvs.end(), "msg"));
            tv["%%ALIGNMENT%%"] = "left";
            tv["%%EXTRA_CLS%%"] = "fmsg";   // forwarded msg
            
            std::string nestedFwdMsgTitle;
            std::string nestedFwdMsg;
            int dataType = fmsg.dataType.empty() ? 0 : std::atoi(fmsg.dataType.c_str());
            switch (dataType)
            {
                case FWDMSG_DATATYPE_TEXT:  // 1
                    parseFwdMsgText(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_IMAGE: // 2
                    parseFwdMsgImage(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_VIDEO: // 4
                    parseFwdMsgVideo(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_LINK: //
                    parseFwdMsgLink(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_LOCATION: //
                    parseFwdMsgLocation(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_ATTACH: //
                    parseFwdMsgAttach(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_CARD: //
                    parseFwdMsgCard(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_NESTED_FWD_MSG: //
                    parseFwdMsgNestedFwdMsg(fmsg, xmlParser, node, session, nestedFwdMsg, nestedFwdMsgTitle, tv);
                    break;
                case FWDMSG_DATATYPE_MINI_PROGRAM: //   19
                    parseFwdMsgMiniProgram(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_CHANNELS: //   22
                    parseFwdMsgChannels(fmsg, xmlParser, node, session, tv);
                    break;
                case FWDMSG_DATATYPE_CHANNEL_CARD: //    26
                    parseFwdMsgChannelCard(fmsg, xmlParser, node, session, tv);
                    break;
                default:
                    parseFwdMsgText(fmsg, xmlParser, node, session, tv);
#ifndef NDEBUG
                    writeFile(combinePath(m_outputPath, "../dbg", "fwdmsg_unknwn_" + fmsg.dataType + ".txt"), fmsg.rawMessage);
#endif
                    break;
            }
            
            tv["%%NAME%%"] = fmsg.displayName;
            tv["%%MSGID%%"] = record.msgId + "_" + fmsg.dataId;
            tv["%%TIME%%"] = fmsg.srcMsgTime.empty() ? fmsg.msgTime : fromUnixTime(static_cast<unsigned int>(std::atoi(fmsg.srcMsgTime.c_str())));

            // std::string localPortrait;
            // bool hasPortrait = false;
            // localPortrait = combinePath(portraitPath, fmsg.usrName + ".jpg");
            if (copyPortraitIcon(fmsg.usrName, fmsg.portrait, combinePath(m_outputPath, portraitPath)))
            {
                tv["%%AVATAR%%"] = portraitPath + "/" + fmsg.usrName + ".jpg";
            }
            else
            {
                ensureDefaultPortraitIconExisted(portraitPath);
                tv["%%AVATAR%%"] = portraitPath + "/DefaultProfileHead@2x.png";
            }

            if ((dataType == FWDMSG_DATATYPE_NESTED_FWD_MSG) && !nestedFwdMsg.empty())
            {
                parseForwardedMsgs(session, record, nestedFwdMsgTitle, nestedFwdMsg, tvs);
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

bool MessageParser::copyPortraitIcon(const std::string& usrName, const std::string& portraitUrl, const std::string& destPath)
{
    return copyPortraitIcon(usrName, md5(usrName), portraitUrl, destPath);
}

bool MessageParser::copyPortraitIcon(const std::string& usrName, const std::string& usrNameHash, const std::string& portraitUrl, const std::string& destPath)
{
    std::string destFileName = usrName + ".jpg";
    std::string avatarPath = "share/" + m_myself.getHash() + "/session/headImg/" + usrNameHash + ".pic";
    bool hasPortrait = m_iTunesDbShare.copyFile(avatarPath, destPath, destFileName);
    if (!hasPortrait)
    {
        if (portraitUrl.empty())
        {
            const Friend *f = (m_myself.getUsrName() == usrName) ? &m_myself : m_friends.getFriend(usrNameHash);
            if (NULL != f)
            {
                std::string url = f->getPortrait();
                if (!url.empty())
                {
                    m_downloader.addTask(portraitUrl, combinePath(destPath, destFileName), 0);
                    hasPortrait = true;
                }
            }
        }
        else
        {
            m_downloader.addTask(portraitUrl, combinePath(destPath, destFileName), 0);
            hasPortrait = true;
        }
    }
    
    return hasPortrait;
}

void MessageParser::ensureDefaultPortraitIconExisted(const std::string& portraitPath)
{
    std::string dest = combinePath(m_outputPath, portraitPath);
    ensureDirectoryExisted(dest);
    dest = combinePath(m_outputPath, "DefaultProfileHead@2x.png");
    if (!existsFile(dest))
    {
        copyFile(combinePath(m_resPath, "res", "DefaultProfileHead@2x.png"), dest, false);
    }
}
