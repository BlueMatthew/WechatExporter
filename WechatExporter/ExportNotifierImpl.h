#pragma once

#include "ExportNotifier.h"

class ExportNotifierImpl : public ExportNotifier
{
protected:
    NSProgressIndicator *m_progressBar;
	NSMutableArray<NSControl *> *m_ctrls;
	bool m_isCancelled;

public:
	ExportNotifierImpl(NSProgressIndicator* progressBar, std::vector<NSControl *>& ctrls) : m_isCancelled(false)
	{
        m_progressBar = progressBar;
		m_ctrls = [NSMutableArray<NSControl *> arrayWithCapacity:ctrls.size()];
        for (std::vector<NSControl *>::iterator it = ctrls.begin(); it != ctrls.end(); ++it)
        {
            [m_ctrls addObject:*it];
        }
	}

	~ExportNotifierImpl()
	{
        m_progressBar = nil;
        [m_ctrls removeAllObjects];
        m_ctrls = nil;
	}

	void cancel()
	{
		m_isCancelled = true;
	}

	bool isCancelled() const
	{
		return m_isCancelled;
	}
    
    void onStart() const
    {
        __block NSProgressIndicator* progressBar = m_progressBar;
        __block NSMutableArray<NSControl *> *ctrls = m_ctrls;
        
        dispatch_async(dispatch_get_main_queue(), ^{
            progressBar.hidden = NO;
            [progressBar startAnimation:nil];
            for (NSControl* ctrl : ctrls)
            {
                ctrl.enabled = NO;
            }
        });
    }
    
	void onProgress(double progress) const
	{
        
	}
	
	void onComplete(bool cancelled) const
	{
        __block NSProgressIndicator* progressBar = m_progressBar;
        __block NSMutableArray<NSControl *> *ctrls = m_ctrls;
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [progressBar stopAnimation:nil];
            for (NSControl* ctrl : ctrls)
            {
                ctrl.enabled = YES;
            }
        });
	}

	
};

