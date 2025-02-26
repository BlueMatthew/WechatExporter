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

#define ALIGNMENT_LEFT  "left"
#define ALIGNMENT_RIGHT  "right"

#define DIR_ASSETS  "Files"

MessageParser::MessageParser(const ITunesDb& iTunesDb, const ITunesDb& iTunesDbShare, TaskManager& taskManager, Friends& friends, Friend myself, const ExportOption& options, const std::string& resPath, const std::string& outputPath, const ResManager& resManager) : m_iTunesDb(iTunesDb), m_iTunesDbShare(iTunesDbShare), m_resManager(resManager), m_taskManager(taskManager), m_friends(friends), m_myself(myself), m_options(options), m_resPath(resPath), m_outputPath(outputPath)
{
    m_userBase = "Documents/" + m_myself.getHash();
}

bool MessageParser::parse(WXMSG& msg, const Session& session, std::vector<TemplateValues>& tvs) const
{
    TemplateValues& tv = *(tvs.emplace(tvs.end(), "msg"));
    
    tv["%%MSGID%%"] = msg.msgId;
    tv["%%NAME%%"] = "";
    tv["%%WXNAME%%"] = "";
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
    writeFile(combinePath(m_outputPath, "../dbg", "msg_type_" + std::to_string(msg.type) + ".txt"), msg.content);
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + msg.msgId + ".txt"), msg.content);
#endif
#if !defined(NDEBUG) || defined(DBG_PERF)
    writeFile(combinePath(m_outputPath, "../dbg", "lastmsg.txt"), msg.content);
