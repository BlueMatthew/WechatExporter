//
//  ExportContext.h
//  WechatExporter
//
//  Created by Matthew on 2021/7/15.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef ExportContext_h
#define ExportContext_h

#define EXPORT_CONTEXT_VERSION_2_0  "2.0"
#define EXPORT_CONTEXT_VERSION      "3.0"

#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>

#include <sqlite3.h>

#define RETURN_FALSE_IF_FAILED(rc) if (SQLITE_OK != rc) { return false; }


#define WXEXP_DATA_FOLDER   ".wxexp"
#ifndef NDEBUG
#define WXEXP_DATA_FILE   "wxexp.txt"
#else
#define WXEXP_DATA_FILE   "wxexp.dat"
#endif

class ExportContext
{
private:
    std::string m_version;
    uint64_t m_options;
    std::time_t m_exportTime;
    
    std::string m_usrName;
    std::vector<std::pair<std::string, std::list<std::string>>> m_accountAndSessions;
    
    std::map<std::string, int64_t> m_maxIdForSessions;  //
    
    sqlite3*        m_db;
    sqlite3_stmt*   m_stmt;
    
    uint64_t        m_maxIdForSession;
    
private:
    void closeDb()
    {
        if (NULL != m_stmt)
        {
            sqlite3_finalize(m_stmt);
            m_stmt = NULL;
        }
        if (NULL != m_db)
        {
            sqlite3_close(m_db);
            m_db = NULL;
        }
    }
    
public:
    ExportContext() : m_db(NULL), m_stmt(NULL), m_maxIdForSession(0)
    {
    }
    
    ~ExportContext()
    {
        closeDb();
    }
    
    uint64_t getOptions() const
    {
        return m_options;
    }
    
    void setOptions(uint64_t options)
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
    
    size_t getNumberOfAccounts() const
    {
        return m_accountAndSessions.size();
    }
    
    std::string getVersion() const
    {
        return m_version;
    }
    
    size_t getNumberOfSessions(const std::string& account) const
    {
        size_t numberOfSessions = 0;
        for (auto it = m_accountAndSessions.cbegin(); it != m_accountAndSessions.cend(); ++it)
        {
            if (it->first == account)
            {
                numberOfSessions = it->second.size();
                break;
            }
        }
        return numberOfSessions;
    }

    bool getMaxId(const std::string& accountUsrName, const std::string& sessionUsrName, int64_t& maxId) const
    {
        std::string key = accountUsrName + "\t" + sessionUsrName;
        std::map<std::string, int64_t>::const_iterator it = m_maxIdForSessions.find(key);
        if (it != m_maxIdForSessions.cend())
        {
            maxId = it->second;
            return true;
        }
        return false;
    }

    void setMaxId(const std::string& accountUsrName, const std::string& sessionUsrName, int64_t maxId)
    {
        auto it = m_accountAndSessions.begin();
        for (; it != m_accountAndSessions.end(); ++it)
        {
            if (it->first == accountUsrName)
            {
                break;
            }
        }
        // m_accountAndSessions
        if (it == m_accountAndSessions.end())
        {
            it = m_accountAndSessions.insert(it, std::pair<std::string, std::list<std::string>>(accountUsrName, std::list<std::string>()));
        }
        it->second.push_back(sessionUsrName);

        std::string key = accountUsrName + "\t" + sessionUsrName;
        std::map<std::string, int64_t>::iterator it2 = m_maxIdForSessions.find(key);
        if (it2 != m_maxIdForSessions.end())
        {
            maxId = it2->second;
        }
        else
        {
            m_maxIdForSessions.insert(it2, std::pair<std::string, int64_t>(key, maxId));
        }
    }
    
    bool prepareUserDatabase(const Friend& user, const std::string& outputDir)
    {
        closeDb();
        std::string dbPath = combinePath(outputDir, WXEXP_DATA_FOLDER, user.getHash() + ".db");
        int rc = openSqlite3Database(dbPath, &m_db, false);
        
        if (SQLITE_OK == rc)
        {
            sqlite3_exec(m_db, "PRAGMA mmap_size=268435456;PRAGMA synchronous=OFF;", NULL, NULL, NULL); // 256M:268435456  2M 2097152
        }

        return (rc == SQLITE_OK);
    }
    
