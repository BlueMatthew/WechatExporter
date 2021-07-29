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

MessageParser::MessageParser(const ITunesDb& iTunesDb, const ITunesDb& iTunesDbShare, TaskManager& taskManager, Friends& friends, Friend myself, int options, const std::string& resPath, const std::string& outputPath, std::function<std::string(const std::string&)>& localeFunc) : m_iTunesDb(iTunesDb), m_iTunesDbShare(iTunesDbShare), m_taskManager(taskManager), m_friends(friends), m_myself(myself), m_options(options), m_resPath(resPath), m_outputPath(outputPath)
{
    m_userBase = "Documents/" + m_myself.getHash();
    m_localFunction = std::move(localeFunc);
}

bool MessageParser::parse(WXMSG& msg, const Session& session, std::vector<TemplateValues>& tvs) const
{
    TemplateValues& tv = *(tvs.emplace(tvs.end(), "msg"));

    std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName() + "_files");
    
    tv["%%MSGID%%"] = msg.msgId;
    tv["%%NAME%%"] = "";
    tv["%%TIME%%"] = fromUnixTime(msg.createTime);
    tv["%%MSGTYPE%%"] = std::to_string(msg.type);
    tv["%%MESSAGE%%"] = "";
    
    std::string forwardedMsg;
    std::string forwardedMsgTitle;

    std::string senderId = "";
    if (session.isChatroom())
    {
        if (msg.des != 0)
        {
            std::string::size_type enter = msg.content.find(":\n");
            if (enter != std::string::npos && enter + 2 < msg.content.size())
            {
                senderId = msg.content.substr(0, enter);
                msg.content = msg.content.substr(enter + 2);
            }
        }
    }

