//
//  Exporter.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <set>
#include <string>
#include <thread>
#include <atomic>

#include "Logger.h"
#include "WechatObjects.h"
#include "ITunesParser.h"
#include "ExportNotifier.h"

#ifndef Exporter_h
#define Exporter_h

class MessageParser;
class TemplateValues;

class Exporter
{
protected:
    std::atomic_bool m_running;
    std::thread m_thread;

    // semaphore& m_signal;
    std::string m_workDir;
    
    WechatInfo m_wechatInfo;
    std::string m_backup;
    std::string m_output;
    Logger* m_logger;
    
    ITunesDb *m_iTunesDb;
    ITunesDb *m_iTunesDbShare;
    
    std::map<std::string, std::string> m_templates;
    std::map<std::string, std::string> m_localeStrings;

    ExportNotifier* m_notifier;
    
    std::atomic<bool> m_cancelled;
    int m_options;
    bool m_loadingDataOnScroll;
    std::string m_extName;
    std::string m_templatesName;
    
    std::map<std::string, std::set<std::string>> m_usersAndSessionsFilter;
    
    std::vector<std::pair<Friend, std::vector<Session>>> m_usersAndSessions;
    
public:
    Exporter(const std::string& workDir, const std::string& backup, const std::string& output, Logger* logger);
    ~Exporter();

    void setNotifier(ExportNotifier *notifier);
    
    bool loadUsersAndSessions();
    void swapUsersAndSessions(std::vector<std::pair<Friend, std::vector<Session>>>& usersAndSessions);

    bool run();
    bool isRunning() const;
    void cancel();
    void waitForComplition();
    
    void filterUsersAndSessions(const std::map<std::string, std::set<std::string>>& usersAndSessions);
    void setTextMode(bool textMode = true);
    void setOrder(bool asc = true);
    void saveFilesInSessionFolder(bool flags = true);
    void setSyncLoading(bool syncLoading = true);
    void setLoadingDataOnScroll(bool loadingDataOnScroll = true);
    void supportsFilter(bool supportsFilter = true);
    void setExtName(const std::string& extName);
    void setTemplatesName(const std::string& templatesName);
    
    static void initializeExporter();
    static void uninitializeExporter();

protected:
    bool runImpl();
    bool exportUser(Friend& user, std::string& userOutputPath);
    // bool loadUserSessions(Friend& user, std::vector<Session>& sessions) const;
    bool loadUserFriendsAndSessions(const Friend& user, Friends& friends, std::vector<Session>& sessions, bool detailedInfo = true) const;
    int exportSession(const Friend& user, const MessageParser& msgParser, const Session& session, const std::string& userBase, const std::string& outputBase);
    
    bool exportMessage(const Session& session, const std::vector<TemplateValues>& tvs, std::vector<std::string>& messages);

    bool fillSession(Session& session, const Friends& friends) const;
    void releaseITunes();
    bool loadITunes(bool detailedInfo = true);
    bool loadTemplates();
    bool loadStrings();
    std::string getTemplate(const std::string& key) const;
    std::string getLocaleString(const std::string& key) const;
    
    void notifyStart();
    void notifyComplete(bool cancelled = false);
    void notifyProgress(uint32_t numberOfMessages, uint32_t numberOfTotalMessages);
    bool buildFileNameForUser(Friend& user, std::set<std::string>& existingFileNames);
    std::string buildContentFromTemplateValues(const TemplateValues& values) const;
    
    bool filterITunesFile(const char * file, int flags) const;
};

#endif /* Exporter_h */
