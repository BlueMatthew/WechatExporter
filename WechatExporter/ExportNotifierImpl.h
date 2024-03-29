#pragma once

#include "ExportNotifier.h"
#include "ViewController.h"

class ExportNotifierImpl : public ExportNotifier
{
protected:
    __weak ViewController *m_viewController;
	

public:
	ExportNotifierImpl(ViewController* viewController)
	{
        m_viewController = viewController;
	}

	~ExportNotifierImpl()
	{
        m_viewController = nil;
	}
    
    void onStart() const
    {
        __block __weak ViewController* viewController = m_viewController;
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong __typeof(viewController)strongVC = viewController;
            if (strongVC)
            {
                [strongVC onStart];
                strongVC = nil;
            }
        });
    }
    
	void onProgress(uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const
	{
	}
	
	void onComplete(bool cancelled) const
	{
        __block __weak ViewController* viewController = m_viewController;
        __block BOOL localCancelled = cancelled;
        
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong __typeof(viewController)strongVC = viewController;
            if (strongVC)
            {
                [strongVC onComplete:localCancelled];
                strongVC = nil;
            }
        });
	}
    
    void onUserSessionStart(const std::string& usrName, uint32_t numberOfSessions) const
    {
        
    }
    
    void onUserSessionComplete(const std::string& usrName) const
    {
        
    }

    void onSessionStart(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfTotalMessages) const
    {
        
    }
    
    void onSessionProgress(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const
    {
        
    }
    
    void onSessionComplete(const std::string& sessionUsrName, void * sessionData, bool cancelled) const
    {
        
    }
    
    void onTasksStart(const std::string& usrName, uint32_t numberOfTotalTasks) const
    {
        
    }
    
    void onTasksProgress(const std::string& usrName, uint32_t numberOfCompletedTasks, uint32_t numberOfTotalMessages) const
    {
        
    }
    
    void onTasksComplete(const std::string& usrName, bool cancelled) const
    {
        
    }
	
};