    bool prepareSessionTable(const Session& session)
    {
        if (NULL == m_db)
        {
            return false;
        }
        
        if (NULL != m_stmt)
        {
            sqlite3_finalize(m_stmt);
            m_stmt = NULL;
        }
        
        std::string sql = "CREATE TABLE IF NOT EXISTS Chat_" + session.getHash() + "(CreateTime INTEGER DEFAULT 0, Des INTEGER, MesLocalID INTEGER PRIMARY KEY, Message TEXT, MesSvrID INTEGER DEFAULT 0, Status INTEGER DEFAULT 0, TableVer INTEGER DEFAULT 1, Type INTEGER);";
        sql += "CREATE INDEX IF NOT EXISTS Chat_" + session.getHash() + "_Index ON Chat_" + session.getHash() + "(MesSvrID);";
        sql += "CREATE INDEX IF NOT EXISTS Chat_" + session.getHash() + "_Index2 ON Chat_" + session.getHash() + "(CreateTime)";
        
        int rc = sqlite3_exec(m_db, sql.c_str(), NULL, NULL, NULL);
        RETURN_FALSE_IF_FAILED(rc);
        
        m_maxIdForSession = 0;
        sql = "SELECT MAX(MesLocalID) from Chat_" + session.getHash();
        
        rc = sqlite3_prepare_v2(m_db, sql.c_str(), (int)(sql.size()), &m_stmt, NULL);
        if (rc == SQLITE_OK)
        {
            if ((rc = sqlite3_step(m_stmt)) == SQLITE_ROW)
            {
                m_maxIdForSession = sqlite3_column_int64(m_stmt, 0);
            }
            sqlite3_finalize(m_stmt);
            m_stmt = NULL;
        }

        sql = "INSERT INTO Chat_" + session.getHash() + "(CreateTime,Des,MesLocalID,Message,MesSvrID,Status,TableVer,Type) VALUES(?,?,?,?,?,?,?,?)";
        rc = sqlite3_prepare_v2(m_db, sql.c_str(), (int)(sql.size()), &m_stmt, NULL);
        
        return (rc == SQLITE_OK);
    }
    
    bool insertMessage(const Session& session, const WXMSG& msg)
    {
        if (msg.msgIdValue <= m_maxIdForSession)
        {
            return true;
        }
        
        if (NULL == m_db || NULL == m_stmt)
        {
            return false;
        }

        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
        
        int rc = sqlite3_bind_int(m_stmt, 1, (int)msg.createTime);
        rc = sqlite3_bind_int(m_stmt, 2, msg.des);
        RETURN_FALSE_IF_FAILED(rc);
        rc = sqlite3_bind_int64(m_stmt, 3, msg.msgIdValue);
        RETURN_FALSE_IF_FAILED(rc);
        rc = sqlite3_bind_text(m_stmt, 4, msg.content.c_str(), (int)(msg.content.size()), NULL);
        RETURN_FALSE_IF_FAILED(rc);
        rc = sqlite3_bind_int64(m_stmt, 5, (sqlite_int64)msg.msgSvrId);
        RETURN_FALSE_IF_FAILED(rc);
        rc = sqlite3_bind_int(m_stmt, 6, msg.status);
        RETURN_FALSE_IF_FAILED(rc);
        rc = sqlite3_bind_int(m_stmt, 7, msg.tableVersion);
        RETURN_FALSE_IF_FAILED(rc);
        rc = sqlite3_bind_int(m_stmt, 8, msg.type);
        RETURN_FALSE_IF_FAILED(rc);
        
        rc = sqlite3_step(m_stmt);
        if (SQLITE_DONE != rc && SQLITE_ROW != rc)
        {
            const char* err = sqlite3_errmsg(m_db);
            int aa = 0;
        }
        
        return (rc == SQLITE_DONE || rc == SQLITE_ROW);
    }
    
    void saveMessage(const WXMSG& msg)
    {
        // CREATE TABLE IF NOT EXISTS Chat_%%CHAT_ID%%(CreateTime INTEGER DEFAULT 0, Des INTEGER, ImgStatus INTEGER DEFAULT 0, MesLocalID INTEGER, Message TEXT, MesSvrID INTEGER DEFAULT 0  PRIMARY KEY, Status INTEGER DEFAULT 0, Type INTEGER)

        // CREATE INDEX IF NOT EXISTS Chat_%%CHAT_ID%%_ct ON Chat_%%CHAT_ID%% (CreateTime)
    }

