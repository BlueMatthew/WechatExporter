//
//  Exporter.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <cstdio>
#include <ctime>
#include <string>
#include <atomic>
#include <thread>
#include <atomic>

#include "Logger.h"
#include "Shell.h"
#include "WechatObjects.h"
#include "ITunesParser.h"
#include "Downloader.h"
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
    
public:
    Exporter(const std::string& workDir, const std::string& backup, const std::string& output, Shell* shell, Logger* logger);
    ~Exporter();

	void setNotifier(ExportNotifier *notifier);
    
    bool run();
	bool isRunning() const;
    void cancel();
	void waitForComplition();
    
    void ignoreAudio(bool ignoreAudio = true);
    void setOrder(bool asc = true);

protected:
	bool runImpl();
	bool exportUser(Friend& user, Downloader& downloadPool);
	int exportSession(Friend& user, const SessionParser& sessionParser, const Session& session, const std::string& userBase, const std::string& outputBase);

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