#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(msg.type) + ".txt"), msg.content);
#endif
    
    switch (msg.type)
    {
        case MSGTYPE_TEXT:  // 1
            parseText(msg, session, tv);
            break;
        case MSGTYPE_IMAGE:  // 3
            parseImage(msg, session, tv);
            break;
        case MSGTYPE_VOICE:  // 34
            parseVoice(msg, session, tv);
            break;
        case MSGTYPE_PUSHMAIL:  // 35
            parsePushMail(msg, session, tv);
            break;
        case MSGTYPE_VERIFYMSG: // 37
            parseVerification(msg, session, tv);
            break;
        case MSGTYPE_POSSIBLEFRIEND:    // 40
            parsePossibleFriend(msg, session, tv);
            break;
        case MSGTYPE_SHARECARD:  // 42
        case MSGTYPE_IMCARD: // 66
            parseCard(msg, session, tv);
            break;
        case MSGTYPE_VIDEO: // 43
        case MSGTYPE_MICROVIDEO:    // 62
            parseVideo(msg, session, senderId, tv);
            break;
        case MSGTYPE_EMOTICON:   // 47
            parseEmotion(msg, session, tv);
            break;
        case MSGTYPE_LOCATION:   // 48
            parseLocation(msg, session, tv);
            break;
        case MSGTYPE_APP:    // 49
            parseAppMsg(msg, session, senderId, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case MSGTYPE_VOIPMSG:   // 50
            parseCall(msg, session, tv);
            break;
        case MSGTYPE_VOIPNOTIFY:    // 52
        case MSGTYPE_VOIPINVITE:    // 53
            parseSysNotice(msg, session, tv);
            break;
        case MSGTYPE_STATUSNOTIFY:  // 51
            parseStatusNotify(msg, session, tv);
            break;
        case MSGTYPE_NOTICE: // 64
            parseNotice(msg, session, tv);
            break;
        case MSGTYPE_SYSNOTICE: // 9999
            parseNotice(msg, session, tv);
            break;
        case MSGTYPE_SYS:   // 10000
        case MSGTYPE_RECALLED:  // 10002
            parseSystem(msg, session, tv);
            break;
        default:
#if !defined(NDEBUG) || defined(DBG_PERF)
            writeFile(combinePath(m_outputPath, "../dbg", "msg_unknwn_type_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
            parseText(msg, session, tv);
            break;
    }
    
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait/" : "Portrait/";
    // std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Emoji/" : "Emoji/";
    
    std::string localPortrait;

    const Friend* protraitUser = NULL;
    if (session.isChatroom())
    {
        tv["%%ALIGNMENT%%"] = (msg.des == 0) ? "right" : "left";
        if (msg.des == 0)
        {
            tv["%%NAME%%"] = m_myself.getDisplayName();    // CSS will prevent showing the name for self
            tv["%%AVATAR%%"] = portraitPath + m_myself.getLocalPortrait();
            // remotePortrait = m_myself.getPortrait();
            protraitUser = &m_myself;
        }
        else
        {
            if (!senderId.empty())
            {
                std::string senderHash = md5(senderId);
                std::string senderDisplayName = session.getMemberName(senderHash);
                const Friend *f = m_friends.getFriend(senderHash);
                if (senderDisplayName.empty() && NULL != f)
                {
                    senderDisplayName = f->getDisplayName();
                }
                tv["%%NAME%%"] = senderDisplayName.empty() ? senderId : senderDisplayName;
                if (NULL != f)
                {
                    protraitUser = f;
                }
                if (NULL == f)
                {
                    ensureDefaultPortraitIconExisted(portraitPath);
                }
                tv["%%AVATAR%%"] = portraitPath + ((NULL != f) ? f->getLocalPortrait() : "DefaultProfileHead@2x.png");
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
        if (msg.des == 0 || session.getUsrName() == m_myself.getUsrName())
        {
            tv["%%ALIGNMENT%%"] = "right";
            tv["%%NAME%%"] = m_myself.getDisplayName();
            tv["%%AVATAR%%"] = portraitPath + m_myself.getLocalPortrait();
            
            protraitUser = &m_myself;
        }
        else
        {
            tv["%%ALIGNMENT%%"] = "left";

            const Friend *f = m_friends.getFriend(session.getHash());
            if (NULL == f)
            {
                tv["%%NAME%%"] = session.getDisplayName();
                if (session.isPortraitEmpty())
                {
                    ensureDefaultPortraitIconExisted(portraitPath);
                }
                localPortrait = portraitPath + (session.isPortraitEmpty() ? "DefaultProfileHead@2x.png" : session.getLocalPortrait());
                // remotePortrait = session.getPortrait();
                tv["%%AVATAR%%"] = localPortrait;
                
                protraitUser = &session;
            }
            else
            {
                tv["%%NAME%%"] = f->getDisplayName();
                localPortrait = portraitPath + f->getLocalPortrait();
                // remotePortrait = f->getPortrait();
                tv["%%AVATAR%%"] = localPortrait;
                
                protraitUser = f;
            }
        }
    }

    if ((m_options & SPO_IGNORE_AVATAR) == 0)
    {
        if (NULL != protraitUser)
        {
            copyPortraitIcon(&session, *protraitUser, combinePath(m_outputPath, portraitPath));
            // m_downloader.addTask(remotePortrait, combinePath(m_outputPath, localPortrait), msg.createTime);
        }
    }
    
    if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
    {
        tv["%%NAME%%"] = safeHTML(tv["%%NAME%%"]);
    }

    if (!forwardedMsg.empty())
    {
        // This funtion will change tvs and causes tv invalid, so we do it at last
        parseForwardedMsgs(session, msg, forwardedMsgTitle, forwardedMsg, tvs);
    }
    return true;
}

void MessageParser::parsePortrait(const WXMSG& msg, const Session& session, const std::string& senderId, TemplateValues& tv) const
{
    
}

/////////////////////////////////////

void MessageParser::parseText(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
    {
        tv["%%MESSAGE%%"] = safeHTML(msg.content);
    }
    else
    {
        tv["%%MESSAGE%%"] = msg.content;
    }
}

void MessageParser::parseImage(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string vFile = combinePath(m_userBase, "Img", session.getHash(), msg.msgId);
    parseImage(m_outputPath, session.getOutputFileName() + "_files", vFile + ".pic", "", msg.msgId + ".jpg", vFile + ".pic_thum", msg.msgId + "_thumb.jpg", tv);
}

void MessageParser::parseVoice(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string audioSrc;
    int voiceLen = -1;
    const ITunesFile* audioSrcFile = NULL;
    if ((m_options & SPO_IGNORE_AUDIO) == 0)
    {
        std::string vLenStr;
        XmlParser xmlParser(msg.content);
        if (xmlParser.parseAttributeValue("/msg/voicemsg", "voicelength", vLenStr) && !vLenStr.empty())
        {
            voiceLen = std::stoi(vLenStr);
        }
        
        audioSrcFile = m_iTunesDb.findITunesFile(combinePath(m_userBase, "Audio", session.getHash(), msg.msgId + ".aud"));
        if (NULL != audioSrcFile)
        {
            audioSrc = m_iTunesDb.getRealPath(*audioSrcFile);
        }
    }
    
    bool result = false;
    if (!audioSrc.empty())
    {
#ifdef USING_ASYNC_TASK_FOR_MP3
        std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName() + "_files");
        ensureDirectoryExisted(assetsDir);
        std::string mp3Path = combinePath(assetsDir, msg.msgId + ".mp3");
        m_taskManager.convertAudio(&session, audioSrc, mp3Path, ITunesDb::parseModifiedTime(audioSrcFile->blob));
        
        tv.setName("audio");
        tv["%%AUDIOPATH%%"] = session.getOutputFileName() + "_files/" + msg.msgId + ".mp3";
        result = true;
#else
        m_pcmData.clear();
        if (silkToPcm(audioSrc, m_pcmData) && !m_pcmData.empty())
        {
            std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName() + "_files");
            std::string mp3Path = combinePath(assetsDir, msg.msgId + ".mp3");
            ensureDirectoryExisted(assetsDir);
            if (pcmToMp3(m_pcmData, mp3Path))
            {
                updateFileTime(mp3Path, ITunesDb::parseModifiedTime(audioSrcFile->blob));
                tv.setName("audio");
                tv["%%AUDIOPATH%%"] = session.getOutputFileName() + "_files/" + msg.msgId + ".mp3";
                result = true;
            }
        }
#endif
    }
    
    if (!result)
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = voiceLen == -1 ? getLocaleString("[Audio]") : formatString(getLocaleString("[Audio %s]"), getDisplayTime(voiceLen).c_str());
    }
}

void MessageParser::parsePushMail(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string subject;
    std::string digest;
    XmlParser xmlParser(msg.content);
    xmlParser.parseNodeValue("/msg/pushmail/content/subject", subject);
    xmlParser.parseNodeValue("/msg/pushmail/content/digest", digest);
    
    tv.setName("plainshare");

    tv["%%SHARINGURL%%"] = "##";
    tv["%%SHARINGTITLE%%"] = subject;
    tv["%%MESSAGE%%"] = digest;
}

void MessageParser::parseVideo(const WXMSG& msg, const Session& session, std::string& senderId, TemplateValues& tv) const
{
    std::map<std::string, std::string> attrs = { {"fromusername", ""}, {"cdnthumbwidth", ""}, {"cdnthumbheight", ""} };
    XmlParser xmlParser(msg.content);
    if (xmlParser.parseAttributesValue("/msg/videomsg", attrs))
    {
    }
    
    if (senderId.empty())
    {
        senderId = attrs["fromusername"];
    }
    
    std::string vfile = combinePath(m_userBase, "Video", session.getHash(), msg.msgId);
    parseVideo(m_outputPath, session.getOutputFileName() + "_files", vfile + ".mp4", msg.msgId + ".mp4", vfile + ".video_thum", msg.msgId + "_thum.jpg", attrs["cdnthumbwidth"], attrs["cdnthumbheight"], tv);
}

void MessageParser::parseEmotion(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string url;
    if ((m_options & SPO_IGNORE_EMOJI) == 0)
    {
        XmlParser xmlParser(msg.content);
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
        std::string emojiFile = url;
        std::smatch sm2;
        static std::regex pattern47_2("\\/(\\w+?)\\/\\w*$");
        if (std::regex_search(emojiFile, sm2, pattern47_2))
        {
            emojiFile = sm2[1];
        }
        else
        {
            static int uniqueFileName = 1000000000;
            emojiFile = std::to_string(uniqueFileName++);
        }
        
        std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Emoji/" : "Emoji/";
		std::string localEmojiPath = normalizePath((const std::string&)emojiPath);
		std::string localEmojiFile = localEmojiPath + emojiFile + ".gif";
		emojiFile = emojiPath + emojiFile + ".gif";
        ensureDirectoryExisted(combinePath(m_outputPath, localEmojiPath));
        
#ifdef USING_DOWNLOADER
        m_downloader.addTask(url, combinePath(m_outputPath, localEmojiFile), msg.createTime, "emoji");
#else
        m_taskManager.download(&session, url, "", combinePath(m_outputPath, localEmojiFile), msg.createTime, "", "emoji");
#endif
        
        tv.setName("emoji");
        tv["%%EMOJIPATH%%"] = emojiFile;
        tv["%%RAWEMOJIPATH%%"] = url;
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = getLocaleString("[Emoji]");
    }
}

void MessageParser::parseAppMsg(const WXMSG& msg, const Session& session, std::string& senderId, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv) const
{
    WXAPPMSG appMsg = {&msg, 0};
    XmlParser xmlParser(msg.content, true);
    if (senderId.empty())
    {
        xmlParser.parseNodeValue("/msg/fromusername", senderId);
    }
    
    std::string appMsgTypeStr;
    if (!xmlParser.parseNodeValue("/msg/appmsg/type", appMsgTypeStr))
    {
        // Failed to parse APPMSG type
#ifndef NDEBUG
        writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(msg.type) + "_app_invld_" + msg.msgId + ".txt"), msg.content);
#endif
        tv["%%MESSAGE%%"] = getLocaleString("[Link]");
        return;
    }

    tv.setName("plainshare");
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(msg.type) + "_app_" + appMsgTypeStr + ".txt"), msg.content);
#endif
    appMsg.appMsgType = std::atoi(appMsgTypeStr.c_str());
    xmlParser.parseAttributeValue("/msg/appmsg", "appid", appMsg.appId);
    if (!appMsg.appId.empty())
    {
        xmlParser.parseNodeValue("/msg/appinfo/appname", appMsg.appName);
        tv["%%APPNAME%%"] = appMsg.appName;
        std::string vFile = combinePath(m_userBase, "appicon", appMsg.appId + ".png");
        std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";

        if (m_iTunesDb.copyFile(vFile, combinePath(m_outputPath, portraitDir), "appicon_" + appMsg.appId + ".png"))
        {
            appMsg.localAppIcon = portraitDir + "/appicon_" + appMsg.appId + ".png";
            tv["%%APPICONPATH%%"] = appMsg.localAppIcon;
        }
    }

    switch (appMsg.appMsgType)
    {
        case APPMSGTYPE_TEXT: // 1
            parseAppMsgText(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_IMG: // 2
            parseAppMsgImage(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_AUDIO: // 3
            parseAppMsgAudio(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_VIDEO: // 4
            parseAppMsgVideo(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_URL: // 5
            parseAppMsgUrl(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_ATTACH: // 6
            parseAppMsgAttachment(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_OPEN: // 7
            parseAppMsgOpen(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_EMOJI: // 8
            parseAppMsgEmoji(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_VOICE_REMIND: // 9
            parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;

        case APPMSGTYPE_SCAN_GOOD: // 10
            parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;
            
        case APPMSGTYPE_GOOD: // 13
            parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;

        case APPMSGTYPE_EMOTION: // 15
            parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;

        case APPMSGTYPE_CARD_TICKET: // 16
            parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;

        case APPMSGTYPE_REALTIME_LOCATION: // 17
            parseAppMsgRtLocation(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_FWD_MSG: // 19
            parseAppMsgFwdMsg(appMsg, xmlParser, session, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case APPMSGTYPE_CHANNEL_CARD:   // 50
            parseAppMsgChannelCard(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_CHANNELS:   // 51
            parseAppMsgChannels(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_REFER:  // 57
            parseAppMsgRefer(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_TRANSFERS: // 2000
            parseAppMsgTransfer(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_RED_ENVELOPES: // 2001
            parseAppMsgRedPacket(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_READER_TYPE: // 100001
            parseAppMsgReaderType(appMsg, xmlParser, session, tv);
            break;
        default:
            parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;
    }
    
#ifndef NDEBUG
    std::string vThumbFile = m_userBase + "/OpenData/" + session.getHash() + "/" + appMsg.msg->msgId + ".pic_thum";
    std::string destPath = combinePath(m_outputPath, session.getOutputFileName() + "_files", appMsg.msg->msgId + "_thum.jpg");
    
    std::string fileId = m_iTunesDb.findFileId(vThumbFile);
    if (!fileId.empty() && !existsFile(destPath))
    {
        if (appMsg.appMsgType != 19)
        {
            // assert(false);
        }
    }
#endif
}

void MessageParser::parseCall(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    tv.setName("msg");
    tv["%%MESSAGE%%"] = getLocaleString("[Video/Audio Call]");
}

void MessageParser::parseLocation(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::map<std::string, std::string> attrs = { {"x", ""}, {"y", ""}, {"label", ""}, {"poiname", ""} };
    
    XmlParser xmlParser(msg.content);
    xmlParser.parseAttributesValue("/msg/location", attrs);
    
    std::string location = (!attrs["poiname"].empty() && !attrs["label"].empty()) ? (attrs["poiname"] + " - " + attrs["label"]) : (attrs["poiname"] + attrs["label"]);
    if (!location.empty())
    {
        tv["%%MESSAGE%%"] = formatString(getLocaleString("[Location] %s (%s,%s)"), location.c_str(), attrs["x"].c_str(), attrs["y"].c_str());
    }
    else
    {
        tv["%%MESSAGE%%"] = getLocaleString("[Location]");
    }
    tv.setName("msg");
}

void MessageParser::parseStatusNotify(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    parseText(msg, session, tv);
}

void MessageParser::parsePossibleFriend(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    parseText(msg, session, tv);
}

void MessageParser::parseVerification(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    parseText(msg, session, tv);
}

void MessageParser::parseCard(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    parseCard(session, m_outputPath, portraitDir, msg.content, tv);
}

void MessageParser::parseNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    tv.setName("notice");

    Json::Reader reader;
    Json::Value root;
    if (reader.parse(msg.content, root))
    {
        tv["%%MESSAGE%%"] = root["msgContent"].asString();
    }
}

void MessageParser::parseSysNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    tv.setName("notice");
    std::string sysMsg = msg.content;
    removeHtmlTags(sysMsg);
    tv["%%MESSAGE%%"] = sysMsg;
}

void MessageParser::parseSystem(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    tv.setName("notice");
    if (startsWith(msg.content, "<sysmsg"))
    {
        XmlParser xmlParser(msg.content, true);
        std::string sysMsgType;
        xmlParser.parseAttributeValue("/sysmsg", "type", sysMsgType);
        if (sysMsgType == "sysmsgtemplate")
        {
            std::string plainText;
            std::string templateContent;
            std::string templateType;
            xmlParser.parseAttributeValue("/sysmsg/sysmsgtemplate/content_template", "type", templateType);
#ifndef NDEBUG
            writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + "_" + sysMsgType + ".txt"), msg.content);
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
                    tv["%%MESSAGE%%"] = msg.content;
                }
            }
            else
            {
#ifndef NDEBUG
                writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + "_" + sysMsgType + ".txt"), msg.content);
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
                writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + "_" + sysMsgType + ".txt"), msg.content);
                assert(false);
#endif
            }
        }
    }
    else
    {
#ifndef NDEBUG
        if (startsWith(msg.content, "<") && !startsWith(msg.content, "<img"))
        {
            writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + "_unkwn_fmt_" + msg.msgId + ".txt"), msg.content);
            assert(false);
        }
#endif
        // Plain Text
        std::string sysMsg = msg.content;
        removeHtmlTags(sysMsg);
        tv["%%MESSAGE%%"] = sysMsg;
    }
}

////////////////////////////////

void MessageParser::parseAppMsgText(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    tv["%%MESSAGE%%"] = title.empty() ? getLocaleString("[Link]") : title;
}

void MessageParser::parseAppMsgImage(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgAudio(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgVideo(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgEmotion(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgUrl(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    std::string title;
    std::string desc;
    std::string url;
    std::string thumbUrl;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/des", desc);
    xmlParser.parseNodeValue("/msg/appmsg/url", url);
    
    // Check Local File
    std::string vThumbFile = m_userBase + "/OpenData/" + session.getHash() + "/" + appMsg.msg->msgId + ".pic_thum";
    std::string destPath = combinePath(m_outputPath, session.getOutputFileName() + "_files");
    if (m_iTunesDb.copyFile(vThumbFile, destPath, appMsg.msg->msgId + "_thum.jpg"))
    {
        thumbUrl = session.getOutputFileName() + "_files/" + appMsg.msg->msgId + "_thum.jpg";
    }
    else
    {
        xmlParser.parseNodeValue("/msg/appmsg/thumburl", thumbUrl);
        if (thumbUrl.empty())
        {
            thumbUrl = appMsg.localAppIcon;
        }
    }
    
    tv.setName(thumbUrl.empty() ? "plainshare" : "share");
    tv["%%SHARINGIMGPATH%%"] = thumbUrl;
    tv["%%SHARINGTITLE%%"] = title;
    tv["%%SHARINGURL%%"] = url;
    tv["%%MESSAGE%%"] = desc;
}

void MessageParser::parseAppMsgAttachment(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(appMsg.msg->type) + "_attach_" + appMsg.msg->msgId + ".txt"), appMsg.msg->content);
#endif
    std::string title;
    std::string attachFileExtName;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/appattach/fileext", attachFileExtName);
    
    std::string attachFileName = m_userBase + "/OpenData/" + session.getHash() + "/" + appMsg.msg->msgId;
    std::string attachOutputFileName = appMsg.msg->msgId;
    if (!attachFileExtName.empty())
    {
        attachFileName += "." + attachFileExtName;
        attachOutputFileName += "." + attachFileExtName;
    }
    parseFile(m_outputPath, session.getOutputFileName() + "_files", attachFileName, attachOutputFileName, title, tv);
}

void MessageParser::parseAppMsgOpen(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgEmoji(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    // Can't parse the detail info of emoji as the url is encrypted
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgRtLocation(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    tv["%%MESSAGE%%"] = getLocaleString("[Real-time Location]");
}

void MessageParser::parseAppMsgFwdMsg(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv) const
{
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/recorditem", forwardedMsg);
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_19.txt"), forwardedMsg);
#endif
    tv.setName("msg");
    tv["%%MESSAGE%%"] = title;

    forwardedMsgTitle = title;
}

void MessageParser::parseAppMsgCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgChannelCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    // Channel Card
    std::map<std::string, std::string> nodes = { {"username", ""}, {"avatar", ""}, {"nickname", ""}};
    xmlParser.parseNodesValue("/msg/appmsg/findernamecard/*", nodes);
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    
    parseChannelCard(session, portraitDir, nodes["username"], nodes["avatar"], "", nodes["nickname"], tv);
}

void MessageParser::parseAppMsgChannels(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_" + std::to_string(appMsg.appMsgType) + "_" + appMsg.msg->msgId + ".txt"), appMsg.msg->content);
#endif
    
    parseChannels(appMsg.msg->msgId, xmlParser, NULL, "/msg/appmsg/finderFeed", session, tv);
}

void MessageParser::parseAppMsgRefer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    // Refer Message
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    std::map<std::string, std::string> nodes = { {"displayname", ""}, {"content", ""}, {"type", ""}};
    if (xmlParser.parseNodesValue("/msg/appmsg/refermsg/*", nodes))
    {
#ifndef NDEBUG
        writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_" + std::to_string(APPMSGTYPE_REFER) + "_ref_" + nodes["type"] + " .txt"), nodes["content"]);
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

void MessageParser::parseAppMsgTransfer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    tv["%%MESSAGE%%"] = getLocaleString("[Transfer]");
}

void MessageParser::parseAppMsgRedPacket(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    tv["%%MESSAGE%%"] = getLocaleString("[Red Packet]");
}

void MessageParser::parseAppMsgReaderType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgUnknownType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
#if !defined(NDEBUG) || defined(DBG_PERF)
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_unknwn_" + std::to_string(appMsg.appMsgType) + ".txt"), appMsg.msg->content);
#endif
    parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

void MessageParser::parseAppMsgDefault(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
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

void MessageParser::parseFwdMsgText(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNode *itemNode, const Session& session, TemplateValues& tv) const
{
    std::string message;
    xmlParser.getChildNodeContent(itemNode, "datadesc", message);
    static std::vector<std::pair<std::string, std::string>> replaces = { {"\r\n", "<br />"}, {"\r", "<br />"}, {"\n", "<br />"}};
    replaceAll(message, replaces);
    tv["%%MESSAGE%%"] = message;
}

void MessageParser::parseFwdMsgImage(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNode *itemNode, const Session& session, TemplateValues& tv) const
{
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId;
    parseImage(m_outputPath, session.getOutputFileName() + "_files/" + fwdMsg.msg->msgId, vfile + fileExtName, vfile + fileExtName + "_pre3", fwdMsg.dataId + ".jpg", vfile + ".record_thumb", fwdMsg.dataId + "_thumb.jpg", tv);
}

void MessageParser::parseFwdMsgVideo(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId;
    parseVideo(m_outputPath, session.getOutputFileName() + "_files/" + fwdMsg.msg->msgId, vfile + fileExtName, fwdMsg.dataId + fileExtName, vfile + ".record_thumb", fwdMsg.dataId + "_thumb.jpg", "", "", tv);
                    
}

void MessageParser::parseFwdMsgLink(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
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
        std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId + ".record_thumb";
        hasThumb = m_iTunesDb.copyFile(vfile, combinePath(m_outputPath, session.getOutputFileName() + "_files", fwdMsg.msg->msgId), fwdMsg.dataId + "_thumb.jpg");
    }
    
    if (!(link.empty()))
    {
        tv.setName(hasThumb ? "share" : "plainshare");

        tv["%%SHARINGIMGPATH%%"] = session.getOutputFileName() + "_files/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId + "_thumb.jpg";
        tv["%%SHARINGURL%%"] = link;
        tv["%%SHARINGTITLE%%"] = title;
        tv["%%MESSAGE%%"] = message;
    }
    else
    {
        tv["%%MESSAGE%%"] = title;
    }
}

void MessageParser::parseFwdMsgLocation(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
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

    std::string location = (!message.empty() && !label.empty()) ? (message + " - " + label) : (message + label);
    if (!location.empty())
    {
        tv["%%MESSAGE%%"] = formatString(getLocaleString("[Location] %s (%s,%s)"), location.c_str(), lat.c_str(), lng.c_str());
    }
    else
    {
        tv["%%MESSAGE%%"] = getLocaleString("[Location]");
    }
    tv.setName("msg");
}

void MessageParser::parseFwdMsgAttach(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string message;
    xmlParser.getChildNodeContent(itemNode, "datatitle", message);
    
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId;
    parseFile(m_outputPath, session.getOutputFileName() + "_files/" + fwdMsg.msg->msgId, vfile + fileExtName, fwdMsg.dataId + fileExtName, message, tv);
}

void MessageParser::parseFwdMsgCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string cardContent;
    xmlParser.getChildNodeContent(itemNode, "datadesc", cardContent);
    
    std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    parseCard(session, m_outputPath, portraitDir, cardContent, tv);
}

void MessageParser::MessageParser::parseFwdMsgNestedFwdMsg(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, std::string& nestedFwdMsg, std::string& nestedFwdMsgTitle, TemplateValues& tv) const
{
    xmlParser.getChildNodeContent(itemNode, "datadesc", nestedFwdMsgTitle);
    xmlNodePtr nodeRecordInfo = XmlParser::getChildNode(itemNode, "recordinfo");
    if (NULL != nodeRecordInfo)
    {
        nestedFwdMsg = XmlParser::getNodeOuterXml(nodeRecordInfo);
    }
    
    tv["%%MESSAGE%%"] = nestedFwdMsgTitle;
}

void MessageParser::MessageParser::parseFwdMsgMiniProgram(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string title;
    xmlParser.getChildNodeContent(itemNode, "datatitle", title);
    tv["%%MESSAGE%%"] = title;
}

void MessageParser::MessageParser::parseFwdMsgChannels(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    parseChannels(fwdMsg.msg->msgId, xmlParser, itemNode, "./finderFeed", session, tv);
}

void MessageParser::MessageParser::parseFwdMsgChannelCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
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
    parseChannelCard(session, portraitDir, usrName, avatar, "", nickName, tv);
}

///////////////////////////////
// Implementation

void MessageParser::parseVideo(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& srcVideo, const std::string& destVideo, const std::string& srcThumb, const std::string& destThumb, const std::string& width, const std::string& height, TemplateValues& tv) const
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
        tv.setName("video");
        tv["%%THUMBPATH%%"] = hasThumb ? (sessionAssertsPath + "/" + destThumb) : "";
        tv["%%VIDEOPATH%%"] = sessionAssertsPath + "/" + destVideo;
        tv["%%MSGTYPE%%"] = "video";
    }
    else if (hasThumb)
    {
        tv.setName("thumb");
        tv["%%IMGTHUMBPATH%%"] = sessionAssertsPath + "/" + destThumb;
        tv["%%MESSAGE%%"] = getLocaleString("(Video Missed)");
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = getLocaleString("[Video]");
    }
    
    tv["%%VIDEOWIDTH%%"] = width;
    tv["%%VIDEOHEIGHT%%"] = height;
}

void MessageParser::parseImage(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& srcPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& tv) const
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
        tv.setName("image");
        tv["%%IMGPATH%%"] = sessionAssertsPath + "/" + dest;
        // If it is PDF mode, use the raw image directly for print quaility
        tv["%%IMGTHUMBPATH%%"] = sessionAssertsPath + "/" + (((!hasThumb) || (m_options & SPO_PDF_MODE)) ? dest : destThumb);
        tv["%%MSGTYPE%%"] = "image";
        tv["%%EXTRA_CLS%%"] = "raw-img";
    }
    else if (hasThumb)
    {
        tv.setName("thumb");
        tv["%%IMGTHUMBPATH%%"] = sessionAssertsPath + "/" + destThumb;
        tv["%%MESSAGE%%"] = "";
        tv["%%MSGTYPE%%"] = "image";
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = getLocaleString("[Photo]");
    }
}