    std::string serialize() const
    {
        Json::Value contextObj(Json::objectValue);
        contextObj["version"] = Json::Value(EXPORT_CONTEXT_VERSION);
        contextObj["options"] = Json::Value(m_options);
        contextObj["exportTime"] = Json::Value(static_cast<int64_t>(m_exportTime));
        contextObj["accounts"] = Json::Value(Json::arrayValue);
        Json::Value& accounts = contextObj["accounts"];
        
        int64_t maxId = 0;
        auto it3 = m_maxIdForSessions.cend();
        
        for (auto it = m_accountAndSessions.cbegin(); it != m_accountAndSessions.cend(); ++it)
        {
            Json::Value& accountObj = accounts.append(Json::Value(Json::objectValue));
            
            accountObj["usrName"] = Json::Value(it->first);
            accountObj["sessions"] = Json::Value(Json::arrayValue);
            
            Json::Value& sessionsObj = accountObj["sessions"];
            const std::list<std::string>& sessions = it->second;
            
            std::set<std::string> uniqueSessions;
            
            for (auto it2 = sessions.cbegin(); it2 != sessions.cend(); ++it2)
            {
                Json::Value& itemObj = sessionsObj.append(Json::Value(Json::objectValue));
                
                std::string key = it->first + "\t" + *it2;
                
                itemObj["usrName"] = Json::Value(*it2);
                maxId = 0;
                it3 = m_maxIdForSessions.find(key);
                if (it3 != m_maxIdForSessions.cend())
                {
                    maxId = it3->second;
                }
                itemObj["maxId"] = Json::Value(maxId);
            }
        }

        Json::StreamWriterBuilder builder;
#ifndef NDEBUG
        builder["indentation"] = "\t";
        builder["emitUTF8"] = true;
#else
        builder["indentation"] = "";
#endif
        
        return Json::writeString(builder, contextObj);
    }
    
    bool upgrade(const std::string& data)
    {
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

        Json::Value contextObj;
        Json::String error;
        if (!reader->parse(data.c_str(), data.c_str() + data.size(), &contextObj, &error))
        {
            return false;
        }
        
        if (contextObj.isMember("version"))
        {
            m_version = contextObj["version"].asString();
        }
        else
        {
            m_version.clear();
        }
        
        if (m_version == EXPORT_CONTEXT_VERSION)
        {
            return true;
        }

        // First version
        if (!contextObj.isObject() || !contextObj.isMember("options") || !contextObj.isMember("exportTime") || !contextObj.isMember("sessions"))
        {
            return false;
        }
        
        const Json::Value& maxIdForSessions = contextObj["sessions"];
        if (!maxIdForSessions.isArray())
        {
            return false;
        }

        if (m_version != EXPORT_CONTEXT_VERSION)
        {
            int options = contextObj["options"].asInt();
            ExportOption eo;
            eo.fromSessionParsingOptions(options);
            m_options = (uint64_t)eo;
        }
        else
        {
            m_options = contextObj["options"].asUInt64();
        }
        
        // m_options = contextObj["options"].asInt();
        m_exportTime = static_cast<std::time_t>(contextObj["exportTime"].asInt64());
        
        m_maxIdForSessions.clear();
    
        for (Json::ArrayIndex idx = 0; idx < maxIdForSessions.size(); idx++)
        {
            const Json::Value& itemObj = maxIdForSessions[idx];
            
            if (!itemObj.isObject() || !itemObj.isMember("usrName") || !itemObj.isMember("maxId"))
            {
                return false;
            }
            
            m_maxIdForSessions.insert(std::pair<std::string, int64_t>(m_usrName + "\t" + itemObj["usrName"].asString(), itemObj["maxId"].asInt64()));
        }
        
        return true;
    }

