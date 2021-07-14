//
//  MMKVReader.h
//  WechatExporter
//
//  Created by Matthew on 2021/1/26.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef MMKVReader_h
#define MMKVReader_h


class MMKVReader
{
private:
    const unsigned char *m_ptr;
	size_t m_size;
    mutable size_t m_position;

public:
    
    MMKVReader(const unsigned char *ptr, size_t size) : m_ptr(ptr), m_size(size), m_position(0)
    {
    }
    std::string readKey() const
    {
        std::string key;
        uint32_t keyLength = 0;
        const unsigned char* data = calcVarint32Ptr(m_ptr + m_position, m_ptr + m_size, &keyLength);
        m_position += data - (m_ptr + m_position);
        if (keyLength > 0)
        {
            auto s_size = static_cast<size_t>(keyLength);
            if (s_size <= m_size - m_position)
            {
                key.assign((char *) (m_ptr + m_position), s_size);
                m_position += s_size;
            }
            else
            {
                m_position = m_size;
            }
        }
        
        return key;
    }
    
    void skipValue() const
    {
        uint32_t valueLength = 0;
        const unsigned char* data = calcVarint32Ptr(m_ptr + m_position, m_ptr + m_size, &valueLength);
        m_position += data - (m_ptr + m_position);
        if (valueLength > 0)
        {
            m_position += valueLength;
        }
    }
    
    std::string readStringValue() const
    {
        // MMBuffer
        std::string value;
        uint32_t valueLength = 0;
        const unsigned char* data = calcVarint32Ptr(m_ptr + m_position, m_ptr + m_size, &valueLength);
        m_position += data - (m_ptr + m_position);
        if (valueLength > 0)
        {
            // MMBuffer
            uint32_t mbbLength = 0;
            const unsigned char *ptr = m_ptr + m_position;
            ptr = calcVarint32Ptr(ptr, m_ptr + m_size, &mbbLength);
            if (mbbLength > 0)
            {
                auto s_size = static_cast<size_t>(mbbLength);
                if (s_size <= m_size - m_position)
                {
                    value.assign((char *)(ptr), s_size);
                    m_position += valueLength;
                }
                else
                {
                    m_position = m_size;
                }
            }
            else
            {
                m_position += valueLength;
            }
        }
        
        return value;
    }
    
    void seek(size_t position) const
    {
        m_position = position;
    }
    
    size_t getPos() const
    {
        return m_position;
    }
    
    bool isAtEnd() const
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        assert(m_position <= m_size);
#endif
        return m_position == m_size;
    }
};

#endif /* MMKVReader_h */
