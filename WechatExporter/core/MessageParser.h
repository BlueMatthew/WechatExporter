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
#include "Downloader.h"
#include "ITunesParser.h"


struct MsgRecord
{
    int createTime;
    std::string message;
    int des;
    int type;
    int msgId;
};

struct ForwardMsg
{
    int msgid;
    std::string usrName;
    std::string displayName;
    std::string protrait;
    std::string dataType;
    std::string subType;
    std::string dataId;
    std::string dataFormat;
    std::string msgTime;
    std::string srcMsgTime;
    std::string message;
    std::string link;
    std::string nestedMsgs;
};


class MessageParserBase
{
protected:
    const ITunesDb& m_iTunesDb;
    Downloader& m_downloader;
    
    const std::string& m_sessionPath;
    const std::string& m_portraitPath;

public:
    MessageParserBase(const ITunesDb& iTunesDb, Downloader& downloader, const std::string& sessionPath, const std::string& protraitPath);
    
};

class MessageParser : public MessageParserBase
{
public:
    bool parseText();
};


class FwdMessageParser : public MessageParserBase
{
    
};

#endif /* MessageParser_h */