#endif
    
    bool res = false;
    switch (msg.type)
    {
        case MSGTYPE_TEXT:  // 1
            res = parseText(msg, session, tv);
            break;
        case MSGTYPE_IMAGE:  // 3
            res = parseImage(msg, session, tv);
            break;
        case MSGTYPE_VOICE:  // 34
            res = parseVoice(msg, session, tv);
            break;
        case MSGTYPE_PUSHMAIL:  // 35
            res = parsePushMail(msg, session, tv);
            break;
        case MSGTYPE_VERIFYMSG: // 37
            res = parseVerification(msg, session, tv);
            break;
        case MSGTYPE_POSSIBLEFRIEND:    // 40
            res = parsePossibleFriend(msg, session, tv);
            break;
        case MSGTYPE_SHARECARD:  // 42
        case MSGTYPE_IMCARD: // 66
            res = parseCard(msg, session, tv);
            break;
        case MSGTYPE_VIDEO: // 43
        case MSGTYPE_MICROVIDEO:    // 62
            res = parseVideo(msg, session, senderId, tv);
            break;
        case MSGTYPE_EMOTICON:   // 47
            res = parseEmotion(msg, session, tv);
            break;
        case MSGTYPE_LOCATION:   // 48
            res = parseLocation(msg, session, tv);
            break;
        case MSGTYPE_APP:    // 49
            res = parseAppMsg(msg, session, senderId, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case MSGTYPE_VOIPMSG:   // 50
            res = parseCall(msg, session, tv);
            break;
        case MSGTYPE_VOIPNOTIFY:    // 52
        case MSGTYPE_VOIPINVITE:    // 53
            res = parseSysNotice(msg, session, tv);
            break;
        case MSGTYPE_STATUSNOTIFY:  // 51
            res = parseStatusNotify(msg, session, tv);
            break;
        case MSGTYPE_NOTICE: // 64
            res = parseNotice(msg, session, tv);
            break;
        case MSGTYPE_SYSNOTICE: // 9999
            res = parseNotice(msg, session, tv);
            break;
        case MSGTYPE_SYS:   // 10000
        case MSGTYPE_RECALLED:  // 10002
            res = parseSystem(msg, session, tv);
            break;
        default:
#if !defined(NDEBUG) || defined(DBG_PERF)
            writeFile(combinePath(m_outputPath, "../dbg", "msg_unknwn_type_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
            res = parseText(msg, session, tv);
            break;
    }
    
#ifndef NDEBUG
    if (m_resManager.hasEmojiTag(tv["%%MESSAGE%%"]))
    {
        writeFile(combinePath(m_outputPath, "../dbg", "wxemoji" + std::to_string(msg.type) + ".txt"), tv["%%MESSAGE%%"] + "\r\n\r\n");
        appendFile(combinePath(m_outputPath, "../dbg", "wxemoji" + std::to_string(msg.type) + ".txt"), msg.content);
    }
    
#endif

    std::string portraitPath = (DIR_ASSETS DIR_SEP_STR "Portrait" DIR_SEP_STR);
    std::string portraitUrlPath = (DIR_ASSETS "/Portrait/");

    // std::string localPortrait;

    const Friend* protraitUser = NULL;
    if (session.isChatroom())
    {
        tv["%%ALIGNMENT%%"] = (msg.des == 0) ? ALIGNMENT_RIGHT : ALIGNMENT_LEFT;
        if (msg.des == 0)
        {
            tv["%%NAME%%"] = m_myself.getDisplayName();    // CSS will prevent showing the name for self
            tv["%%WXNAME%%"] = m_myself.getWxName();
            tv["%%AVATAR%%"] = portraitUrlPath + m_myself.getLocalPortrait();
            // remotePortrait = m_myself.getPortrait();
            protraitUser = &m_myself;
        }
        else
        {
            if (!senderId.empty())
            {
                std::string senderHash = md5(senderId);
                std::string senderDisplayName = session.getMemberName(senderId);
                const Friend *f = m_friends.getFriend(senderHash);
                if (senderDisplayName.empty() && NULL != f)
                {
                    senderDisplayName = f->getDisplayName();
                }
                tv["%%NAME%%"] = senderDisplayName.empty() ? senderId : senderDisplayName;
                if (NULL != f)
                {
                    protraitUser = f;
                    tv["%%WXNAME%%"] = f->getWxName();
                }
                if (NULL == f)
                {
                    ensureDefaultPortraitIconExisted(combinePath(session.getOutputFileName(), portraitPath));
                }
                tv["%%AVATAR%%"] = portraitUrlPath + ((NULL != f) ? f->getLocalPortrait() : "DefaultAvatar.png");
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
            tv["%%ALIGNMENT%%"] = ALIGNMENT_RIGHT;
            tv["%%NAME%%"] = m_myself.getDisplayName();
            tv["%%WXNAME%%"] = m_myself.getWxName();
            tv["%%AVATAR%%"] = portraitUrlPath + m_myself.getLocalPortrait();
            
            protraitUser = &m_myself;
        }
        else
        {
            tv["%%ALIGNMENT%%"] = ALIGNMENT_LEFT;

            const Friend *f = m_friends.getFriend(session.getHash());
            if (NULL == f)
            {
                tv["%%NAME%%"] = session.getDisplayName();
                tv["%%WXNAME%%"] = session.getWxName();
                if (session.isPortraitEmpty())
                {
                    ensureDefaultPortraitIconExisted(portraitPath);
                }
                // localPortrait = combinePath(session.getOutputFileName(), portraitPath + (session.isPortraitEmpty() ? "DefaultAvatar.png" : session.getLocalPortrait()));
                // remotePortrait = session.getPortrait();
                tv["%%AVATAR%%"] = portraitUrlPath + (session.isPortraitEmpty() ? "DefaultAvatar.png" : session.getLocalPortrait());
                
                protraitUser = &session;
            }
            else
            {
                tv["%%NAME%%"] = f->getDisplayName();
                tv["%%WXNAME%%"] = f->getWxName();
                // localPortrait = portraitPath + f->getLocalPortrait();
                // remotePortrait = f->getPortrait();
                tv["%%AVATAR%%"] = portraitUrlPath + f->getLocalPortrait();
                
                protraitUser = f;
            }
        }
    }

    if (!m_options.isTextMode())
    {
        if (NULL != protraitUser)
        {
            copyPortraitIcon(&session, *protraitUser, combinePath(m_outputPath, session.getOutputFileName(), portraitPath));
            // m_downloader.addTask(remotePortrait, combinePath(m_outputPath, session.getOutputFileName(), localPortrait), msg.createTime);
        }
    }
    
    if (!m_options.isTextMode())
    {
        tv["%%NAME%%"] = safeHTML(tv["%%NAME%%"]);
    }

    if (!forwardedMsg.empty())
    {
        // This funtion will change tvs and causes tv invalid, so we do it at last
        parseForwardedMsgs(session, msg, forwardedMsgTitle, forwardedMsg, tvs);
    }
    return res;
}

bool MessageParser::parsePortrait(const WXMSG& msg, const Session& session, const std::string& senderId, TemplateValues& tv) const
{
    return true;
}

/////////////////////////////////////

bool MessageParser::parseText(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    if (!m_options.isTextMode())
    {
        // std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName(), DIR_ASSETS);
        // tv["%%MESSAGE%%"] = safeHTML(msg.content);
        // tv["%%MESSAGE%%"] = m_resManager.convertEmojis(safeHTML(msg.content), combinePath(m_outputPath, session.getOutputFileName()), DIR_ASSETS);
        tv["%%MESSAGE%%"] = m_resManager.convertEmojis(msg.content, combinePath(m_outputPath, session.getOutputFileName()), DIR_ASSETS, DIR_ASSETS);
    }
    else
    {
        tv["%%MESSAGE%%"] = msg.content;
    }
    
    return true;
}

bool MessageParser::parseImage(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string vFile = combinePath(m_userBase, "Img", session.getHash(), msg.msgId);
    return parseImage(combinePath(m_outputPath, session.getOutputFileName()), DIR_ASSETS, DIR_ASSETS, vFile + ".pic", vFile + ".pic_hd", msg.msgId + ".jpg", vFile + ".pic_thum", msg.msgId + "_thumb.jpg", tv);
}

bool MessageParser::parseVoice(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string audioSrc;
    int voiceLen = -1;
    std::string vLenStr;
    std::string voiceFormat;
    XmlParser xmlParser(msg.content);
    if (xmlParser.parseAttributeValue("/msg/voicemsg", "voicelength", vLenStr) && !vLenStr.empty())
    {
        voiceLen = std::stoi(vLenStr);
    }
    if (!xmlParser.parseAttributeValue("/msg/voicemsg", "voiceformat", voiceFormat))
    {
        voiceFormat = "4";
    }
    const ITunesFile* audioSrcFile = NULL;
    if (!m_options.isTextMode())
    {
        audioSrcFile = m_iTunesDb.findITunesFile(combinePath(m_userBase, "Audio", session.getHash(), msg.msgId + ".aud"));
        if (NULL != audioSrcFile)
        {
            audioSrc = m_iTunesDb.getRealPath(*audioSrcFile);
        }
    }
    
    bool result = false;
    if (!audioSrc.empty())
    {
        // std::string audCopyPath = combinePath(m_outputPath, "..", "dbg");
        // if (!existsDirectory(audCopyPath))
        // {
        //     makeDirectory(audCopyPath);
        // }
        // audCopyPath = combinePath(audCopyPath, msg.msgId + ".aud");
        // copyFile(audioSrc, combinePath(audCopyPath, msg.msgId + ".aud"));
        
        // writeFile(combinePath(audCopyPath, msg.msgId + ".aud.xml"), msg.content);
        
#ifdef USING_ASYNC_TASK_FOR_MP3
        std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName(), DIR_ASSETS);
        ensureDirectoryExisted(assetsDir);
        std::string mp3Path = combinePath(assetsDir, msg.msgId + ".mp3");
        m_taskManager.convertAudio(&session, audioSrc, mp3Path, (voiceFormat == "0") ? TaskManager::AUDIO_FORMAT_AMR : TaskManager::AUDIO_FORMAT_SILK, ITunesDb::parseModifiedTime(audioSrcFile->blob));
        result = true;
#else
        std::string assetsDir = combinePath(m_outputPath, session.getOutputFileName(), DIR_ASSETS);
        std::string mp3Path = combinePath(assetsDir, msg.msgId + ".mp3");
        ensureDirectoryExisted(assetsDir);
        
        m_pcmData.clear();
        m_error.clear();
        std::string err;
        bool isSilk = false;

        // SILK-v3
        if (silkToPcm(audioSrc, m_pcmData, isSilk, &err) && !m_pcmData.empty())
        {
#ifndef NDEBUG
            // std::string audCopyPath = combinePath(m_outputPath, "..", "dbg");
            // audCopyPath = combinePath(audCopyPath, "aud", msg.msgId + ".pcm");
            // writeFile(audCopyPath, m_pcmData);
#endif
            if (pcmToMp3(m_pcmData, mp3Path, &err))
            {
                // copyFile(mp3Path, combinePath(audCopyPath, msg.msgId + ".slk." + voiceFormat + ".mp3"));
                result = true;
            }
        }
        else if (!isSilk)
        {
            // AMR
            if (amrToPcm(audioSrc, m_pcmData, &err) && !m_pcmData.empty())
            {
                if (amrPcmToMp3(m_pcmData, mp3Path, &err))
                {
                    // copyFile(mp3Path, combinePath(audCopyPath, msg.msgId + ".amr." + (voiceFormat != "" ? voiceFormat : "empty") + ".mp3"));
#ifndef NDEBUG
                    // std::string audCopyPath = combinePath(m_outputPath, "..", "dbg");
                    // audCopyPath = combinePath(audCopyPath, "aud", msg.msgId + ".amr.mp3");
                    // copyFile(mp3Path, audCopyPath);
#endif
                    result = true;
                }
            }
        }
        
        if (result)
        {
            updateFileTime(mp3Path, ITunesDb::parseModifiedTime(audioSrcFile->blob));
        }
        else
        {
            m_error += err + "\r\nvoiceFormat:*" + voiceFormat + "* " + combinePath(m_userBase, "Audio", session.getHash(), msg.msgId + ".aud");
        }
        
        if (!result)
        {
#ifndef NDEBUG
            std::string audCopyPath = combinePath(m_outputPath, "..", "dbg");
            if (!existsDirectory(audCopyPath))
            {
                makeDirectory(audCopyPath);
            }
            
            audCopyPath = combinePath(audCopyPath, msg.msgId + ".aud");
            copyFile(audioSrc, audCopyPath);
            
            writeFile(audCopyPath + ".xml", msg.content);
            
#endif
        }
#endif
    }
    
    if (result)
    {
        tv.setName("audio");
        tv["%%AUDIOPATH%%"] = (DIR_ASSETS "/") + msg.msgId + ".mp3";
        tv["%%AUDIOTIME%%"] = getDisplayTime(voiceLen);
        tv["%%AUDIOLENGTH%%"] = voiceLen == -1 ? "" : std::to_string((int)(voiceLen * 300 / 60000));    // max-width: 300px <=> 60s
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = voiceLen == -1 ? m_resManager.getLocaleString("[Audio]") : formatString(m_resManager.getLocaleString("[Audio %s]"), getDisplayTime(voiceLen).c_str());
    }
    
    return result;
}

bool MessageParser::parsePushMail(const WXMSG& msg, const Session& session, TemplateValues& tv) const
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
    
    return true;
}

bool MessageParser::parseVideo(const WXMSG& msg, const Session& session, std::string& senderId, TemplateValues& tv) const
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
    return parseVideo(combinePath(m_outputPath, session.getOutputFileName()), DIR_ASSETS, DIR_ASSETS, vfile + ".mp4", vfile + "_raw.mp4", msg.msgId + ".mp4", vfile + ".video_thum", msg.msgId + "_thum.jpg", attrs["cdnthumbwidth"], attrs["cdnthumbheight"], tv);
}

