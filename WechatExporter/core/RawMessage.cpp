//
//  RawMessage.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/10/13.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "RawMessage.h"
#ifdef _WIN32
#include <atlstr.h>
#endif

bool convertUnknownField(const UnknownField &uf, std::string& value)
{
    if (uf.type() == UnknownField::TYPE_LENGTH_DELIMITED)
    {
        value.assign(uf.length_delimited().c_str(), uf.GetLengthDelimitedSize());
        return true;
    }
    else if (uf.type() == UnknownField::TYPE_VARINT)
    {
        value = std::to_string(uf.varint());
        return true;
    }
    else if (uf.type() == UnknownField::TYPE_FIXED32)
    {
        value = std::to_string(uf.fixed32());
        return true;
    }
    else if (uf.type() == UnknownField::TYPE_FIXED64)
    {
        value = std::to_string(uf.fixed64());
        return true;
    }
    
    return false;
}

bool convertUnknownField(const UnknownField &uf, int& value)
{
    if (uf.type() == UnknownField::TYPE_VARINT)
    {
        value = static_cast<int>(uf.varint());
        return true;
    }
    else if (uf.type() == UnknownField::TYPE_FIXED32)
    {
        value = static_cast<int>(uf.fixed32());
        return true;
    }
    else if (uf.type() == UnknownField::TYPE_FIXED64)
    {
        value = static_cast<int>(uf.fixed64());
        return true;
    }
    
    return false;
}

std::string RawMessage::toUtf8String(const std::string& str)
{
    return UnescapeCEscapeString(str);
}

RawMessage::RawMessage() : m_pool(NULL), m_factory(NULL), m_message(NULL)
{
}

RawMessage::~RawMessage()
{
    release();
}

void RawMessage::release()
{
    if (NULL != m_message)
    {
        delete m_message;
        m_message = NULL;
    }
    if (NULL != m_factory)
    {
        delete m_factory;
        m_factory = NULL;
    }
    if (NULL != m_pool)
    {
        delete m_pool;
        m_pool = NULL;
    }
}

bool RawMessage::merge(const char *data, int length)
{
    release();
    
    m_pool = new DescriptorPool();
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name("EmptyMessage");
    GOOGLE_CHECK(m_pool->BuildFile(file) != NULL);

    const Descriptor *descriptor = m_pool->FindMessageTypeByName("EmptyMessage");
    if (NULL == descriptor)
    {
        // FormatError(outputString, lengthOfOutputString, ERROR_NO_MESSAGE_TYPE, src->messageTypeName);
        return false;
    }

    m_factory = new DynamicMessageFactory(m_pool);
    const Message *message = m_factory->GetPrototype(descriptor);
    if (NULL == message)
    {
        return false;
    }
    
    m_message = message->New();
    if (NULL == m_message)
    {
        return false;
    }
    
    if (!m_message->ParseFromArray(reinterpret_cast<const void *>(data), length))
    {
        delete m_message;
        m_message = NULL;
        return false;
    }

    return true;
}

bool RawMessage::mergeFile(const std::string& path)
{
    release();
    
#ifdef _WIN32
    CA2W pszW(path.c_str(), CP_UTF8);
    std::ifstream file(pszW, std::ios::in|std::ios::binary|std::ios::ate);
#else
    std::ifstream file(path.c_str(), std::ios::in|std::ios::binary|std::ios::ate);
#endif
    if (file.is_open())
    {
        std::streampos size = file.tellg();
        std::vector<char> buffer;
        buffer.resize(size);
        
        file.seekg (0, std::ios::beg);
        file.read((char *)(&buffer[0]), size);
        file.close();

        return merge(&buffer[0], static_cast<int>(size));
    }
    
    return false;
}

/*
bool RawMessage::parse(int fieldNumber, std::string& value)
{
    if (NULL == m_message)
    {
        return false;
    }
    
    const Reflection* reflection = m_message->GetReflection();
    const UnknownFieldSet& ufs = reflection->GetUnknownFields(*m_message);
    
    for (int fieldIdx = 0; fieldIdx < ufs.field_count(); ++fieldIdx)
    {
        const UnknownField uf = ufs.field(fieldIdx);
        if (uf.number() == fieldNumber)
        {
            value = uf.length_delimited();
            return true;
        }
    }

    return false;
}

bool RawMessage::parseFile(const std::string& path, std::string& fields, std::string& value)
{
    std::string data = readFile(path);
    return parse(data, fields, value);
}

bool RawMessage::parse(const std::string& data, std::string& fields, std::string& value)
{
    std::queue<int> fieldNumbers;
    
    std::string::size_type start = 0;
    std::string::size_type end = fields.find('.');
    while (end != std::string::npos)
    {
        std::string field = fields.substr(start, end - start);
        if (field.empty())
        {
            return false;
        }
        
        int fieldNumber = std::stoi(field);
        fieldNumbers.push(fieldNumber);
        start = end + 1;
        end = fields.find('.', start);
    }
    std::string field = fields.substr(start, end);
    if (field.empty())
    {
        return false;
    }
    int fieldNumber = std::stoi(field);
    fieldNumbers.push(fieldNumber);
    
    return parse(data, fieldNumbers, value);
}

bool RawMessage::parse(const std::string& data, std::queue<int>& fieldNumbers, std::string& value)
{
    if (fieldNumbers.empty())
    {
        return false;
    }

    UnknownFieldSet unknownFields;
    if (unknownFields.ParseFromString(data))
    {
        int fieldNumber = fieldNumbers.front();
        fieldNumbers.pop();
        
        for (int fieldIdx = 0; fieldIdx < unknownFields.field_count(); ++fieldIdx)
        {
            const UnknownField uf = unknownFields.field(fieldIdx);
            if (uf.number() == fieldNumber)
            {
                if (fieldNumbers.empty())
                {
                    value = uf.length_delimited();
                    return true;
                }
                else
                {
                    std::string embeded_data = uf.length_delimited();
                    return parse(embeded_data, fieldNumbers, value);
                }

                break;
            }
        }
    }

    return false;
}
*/
