//
//  Exporter.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <cstdio>
#include <set>
#include <ctime>
#include <string>
#include <atomic>
#include <thread>
#include <atomic>

#include "Logger.h"
#include "Shell.h"
#include "WechatObjects.h"
#include "ITunesParser.h"
#include "semaphore.h"
#include "ExportNotifier.h"

#ifndef Exporter_h
#define Exporter_h

class SessionParser;

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
    Shell* m_shell;
    
    ITunesDb *m_iTunesDb;
    
    std::map<std::string, std::string> m_templates;
	std::map<std::string, std::string> m_localeStrings;

	ExportNotifier* m_notifier;
    
    std::atomic<bool> m_cancelled;
    int m_options;
    
    std::map<std::string, std::set<std::string>> m_usersAndSessions;
    
public:
    Exporter(const std::string& workDir, const std::string& backup, const std::string& output, Shell* shell, Logger* logger);
    ~Exporter();

	void setNotifier(ExportNotifier *notifier);
    
    bool loadUsersAndSessions(std::vector<std::pair<Friend, std::vector<Session>>>& usersAndSessions);

    bool run();
	bool isRunning() const;
    void cancel();
	void waitForComplition();
    
	void filterUsersAndSessions(const std::map<std::string, std::set<std::string>>& usersAndSessions);
    void ignoreAudio(bool ignoreAudio = true);
    void setOrder(bool asc = true);
    void saveFilesInSessionFolder(bool flags = true);

protected:
	bool runImpl();
	bool exportUser(Friend& user, std::string& userOutputPath);
    // bool loadUserSessions(Friend& user, std::vector<Session>& sessions) const;
    bool loadUserFriendsAndSessions(const Friend& user, Friends& friends, std::vector<Session>& sessions, bool detailedInfo = true) const;
	int exportSession(const Friend& user, const SessionParser& sessionParser, const Session& session, const std::string& userBase, const std::string& outputBase);

    bool fillUser(Friend& user);
	bool fillSession(Session& session, const Friends& friends) const;
    bool loadTemplates();
	bool loadStrings();
    std::string getTemplate(const std::string& key) const;
	std::string getLocaleString(std::string key) const;
    
    void notifyStart();
    void notifyComplete(bool cancelled = false);
    void notifyProgress(double progress);
};

#endif /* Exporter_h */
