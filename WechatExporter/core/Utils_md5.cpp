//
//  Utils.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>
#include <sstream>
#include <iomanip>

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>

#define MD5_DIGEST_LENGTH 16
#define SHA_DIGEST_LENGTH 20

#elif defined(__APPLE__)
#import <CommonCrypto/CommonDigest.h>
#else

#endif

std::string md5(const std::string& s)
{
    std::stringstream stream;
    stream << std::setfill ('0') << std::hex;
    
#if defined(_WIN32)
    
    HCRYPTPROV hCryptProv = NULL;
    HCRYPTHASH hHash = NULL;
    BYTE bHash[0x7f] = {0};
    DWORD dwHashLen= MD5_DIGEST_LENGTH; // The MD5 algorithm always returns 16 bytes.

    if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET))
    {
        if(CryptCreateHash(hCryptProv, CALG_MD5, 0, 0, &hHash))
        {
            if(CryptHashData(hHash, reinterpret_cast<const BYTE*>(s.c_str()), static_cast<DWORD>(s.size()), 0))
            {
                if(CryptGetHashParam(hHash, HP_HASHVAL, bHash, &dwHashLen, 0))
                {
                    // Make a string version of the numeric digest value
                    for (int idx = 0; idx < 16; idx++)
                    {
                        stream << std::setw(2) << ((unsigned int) bHash[idx]);
                    }
                }
            }
        }
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hCryptProv, 0);

#elif defined(__APPLE__)
    unsigned char digest[CC_MD5_DIGEST_LENGTH] = {0};
    CC_MD5(s.c_str(), (CC_LONG)s.size(), digest); // This is the md5 call

    for (int idx = 0; idx < CC_MD5_DIGEST_LENGTH; idx++)
    {
        stream << std::setw(2) << ((unsigned int) digest[idx]);
    }
#else
#error "Md5 Not implemented."
#endif

    return stream.str();
}

std::string sha1(const std::string& s)
{
    std::stringstream stream;
    stream << std::setfill ('0') << std::hex;
    
#if defined(_WIN32)
    
    HCRYPTPROV hCryptProv = NULL;
    HCRYPTHASH hHash = NULL;
    BYTE bHash[0x7f] = {0};
    DWORD dwHashLen= SHA_DIGEST_LENGTH ; // The SHA1 algorithm always returns 20 bytes.

    if(CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET))
    {
        if(CryptCreateHash(hCryptProv, CALG_SHA1, 0, 0, &hHash))
        {
            if(CryptHashData(hHash, reinterpret_cast<const BYTE*>(s.c_str()), static_cast<DWORD>(s.size()), 0))
            {
                if(CryptGetHashParam(hHash, HP_HASHVAL, bHash, &dwHashLen, 0))
                {
                    // Make a string version of the numeric digest value
                    for (int idx = 0; idx < dwHashLen; idx++)
                    {
                        stream << std::setw(2) << ((unsigned int) bHash[idx]);
                    }
                }
            }
        }
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hCryptProv, 0);

#elif defined(__APPLE__)
    unsigned char digest[CC_SHA1_DIGEST_LENGTH] = {0};
    CC_SHA1(s.c_str(), (CC_LONG)s.size(), digest); // This is the md5 call

    for (int idx = 0; idx < CC_SHA1_DIGEST_LENGTH; idx++)
    {
        stream << std::setw(2) << ((unsigned int) digest[idx]);
    }
#else
#error "SHA1 Not implemented."
#endif

    return stream.str();
}