bool MessageParser::parseEmotion(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string url;
    if (!m_options.isTextMode())
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
        tv.setName("emoji");
        
        if (!m_options.isUsingRemoteEmoji())
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
            
            std::string emojiPath = (DIR_ASSETS DIR_SEP_STR "Emoji" DIR_SEP_STR);
            std::string emojiUrlPath = (DIR_ASSETS "/Emoji/");
            std::string localEmojiPath = normalizePath((const std::string&)emojiPath);
            std::string localEmojiFile = localEmojiPath + emojiFile + ".gif";
            ensureDirectoryExisted(combinePath(m_outputPath, session.getOutputFileName(), localEmojiPath));
            
#ifdef USING_DOWNLOADER
            m_downloader.addTask(url, combinePath(m_outputPath, session.getOutputFileName(), localEmojiFile), msg.createTime, "emoji");
#else
            m_taskManager.download(&session, url, "", combinePath(m_outputPath, session.getOutputFileName(), localEmojiFile), msg.createTime, "", "emoji");
#endif
            
            tv["%%EMOJIPATH%%"] = emojiUrlPath + emojiFile + ".gif";
            tv["%%RAWEMOJIPATH%%"] = url;
        }
        else
        {
            tv["%%EMOJIPATH%%"] = url;
            tv["%%RAWEMOJIPATH%%"] = url;
        }
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Emoji]");
    }
    
    return true;
}

