//
//  Utils.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Utils.h"
#include <string>
#include <sstream>
#include <iomanip>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STRING_H || STDC_HEADERS
#include <string.h>    /* for memcpy() */
#endif

/* Add prototype support.  */
#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#else
#define PROTO(ARGS) ()
#endif
#endif
extern "C"
{
#include "md5.h"
}

std::string md5(const std::string& s)
{
    MD5Context context;
    unsigned char checksum[16];
    
    MD5Init (&context);
    MD5Update (&context, (unsigned char const *)(s.c_str()), (unsigned int)s.size());
    MD5Final (checksum, &context);
    
    std::stringstream stream;
    stream << std::setfill ('0') << std::hex;
    
    for (int idx = 0; idx < 16; idx++)
    {
        stream << std::setw(2) << ((unsigned int) checksum[idx]);
    }
    
    return stream.str();
}
