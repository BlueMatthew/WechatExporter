//
//  RawMessage.h
//  WechatExporter
//
//  Created by Matthew on 2020/10/13.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>
#include <queue>
#include <iostream>
#include <fstream>
#include <iomanip>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/compiler/parser.h>
// #include <google/protobuf/compiler/importer.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/text_format.h>

#include "Utils.h"


using namespace google::protobuf;
using namespace google::protobuf::io;
using namespace google::protobuf::compiler;

#ifndef RawMessage_h
#define RawMessage_h

bool convertUnknownField(const UnknownField &uf, std::string& value);
bool convertUnknownField(const UnknownField &uf, int& value);

class RawMessage
{
public:
    RawMessage();
    ~RawMessage();
    
    bool merge(const char *data, int length);
    bool mergeFile(const std::string& path);
    // bool parse(int fieldNumber, std::string& value);
    
    template<class T>
    bool parse(const std::string& fields, T& value);
    
    // static bool parse(const std::string& data, std::string& fields, std::string& value);
    // static bool parse(const std::string& data, std::string& fields, int& value);
    // static bool parseFile(const std::string& path, std::string& fields, std::string& value);
    
    static std::string toUtf8String(const std::string& str);
    
private:
    
    void release();
    // static bool parse(const std::string& data, std::queue<int>& fieldNumbers, std::string& value);

    google::protobuf::DescriptorPool* m_pool;
    google::protobuf::DynamicMessageFactory* m_factory;
    google::protobuf::Message* m_message;
};

template<class T>
inline bool RawMessage::parse(const std::string& fields, T& value)
{
    if (NULL == m_message)
    {
        return false;
    }
    
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
    
    const Reflection* reflection = m_message->GetReflection();
    const UnknownFieldSet& ufs = reflection->GetUnknownFields(*m_message);
    
    const UnknownFieldSet* pUfs = &ufs;
    
    std::vector<std::unique_ptr<UnknownFieldSet>> ufsList;

    for (int idx = 0; idx < fieldNumbers.size(); ++idx)
    {
        bool found = false;
        for (int fieldIdx = 0; fieldIdx < pUfs->field_count(); ++fieldIdx)
        {
            const UnknownField &uf = pUfs->field(fieldIdx);
            if (uf.number() == fieldNumbers[idx])
            {
                found = true;
                if (idx == fieldNumbers.size() - 1)
                {
                    return convertUnknownField(uf, value);
                }
                else
                {
                    if (uf.type() == UnknownField::TYPE_GROUP)
                    {
                        pUfs = &(uf.group());
                    }
                    else if (uf.type() == UnknownField::TYPE_LENGTH_DELIMITED)
                    {
                        std::unique_ptr<UnknownFieldSet> ufs(new UnknownFieldSet());
                        
                        if (ufs->ParseFromString(uf.length_delimited()))
                        {
                            pUfs = ufs.get();
                        }
                        
                        ufsList.push_back(std::move(ufs));
                    }
                }
                break;
            }
        }
        
        if (!found || NULL == pUfs)
        {
            break;
        }
    }
    
    return false;
}

#endif /* RawMessage_h */