void MessageParser::parseFile(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& tv) const
{
    bool hasFile = false;
    if ((m_options & SPO_IGNORE_FILE) == 0)
    {
        hasFile = m_iTunesDb.copyFile(src, combinePath(sessionPath, sessionAssertsPath), dest);
    }

    if (hasFile)
    {
        tv.setName("plainshare");
        tv["%%SHARINGURL%%"] = sessionAssertsPath + "/" + dest;
        tv["%%SHARINGTITLE%%"] = fileName;
        tv["%%MESSAGE%%"] = "";
        tv["%%MSGTYPE%%"] = "file";
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = formatString(getLocaleString("[File: %s]"), fileName.c_str());
    }
}

void MessageParser::parseCard(const Session& session, const std::string& sessionPath, const std::string& portraitDir, const std::string& cardMessage, TemplateValues& tv) const
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

    tv["%%CARDTYPE%%"] = getLocaleString("[Contact Card]");
    XmlParser xmlParser(cardMessage, true);
    if (xmlParser.parseAttributesValue("/msg", attrs) && !attrs["nickname"].empty())
    {
        std::string portraitUrl = attrs["bigheadimgurl"].empty() ? attrs["smallheadimgurl"] : attrs["bigheadimgurl"];
        bool hasPortrait = !attrs["bigheadimgurl"].empty() || !attrs["smallheadimgurl"].empty();
        if (!attrs["username"].empty() && hasPortrait)
        {
            tv.setName("card");
            // Some username is too long to be created on windows, have to use its md5 string
			std::string imgFileName = startsWith(attrs["username"], "wxid_") ? attrs["username"] : md5(attrs["username"]);
            tv["%%CARDNAME%%"] = attrs["nickname"];
            tv["%%CARDIMGPATH%%"] = portraitDir + "/" + imgFileName + ".jpg";
			std::string localPortraitDir = normalizePath(portraitDir);
            std::string localFile = combinePath(localPortraitDir, imgFileName + ".jpg");
            ensureDirectoryExisted(combinePath(sessionPath, localPortraitDir));
			std::string output = combinePath(sessionPath, localFile);
#ifdef USING_DOWNLOADER
            m_downloader.addTask(portraitUrl, combinePath(sessionPath, localFile), 0, "card");
#else
            m_taskManager.download(&session, attrs["bigheadimgurl"], attrs["smallheadimgurl"], combinePath(sessionPath, localFile), 0, "", "card");
#endif
        }
        else if (!attrs["nickname"].empty())
        {
            tv["%%MESSAGE%%"] = formatString(getLocaleString("[Contact Card] %s"), attrs["nickname"].c_str());
        }
        else
        {
            tv["%%MESSAGE%%"] = getLocaleString("[Contact Card]");
        }
    }
    else
    {
        tv["%%MESSAGE%%"] = getLocaleString("[Contact Card]");
    }
    tv["%%EXTRA_CLS%%"] = "contact-card";
}

