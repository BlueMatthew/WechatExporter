//
//  ExportContext.h
//  WechatExporter
//
//  Created by Matthew on 2021/7/15.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef ExportContext_h
#define ExportContext_h

class ExportContext
{
private:
    int m_options;
    std::time_t m_exportTime;
    std::map<std::string, int64_t> m_maxIdForSessions;
    
public:
    ExportContext()
    {
    }
    
    ~ExportContext()
    {
    }
    
    int getOptions() const
    {
        return m_options;
    }
    void setOptions(int options)
    {
        m_options = options;
    }
    
    void refreshExportTime()
    {
        std::time(&m_exportTime);
    }
    
    std::time_t getExportTime() const
    {
        return m_exportTime;
    }
    
    size_t getNumberOfSessions() const
    {
        return m_maxIdForSessions.size();
    }
    
    bool getMaxId(const std::string& usrName, int64_t& maxId) const
    {
        std::map<std::string, int64_t>::const_iterator it = m_maxIdForSessions.find(usrName);
        if (it != m_maxIdForSessions.cend())
        {
            maxId = it->second;
            return true;
        }
        return false;
    }
    
    void setMaxId(const std::string& usrName, int64_t maxId)
    {
        m_maxIdForSessions[usrName] = maxId;
    }
    
    std::string serialize() const
    {
        Json::Value maxIdForSessions(Json::arrayValue);
        for (std::map<std::string, int64_t>::const_iterator it = m_maxIdForSessions.cbegin(); it != m_maxIdForSessions.cend(); ++it)
        {
            Json::Value itemObj(Json::objectValue);
            itemObj["usrName"] = Json::Value(it->first);
            itemObj["maxId"] = Json::Value(it->second);
            
            maxIdForSessions.append(itemObj);
        }
        
        Json::Value contextObj(Json::objectValue);
        contextObj["options"] = Json::Value(m_options);
        contextObj["exportTime"] = Json::Value(static_cast<uint32_t>(m_exportTime));
        contextObj["sessions"] = maxIdForSessions;
        
        Json::FastWriter writer;
        return writer.write(contextObj);
    }
    
    bool unserialize(const std::string& data)
    {
        Json::Reader reader;
        Json::Value contextObj;
        if (!reader.parse(data, contextObj))
        {
            return false;
        }
        
        if (!contextObj.isObject() || !contextObj.isMember("options") || !contextObj.isMember("exportTime") || !contextObj.isMember("sessions"))
        {
            return false;
        }
        
        const Json::Value& maxIdForSessions = contextObj["sessions"];
        if (!maxIdForSessions.isArray())
        {
            return false;
        }
        
        m_options = contextObj["options"].asInt();
        m_exportTime = static_cast<std::time_t>(contextObj["exportTime"].asInt());
        
        m_maxIdForSessions.clear();
        
        for (Json::ArrayIndex idx = 0; idx < maxIdForSessions.size(); idx++)
        {
            const Json::Value& itemObj = maxIdForSessions[idx];
            
            if (!itemObj.isObject() || !itemObj.isMember("usrName") || !itemObj.isMember("maxId"))
            {
                return false;
            }
            
            m_maxIdForSessions.insert(std::pair<std::string, int64_t>(itemObj["usrName"].asString(), itemObj["maxId"].asInt64()));
        }
        
        return true;
    }
};

#endif /* ExportContext_h */
