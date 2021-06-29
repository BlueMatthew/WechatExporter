//
//  MbdbReader.h
//  WechatExporter
//
//  Created by Matthew on 2021/6/25.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include <fstream>

#ifndef MbdbReader_h
#define MbdbReader_h

// Refer: https://www.theiphonewiki.com/wiki/ITunes_Backup#Manifest.mbdb

// string Domain
    // string Path
    // string LinkTarget absolute path
    // string DataHash SHA.1 (some files only)
    // string unknown always N/A
    // uint16 Mode same as mbdx.Mode
    // uint32 unknown always 0
    // uint32 unknown
    // uint32 UserId
    // uint32 GroupId mostly 501 for apps
    // uint32 Time1 relative to unix epoch (e.g time_t)
    // uint32 Time2 Time1 or Time2 is the former ModificationTime
    // uint32 Time3
    // uint64 FileLength always 0 for link or directory
    // uint8 Flag 0 if special (link, directory), otherwise unknown
    // uint8 PropertyCount number of properties following
    //
    // Property is a couple of strings:
    //
    // string name
    // string value can be a string or a binary content

class MbdbReader {
    
    std::ifstream m_ifs;
    std::vector<char> m_buffer;
    
#ifndef NDEBUG
    std::ifstream::streampos m_pos;
#endif
public:
    
    ~MbdbReader()
    {
        if (m_ifs.is_open())
        {
            m_ifs.close();
        }
    }
    
    bool open(const std::string& fileName)
    {
        m_ifs.open(fileName, std::ios_base::in | std::ios_base::binary);
        if (!m_ifs.is_open())
        {
            return false;
        }
        
        char signature[7] = { 0 };
        m_ifs.read(&signature[0], 6);
        if (strcmp(signature, "mbdb\5\0") != 0)
        {
            m_ifs.close();
            return false;
        }
        
        return true;
    }
    
    bool hasMoreData()
    {
        return !m_ifs.eof();
    }
    
    bool read(unsigned char *buffer, size_t length)
    {
        m_ifs.read(reinterpret_cast<char *>(buffer), length);

        return true;
    }
    
    bool read(std::string& str)
    {
        int b0 = m_ifs.get();
        int b1 = m_ifs.get();
        
        if ((b0 == 255 && b1 == 255) || (b0 == 0 && b1 == 0))
        {
            str.clear();
            return true;
        }
        
        if (b0 == std::ifstream::traits_type::eof() || b1 == std::ifstream::traits_type::eof())
        {
            return false;
        }
        
        int lengthOfString = b0 * 256 + b1;
        m_buffer.resize(lengthOfString);
        m_ifs.read(&m_buffer[0], lengthOfString);

        str.clear();
        std::copy(m_buffer.begin(), m_buffer.end(), std::back_inserter(str));
        
        return true;
    }
    
    bool readD(std::string& str)
    {
        if (!read(str))
        {
            return false;
        }
        
        // If only ASCII printable characters, return the string
        size_t i = 0, length = str.size();
        for (; i < length; ++i)
        {
            if (str[i] < 32 || str[i] >= 128)
            {
                break;
            }
        }
        if (i == length)
        {
            return true;
        }

        // otherwise the hexadecimal dump
        std::string result;
        for (i = 0; i < length; ++i)
        {
            result.push_back(toHex(str[i] >> 4));
            result.push_back(toHex(str[i] & 15));
        }

        str.swap(result);
        return true;
    }

    bool skipString()
    {
        int b0 = m_ifs.get();
        int b1 = m_ifs.get();
        
        if ((b0 == 255 && b1 == 255) || (b0 == 0 && b1 == 0))
        {
            return true;
        }
        
        if (b0 == std::ifstream::traits_type::eof() || b1 == std::ifstream::traits_type::eof())
        {
            return false;
        }
        
        int lengthOfString = b0 * 256 + b1;
        m_ifs.seekg(lengthOfString, std::ios_base::cur);

        return true;
    }
    // static size_t skipString(const unsigned char *data, size_t length);
    
    bool skip(size_t length)
    {
        m_ifs.seekg(length, std::ios_base::cur);
        return true;
    }
    
protected:
    char toHex(int value)
    {
        value &= 0xF;
        return (value >= 0 && value <= 9) ? (char)('0' + value) : (char)('A' + (value - 10));
    }

    char toHexLow(int value)
    {
        value &= 0xF;
        return (value >= 0 && value <= 9) ? (char)('0' + value) : (char)('a' + (value - 10));
    }
};


#endif /* MbdbReader_h */
