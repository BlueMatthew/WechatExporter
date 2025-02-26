//
//  IDevice.h
//  WechatExporter
//
//  Created by Matthew on 2021/12/7.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef IDevice_h
#define IDevice_h

#include <string>
#include <vector>
#include <atomic>

class DeviceInfo
{
private:
    std::string m_udid;
    std::string m_name;
    bool m_usb;
	bool m_locked;
    bool m_trustPending;
	bool m_trusted;
    std::string m_wechatPath;
    std::string m_wechatUuid;

public:
    DeviceInfo()
    {
    }
    
    ~DeviceInfo()
    {
        
    }
    
    std::string getUdid() const
    {
        return m_udid;
    }
    
    void setUdid(const std::string& udid)
    {
        m_udid = udid;
    }
    
    std::string getName() const
    {
        return m_name;
    }
    
    void setName(const std::string& name)
    {
        m_name = name;
    }
    
    bool isUsb() const
    {
        return m_usb;
    }
    
    void setUsb(bool usb)
    {
        m_usb = usb;
    }
    
    void setLocked(bool locked)
    {
        m_locked = locked;
    }

	bool needUnlock() const
	{
		return m_locked;
	}
    
    
    void setTrustPending(bool trustPending)
    {
        m_trustPending = trustPending;
    }
    
    bool isTrustPending() const
    {
        return m_trustPending;
    }
    
    void setTrusted(bool trusted)
    {
        m_trusted = trusted;
    }

	bool needTrust() const
	{
		return !m_trusted;
	}
    
    std::string getWechatPath() const
    {
        return m_wechatPath;
    }
    
    void setWechatPath(const std::string& wechatPath)
    {
        m_wechatPath = wechatPath;
        parseWechatUuid();
    }
    
    std::string getWechatUuid() const
    {
        return m_wechatUuid;
    }
    
private:
    
    void parseWechatUuid()
    {
        m_wechatUuid.clear();
        
        // /private/var/mobile/Containers/Data/Application/5A875B93-B816-459E-B6BA-C4A3D4162B6F
        const std::string startTag = "/Containers/Data/Application/";
        std::string::size_type pos = m_wechatPath.find(startTag);
        if (pos != std::string::npos)
        {
            std::string::size_type pos2 = m_wechatPath.find("/", pos + startTag.size());
            if (pos2 != std::string::npos)
            {
                m_wechatUuid = m_wechatPath.substr(pos + startTag.size(), pos2 - (pos + startTag.size()));
            }
            else
            {
                m_wechatUuid = m_wechatPath.substr(pos + startTag.size());
            }
        }
    }
    
};

#define BUNDLEID_WECHAT "com.tencent.xin"
#define CLIENT_ID "wxexp"

#define LOCK_ATTEMPTS 50
#define LOCK_WAIT 200000

// IDeviceBackupClient will refer libmobiledevice
class IDeviceBackupClient;

class IDeviceBackup
{
public:

    static bool queryDevices(std::vector<DeviceInfo>& devices);
    
    static bool queryDeviceName(const std::string& udid, std::string& name);
    static bool queryWechatPath(const std::string& udid, std::string& wechatPath);
    
public:
    IDeviceBackup(const DeviceInfo& deviceInfo, const std::string& outputPath);
    bool backup();
    // bool backup1();
    
    
    bool forceFullBackup() const;
    void cancel();
    bool isCanclled() const;
    
    bool filter(const std::string& srcPath, const std::string& destPath) const;
    
    double getOverallProgress() const;
    void setOverallProgress(double progress);
    
    int getErrCode() const;
    const std::string getErrMsg() const;
    void setError(int errCode, const std::string& errMsg);
private:
    DeviceInfo  m_deviceInfo;
    std::string m_outputPath;
    
    unsigned int m_quitFlag;
    
    int m_errCode;
    std::string m_errMsg;
    
    std::atomic<double> m_overallProgress;
};

#endif /* IDevice_h */