void MessageParser::parseChannelCard(const Session& session, const std::string& portraitDir, const std::string& usrName, const std::string& avatar, const std::string& avatarLD, const std::string& name, TemplateValues& tv) const
{
    bool hasImg = false;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        hasImg = (!usrName.empty() && !avatar.empty());
    }
    tv["%%CARDTYPE%%"] = getLocaleString("[Channel Card]");
    if (!name.empty())
    {
        if (hasImg)
        {
            tv.setName("card");
            tv["%%CARDNAME%%"] = name;
            tv["%%CARDIMGPATH%%"] = portraitDir + "/" + usrName + ".jpg";
			std::string localPortraitDir = normalizePath(portraitDir);
            std::string localFile = combinePath(localPortraitDir, usrName + ".jpg");
            ensureDirectoryExisted(combinePath(m_outputPath, localPortraitDir));
#ifdef USING_DOWNLOADER
            m_downloader.addTask(avatar, combinePath(m_outputPath, localfile), 0, "card");
#else
            m_taskManager.download(&session, avatar, avatarLD, combinePath(m_outputPath, localFile), 0, "", "card");
#endif
        }
        else
        {
            tv.setName("msg");
            tv["%%MESSAGE%%"] = formatString(getLocaleString("[Channel Card] %s"), name.c_str());
        }
    }
    else
    {
        tv["%%MESSAGE%%"] = getLocaleString("[Channel Card]");
    }
    tv["%%EXTRA_CLS%%"] = "channel-card";
}