    bool unserialize(const std::string& data)
    {
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

        Json::Value contextObj;
        Json::String error;
        if (!reader->parse(data.c_str(), data.c_str() + data.size(), &contextObj, &error))
        {
            return false;
        }
        
        if (contextObj.isMember("version"))
        {
            m_version = contextObj["version"].asString();
        }
        else
        {
            m_version.clear();
        }
        
        if (m_version.empty())
        {
            // First version
            if (!contextObj.isObject() || !contextObj.isMember("options") || !contextObj.isMember("exportTime") || !contextObj.isMember("sessions"))
            {
                return false;
            }
            
            const Json::Value& maxIdForSessions = contextObj["sessions"];
            if (!maxIdForSessions.isArray())
            {
                return false;
            }

            m_options = contextObj["options"].asUInt64();
            m_exportTime = static_cast<std::time_t>(contextObj["exportTime"].asInt64());
            
            m_accountAndSessions.clear();
            m_maxIdForSessions.clear();

            for (Json::ArrayIndex idx = 0; idx < maxIdForSessions.size(); idx++)
            {
                const Json::Value& itemObj = maxIdForSessions[idx];
                
                if (!itemObj.isObject() || !itemObj.isMember("usrName") || !itemObj.isMember("maxId"))
                {
                    return false;
                }
                
                m_maxIdForSessions.insert(std::pair<std::string, int64_t>(m_usrName + "\t" + itemObj["usrName"].asString(), itemObj["maxId"].asInt64()));
            }
        }
        else
        {
            if (!contextObj.isObject() || !contextObj.isMember("options") || !contextObj.isMember("exportTime") || !contextObj.isMember("accounts"))
            {
                return false;
            }
            
            const Json::Value& accounts = contextObj["accounts"];
            if (!accounts.isArray())
            {
                return false;
            }

            if (m_version == EXPORT_CONTEXT_VERSION_2_0)
            {
                int options = contextObj["options"].asInt();
                ExportOption eo;
                eo.fromSessionParsingOptions(options);
                m_options = (uint64_t)eo;
            }
            else
            {
                m_options = contextObj["options"].asUInt64();
            }
			m_exportTime = static_cast<std::time_t>(contextObj["exportTime"].asInt64());
            
            m_accountAndSessions.clear();
            m_maxIdForSessions.clear();
            
            if (!accounts.empty())
            {
                m_accountAndSessions.reserve(accounts.size());
            }

            for (Json::ArrayIndex idx = 0; idx < accounts.size(); idx++)
            {
                const Json::Value& accountObj = accounts[idx];
                
                if (!accountObj.isObject() || !accountObj.isMember("usrName") || !accountObj.isMember("sessions"))
                {
                    return false;
                }
                const Json::Value& sessionsObj = accountObj["sessions"];
                if (!sessionsObj.isArray())
                {
                    return false;
                }
                
                std::string account = accountObj["usrName"].asString();
                auto it = m_accountAndSessions.insert(m_accountAndSessions.end(), std::pair<std::string, std::list<std::string>>(account, std::list<std::string>()));

                for (Json::ArrayIndex idx = 0; idx < sessionsObj.size(); idx++)
                {
                    const Json::Value& itemObj = sessionsObj[idx];
                    
                    if (!itemObj.isObject() || !itemObj.isMember("usrName") || !itemObj.isMember("maxId"))
                    {
                        return false;
                    }
                    
                    auto it2 = it->second.insert(it->second.begin(), itemObj["usrName"].asString());
                    int64_t maxId = itemObj["maxId"].asInt64();
                    if (maxId != 0)
                    {
                        m_maxIdForSessions.insert(std::pair<std::string, int64_t>(account + "\t" + (*it2), itemObj["maxId"].asInt64()));
                    }
                }
                
            }
        }

        return true;
    }
};

class PageInfo
{
private:
    uint32_t m_page;
    uint32_t m_count;
    uint16_t m_year;
    uint16_t m_month;
    std::string m_text;
    std::string m_fileName;

public:
    
    PageInfo(uint32_t page, uint32_t count, uint16_t year, uint16_t month, const std::string& fileName) : m_page(page), m_count(count), m_year(year), m_month(month), m_text(std::to_string(page + 1)), m_fileName(fileName)
    {
    }
    
    PageInfo(uint32_t page, uint32_t count, uint16_t year, uint16_t month, const std::string& fileName, const std::string& text) : m_page(page), m_count(count), m_year(year), m_month(month), m_text(text), m_fileName(fileName)
    {
    }

    uint32_t getPage() const
    {
        return m_page;
    }
    
    uint32_t getCount() const
    {
        return m_count;
    }
    
    void setCount(uint32_t count)
    {
        m_count = count;
    }
    
    uint16_t getYear() const
    {
        return m_year;
    }
    
