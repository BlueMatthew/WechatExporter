//
//  Utils.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Utils.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>



const char* GetVarint32PtrFallback(const char* p, const char* limit, uint32_t* value)
{
    uint32_t result = 0;
    for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7)
    {
        uint32_t byte = *(reinterpret_cast<const unsigned char*>(p));
        p++;
        if (byte & 128)
        {
            // More bytes are present
            result |= ((byte & 127) << shift);
        }
        else
        {
            result |= (byte << shift);
            *value = result;
            return reinterpret_cast<const char*>(p);
        }
    }
    return NULL;
}

const char* calcVarint32Ptr(const char* p, const char* limit, uint32_t* value)
{
    if (p < limit)
    {
        uint32_t result = *(reinterpret_cast<const unsigned char*>(p));
        if ((result & 128) == 0)
        {
            *value = result;
            return p + 1;
        }
    }
    return GetVarint32PtrFallback(p, limit, value);
}

const unsigned char* calcVarint32Ptr(const unsigned char* p, const unsigned char* limit, uint32_t* value)
{
    const char* p1 = calcVarint32Ptr(reinterpret_cast<const char*>(p), reinterpret_cast<const char*>(limit), value);
    return reinterpret_cast<const unsigned char*>(p1);
}

/*
class Protobuf2JsonErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
    virtual void AddError(const std::string & filename, int line, int column, const std::string & message) {
        // define import error collector
        printf("%s, %d, %d, %s\n", filename.c_str(), line, column, message.c_str());
    }
};
*/

/*
bool parseFieldValueFromProtobuf(const unsigned char *data, size_t length, const std::string &fields, std::string& value)
{
    std::vector<unsigned int> fieldNumbers;
    
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
        fieldNumbers.push_back(fieldNumber);
        start = end + 1;
        end = fields.find('.', start);
    }
    std::string field = fields.substr(start, end);
    if (field.empty())
    {
        return false;
    }
    int fieldNumber = std::stoi(field);
    fieldNumbers.push_back(fieldNumber);

    DescriptorPool pool;
    FileDescriptorProto file;
    file.set_name("empty_message.proto");
    file.add_message_type()->set_name("EmptyMessage");
    GOOGLE_CHECK(pool.BuildFile(file) != NULL);

    const Descriptor *descriptor = pool.FindMessageTypeByName("EmptyMessage");
    if (NULL == descriptor)
    {
        // FormatError(outputString, lengthOfOutputString, ERROR_NO_MESSAGE_TYPE, src->messageTypeName);
        return false;
    }

    DynamicMessageFactory factory(&pool);
    const Message *message = factory.GetPrototype(descriptor);
    
    std::unique_ptr<Message> msg(message->New());
    // Message *msg = message->New();
    if (NULL == msg)
    {
        // FormatError(outputString, lengthOfOutputString, ERROR_NEW_MESSAGE, src->messageTypeName);
        return false;
    }
    
    
    // ZeroCopyInputStream* in_stream = new FileInputStream(infd, 128);
    
    if (!msg->ParseFromArray(reinterpret_cast<const void *>(data), static_cast<int>(length)))
    {
        return false;
    }
    
    const UnknownFieldSet& ufs = msg->GetReflection()->GetUnknownFields(*msg);
    
    const UnknownFieldSet* pUfs = &ufs;
    
    for (int idx = 0; idx < fieldNumbers.size(); ++idx)
    {
        bool found = false;
        for (int fieldIdx = 0; fieldIdx < pUfs->field_count(); ++fieldIdx)
        {
            const UnknownField uf = pUfs->field(fieldIdx);
            if (uf.number() == fieldNumbers[idx])
            {
                found = true;
                if (idx == fieldNumbers.size() - 1)
                {
                    value = uf.length_delimited();
                    return true;
                }
                else
                {
                    pUfs = &(uf.group());
                }
                break;
            }
        }
        
        if (!found)
        {
            break;
        }
    }

    return false;
}

bool parseFieldValueFromProtobuf(const std::string& path, const std::string &fields, std::string& value)
{
    std::ifstream file(path.c_str(), std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open())
    {
        std::streampos size = file.tellg();
        std::vector<unsigned char> buffer;
        buffer.resize(size);
        
        file.seekg (0, std::ios::beg);
        file.read((char *)(&buffer[0]), size);
        file.close();

        return parseFieldValueFromProtobuf(&buffer[0], size, fields, value);
    }
    
    return false;
}


*/