void MessageParser::parseChannels(const std::string& msgId, const XmlParser& xmlParser, xmlNodePtr parentNode, const std::string& finderFeedXPath, const Session& session, TemplateValues& tv) const
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
    
    const std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
    
    tv["%%CARDNAME%%"] = nodes["nickname"];
    tv["%%CHANNELS%%"] = getLocaleString("Channels");
    tv["%%MESSAGE%%"] = nodes["desc"];
    tv["%%EXTRA_CLS%%"] = "channels";
    
    if (!thumbUrl.empty())
    {
        tv.setName("channels");
        tv["%%MSGTYPE%%"] = "channels";
        std::string thumbFile = session.getOutputFileName() + "_files/" + msgId + ".jpg";
		std::string localThumbFile = session.getOutputFileName() + "_files" + DIR_SEP + msgId + ".jpg";
        tv["%%CHANNELTHUMBPATH%%"] = thumbFile;
        ensureDirectoryExisted(combinePath(m_outputPath, session.getOutputFileName() + "_files"));

#ifdef USING_DOWNLOADER
        m_downloader.addTask(thumbUrl, combinePath(m_outputPath, localThumbFile), 0, "thumb");
#else
        m_taskManager.download(&session, thumbUrl, "", combinePath(m_outputPath, localThumbFile), 0, "", "thumb");
#endif
        
        if (!nodes["avatar"].empty())
        {
            std::string fileName = nodes["username"].empty() ? nodes["objectId"] : nodes["username"];
            tv["%%CARDIMGPATH%%"] = portraitDir + "/" + fileName + ".jpg";
			std::string localPortraitDir = normalizePath(portraitDir);
            std::string localFile = combinePath(localPortraitDir, fileName + ".jpg");
            ensureDirectoryExisted(combinePath(m_outputPath, localPortraitDir));
#ifdef USING_DOWNLOADER
            m_downloader.addTask(nodes["avatar"], combinePath(m_outputPath, localFile), 0, "card");
#else
            m_taskManager.download(&session, nodes["avatar"], "", combinePath(m_outputPath, localFile), 0, "", "card");
#endif
        }

        tv["%%CHANNELURL%%"] = videoNodes["url"];
    }
}

