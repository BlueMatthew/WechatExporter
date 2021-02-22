//
//  MessageParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/2/22.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "MessageParser.h"


MessageParserBase::MessageParserBase(const ITunesDb& iTunesDb, Downloader& downloader, const std::string& sessionPath, const std::string& portraitPath) : m_iTunesDb(iTunesDb), m_downloader(downloader), m_sessionPath(sessionPath), m_portraitPath(portraitPath)
{
    
}