bool MessageParser::parseAppMsg(const WXMSG& msg, const Session& session, std::string& senderId, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv) const
{
    WXAPPMSG appMsg = {&msg, 0, std::string(), std::string(), senderId};
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
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Link]");
        return true;
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
        std::string portraitDir = (DIR_ASSETS DIR_SEP_STR "Portrait");

        if (m_iTunesDb.copyFile(vFile, combinePath(m_outputPath, session.getOutputFileName(), portraitDir), "appicon_" + appMsg.appId + ".png"))
        {
            std::string portraitUrlDir = (DIR_ASSETS "/Portrait");
            appMsg.localAppIcon = portraitUrlDir + "/appicon_" + appMsg.appId + ".png";
            tv["%%APPICONPATH%%"] = appMsg.localAppIcon;
        }
    }

    bool res = false;
    switch (appMsg.appMsgType)
    {
        case APPMSGTYPE_TEXT: // 1
            res = parseAppMsgText(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_IMG: // 2
            res = parseAppMsgImage(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_AUDIO: // 3
            res = parseAppMsgAudio(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_VIDEO: // 4
            res = parseAppMsgVideo(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_URL: // 5
            res = parseAppMsgUrl(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_ATTACH: // 6
            res = parseAppMsgAttachment(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_OPEN: // 7
            res = parseAppMsgOpen(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_EMOJI: // 8
            res = parseAppMsgEmoji(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_VOICE_REMIND: // 9
            res = parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_SCAN_GOOD: // 10
            res = parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_GOOD: // 13
            res = parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_EMOTION: // 15
            res = parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_CARD_TICKET: // 16
            res = parseAppMsgDefault(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_REALTIME_LOCATION: // 17
            res = parseAppMsgRtLocation(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_FWD_MSG: // 19
            res = parseAppMsgFwdMsg(appMsg, xmlParser, session, forwardedMsg, forwardedMsgTitle, tv);
            break;
        case APPMSGTYPE_NOTE:    // 24
            res = parseAppMsgNote(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_CHANNEL_CARD:   // 50
            res = parseAppMsgChannelCard(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_CHANNELS:   // 51
            res = parseAppMsgChannels(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_REFER:  // 57
            res = parseAppMsgRefer(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_PAT:    // 62
            res = parseAppMsgPat(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_TRANSFERS: // 2000
            res = parseAppMsgTransfer(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_RED_ENVELOPES: // 2001
            res = parseAppMsgRedPacket(appMsg, xmlParser, session, tv);
            break;
        case APPMSGTYPE_READER_TYPE: // 100001
            res = parseAppMsgReaderType(appMsg, xmlParser, session, tv);
            break;
        default:
            res = parseAppMsgUnknownType(appMsg, xmlParser, session, tv);
            break;
    }
    
#ifndef NDEBUG
    if (m_resManager.hasEmojiTag(tv["%%MESSAGE%%"]))
    {
        writeFile(combinePath(m_outputPath, "../dbg", "wxemoji_app_" + std::to_string(msg.type) + "_" + std::to_string(appMsg.appMsgType) + ".txt"), tv["%%MESSAGE%%"] + "\r\n\r\n");
        appendFile(combinePath(m_outputPath, "../dbg", "wxemoji_app_" + std::to_string(msg.type) + "_" + std::to_string(appMsg.appMsgType) + ".txt"), msg.content);
    }
#endif
    
#ifndef NDEBUG
    std::string vThumbFile = m_userBase + "/OpenData/" + session.getHash() + "/" + appMsg.msg->msgId + ".pic_thum";
    std::string destPath = combinePath(m_outputPath, session.getOutputFileName(), DIR_ASSETS, appMsg.msg->msgId + "_thum.jpg");
    
    std::string fileId = m_iTunesDb.findFileId(vThumbFile);
    if (!fileId.empty() && !existsFile(destPath))
    {
        if (appMsg.appMsgType != 19)
        {
            // assert(false);
        }
    }
#endif
    
    return res;
}

bool MessageParser::parseCall(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    tv.setName("msg");
    tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Video/Audio Call]");
    return true;
}

bool MessageParser::parseLocation(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::map<std::string, std::string> attrs = { {"x", ""}, {"y", ""}, {"label", ""}, {"poiname", ""} };
    
    XmlParser xmlParser(msg.content);
    xmlParser.parseAttributesValue("/msg/location", attrs);
    
    std::string location = (!attrs["poiname"].empty() && !attrs["label"].empty()) ? (attrs["poiname"] + " - " + attrs["label"]) : (attrs["poiname"] + attrs["label"]);
    if (!location.empty())
    {
        tv["%%MESSAGE%%"] = formatString(m_resManager.getLocaleString("[Location] %s (%s,%s)"), location.c_str(), attrs["x"].c_str(), attrs["y"].c_str());
    }
    else
    {
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Location]");
    }
    tv.setName("msg");
    
    return true;
}

bool MessageParser::parseStatusNotify(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    return parseText(msg, session, tv);
}

bool MessageParser::parsePossibleFriend(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    return parseText(msg, session, tv);
}

bool MessageParser::parseVerification(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    return parseText(msg, session, tv);
}

bool MessageParser::parseCard(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
    std::string portraitDir = (DIR_ASSETS DIR_SEP_STR "Portrait");
    std::string portraitUrlDir = (DIR_ASSETS "/Portrait");
    return parseCard(session, m_outputPath, portraitDir, portraitUrlDir, msg.content, tv);
}

bool MessageParser::parseNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    tv.setName("notice");

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    Json::Value root;
    if (reader->parse(msg.content.c_str(), msg.content.c_str() + msg.content.size(), &root, NULL))
    {
        tv["%%MESSAGE%%"] = root["msgContent"].asString();
    }
    return true;
}

bool MessageParser::parseSysNotice(const WXMSG& msg, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg_" + std::to_string(msg.type) + msg.msgId + ".txt"), msg.content);
#endif
    tv.setName("notice");
    std::string sysMsg = msg.content;
    removeHtmlTags(sysMsg);
    tv["%%MESSAGE%%"] = sysMsg;
    return true;
}

bool MessageParser::parseSystem(const WXMSG& msg, const Session& session, TemplateValues& tv) const
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
        else if (sysMsgType == "paymsg")
        {
            std::string content;
            xmlParser.parseNodeValue("/sysmsg/" + sysMsgType + "/appmsgcontent", content);
            content = decodeUrl(content);
            removeHtmlTags(content);
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
    else if (startsWith(msg.content, "<_wc_custom_link_"))
    {
        // <_wc_custom_link_ href="weixin://me/profile/mypatsuffix">设置拍一拍</_wc_custom_link_>，朋友拍了拍你的头像后会出现设置的内容。
        
        std::string plainText = msg.content;
        removeHtmlTags(plainText);
        tv["%%MESSAGE%%"] = plainText;
#ifndef NDEBUG
        plainText.clear();
        auto pos = msg.content.find("</_wc_custom_link_>");
        if (pos != std::string::npos)
        {
            auto pos2 = msg.content.find(">", 17);    // length of <_wc_custom_link_
            if (pos2 != std::string::npos)
            {
                plainText = msg.content.substr(pos2 + 1, pos - (pos2 + 1));
            }
            plainText += msg.content.substr(pos + 19);   //
        }
        if (plainText != tv["%%MESSAGE%%"])
        {
            // int aa = 0;
        }
#endif
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
    
    return true;
}

////////////////////////////////

bool MessageParser::parseAppMsgText(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    std::string title;
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    xmlParser.parseNodeValue("/msg/appmsg/title", title);
    tv["%%MESSAGE%%"] = title.empty() ? m_resManager.getLocaleString("[Link]") : title;
    
    return true;
}

bool MessageParser::parseAppMsgImage(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgAudio(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgVideo(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgEmotion(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgUrl(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
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
    std::string destPath = combinePath(m_outputPath, session.getOutputFileName(), DIR_ASSETS);
    if (m_iTunesDb.copyFile(vThumbFile, destPath, appMsg.msg->msgId + "_thum.jpg"))
    {
        thumbUrl = (DIR_ASSETS "/") + appMsg.msg->msgId + "_thum.jpg";
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
    
    return true;
}

bool MessageParser::parseAppMsgAttachment(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
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
    return parseFile(combinePath(m_outputPath, session.getOutputFileName()), DIR_ASSETS, DIR_ASSETS, attachFileName, attachOutputFileName, title, tv);
}

bool MessageParser::parseAppMsgOpen(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgEmoji(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    // Can't parse the detail info of emoji as the url is encrypted
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgRtLocation(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Real-time Location]");
    return true;
}

bool MessageParser::parseAppMsgFwdMsg(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, std::string& forwardedMsg, std::string& forwardedMsgTitle, TemplateValues& tv) const
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
    return true;
}

bool MessageParser::parseAppMsgCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgChannelCard(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    // Channel Card
    std::map<std::string, std::string> nodes = { {"username", ""}, {"avatar", ""}, {"nickname", ""}};
    xmlParser.parseNodesValue("/msg/appmsg/findernamecard/*", nodes);
    
    std::string portraitDir = (DIR_ASSETS DIR_SEP_STR "Portrait");
    std::string portraitUrlDir = (DIR_ASSETS "/Portrait");
    
    return parseChannelCard(session, portraitDir, portraitUrlDir, nodes["username"], nodes["avatar"], "", nodes["nickname"], tv);
}

bool MessageParser::parseAppMsgChannels(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_" + std::to_string(appMsg.appMsgType) + "_" + appMsg.msg->msgId + ".txt"), appMsg.msg->content);
#endif
    
    return parseChannels(appMsg.msg->msgId, xmlParser, NULL, "/msg/appmsg/finderFeed", session, tv);
}

bool MessageParser::parseAppMsgRefer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
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
        tv["%%MESSAGE%%"] = m_resManager.convertEmojis(title, combinePath(m_outputPath, session.getOutputFileName()), DIR_ASSETS, DIR_ASSETS);
        tv["%%REFERNAME%%"] = nodes["displayname"];
        if (nodes["type"] == "43")
        {
            tv["%%REFERMSG%%"] = m_resManager.getLocaleString("[Video]");
        }
        else if (nodes["type"] == "1")
        {
            if (!m_options.isTextMode())
            {
                tv["%%REFERMSG%%"] = m_resManager.convertEmojis(nodes["content"], combinePath(m_outputPath, session.getOutputFileName()), DIR_ASSETS, DIR_ASSETS);
            }
            else
            {
                tv["%%REFERMSG%%"] = nodes["content"];
            }
        }
        else if (nodes["type"] == "3")
        {
            tv["%%REFERMSG%%"] = m_resManager.getLocaleString("[Photo]");
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
    
    return true;
}

bool MessageParser::parseAppMsgTransfer(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
#ifndef NDEBUG
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_transfer_" + appMsg.msg->msgId + ".xml"), appMsg.msg->content);
#endif
    std::map<std::string, std::string> nodes = { {"title", ""}, {"type", ""}, {"des", ""}, {"url", ""}, {"thumburl", ""}, {"recorditem", ""} };
    xmlParser.parseNodesValue("/msg/appmsg/*", nodes);
    
    std::map<std::string, std::string> transferNode = { {"paysubtype", ""}, {"feedesc", ""}, {"pay_memo", ""}, {"receiver_username", ""}, {"payer_username", ""} };
    xmlParser.parseNodesValue("/msg/appmsg/wcpayinfo/*", transferNode);
    
    // paysubtype:
    //
    // 3,4 received receive rejected
    // 8,9 send send rejected
    std::string content = transferNode["feedesc"];
    const std::string& memo = transferNode["pay_memo"];
    const std::string& paySubType = transferNode["paysubtype"];
    std::string paySubTypeKey = (session.isChatroom() ? "Group_Transfer_Subtype_" : "Transfer_Subtype_") + paySubType;
    std::string paySubTypeDesc = m_resManager.getLocaleString(paySubTypeKey);

    if (paySubTypeDesc == paySubTypeKey)
    {
        paySubTypeDesc = "";
    }
    else
    {
        if (session.isChatroom())
        {
            if (paySubType == "1" || paySubType == "8" || paySubType == "9")
            {
                // Pay to
                std::string displayName = getMemberDisplayName(transferNode["receiver_username"], session);
                paySubTypeDesc = formatString(paySubTypeDesc, displayName.c_str());
            }
            else if (paySubType == "3" || paySubType == "4" || paySubType == "5")
            {
                // 3 Accepted
                // 5 Accepted and waiting for ...
                // From
                std::string displayName = getMemberDisplayName(transferNode["payer_username"], session);
                paySubTypeDesc = formatString(paySubTypeDesc, displayName.c_str());
            }
        }
    }
    if (!paySubTypeDesc.empty())
    {
        content += " - ";
        content += paySubTypeDesc;
    }
    if (!memo.empty())
    {
        content += "\r\n";
        content += memo;
    }
    tv["%%MESSAGE%%"] = content;
    
    tv.setName("transfer");
    return true;
}

bool MessageParser::parseAppMsgRedPacket(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Red Packet]");
    return true;
}

bool MessageParser::parseAppMsgPat(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    // int recordNum = 0;
    // xmlParser.parseNodeValue("/msg/appmsg/patMsg/records/record/fromUser", fromUser);
    
    std::string fromUser;
    std::string temp;
    std::string pattedUser;
    xmlParser.parseNodeValue("/msg/appmsg/patMsg/records/record/fromUser", fromUser);
    xmlParser.parseNodeValue("/msg/appmsg/patMsg/records/record/templete", temp);
    xmlParser.parseNodeValue("/msg/appmsg/patMsg/records/record/pattedUser", pattedUser);
    
    std::string fromUserName;
    std::string pattedUserName;
    
    Friend* f = m_friends.getFriendByUid(fromUser);
    if (NULL != f)
    {
        fromUserName = f->getDisplayName();
    }
    else
    {
        fromUserName = session.getMemberName(fromUser);
    }
    
    f = m_friends.getFriendByUid(pattedUser);
    if (NULL != f)
    {
        pattedUserName = f->getDisplayName();
    }
    else
    {
        pattedUserName = session.getMemberName(pattedUser);
    }
    
    replaceAll(temp, "${" + fromUser + "}", fromUserName);
    replaceAll(temp, "${" + pattedUser + "}", pattedUserName);
    
    tv["%%MESSAGE%%"] = temp;
    
    tv.setName("system");
    
    return true;
}

bool MessageParser::parseAppMsgReaderType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgNote(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
#if !defined(NDEBUG) || defined(DBG_PERF)
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_24_" + appMsg.msg->msgId + ".txt"), appMsg.msg->content);
#endif
    std::map<std::string, std::string> nodes = { {"title", ""}, {"type", ""}, {"des", ""}, {"url", ""}, {"thumburl", ""}, {"recorditem", ""} };
    xmlParser.parseNodesValue("/msg/appmsg/*", nodes);
    removeSupportUrl(nodes["url"]);
    if (nodes["title"].empty() && !nodes["des"].empty())
    {
        nodes["title"].swap(nodes["des"]);
    }
    
    if (!nodes["recorditem"].empty())
    {
        XmlParser xmlParser2(nodes["recorditem"], true);
        
        std::map<std::string, std::string> nodes2 = { {"info", ""}, {"desc", ""}, {"edittime", ""}, {"favusername", ""} };
        xmlParser2.parseNodesValue("/recordinfo/*", nodes2);
        
        if (!nodes2["info"].empty())
        {
            nodes["title"] = nodes2["info"];
        }
        else if (!nodes2["desc"].empty())
        {
            nodes["title"] = nodes2["desc"];
        }
    }

    if (!nodes["title"].empty() && !nodes["url"].empty())
    {
        bool isPlainShare = nodes["thumburl"].empty();
        tv.setName(isPlainShare ? "plainshare" : "share");

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
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Link]");
    }
    
    return true;
}

bool MessageParser::parseAppMsgUnknownType(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
{
#if !defined(NDEBUG) || defined(DBG_PERF)
    writeFile(combinePath(m_outputPath, "../dbg", "msg" + std::to_string(appMsg.msg->type) + "_app_unknwn_" + std::to_string(appMsg.appMsgType) + ".txt"), appMsg.msg->content);
#endif
    return parseAppMsgDefault(appMsg, xmlParser, session, tv);
}

bool MessageParser::parseAppMsgDefault(const WXAPPMSG& appMsg, const XmlParser& xmlParser, const Session& session, TemplateValues& tv) const
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
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Link]");
    }
    
    return true;
}

////////////////////////////////

bool MessageParser::parseFwdMsgText(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNode *itemNode, const Session& session, TemplateValues& tv) const
{
    std::string message;
    xmlParser.getChildNodeContent(itemNode, "datadesc", message);
    static std::vector<std::pair<std::string, std::string>> replaces = { {"\r\n", "<br />"}, {"\r", "<br />"}, {"\n", "<br />"}};
    replaceAll(message, replaces);
    tv["%%MESSAGE%%"] = message;
    
    return true;
}

bool MessageParser::parseFwdMsgImage(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNode *itemNode, const Session& session, TemplateValues& tv) const
{
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId;
    return parseImage(combinePath(m_outputPath, session.getOutputFileName()), (DIR_ASSETS "/") + fwdMsg.msg->msgId, (DIR_ASSETS "/") + fwdMsg.msg->msgId, vfile + fileExtName, vfile + fileExtName + "_pre3", fwdMsg.dataId + ".jpg", vfile + ".record_thumb", fwdMsg.dataId + "_thumb.jpg", tv);
}

bool MessageParser::parseFwdMsgVideo(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId;
    return parseVideo(combinePath(m_outputPath, session.getOutputFileName()), (DIR_ASSETS "/") + fwdMsg.msg->msgId, (DIR_ASSETS "/") + fwdMsg.msg->msgId, vfile + fileExtName, "", fwdMsg.dataId + fileExtName, vfile + ".record_thumb", fwdMsg.dataId + "_thumb.jpg", "", "", tv);
}

bool MessageParser::parseFwdMsgLink(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
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
    if (!m_options.isTextMode())
    {
        std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId + ".record_thumb";
        hasThumb = m_iTunesDb.copyFile(vfile, combinePath(m_outputPath, session.getOutputFileName(), DIR_ASSETS, fwdMsg.msg->msgId), fwdMsg.dataId + "_thumb.jpg");
    }
    
    if (!(link.empty()))
    {
        tv.setName(hasThumb ? "share" : "plainshare");

        tv["%%SHARINGIMGPATH%%"] = (DIR_ASSETS "/") + fwdMsg.msg->msgId + "/" + fwdMsg.dataId + "_thumb.jpg";
        tv["%%SHARINGURL%%"] = link;
        tv["%%SHARINGTITLE%%"] = title;
        tv["%%MESSAGE%%"] = message;
    }
    else
    {
        tv["%%MESSAGE%%"] = title;
    }
    
    return true;
}

bool MessageParser::parseFwdMsgLocation(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
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
        tv["%%MESSAGE%%"] = formatString(m_resManager.getLocaleString("[Location] %s (%s,%s)"), location.c_str(), lat.c_str(), lng.c_str());
    }
    else
    {
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Location]");
    }
    tv.setName("msg");
    
    return true;
}

bool MessageParser::parseFwdMsgAttach(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string message;
    xmlParser.getChildNodeContent(itemNode, "datatitle", message);
    
    std::string fileExtName = fwdMsg.dataFormat.empty() ? "" : ("." + fwdMsg.dataFormat);
    std::string vfile = m_userBase + "/OpenData/" + session.getHash() + "/" + fwdMsg.msg->msgId + "/" + fwdMsg.dataId;
    return parseFile(combinePath(m_outputPath, session.getOutputFileName()), (DIR_ASSETS "/") + fwdMsg.msg->msgId, (DIR_ASSETS "/") + fwdMsg.msg->msgId, vfile + fileExtName, fwdMsg.dataId + fileExtName, message, tv);
}

bool MessageParser::parseFwdMsgCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string cardContent;
    xmlParser.getChildNodeContent(itemNode, "datadesc", cardContent);
    
    std::string portraitDir = (DIR_ASSETS DIR_SEP_STR "Portrait");
    std::string portraitUrlDir = (DIR_ASSETS "/Portrait");
    return parseCard(session, m_outputPath, portraitDir, portraitUrlDir, cardContent, tv);
}

bool MessageParser::parseFwdMsgNestedFwdMsg(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, std::string& nestedFwdMsg, std::string& nestedFwdMsgTitle, TemplateValues& tv) const
{
    xmlParser.getChildNodeContent(itemNode, "datadesc", nestedFwdMsgTitle);
    xmlNodePtr nodeRecordInfo = XmlParser::getChildNode(itemNode, "recordinfo");
    if (NULL != nodeRecordInfo)
    {
        nestedFwdMsg = XmlParser::getNodeOuterXml(nodeRecordInfo);
    }
    
    tv["%%MESSAGE%%"] = nestedFwdMsgTitle;
    
    return true;
}

bool MessageParser::parseFwdMsgMiniProgram(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    std::string title;
    xmlParser.getChildNodeContent(itemNode, "datatitle", title);
    tv["%%MESSAGE%%"] = title;
    
    return true;
}

bool MessageParser::parseFwdMsgChannels(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
{
    return parseChannels(fwdMsg.msg->msgId, xmlParser, itemNode, "./finderFeed", session, tv);
}

bool MessageParser::parseFwdMsgChannelCard(const WXFWDMSG& fwdMsg, const XmlParser& xmlParser, xmlNodePtr itemNode, const Session& session, TemplateValues& tv) const
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
    
    std::string portraitDir = (DIR_ASSETS DIR_SEP_STR "Portrait");
    std::string portraitUrlDir = (DIR_ASSETS "/Portrait");
    return parseChannelCard(session, portraitDir, portraitUrlDir, usrName, avatar, "", nickName, tv);
}

///////////////////////////////
// Implementation

bool MessageParser::parseVideo(const std::string& sessionPath, const std::string& sessionAssetsPath, const std::string& sessionAssetsUrlPath, const std::string& srcVideo, const std::string& srcRawVideo, const std::string& destVideo, const std::string& srcThumb, const std::string& destThumb, const std::string& width, const std::string& height, TemplateValues& tv) const
{
    bool hasThumb = false;
    bool hasVideo = false;
    
    if (!m_options.isTextMode())
    {
        std::string fullAssetsPath = combinePath(sessionPath, sessionAssetsPath);
        ensureDirectoryExisted(fullAssetsPath);
        hasThumb = m_iTunesDb.copyFile(srcThumb, combinePath(fullAssetsPath, destThumb));
        if (!srcRawVideo.empty())
        {
            hasVideo = m_iTunesDb.copyFile(srcRawVideo, combinePath(fullAssetsPath, destVideo), true);
        }
        if (!hasVideo)
        {
            hasVideo = m_iTunesDb.copyFile(srcVideo, combinePath(fullAssetsPath, destVideo));
        }
    }

    if (hasVideo)
    {
        tv.setName("video");
        tv["%%THUMBPATH%%"] = hasThumb ? (sessionAssetsUrlPath + "/" + destThumb) : "";
        tv["%%VIDEOPATH%%"] = sessionAssetsUrlPath + "/" + destVideo;
        tv["%%MSGTYPE%%"] = "video";
    }
    else if (hasThumb)
    {
        tv.setName("thumb");
        tv["%%IMGTHUMBPATH%%"] = sessionAssetsUrlPath + "/" + destThumb;
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("(Video Missed)");
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Video]");
    }
    
    tv["%%VIDEOWIDTH%%"] = width;
    tv["%%VIDEOHEIGHT%%"] = height;
    
    return true;
}

bool MessageParser::parseImage(const std::string& sessionPath, const std::string& sessionAssetsPath, const std::string& sessionAssetsUrlPath, const std::string& src, const std::string& srcHdOrPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& tv) const
{
    bool hasThumb = false;
    bool hasImage = false;
    if (!m_options.isTextMode())
    {
        std::string fullAssetsPath = combinePath(sessionPath, sessionAssetsPath);
        hasThumb = m_iTunesDb.copyFile(srcThumb, fullAssetsPath, destThumb);
        if (!srcHdOrPre.empty())
        {
            hasImage = m_iTunesDb.copyFile(srcHdOrPre, fullAssetsPath, dest);
        }
        if (!hasImage)
        {
            hasImage = m_iTunesDb.copyFile(src, fullAssetsPath, dest);
        }
    }

    if (hasImage)
    {
        tv.setName("image");
        tv["%%IMGPATH%%"] = sessionAssetsUrlPath + "/" + dest;
        // If it is PDF mode, use the raw image directly for print quaility
        tv["%%IMGTHUMBPATH%%"] = sessionAssetsUrlPath + "/" + (((!hasThumb) || m_options.isPdfMode()) ? dest : destThumb);
        tv["%%MSGTYPE%%"] = "image";
        tv["%%EXTRA_CLS%%"] = "raw-img";
    }
    else if (hasThumb)
    {
        tv.setName("thumb");
        tv["%%IMGTHUMBPATH%%"] = sessionAssetsUrlPath + "/" + destThumb;
        tv["%%MESSAGE%%"] = "";
        tv["%%MSGTYPE%%"] = "image";
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Photo]");
    }
    
    return true;
}

bool MessageParser::parseFile(const std::string& sessionPath, const std::string& sessionAssetsPath, const std::string& sessionAssetsUrlPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& tv) const
{
    bool hasFile = false;
    if (!m_options.isTextMode())
    {
        hasFile = m_iTunesDb.copyFile(src, combinePath(sessionPath, sessionAssetsPath), dest);
    }

    if (hasFile)
    {
        tv.setName("plainshare");
        tv["%%SHARINGURL%%"] = sessionAssetsUrlPath + "/" + dest;
        tv["%%SHARINGTITLE%%"] = fileName;
        tv["%%MESSAGE%%"] = "";
        tv["%%MSGTYPE%%"] = "file";
    }
    else
    {
        tv.setName("msg");
        tv["%%MESSAGE%%"] = formatString(m_resManager.getLocaleString("[File: %s]"), fileName.c_str());
    }
    
    return true;
}

bool MessageParser::parseCard(const Session& session, const std::string& sessionPath, const std::string& portraitDir, const std::string& portraitUrlDir, const std::string& cardMessage, TemplateValues& tv) const
{
    // static std::regex pattern42_1("nickname ?= ?\"(.+?)\"");
    // static std::regex pattern42_2("smallheadimgurl ?= ?\"(.+?)\"");
    std::map<std::string, std::string> attrs;
    if (!m_options.isTextMode())
    {
        attrs = { {"nickname", ""}, {"smallheadimgurl", ""}, {"bigheadimgurl", ""}, {"username", ""} };
    }
    else
    {
        attrs = { {"nickname", ""}, {"username", ""} };
    }

    tv["%%CARDTYPE%%"] = m_resManager.getLocaleString("[Contact Card]");
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
            tv["%%CARDIMGPATH%%"] = portraitUrlDir + "/" + imgFileName + ".jpg";
			std::string localPortraitDir = combinePath(session.getOutputFileName(), normalizePath(portraitDir));
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
            tv["%%MESSAGE%%"] = formatString(m_resManager.getLocaleString("[Contact Card] %s"), attrs["nickname"].c_str());
        }
        else
        {
            tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Contact Card]");
        }
    }
    else
    {
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Contact Card]");
    }
    tv["%%EXTRA_CLS%%"] = "contact-card";
    
    return true;
}

bool MessageParser::parseChannelCard(const Session& session, const std::string& portraitDir, const std::string& portraitUrlDir, const std::string& usrName, const std::string& avatar, const std::string& avatarLD, const std::string& name, TemplateValues& tv) const
{
    bool hasImg = false;
    if (!m_options.isTextMode())
    {
        hasImg = (!usrName.empty() && !avatar.empty());
    }
    tv["%%CARDTYPE%%"] = m_resManager.getLocaleString("[Channel Card]");
    if (!name.empty())
    {
        if (hasImg)
        {
            tv.setName("card");
            tv["%%CARDNAME%%"] = name;
            tv["%%CARDIMGPATH%%"] = portraitUrlDir + "/" + usrName + ".jpg";
			std::string localPortraitDir = normalizePath(portraitDir);
            std::string localFile = combinePath(localPortraitDir, usrName + ".jpg");
            ensureDirectoryExisted(combinePath(m_outputPath, session.getOutputFileName(), localPortraitDir));
#ifdef USING_DOWNLOADER
            m_downloader.addTask(avatar, combinePath(m_outputPath, localfile), 0, "card");
#else
            m_taskManager.download(&session, avatar, avatarLD, combinePath(m_outputPath, localFile), 0, "", "card");
#endif
        }
        else
        {
            tv.setName("msg");
            tv["%%MESSAGE%%"] = formatString(m_resManager.getLocaleString("[Channel Card] %s"), name.c_str());
        }
    }
    else
    {
        tv["%%MESSAGE%%"] = m_resManager.getLocaleString("[Channel Card]");
    }
    tv["%%EXTRA_CLS%%"] = "channel-card";
    
    return true;
}

bool MessageParser::parseChannels(const std::string& msgId, const XmlParser& xmlParser, xmlNodePtr parentNode, const std::string& finderFeedXPath, const Session& session, TemplateValues& tv) const
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
    if (nodes["mediaCount"] == "")
    {
        int aa = 0;
    }
#endif
    std::string thumbUrl;
    if (!m_options.isTextMode())
    {
        thumbUrl = videoNodes["thumbUrl"].empty() ? videoNodes["coverUrl"] : videoNodes["thumbUrl"];
    }
    
    const std::string portraitDir = (DIR_ASSETS DIR_SEP_STR "Portrait");
    const std::string portraitUrlDir = (DIR_ASSETS "/Portrait");
    
    tv["%%CARDNAME%%"] = nodes["nickname"];
    tv["%%CHANNELS%%"] = m_resManager.getLocaleString("Channels");
    tv["%%MESSAGE%%"] = nodes["desc"];
    tv["%%EXTRA_CLS%%"] = "channels";
    
    if (!thumbUrl.empty())
    {
        tv.setName("channels");
        tv["%%MSGTYPE%%"] = "channels";
        std::string thumbFile = (DIR_ASSETS "/") + msgId + ".jpg";
        std::string localThumbFile = (DIR_ASSETS DIR_SEP_STR) + msgId + ".jpg";
        tv["%%CHANNELTHUMBPATH%%"] = thumbFile;
        ensureDirectoryExisted(combinePath(m_outputPath, session.getOutputFileName(), DIR_ASSETS));

#ifdef USING_DOWNLOADER
        m_downloader.addTask(thumbUrl, combinePath(m_outputPath, session.getOutputFileName(), localThumbFile), 0, "thumb");
#else
        m_taskManager.download(&session, thumbUrl, "", combinePath(m_outputPath, session.getOutputFileName(), localThumbFile), 0, "", "thumb");
#endif
        
        if (!nodes["avatar"].empty())
        {
            std::string fileName = nodes["username"].empty() ? nodes["objectId"] : nodes["username"];
            tv["%%CARDIMGPATH%%"] = portraitUrlDir + "/" + fileName + ".jpg";
			std::string localPortraitDir = normalizePath(portraitDir);
            std::string localFile = combinePath(localPortraitDir, fileName + ".jpg");
            ensureDirectoryExisted(combinePath(m_outputPath, session.getOutputFileName(), localPortraitDir));
#ifdef USING_DOWNLOADER
            m_downloader.addTask(nodes["avatar"], combinePath(m_outputPath, session.getOutputFileName(), localFile), 0, "card");
#else
            m_taskManager.download(&session, nodes["avatar"], "", combinePath(m_outputPath, session.getOutputFileName(), localFile), 0, "", "card");
#endif
        }

        tv["%%CHANNELURL%%"] = videoNodes["url"];
    }
    
    return true;
}

bool MessageParser::parseForwardedMsgs(const Session& session, const WXMSG& msg, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs) const
{
    std::string portraitPath = (DIR_ASSETS DIR_SEP_STR "Portrait" DIR_SEP_STR);
    std::string portraitUrlPath = (DIR_ASSETS "/Portrait/");
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& beginTv = tvs.back();
    beginTv["%%MESSAGE%%"] = formatString(m_resManager.getLocaleString("<< %s"), title.c_str());
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
            if (copyPortraitIcon(&session, fmsg.usrName, fmsg.portrait, fmsg.portraitLD, combinePath(m_outputPath, session.getOutputFileName(), portraitPath)))
            {
                tv["%%AVATAR%%"] = portraitUrlPath + "/" + fmsg.usrName + ".jpg";
            }
            else
            {
                ensureDefaultPortraitIconExisted(portraitPath);
                tv["%%AVATAR%%"] = portraitUrlPath + "DefaultAvatar.png";
            }

            if ((dataType == FWDMSG_DATATYPE_NESTED_FWD_MSG) && !nestedFwdMsg.empty())
            {
                parseForwardedMsgs(session, msg, nestedFwdMsgTitle, nestedFwdMsg, tvs);
            }
        }
    }
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& endTv = tvs.back();
    endTv["%%MESSAGE%%"] = formatString(m_resManager.getLocaleString("%s Ends >>"), title.c_str());
    endTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg
    
    return true;
}

/////////////////////////////


/////////////////////////

std::string MessageParser::getDisplayTime(int ms) const
{
    if (ms < 1000) return "1\"";
    ms /= 1000;
    if (ms < 60) return std::to_string(ms) + "\"";
    int seconds = ms % 60;
    return seconds != 0 ? (std::to_string((int)(std::round((double)ms / 60))) + "\'" + std::to_string(seconds) + "\"") : (std::to_string((int)(std::round((double)ms / 60))) + "\'");
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
    std::string destFullPath = combinePath(destPath, destFileName);
    if (existsFile(destFullPath))
    {
        return true;
    }
    
    if (!existsDirectory(destPath))
    {
        makeDirectory(destPath);
    }
    
    bool hasPortrait = false;
    std::string avatarPath = "share/" + m_myself.getHash() + "/session/headImg/" + usrNameHash + ".pic";
    const ITunesFile* file = m_iTunesDbShare.findITunesFile(avatarPath);
    if (NULL != file)
    {
        ITunesDb::parseFileInfo(file);
        
        std::string srcPath = m_iTunesDbShare.getRealPath(*file);
        if (!srcPath.empty() && !Friend::isDefaultAvatar(file->size, srcPath))
        {
            normalizePath(srcPath);
            
            bool result = ::copyFile(srcPath, destFullPath, true);
            if (result)
            {
                if (file->modifiedTime != 0)
                {
                    updateFileTime(destFullPath, static_cast<time_t>(file->modifiedTime));
                }
                else if (!file->blob.empty())
                {
                    updateFileTime(destFullPath, ITunesDb::parseModifiedTime(file->blob));
                }
                hasPortrait = true;
            }
        }
    }

    // bool hasPortrait = m_iTunesDbShare.copyFile(avatarPath, destPath, destFileName);
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
                    m_taskManager.download(session, url, urlLD, combinePath(localDestPath, destFileName), 0, m_resManager.getDefaultAvatarPath(), "avatar");
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
            m_taskManager.download(session, portraitUrl, portraitUrlLD, combinePath(localDestPath, destFileName), 0, m_resManager.getDefaultAvatarPath(), "avatar");
#endif
            hasPortrait = true;
        }
    }
    
    if (!hasPortrait)
    {
        std::string localDestPath = normalizePath(destPath);
        hasPortrait = ::copyFile(m_resManager.getDefaultAvatarPath(), combinePath(localDestPath, destFileName));
    }
    
    return hasPortrait;
}

