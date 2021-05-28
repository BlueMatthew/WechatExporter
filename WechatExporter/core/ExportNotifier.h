#ifndef ExportNotifier_h
#define ExportNotifier_h

class ExportNotifier
{
public:

	virtual ~ExportNotifier() {}

    virtual void onStart() const = 0;
	virtual void onProgress(uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const = 0;
	virtual void onComplete(bool cancelled) const = 0;
    
    virtual void onUserSessionStart(const std::string& usrName, uint32_t numberOfSessions) const = 0;
    virtual void onUserSessionComplete(const std::string& usrName) const = 0;
    
    virtual void onSessionStart(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfTotalMessages) const = 0;
    virtual void onSessionProgress(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const = 0;
    virtual void onSessionComplete(const std::string& sessionUsrName, void * sessionData, bool cancelled) const = 0;
    
    virtual void onTasksStart(const std::string& usrName, uint32_t numberOfTotalTasks) const = 0;
    virtual void onTasksProgress(const std::string& usrName, uint32_t numberOfCompletedTasks, uint32_t numberOfTotalMessages) const = 0;
    virtual void onTasksComplete(const std::string& usrName, bool cancelled) const = 0;

};

#endif /* ExportNotifier_h */