    uint16_t getMonth() const
    {
        return m_month;
    }
    
    std::string getText() const
    {
        return m_text;
    }
    
    std::string getFileName() const
    {
        return m_fileName;
    }
    
};

class WXMSG;

class Pager
{
public:
    
    Pager() : m_last(m_pages.end()), m_totalNumberOfPreviousPages(0)
    {
    }
    
    virtual ~Pager()
    {
    }
    
    virtual bool buildNewPage(const WXMSG *msg, const std::vector<std::string>& messages)
    {
        return false;
    }
    
    const std::vector<PageInfo>& getPages() const
    {
        return m_pages;
    }
    
    bool hasPages() const
    {
        return !m_pages.empty();
    }
    
protected:
    std::vector<PageInfo> m_pages;
    std::vector<PageInfo>::iterator m_last;
    uint64_t m_totalNumberOfPreviousPages;
};

class NumberPager : public Pager
{
public:
    
    NumberPager(uint32_t pageSize) : Pager(), m_pageSize(pageSize), m_numberOfRawMsgs(0)
    {
    }
    
    virtual ~NumberPager()
    {
    }
    
    virtual bool buildNewPage(const WXMSG *msg, const std::vector<std::string>& messages)
    {
        m_numberOfRawMsgs++;
        bool newPage = ((m_numberOfRawMsgs % m_pageSize) == 1);
        if (newPage)
        {
            m_last = m_pages.emplace(m_pages.end(), (uint32_t)m_pages.size(), (uint32_t)(messages.size() - m_totalNumberOfPreviousPages), 0, 0, std::to_string(m_pages.size() + 1));
            m_totalNumberOfPreviousPages = messages.size();
            return false;
        }
        else
        {
            m_last->setCount(m_last->getCount() + (uint32_t)(messages.size() - m_totalNumberOfPreviousPages));
            m_totalNumberOfPreviousPages = messages.size();
        }
        
        return true;
        

        return newPage;
    }
    
private:
    uint32_t m_pageSize;
    uint64_t m_numberOfRawMsgs;
};

class YearPager : public Pager
{
public:
    
    YearPager() : Pager(), m_previousYear(0)
    {
    }
    
    bool buildNewPage(const WXMSG *msg, const std::vector<std::string>& messages)
    {
        std::time_t ts = msg->createTime;
        std::tm* t1 = std::localtime(&ts);
        uint16_t year = t1->tm_year + 1900;
        
        if (m_previousYear != year)
        {
            m_last = m_pages.emplace(m_pages.end(), (uint32_t)m_pages.size(), (uint32_t)(messages.size() - m_totalNumberOfPreviousPages), year, 0, std::to_string(m_pages.size() + 1));
            
            m_totalNumberOfPreviousPages = messages.size();
            m_previousYear = year;
            return false;
        }
        else
        {
            m_last->setCount(m_last->getCount() + (uint32_t)(messages.size() - m_totalNumberOfPreviousPages));
            m_totalNumberOfPreviousPages = messages.size();
        }
        
        return true;
    }
    
protected:
    uint16_t m_previousYear;
    
};

class YearMonthPager : public YearPager
{
public:
    
    YearMonthPager() : YearPager(), m_previousMonth(0)
    {
    }
    
    bool buildNewPage(const WXMSG *msg, const std::vector<std::string>& messages)
    {
        std::time_t ts = msg->createTime;
        std::tm* t1 = std::localtime(&ts);
        uint16_t year = t1->tm_year + 1900;
        uint16_t month = t1->tm_mon + 1;
        
        if ((m_previousYear != year) || (m_previousMonth != month))
        {
            m_last = m_pages.emplace(m_pages.end(), (uint32_t)m_pages.size(), (uint32_t)(messages.size() - m_totalNumberOfPreviousPages), year, month, std::to_string(m_pages.size() + 1));
            
            m_totalNumberOfPreviousPages = messages.size();
            m_previousYear = year;
            m_previousMonth = month;
            return false;
        }
        else
        {
            m_last->setCount(m_last->getCount() + (uint32_t)(messages.size() - m_totalNumberOfPreviousPages));
            m_totalNumberOfPreviousPages = messages.size();
        }
        
        return true;
    }
    
protected:
    uint16_t m_previousMonth;
    
};


#endif /* ExportContext_h */