void MessageParser::ensureDefaultPortraitIconExisted(const std::string& portraitPath) const
{
    std::string dest = combinePath(m_outputPath, portraitPath);
    ensureDirectoryExisted(dest);
    dest = combinePath(dest, "DefaultAvatar.png");
    if (!existsFile(dest))
    {
        copyFile(combinePath(m_resPath, "res", "DefaultAvatar.png"), dest, false);
    }
}

bool MessageParser::removeSupportUrl(std::string& url)
{
    if (startsWith(url, "https://support.weixin.qq.com"))
    {
        url.clear();
        return true;
    }
    
    return false;
}

std::string MessageParser::getMemberDisplayName(const std::string& usrName, const Session& session) const
{
    std::string displayName;
    if (session.isChatroom())
    {
        // Should fix the priority later:
        // 1 nick name of chatroom
        // 2 nick name of friend
        // 3: display name in chatroom
        // 4: display name of friend
        
        displayName = session.getMemberName(usrName);
        if (displayName.empty())
        {
            const Friend *f = usrName == m_myself.getUsrName() ? &m_myself : m_friends.getFriend(md5(usrName));
            if (NULL != f)
            {
                displayName = f->getDisplayName();
            }
            else
            {
                displayName = usrName;
            }
        }
    }
    else
    {
        if (usrName == m_myself.getUsrName())
        {
            displayName = m_myself.getDisplayName();
        }
        else if (usrName == session.getUsrName())
        {
            displayName = session.getDisplayName();
        }
        else
        {
            const Friend *f = m_friends.getFriend(md5(usrName));
            if (NULL != f)
            {
                displayName = f->getDisplayName();
            }
            else
            {
                displayName = usrName;
            }
        }
    }
    
    return displayName;
}