bool MessageParser::parseForwardedMsgs(const Session& session, const WXMSG& msg, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs) const
{
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait/" : "Portrait/";
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& beginTv = tvs.back();
    beginTv["%%MESSAGE%%"] = formatString(getLocaleString("<< %s"), title.c_str());
    beginTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg
    
    XmlParser xmlParser(message);
    XmlParser::XPathEnumerator enumerator(xmlParser, "/recordinfo/datalist/dataitem");
    while (enumerator.hasNext())
    {
        xmlNodePtr node = enumerator.nextNode();
        if (NULL != node)
        {
            WXFWDMSG fmsg = { &msg };

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
            
#if !defined(NDEBUG) || defined(DBG_PERF)
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
#if !defined(NDEBUG) || defined(DBG_PERF)
                    writeFile(combinePath(m_outputPath, "../dbg", "fwdmsg_unknwn_" + fmsg.dataType + ".txt"), fmsg.rawMessage);
#endif
                    break;
            }
            
            tv["%%NAME%%"] = fmsg.displayName;
            tv["%%MSGID%%"] = msg.msgId + "_" + fmsg.dataId;
            tv["%%TIME%%"] = fmsg.srcMsgTime.empty() ? fmsg.msgTime : fromUnixTime(static_cast<unsigned int>(std::atoi(fmsg.srcMsgTime.c_str())));

            // std::string localPortrait;
            // bool hasPortrait = false;
            // localPortrait = combinePath(portraitPath, fmsg.usrName + ".jpg");
            if (copyPortraitIcon(&session, fmsg.usrName, fmsg.portrait, fmsg.portraitLD, combinePath(m_outputPath, portraitPath)))
            {
                tv["%%AVATAR%%"] = portraitPath + "/" + fmsg.usrName + ".jpg";
            }
            else
            {
                ensureDefaultPortraitIconExisted(portraitPath);
                tv["%%AVATAR%%"] = portraitPath + "DefaultProfileHead@2x.png";
            }

            if ((dataType == FWDMSG_DATATYPE_NESTED_FWD_MSG) && !nestedFwdMsg.empty())
            {
                parseForwardedMsgs(session, msg, nestedFwdMsgTitle, nestedFwdMsg, tvs);
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

bool MessageParser::copyPortraitIcon(const Session* session, const std::string& usrName, const std::string& portraitUrl, const std::string& portraitUrlLD, const std::string& destPath) const
{
    return copyPortraitIcon(session, usrName, md5(usrName), portraitUrl, portraitUrlLD, destPath);
}

bool MessageParser::copyPortraitIcon(const Session* session, const Friend& f, const std::string& destPath) const
{
    return copyPortraitIcon(session, f.getUsrName(), f.getHash(), f.getPortrait(), f.getSecondaryPortrait(), destPath);
}

bool MessageParser::copyPortraitIcon(const Session* session, const std::string& usrName, const std::string& usrNameHash, const std::string& portraitUrl, const std::string& portraitUrlLD, const std::string& destPath) const
{
    std::string destFileName = usrName + ".jpg";
    std::string avatarPath = "share/" + m_myself.getHash() + "/session/headImg/" + usrNameHash + ".pic";
    bool hasPortrait = m_iTunesDbShare.copyFile(avatarPath, destPath, destFileName);
    if (!hasPortrait)
    {
        if (portraitUrl.empty() && portraitUrlLD.empty())
        {
            const Friend *f = (m_myself.getUsrName() == usrName) ? &m_myself : m_friends.getFriend(usrNameHash);
            if (NULL != f)
            {
                std::string url = f->getPortrait();
                std::string urlLD = f->getSecondaryPortrait();
                if (!url.empty() || !urlLD.empty())
                {
					std::string localDestPath = normalizePath(destPath);
#ifdef USING_DOWNLOADER
                    m_downloader.addTask(url, combinePath(localDestPath, destFileName), 0, "avatar");
#else
                    m_taskManager.download(session, url, urlLD, combinePath(localDestPath, destFileName), 0, "", "avatar");
#endif
                    hasPortrait = true;
                }
            }
        }
        else
        {
			std::string localDestPath = normalizePath(destPath);
#ifdef USING_DOWNLOADER
            m_downloader.addTask(portraitUrl, combinePath(localDestPath, destFileName), 0, "avatar");
#else
            m_taskManager.download(session, portraitUrl, portraitUrlLD, combinePath(localDestPath, destFileName), 0, combinePath(m_resPath, "res", "DefaultProfileHead@2x.png"), "avatar");
#endif
            hasPortrait = true;
        }
    }
    
    return hasPortrait;
}

void MessageParser::ensureDefaultPortraitIconExisted(const std::string& portraitPath) const
{
    std::string dest = combinePath(m_outputPath, portraitPath);
    ensureDirectoryExisted(dest);
    dest = combinePath(dest, "DefaultProfileHead@2x.png");
    if (!existsFile(dest))
    {
        copyFile(combinePath(m_resPath, "res", "DefaultProfileHead@2x.png"), dest, false);
    }
}
