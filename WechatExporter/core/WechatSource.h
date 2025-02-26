//
//  WechatDataSource.h
//  WechatExporter
//
//  Created by Matthew on 2021/12/23.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef WechatDataSource_h
#define WechatDataSource_h

#include "ITunesParser.h"
#include "IDeviceBackup.h"

class WechatSource
{
public:
    WechatSource() : m_deviceInfo(NULL)
    {
    }
    WechatSource(const BackupItem& backupItem) : m_backupItem(backupItem), m_deviceInfo(NULL)
    {
    }
    WechatSource(const DeviceInfo& deviceInfo) : m_deviceInfo(NULL)
    {
        m_deviceInfo = new DeviceInfo(deviceInfo);
    }
    
    ~WechatSource()
    {
        if (NULL != m_deviceInfo)
        {
            delete m_deviceInfo;
        }
    }
    
    bool operator==(const WechatSource& rhs)
    {
        if (isDevice() != rhs.isDevice())
        {
            return false;
        }
        
        if (isDevice())
        {
            return m_deviceInfo->getUdid() == rhs.m_deviceInfo->getUdid();
        }
        else
        {
            return m_backupItem.getPath() == rhs.m_backupItem.getPath();
        }
        
    }
    
    const BackupItem& getBackupItem() const
    {
        return m_backupItem;
    }
    
    void setBackupItem(const BackupItem& backupItem)
    {
        m_backupItem = backupItem;
    }
    
    const DeviceInfo* getDeviceInfo() const
    {
        return m_deviceInfo;
    }
    
    DeviceInfo* getDeviceInfo()
    {
        return m_deviceInfo;
    }
    
    bool isDevice() const
    {
        return NULL != m_deviceInfo;
    }
    
    std::string getDisplayName() const
    {
        if (isDevice())
        {
            return m_deviceInfo->getName();
        }
        
        return m_backupItem.toString();
    }
private:
    
    BackupItem m_backupItem;
    DeviceInfo* m_deviceInfo;
};

#endif /* WechatDataSource_h */
