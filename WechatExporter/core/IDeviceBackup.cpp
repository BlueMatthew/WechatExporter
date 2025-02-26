//
//  IDevice.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/12/7.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "IDeviceBackup.h"
#include <algorithm>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#if defined(_WIN32)
#include <windows.h>
#include <atlstr.h>
#include <shlobj_core.h>
#else
#include <libgen.h>
#include <unistd.h>
#endif


#include <plist/plist.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/notification_proxy.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/mobilebackup2.h>
#include <libimobiledevice/sbservices.h>
#include <libimobiledevice/diagnostics_relay.h>


#include "endianness.h"

#include "Utils.h"
#include "ITunesParser.h"
#include "FileSystem.h"

#ifdef _WIN32


int __cdecl printlog(const char *format, ...)
{
    char str[1024];

    va_list argptr;
    va_start(argptr, format);
    int ret = vsnprintf(str, sizeof(str), format, argptr);
    va_end(argptr);


    OutputDebugString((LPCTSTR)CW2T(CA2W(str, CP_UTF8)));

    return ret;
}

#else

#ifndef NDEBUG
#define printlog printf
#else
#define printlog(format, ...) (void)0
#endif
#endif

#define DEVICE_VERSION(maj, min, patch) (((maj & 0xFF) << 16) | ((min & 0xFF) << 8) | (patch & 0xFF))
#define PRINT_VERBOSE(min_level, ...) { printlog(__VA_ARGS__); }

#define CODE_SUCCESS 0x00
#define CODE_ERROR_LOCAL 0x06
#define CODE_ERROR_REMOTE 0x0b
#define CODE_FILE_DATA 0x0c

#define MAC_EPOCH 978307200


#ifdef _WIN32
void usleep(__int64 usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#endif

enum plist_format_t
{
    PLIST_FORMAT_XML,
    PLIST_FORMAT_BINARY
};

std::string formatDiskSize(uint64_t size)
{
    char buf[80];
    double sz;
    if (size >= 1000000000000LL) {
        sz = ((double)size / 1000000000000.0f);
        sprintf(buf, "%0.1f TB", sz);
    } else if (size >= 1000000000LL) {
        sz = ((double)size / 1000000000.0f);
        sprintf(buf, "%0.1f GB", sz);
    } else if (size >= 1000000LL) {
        sz = ((double)size / 1000000.0f);
        sprintf(buf, "%0.1f MB", sz);
    } else if (size >= 1000LL) {
        sz = ((double)size / 1000.0f);
        sprintf(buf, "%0.1f KB", sz);
    } else {
        sprintf(buf, "%d Bytes", (int)size);
    }
    return std::string(buf);
}

int writePlistFile(plist_t plist, const std::string& filename, enum plist_format_t format)
{
    char *buffer = NULL;
    uint32_t length;

    if (!plist || filename.empty())
        return 0;

    if (format == PLIST_FORMAT_XML)
        plist_to_xml(plist, &buffer, &length);
    else if (format == PLIST_FORMAT_BINARY)
        plist_to_bin(plist, &buffer, &length);
    else
        return 0;

    bool res = writeFile(filename, (const unsigned char*)buffer, length);

	plist_mem_free(buffer);

    return res ? 1 : 0;
}

int readPlistFile(plist_t *plist, const std::string& filename)
{
    if (filename.empty())
        return 0;

    std::vector<unsigned char> buffer;
    if (!readFile(filename, buffer)) {
        return 0;
    }

    plist_from_memory((const char *)&buffer[0], (uint32_t)buffer.size(), plist);

    return 1;
}

namespace fs
{
#ifdef _WIN32
static int win32err_to_errno(int err_value)
{
    switch (err_value) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
            return ENOENT;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            return EEXIST;
        case ERROR_HANDLE_DISK_FULL:
        case ERROR_DISK_FULL:
            return ENOSPC;
        default:
            return EFAULT;
    }
}

static int win32err_to_device_error(DWORD errValue)
{
    switch (errValue)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
        return -6;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return -7;
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:
        return -15;
    default:
        return -1;
    }
}
#endif

static int convertErrnoToDeviceError(int errnoValue)
{
    switch (errnoValue)
    {
    case ENOENT:    /* No such file or directory */
        return -6;
    case EEXIST:    /* File exists */
        return -7;
    case ENOTDIR:   /* Not a directory */
        return -8;
    case EISDIR:    /* Is a directory */
        return -9;
    case ELOOP:     /* Too many levels of symbolic links */
        return -10;
    case EIO:       /* Input/output error */
        return -11;
    case ENOSPC:    /* No space left on device */
        return -15;
    default:
        return -1;
    }
}

// return errno
static int deleteFile(const std::string& path)
{
    int e = 0;
#ifdef _WIN32
    CW2T szPath(CA2W(path.c_str(), CP_UTF8));
    if (!DeleteFile((LPCTSTR)szPath))
    {
        e = win32err_to_errno(GetLastError());
    }
#else
    if (unlink(path.c_str()) < 0)
    {
        e = errno;
    }
#endif
    return e;
}

// return errno
#ifdef _WIN32
static int deleteDirectoryRecursively(LPCTSTR szPath)
{
#ifndef NDEBUG
	assert((_tcslen(szPath) + 4) < MAX_PATH);
#endif
    TCHAR szPath1[MAX_PATH] = { 0 };
    _tcscpy(szPath1, szPath);
	PathAddBackslash(szPath1);
	PathAppend(szPath1, TEXT("*.*"));

    
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    hFind = ::FindFirstFile((LPCTSTR)szPath1, &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return win32err_to_errno(GetLastError());
    }
    
    int res = 0;
    do
    {
        if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
        {
            continue;
        }
        
#ifndef NDEBUG
		assert((_tcslen(szPath) + 1 + _tcslen(FindFileData.cFileName)) < MAX_PATH);
#endif
        _tcscpy(szPath1, szPath);
		PathAddBackslash(szPath1);
		PathAppend(szPath1, FindFileData.cFileName);
        
        if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            res = deleteDirectoryRecursively(szPath1);
            if (res != 0)
            {
                break;
            }
        }
        else
        {
            if (!DeleteFile(szPath1))
            {
                res = win32err_to_errno(GetLastError());
                break;
            }
        }
    } while (::FindNextFile(hFind, &FindFileData));
    ::FindClose(hFind);
    
    if (res == 0)
    {
        if (!RemoveDirectory(szPath))
        {
            res = win32err_to_errno(GetLastError());
        }
    }
    
    return res;
}
#endif
static int deleteDirectoryRecursively(const std::string& path)
{
#ifdef _WIN32
    std::string p = path;
    std::replace(p.begin(), p.end(), ALT_DIR_SEP, DIR_SEP);
    CW2T pszPath(CA2W(p.c_str(), CP_UTF8));
    return deleteDirectoryRecursively(pszPath);
#else
    int res = 0;
    
    DIR *d = opendir(path.c_str());
    if (NULL == d)
    {
        res = errno;
        return res;
    }
    
    size_t path_len = path.size();
    
    struct dirent *p;

    res = 0;
    while (!res && (p = readdir(d)))
    {
        char *buf = NULL;
        size_t len = 0;

        /* Skip the names "." and ".." as we don't want to recurse on them. */
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            continue;

        len = path_len + strlen(p->d_name) + 2;
        buf = (char *)malloc(len);

        if (NULL == buf)
        {
            res = errno;
            break;
        }
        struct stat statbuf;

        strcpy(buf, path.c_str());
        strcat(buf, "/");
        strcat(buf, p->d_name);
        if (stat(buf, &statbuf) != 0)
        {
            res = errno;
            free(buf);
            break;
        }
        
        if (S_ISDIR(statbuf.st_mode))
            res = deleteDirectoryRecursively(buf);
        else
        {
            if (unlink(buf) != 0)
            {
                res = errno;
            }
        }
        free(buf);
        
        if (res != 0)
        {
            break;
        }
    }
    closedir(d);

    if (!res)
    {
        if (rmdir(path.c_str()) != 0)
        {
            res = errno;
        }
    }

    return res;
#endif
}

// return errno
static int moveFile(const std::string& src, const std::string& dest)
{
    int e = 0;
#ifdef _WIN32
    CW2T szSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T szDest(CA2W(dest.c_str(), CP_UTF8));
    if (!MoveFile((LPCTSTR)szSrc, (LPCTSTR)szDest))
    {
        e = win32err_to_errno(GetLastError());
    }
#else
    if (rename(src.c_str(), dest.c_str()) < 0)
    {
        e = errno;
    }
#endif
    return e;
}

static int makeDirectory(const std::string& path, mode_t mode)
{
#ifdef _WIN32
    CW2T pszPath(CA2W(path.c_str(), CP_UTF8));

    if (PathFileExists(pszPath))
    {
        return 0;
    }
    
    DWORD err = ::SHCreateDirectoryEx(NULL, (LPCTSTR)pszPath, NULL);
    return (err == ERROR_SUCCESS) ? 0 : win32err_to_errno(err);
#else
    if (path.empty()) return -1;
    
    if (mkdir(path.c_str(), mode) == 0 || errno == EEXIST)
    {
        return 0;
    }
    
    std::vector<std::string::value_type> copypath;
    copypath.reserve(path.size() + 1);
    std::copy(path.begin(), path.end(), std::back_inserter(copypath));
    copypath.push_back('\0');
    
    std::vector<std::string::value_type>::iterator itStart = copypath.begin();
    std::vector<std::string::value_type>::iterator it;
    
    int status = 0;
    if (*itStart == '/') ++itStart;  // Skip root path
    while (status == 0 && (it = std::find(itStart, copypath.end(), '/')) != copypath.end())
    {
        // Neither root nor double slash in path
        *it = '\0';
        
        if (mkdir(&copypath[0], mode) != 0)
        {
            status = (errno == EEXIST) ? 0 : errno;
        }
        *it = '/';
        itStart = it + 1;
    }
    if (status == 0)
    {
        if (mkdir(&copypath[0], mode) != 0)
        {
            status = (errno == EEXIST) ? 0 : errno;
        }
    }
    
    return status;
#endif
}

class FileEnumerator
{
public:
	class FileInfo
	{
	public:

		friend FileEnumerator;

		FileInfo() : m_isDir(false), m_isNormalFile(false), m_fileSize(0)
		{

		}

		const std::string& getFileName() const
		{
			return m_fileName;
		}

		bool isDirectory() const
		{
			return m_isDir;
		}

		bool isNormalFile() const
		{
			return m_isNormalFile;
		}

		time_t getModifiedTime() const
		{
			return m_modifiedTime;
		}

		uint64_t getFileSize() const
		{
			return m_fileSize;
		}

	private:
		std::string m_fileName;
		time_t m_modifiedTime;
		bool m_isDir;
		bool m_isNormalFile;
		uint64_t m_fileSize;
	};

	FileEnumerator(const std::string& path) : m_path(path),
#ifdef _WIN32
		m_handle(INVALID_HANDLE_VALUE), 
#else
		m_handle(NULL), 
#endif
		m_first(true)
	{
#ifdef _WIN32
		_tcscpy(m_szPath, CW2T(CA2W(path.c_str(), CP_UTF8)));
#endif
	}
	~FileEnumerator()
	{
#ifdef _WIN32
		if (m_handle != INVALID_HANDLE_VALUE)
		{
			::FindClose(m_handle);
		}
#else
		if (NULL != m_handle)
		{
			closedir(m_handle);
		}
#endif
	}

	bool isValid() const
	{
#ifdef _WIN32
		return INVALID_HANDLE_VALUE != m_handle;
#else
		return NULL != m_handle;
#endif
	}

	bool nextFile(FileInfo& file)
	{
#ifdef _WIN32
		bool res = false;
		WIN32_FIND_DATA FindFileData;

		if (m_first)
		{
			TCHAR findPath[MAX_PATH] = { 0 };
			_tcscpy(findPath, m_szPath);
			PathAppend(findPath, TEXT("*.*"));
			m_first = false;

			m_handle = ::FindFirstFile(findPath, &FindFileData);
			if (m_handle == INVALID_HANDLE_VALUE)
			{
				return res;
			}
			do
			{
				if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
				{
					continue;
				}

				res = true;
				break;
			} while (::FindNextFile(m_handle, &FindFileData));
		}
		else
		{
			if (INVALID_HANDLE_VALUE == m_handle)
			{
				return res;
			}

			while (::FindNextFile(m_handle, &FindFileData))
			{
				if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
				{
					continue;
				}

				res = true;
				break;
			}
		}

		if (res)
		{
			file.m_fileName = (LPCSTR)CW2A(CT2W(FindFileData.cFileName), CP_UTF8);
			file.m_isDir = ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
			file.m_isNormalFile = !file.m_isDir && ((FindFileData.dwFileAttributes & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) != 0);
			if (file.m_isDir)
			{
				TCHAR szPath[MAX_PATH] = { 0 };
				_tcscpy(szPath, m_szPath);
				PathAppend(szPath, FindFileData.cFileName);
				struct _stati64 st;
				_tstat64(szPath, &st);
				file.m_fileSize = st.st_size;
				file.m_modifiedTime = st.st_mtime;
			}
			else
			{
				file.m_fileSize = (FindFileData.nFileSizeHigh * (MAXDWORD + 1)) + FindFileData.nFileSizeLow;
				file.m_modifiedTime = ::FileTimeToTime(FindFileData.ftLastWriteTime);
			}
		}

		return res;
#else
		bool res = false;
		if (m_first)
		{
			m_first = false;
            m_handle = opendir(m_path.c_str());
			if (NULL == m_handle)
			{
				return res;
			}
		}

		struct dirent *entry;
		while ((entry = readdir(m_handle)) != NULL)
		{
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			{
				continue;
			}

			file.m_fileName = entry->d_name;
			std::string fullPath = combinePath(m_path, file.m_fileName);

			struct stat st;
			stat(fullPath.c_str(), &st);

			file.m_fileSize = st.st_size;
			file.m_isDir = S_ISDIR(st.st_mode);
			file.m_isNormalFile = S_ISREG(st.st_mode);
			file.m_modifiedTime = st.st_mtime;

			res = true;
			break;
		}

		return res;
#endif
	}

private:
	std::string m_path;
#ifdef _WIN32
	TCHAR m_szPath[MAX_PATH];
	HANDLE m_handle;
#else
	DIR* m_handle;
#endif
	bool m_first;
};

} // namespace fs

class IDeviceBackupClient
{
public:
    
    struct File
    {
        std::string fileId;
        size_t size;
        
        File() : size(0)
        {
        }
        
        File(const std::string& fid, size_t sz) : fileId(fid), size(sz)
        {
        }
    };
    
	/*
    struct __string_less
    {
        // _LIBCPP_INLINE_VISIBILITY _LIBCPP_CONSTEXPR_AFTER_CXX11
        bool operator()(const std::string& __x, const std::string& __y) const {return __x < __y;}
        bool operator()(const std::pair<std::string, std::string>& __x, const std::string& __y) const {return __x.first < __y;}
        bool operator()(const File& __x, const std::string& __y) const {return __x.fileId < __y;}
        bool operator()(const File& __x, const File& __y) const {return __x.fileId < __y.fileId;}
    };
	*/

	IDeviceBackupClient(const std::string udid) : m_device(NULL), m_afc(NULL), m_lockfile(0), m_np(NULL), m_client(NULL), m_mobilebackup2(NULL), m_udid(udid)
    {
    }
    
    ~IDeviceBackupClient()
    {
        unlockAfc();
        
        if (NULL != m_mobilebackup2)
        {
            mobilebackup2_client_free(m_mobilebackup2);
        }
        
        if (NULL != m_afc)
        {
            afc_client_free(m_afc);
        }
        if (m_np)
        {
            np_client_free(m_np);
        }
        if (NULL != m_client)
        {
            lockdownd_client_free(m_client);
        }
        if (NULL != m_device)
        {
            idevice_free(m_device);
        }
    }
    
    bool init()
    {
        lockdownd_error_t err = LOCKDOWN_E_UNKNOWN_ERROR;
        idevice_error_t devErr = idevice_new_with_options(&m_device, m_udid.c_str(), (idevice_options)(IDEVICE_LOOKUP_USBMUX | IDEVICE_LOOKUP_NETWORK));
        if (devErr == IDEVICE_E_SUCCESS && NULL != m_device)
        {
            err = lockdownd_client_new(m_device, &m_client, CLIENT_ID);
        }
        
        return LOCKDOWN_E_SUCCESS == err;
    }
    
    lockdownd_error_t initWithHandShake()
    {
        lockdownd_error_t err = LOCKDOWN_E_UNKNOWN_ERROR;
        idevice_error_t devErr = idevice_new_with_options(&m_device, m_udid.c_str(), (idevice_options)(IDEVICE_LOOKUP_USBMUX | IDEVICE_LOOKUP_NETWORK));
        if (devErr == IDEVICE_E_SUCCESS && NULL != m_device)
        {
			err = lockdownd_client_new_with_handshake(m_device, &m_client, CLIENT_ID);
        }
        
        return err;
    }
    
    void updateDeviceInfo(DeviceInfo& device, lockdownd_error_t err)
    {
        if (LOCKDOWN_E_SUCCESS == err)
        {
            device.setLocked(false);
            device.setTrustPending(false);
            device.setTrusted(true);
        }
        else
        {
            if (LOCKDOWN_E_PASSWORD_PROTECTED == err)
            {
                device.setLocked(true);
            }
            if (LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING == err)
            {
                device.setTrustPending(true);
            }
            if (LOCKDOWN_E_PAIRING_FAILED == err || LOCKDOWN_E_PAIRING_PROHIBITED_OVER_THIS_CONNECTION)
            {
                device.setTrusted(false);
            }
        }
    }

    void freeClient()
    {
        if (NULL != m_client)
        {
            lockdownd_client_free(m_client);
            m_client = NULL;
        }
    }
    
    inline bool needUnlock(lockdownd_error_t err) const
    {
        return (LOCKDOWN_E_PASSWORD_PROTECTED == err);
    }
    
    inline bool needTrust(lockdownd_error_t err) const
    {
        return (LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING == err);
    }
    
    bool loadFiles(const std::string& backupPath, const std::string& udid)
    {
        
        m_files.clear();
        
        ITunesDb iTunesDb(combinePath(backupPath, udid), "");
        
        std::unique_ptr<ITunesDb::ITunesFileEnumerator> enumerator(iTunesDb.buildEnumerator(std::vector<std::string>(), true));
        if (enumerator->isInvalid())
        {
            return false;
        }

#ifndef NDEBUG
        uint32_t zeroFile = 0;
        uint32_t nonzeroFile = 0;
#endif
        
        ITunesFile file;
        // m_files.reserve(2048);
        while (enumerator->nextFile(file))
        {
            if (file.domain == "AppDomain-com.tencent.xin" || file.domain == "AppDomainGroup-group.com.tencent.xin")
            {
                continue;
            }
            if (!file.blobParsed)
            {
                ITunesDb::parseFileInfo(&file);
            }
#ifndef NDEBUG
            if (file.size > 0)
            {
                nonzeroFile ++;
            }
            else
            {
                zeroFile ++;
            }
#endif
            if (iTunesDb.isMbdb())
            {
                m_files.insert(m_files.end(), std::pair<std::string, size_t>(file.fileId, file.size));
            }
            else
            {
                m_files.insert(m_files.end(), std::pair<std::string, size_t>(file.fileId.substr(0, 2) + "/" + file.fileId, file.size));
            }
        }
        
#ifndef NDEBUG
        printlog("Manifest.db: zero: %u, nonzero: %u\r\n", zeroFile, nonzeroFile);
#endif

        return true;
    }
    
    bool queryFileSize(const std::string& fileId, size_t& fileSize) const
    {
        std::map<std::string, size_t>::const_iterator it = m_files.find(fileId);
        if (it == m_files.cend())
        {
            return false;
        }
        fileSize = it->second;
        return true;
    }
    
    bool queryDeviceName(std::string& name) const
    {
        if (NULL != m_client)
        {
            char *deviceName = NULL;
            if ((LOCKDOWN_E_SUCCESS == lockdownd_get_device_name(m_client, &deviceName)) && NULL != deviceName)
            {
                name = deviceName;
                plist_mem_free(deviceName);
                return true;
            }
        }
        
        return false;
    }
    
    bool queryAppContainer(const std::string& bundleId, std::string& container)
    {
        if (NULL == m_device || NULL == m_client)
        {
            return false;
        }
    
        lockdownd_service_descriptor_t service = NULL;
        lockdownd_error_t lderr = LOCKDOWN_E_SUCCESS;
        if (((lderr = lockdownd_start_service(m_client, "com.apple.mobile.installation_proxy", &service)) != LOCKDOWN_E_SUCCESS) || NULL == service)
        {
            return false;
        }
        
        instproxy_client_t ipc = NULL;
        instproxy_error_t err = instproxy_client_new(m_device, service, &ipc);

        if (service)
        {
            lockdownd_service_descriptor_free(service);
        }
        service = NULL;

        bool res = false;
        if (err == INSTPROXY_E_SUCCESS)
        {
            char *value = NULL;
            err = getContainderForBundleIdentifier(ipc, bundleId.c_str(), &value);
            if (err == INSTPROXY_E_SUCCESS && NULL != value)
            {
                container = value;
                res = true;
            }
            if (NULL != value)
            {
                free(value);
            }
            
            instproxy_client_free(ipc);
        }
        
        return res;;
    }
    
    void setOverallProgressFromMessage(IDeviceBackup *pThis, plist_t message, char* identifier)
    {
        plist_t node = NULL;
        double progress = 0.0;

        if (!strcmp(identifier, "DLMessageDownloadFiles")) {
            node = plist_array_get_item(message, 3);
        } else if (!strcmp(identifier, "DLMessageUploadFiles")) {
            node = plist_array_get_item(message, 2);
        } else if (!strcmp(identifier, "DLMessageMoveFiles") || !strcmp(identifier, "DLMessageMoveItems")) {
            node = plist_array_get_item(message, 3);
        } else if (!strcmp(identifier, "DLMessageRemoveFiles") || !strcmp(identifier, "DLMessageRemoveItems")) {
            node = plist_array_get_item(message, 3);
        }

        if (node != NULL) {
            plist_get_real_val(node, &progress);
            pThis->setOverallProgress(progress);
        }
    }
    
    void doPostNotification(idevice_t device, const char *notification)
    {
        lockdownd_service_descriptor_t service = NULL;
        np_client_t np;

        lockdownd_client_t lockdown = NULL;

        if (lockdownd_client_new_with_handshake(device, &lockdown, CLIENT_ID) != LOCKDOWN_E_SUCCESS) {
            return;
        }

        lockdownd_error_t ldret = lockdownd_start_service(lockdown, NP_SERVICE_NAME, &service);
        if (service && service->port) {
            np_client_new(device, service, &np);
            if (np) {
                np_post_notification(np, notification);
                np_client_free(np);
            }
        } else {
            printlog("ERROR: Could not start service %s: %s\n", NP_SERVICE_NAME, lockdownd_strerror(ldret));
        }

        if (service) {
            lockdownd_service_descriptor_free(service);
            service = NULL;
        }
        lockdownd_client_free(lockdown);
    }

    static void notifyCallback(const char *notification, void *userdata)
    {
        if (strlen(notification) == 0) {
            return;
        }
        if (!strcmp(notification, NP_SYNC_CANCEL_REQUEST)) {
            PRINT_VERBOSE(1, "User has cancelled the backup process on the device.\n");
            IDeviceBackup *pThis = reinterpret_cast<IDeviceBackup *>(userdata);
            pThis->cancel();
        } else if (!strcmp(notification, NP_BACKUP_DOMAIN_CHANGED)) {
            /// backup_domain_changed = 1;
        } else {
            PRINT_VERBOSE(1, "Unhandled notification '%s' (TODO: implement)\n", notification);
        }
    }
    
    static int checkSnapshotState(const std::string& path, const std::string& udid, const char *matches)
    {
        int ret = 0;
        plist_t status_plist = NULL;
        std::string file_path = combinePath(path, udid, "Status.plist");

        readPlistFile(&status_plist, file_path);
        
        if (!status_plist) {
            printlog("Could not read Status.plist!\n");
            return ret;
        }
        plist_t node = plist_dict_get_item(status_plist, "SnapshotState");
        if (node && PLIST_IS_STRING(node)) {
            const char* sval = plist_get_string_ptr(node, NULL);
            if (sval) {
                ret = (strcmp(sval, matches) == 0) ? 1 : 0;
            }
        } else {
            printlog("%s: ERROR could not get SnapshotState key from Status.plist!\n", __func__);
        }
        plist_free(status_plist);
        return ret;
    }

    instproxy_error_t getContainderForBundleIdentifier(instproxy_client_t client, const char* appid, char** container)
    {
        if (!client || !appid)
            return INSTPROXY_E_INVALID_ARG;

        plist_t apps = NULL;

        // create client options for any application types
        plist_t client_opts = instproxy_client_options_new();
        instproxy_client_options_add(client_opts, "ApplicationType", "Any", NULL);

        // only return attributes we need
        instproxy_client_options_set_return_attributes(client_opts, "CFBundleIdentifier", "CFBundleExecutable", "Path", "Container", "EnvironmentVariables", NULL);

        // only query for specific appid
        const char* appids[] = {appid, NULL};

        // query device for list of apps
        instproxy_error_t ierr = instproxy_lookup(client, appids, client_opts, &apps);

        instproxy_client_options_free(client_opts);

        if (ierr != INSTPROXY_E_SUCCESS) {
            return ierr;
        }
        
        plist_t app_found = plist_access_path(apps, 1, appid);
        if (!app_found) {
            if (apps)
                plist_free(apps);
            *container = NULL;
            return INSTPROXY_E_OP_FAILED;
        }

        const char* container_str = NULL;
        uint64_t strLength = 0;
        plist_t container_p = plist_dict_get_item(app_found, "Container");
        if (container_p) {
            container_str = plist_get_string_ptr(container_p, &strLength);
        }

        if (!container_str) {
            plist_free(apps);
            return INSTPROXY_E_OP_FAILED;
        }

        char* ret = (char*)malloc(strLength + 1);
        strcpy(ret, container_str);
        
        *container = ret;

        plist_free(apps);
        return INSTPROXY_E_SUCCESS;
    }
    
    void handleListDirectory(plist_t message, const char *backupDir)
    {
        if (!message || (!PLIST_IS_ARRAY(message)) || plist_array_get_size(message) < 2 || !backupDir) return;

        plist_t node = plist_array_get_item(message, 1);
        char *str = NULL;
        if (plist_get_node_type(node) == PLIST_STRING)
        {
            plist_get_string_val(node, &str);
        }
        if (!str)
        {
            printlog("ERROR: Malformed DLContentsOfDirectory message\n");
            // TODO error handling
            return;
        }

        std::string relativePath = str;
        if (relativePath.size() > (m_udid.size() + 1) && startsWith(relativePath, m_udid))
        {
            relativePath = relativePath.substr(m_udid.size() + 1);
        }
        else
        {
            relativePath.clear();
        }
        std::string path = combinePath(backupDir, str);
        plist_mem_free(str);

        plist_t dirlist = plist_new_dict();

        fs::FileEnumerator fileEnumerator(path);
        fs::FileEnumerator::FileInfo fi;
        while (fileEnumerator.nextFile(fi))
        {
            plist_t fdict = plist_new_dict();

            const char *ftype = "DLFileTypeUnknown";
            if (fi.isDirectory()) {
                ftype = "DLFileTypeDirectory";
            } else if (fi.isNormalFile()) {
                ftype = "DLFileTypeRegular";
            }
            
            size_t fileSize = fi.getFileSize();
            if (!fi.isDirectory() && (fileSize == 0) && fi.isNormalFile())
            {
                bool found = queryFileSize(relativePath + "/" + fi.getFileName(), fileSize);
#ifndef NDEBUG
                if (found && fileSize > 0)
                {
                    static int zeroCnt = 0;
                    zeroCnt ++;

                    // printlog("DBG::Found zero file: %u\r\n", zeroCnt);
                }
#endif
            }
            plist_dict_set_item(fdict, "DLFileType", plist_new_string(ftype));
            plist_dict_set_item(fdict, "DLFileSize", plist_new_uint(fileSize));
            plist_dict_set_item(fdict, "DLFileModificationDate",
                        plist_new_date((uint32_t)((uint64_t)fi.getModifiedTime() - MAC_EPOCH), 0));

            plist_dict_set_item(dirlist, fi.getFileName().c_str(), fdict);
        }

        /* TODO error handling */
        mobilebackup2_error_t err = mobilebackup2_send_status_response(m_mobilebackup2, 0, NULL, dirlist);
        plist_free(dirlist);
        if (err != MOBILEBACKUP2_E_SUCCESS) {
            printlog("Could not send status response, error %d\n", err);
        }
    }
    
    void handleMakeDirectory(plist_t message, const char *backup_dir)
    {
        if (!message || (!PLIST_IS_ARRAY(message)) || plist_array_get_size(message) < 2 || !backup_dir) return;

        plist_t dir = plist_array_get_item(message, 1);
        char *str = NULL;
        int errcode = 0;
        char *errdesc = NULL;
        plist_get_string_val(dir, &str);

        std::string newpath = combinePath(backup_dir, str);
        plist_mem_free(str);

        if ((errcode = fs::makeDirectory(newpath, 0755)) != 0) {
            errdesc = strerror(errcode);
            if (errno != EEXIST) {
                printlog("mkdir: %s (%d)\n", errdesc, errcode);
            }
            errcode = fs::convertErrnoToDeviceError(errcode);
        }
        mobilebackup2_error_t err = mobilebackup2_send_status_response(m_mobilebackup2, errcode, errdesc, NULL);
        if (err != MOBILEBACKUP2_E_SUCCESS) {
            printlog("Could not send status response, error %d\n", err);
        }
    }

    int receiveFilename(IDeviceBackup *pThis, char** filename)
    {
        uint32_t nlen = 0;
        uint32_t rlen = 0;

        do {
            nlen = 0;
            rlen = 0;
            mobilebackup2_receive_raw(m_mobilebackup2, (char*)&nlen, 4, &rlen);
            nlen = be32toh(nlen);

            if ((nlen == 0) && (rlen == 4)) {
                // a zero length means no more files to receive
                return 0;
            } else if(rlen == 0) {
                // device needs more time, waiting...
                continue;
            } else if (nlen > 4096) {
                // filename length is too large
                printlog("ERROR: %s: too large filename length (%d)!\n", __func__, nlen);
                return 0;
            }

            if (*filename != NULL) {
                free(*filename);
                *filename = NULL;
            }

            *filename = (char*)malloc(nlen+1);

            rlen = 0;
            mobilebackup2_receive_raw(m_mobilebackup2, *filename, nlen, &rlen);
            if (rlen != nlen) {
                printlog("ERROR: %s: could not read filename\n", __func__);
                return 0;
            }

            char* p = *filename;
            p[rlen] = 0;

            break;
        } while(1 && !pThis->isCanclled());

        return nlen;
    }

    int handleSendFile(const char *backup_dir, const char *path, plist_t *errplist)
    {
        uint32_t nlen = 0;
        uint32_t pathlen = (uint32_t)strlen(path);
        uint32_t bytes = 0;
#ifdef _WIN32
        std::string localfile = combinePath(backup_dir, normalizePath(path));
        CA2W szLocalFile(localfile.c_str(), CP_UTF8);
        struct _stati64 fst;
#else
        std::string localfile = combinePath(backup_dir, path);
        struct stat fst;
#endif

        char buf[32768];

        FILE *f = NULL;
        uint32_t slen = 0;
        int errcode = -1;
        int result = -1;
        uint32_t length;
#ifdef _WIN32
        uint64_t total;
        uint64_t sent;
#else
        off_t total;
        off_t sent;
#endif

        mobilebackup2_error_t err;

        /* send path length */
        nlen = htobe32(pathlen);
        err = mobilebackup2_send_raw(m_mobilebackup2, (const char*)&nlen, sizeof(nlen), &bytes);
        if (err != MOBILEBACKUP2_E_SUCCESS) {
            goto leave_proto_err;
        }
        if (bytes != (uint32_t)sizeof(nlen)) {
            err = MOBILEBACKUP2_E_MUX_ERROR;
            goto leave_proto_err;
        }

        /* send path */
        err = mobilebackup2_send_raw(m_mobilebackup2, path, pathlen, &bytes);
        if (err != MOBILEBACKUP2_E_SUCCESS) {
            goto leave_proto_err;
        }
        if (bytes != pathlen) {
            err = MOBILEBACKUP2_E_MUX_ERROR;
            goto leave_proto_err;
        }

#ifdef _WIN32
        if (_wstati64((LPCWSTR)szLocalFile, &fst) < 0)
#else
        if (stat(localfile.c_str(), &fst) < 0)
#endif
        {
            if (errno != ENOENT)
                printlog("%s: stat failed on '%s': %d\n", __func__, localfile.c_str(), errno);
            errcode = errno;
            goto leave;
        }

        total = fst.st_size;

        PRINT_VERBOSE(1, "Sending '%s' (%s)\n", path, formatDiskSize(total).c_str());
        
        if (total == 0) {
            errcode = 0;
            goto leave;
        }

#ifdef _WIN32
        f = _wfopen((LPCWSTR)szLocalFile, L"rb");
#else
        f = fopen(localfile.c_str(), "rb");
#endif
        if (!f) {
            printlog("%s: Error opening local file '%s': %d\n", __func__, localfile.c_str(), errno);
            errcode = errno;
            goto leave;
        }

        sent = 0;
        do {
            length = ((total-sent) < (long long)sizeof(buf)) ? (uint32_t)total-sent : (uint32_t)sizeof(buf);
            /* send data size (file size + 1) */
            nlen = htobe32(length+1);
            memcpy(buf, &nlen, sizeof(nlen));
            buf[4] = CODE_FILE_DATA;
            err = mobilebackup2_send_raw(m_mobilebackup2, (const char*)buf, 5, &bytes);
            if (err != MOBILEBACKUP2_E_SUCCESS) {
                goto leave_proto_err;
            }
            if (bytes != 5) {
                goto leave_proto_err;
            }

            /* send file contents */
            size_t r = fread(buf, 1, sizeof(buf), f);
            if (r <= 0) {
                printlog("%s: read error\n", __func__);
                errcode = errno;
                goto leave;
            }
            err = mobilebackup2_send_raw(m_mobilebackup2, buf, r, &bytes);
            if (err != MOBILEBACKUP2_E_SUCCESS) {
                goto leave_proto_err;
            }
            if (bytes != (uint32_t)r) {
                printlog("Error: sent only %d of %d bytes\n", bytes, (int)r);
                goto leave_proto_err;
            }
            sent += r;
        } while (sent < total);
        fclose(f);
        f = NULL;
        errcode = 0;

    leave:
        if (errcode == 0) {
            result = 0;
            nlen = 1;
            nlen = htobe32(nlen);
            memcpy(buf, &nlen, 4);
            buf[4] = CODE_SUCCESS;
            mobilebackup2_send_raw(m_mobilebackup2, buf, 5, &bytes);
        } else {
            if (!*errplist) {
                *errplist = plist_new_dict();
            }
            char *errdesc = strerror(errcode);
            mb2_multi_status_add_file_error(*errplist, path, fs::convertErrnoToDeviceError(errcode), errdesc);

            length = strlen(errdesc);
            nlen = htobe32(length+1);
            memcpy(buf, &nlen, 4);
            buf[4] = CODE_ERROR_LOCAL;
            slen = 5;
            memcpy(buf+slen, errdesc, length);
            slen += length;
            err = mobilebackup2_send_raw(m_mobilebackup2, (const char*)buf, slen, &bytes);
            if (err != MOBILEBACKUP2_E_SUCCESS) {
                printlog("could not send message\n");
            }
            if (bytes != slen) {
                printlog("could only send %d from %d\n", bytes, slen);
            }
        }

    leave_proto_err:
        if (f)
            fclose(f);
        return result;
    }

    void handleSendFiles(plist_t message, const char *backup_dir)
    {
        uint32_t cnt;
        uint32_t i = 0;
        uint32_t sent;
        plist_t errplist = NULL;

        if (!message || (plist_get_node_type(message) != PLIST_ARRAY) || (plist_array_get_size(message) < 2) || !backup_dir) return;

        plist_t files = plist_array_get_item(message, 1);
        cnt = plist_array_get_size(files);

        for (i = 0; i < cnt; i++) {
            plist_t val = plist_array_get_item(files, i);
            if (plist_get_node_type(val) != PLIST_STRING) {
                continue;
            }
            char *str = NULL;
            plist_get_string_val(val, &str);
            if (!str)
                continue;

            if (handleSendFile(backup_dir, str, &errplist) < 0) {
                plist_mem_free(str);
                printlog("Error when sending file '%s' to device\n", str);
                // TODO: perhaps we can continue, we've got a multi status response?!
                break;
            }
            plist_mem_free(str);
        }

        /* send terminating 0 dword */
        uint32_t zero = 0;
        mobilebackup2_send_raw(m_mobilebackup2, (char*)&zero, 4, &sent);

        if (!errplist) {
            plist_t emptydict = plist_new_dict();
            mobilebackup2_send_status_response(m_mobilebackup2, 0, NULL, emptydict);
            plist_free(emptydict);
        } else {
            mobilebackup2_send_status_response(m_mobilebackup2, -13, "Multi status", errplist);
            plist_free(errplist);
        }
    }

    int handleReceiveFiles(IDeviceBackup *pThis, plist_t message, const std::string& backupDir)
    {
        uint64_t backup_real_size = 0;
        uint64_t backup_total_size = 0;
        uint32_t blocksize = 0;
        uint32_t bdone;
        uint32_t rlen;
        uint32_t nlen = 0;
        uint32_t r;
        char buf[32768];
        char *fname = NULL;
        char *dname = NULL;
        std::string bname;
        char code = 0;
        char last_code = 0;
        plist_t node = NULL;
        FILE *f = NULL;
        unsigned int file_count = 0;
        int errcode = 0;
        char *errdesc = NULL;
        
        if (!message || (!PLIST_IS_ARRAY(message)) || plist_array_get_size(message) < 4 || backupDir.empty()) return 0;

        node = plist_array_get_item(message, 3);
        if (plist_get_node_type(node) == PLIST_UINT) {
            plist_get_uint_val(node, &backup_total_size);
        }
        if (backup_total_size > 0) {
            PRINT_VERBOSE(1, "Receiving files\n");
        }

        do {
            if (pThis->isCanclled())
                break;

            nlen = receiveFilename(pThis, &dname);
            if (nlen == 0) {
                break;
            }

            nlen = receiveFilename(pThis, &fname);
            if (!nlen) {
                break;
            }
            std::string destFileName = (fname != NULL) ? fname : "";

            bname = combinePath(backupDir, fname);

#ifndef NDEBUG
            static uint32_t filtered_cnt = 0;
#endif
            
            bool filtered = false;
            if (backup_total_size > 0)
            {
                filtered = pThis->filter(dname, fname);
#ifndef NDEBUG
                if (filtered) filtered_cnt++;
#endif
            }
            
            if (fname != NULL) {
                free(fname);
                fname = NULL;
            }
#ifndef NDEBUG
            if (!filtered)
            {
                // printlog("DBG::dname=%s\r\n", dname);
                // printlog("DBG::Write File: %s\r\n", destFileName.c_str());
            }
            // printlog("DBG::filtered_cnt = %u\r\n", filtered_cnt);
#endif
            
            r = 0;
            nlen = 0;
            mobilebackup2_receive_raw(m_mobilebackup2, (char*)&nlen, 4, &r);
            if (r != 4) {
                printlog("ERROR: %s: could not receive code length!\n", __func__);
                break;
            }
            nlen = be32toh(nlen);

            last_code = code;
            code = 0;

            mobilebackup2_receive_raw(m_mobilebackup2, &code, 1, &r);
            if (r != 1) {
                printlog("ERROR: %s: could not receive code!\n", __func__);
                break;
            }

            /* TODO remove this */
            if ((code != CODE_SUCCESS) && (code != CODE_FILE_DATA) && (code != CODE_ERROR_REMOTE)) {
                PRINT_VERBOSE(1, "Found new flag %02x\n", code);
            }

            fs::deleteFile(bname);
            
#ifdef _WIN32
            f = _wfopen((LPCWSTR)CA2W(bname.c_str(), CP_UTF8), L"wb");
#else
            f = fopen(bname.c_str(), "wb");
#endif
            while (f && (code == CODE_FILE_DATA)) {
                blocksize = nlen-1;
                bdone = 0;
                rlen = 0;
                while (bdone < blocksize) {
                    if ((blocksize - bdone) < sizeof(buf)) {
                        rlen = blocksize - bdone;
                    } else {
                        rlen = sizeof(buf);
                    }
                    mobilebackup2_receive_raw(m_mobilebackup2, buf, rlen, &r);
                    if ((int)r <= 0) {
                        break;
                    }
                    
                    if (!filtered)
                    {
                        fwrite(buf, 1, r, f);
                    }

                    bdone += r;
                }
                if (bdone == blocksize) {
                    backup_real_size += blocksize;
                }
                /*
                if (backup_total_size > 0) {
                    print_progress(backup_real_size, backup_total_size);
                }
                 */
                if (pThis->isCanclled())
                    break;
                nlen = 0;
                mobilebackup2_receive_raw(m_mobilebackup2, (char*)&nlen, 4, &r);
                nlen = be32toh(nlen);
                if (nlen > 0) {
                    last_code = code;
                    mobilebackup2_receive_raw(m_mobilebackup2, &code, 1, &r);
                } else {
                    break;
                }
            }
            if (f) {
                fclose(f);
                file_count++;
                if (filtered)
                {
                    std::map<std::string, size_t>::iterator it = m_files.find(destFileName);
                    if (it == m_files.end())
                    {
                        m_files.insert(it, std::pair<std::string, size_t>(destFileName, blocksize));
                    }
                    else
                    {
                        it->second = blocksize;
                    }
                }
            } else {
                errcode = fs::convertErrnoToDeviceError(errno);
                errdesc = strerror(errno);
                printlog("Error opening '%s' for writing: %s\n", bname.c_str(), errdesc);
                break;
            }
            if (nlen == 0) {
                break;
            }

            /* check if an error message was received */
            if (code == CODE_ERROR_REMOTE) {
                /* error message */
                char *msg = (char*)malloc(nlen);
                mobilebackup2_receive_raw(m_mobilebackup2, msg, nlen-1, &r);
                msg[r] = 0;
                /* If sent using CODE_FILE_DATA, end marker will be CODE_ERROR_REMOTE which is not an error! */
                if (last_code != CODE_FILE_DATA) {
                    printlog("\nReceived an error message from device: %s\n", msg);
                }
                free(msg);
            }
        } while (1);

        if (fname != NULL)
            free(fname);

        /* if there are leftovers to read, finish up cleanly */
        if ((int)nlen-1 > 0) {
            PRINT_VERBOSE(1, "\nDiscarding current data hunk.\n");
            fname = (char*)malloc(nlen-1);
            mobilebackup2_receive_raw(m_mobilebackup2, fname, nlen-1, &r);
            free(fname);
            fs::deleteFile(bname);
        }

        /* clean up */
        if (dname != NULL)
            free(dname);

        plist_t empty_plist = plist_new_dict();
        mobilebackup2_send_status_response(m_mobilebackup2, errcode, errdesc, empty_plist);
        plist_free(empty_plist);

        return file_count;
    }
    
    void handleMoveFiles(IDeviceBackup *pThis, plist_t message, char *dlmsg, const std::string& outputPath)
    {
        /* perform a series of rename operations */
        setOverallProgressFromMessage(pThis, message, dlmsg);
        plist_t moves = plist_array_get_item(message, 1);
        uint32_t cnt = plist_dict_get_size(moves);
        PRINT_VERBOSE(1, "Moving %d file%s\n", cnt, (cnt == 1) ? "" : "s");
        plist_dict_iter iter = NULL;
        plist_dict_new_iter(moves, &iter);
        int errcode = 0;
        const char *errdesc = NULL;
        if (iter) {
            char *key = NULL;
            plist_t val = NULL;
            do {
                plist_dict_next_item(moves, iter, &key, &val);
                if (key && PLIST_IS_STRING(val)) {
                    const char *str = plist_get_string_ptr(val, NULL);
                    if (str) {
                        std::map<std::string, size_t>::iterator it = m_files.find(key);
                        if (it != m_files.end())
                        {
                            size_t fileSize = it->second;
                            m_files.erase(it);
                            m_files[str] = fileSize;
                        }
                        
                        std::string newpath = combinePath(outputPath, str);
                        std::string oldpath = combinePath(outputPath, key);

                        if (existsDirectory(newpath))
                            fs::deleteDirectoryRecursively(newpath);
                        else
                            fs::deleteFile(newpath);
                        if ((errcode = fs::moveFile(oldpath, newpath)) != 0)
                        {
                            printlog("Renameing '%s' to '%s' failed: %s (%d)\n", oldpath.c_str(), newpath.c_str(), strerror(errno), errno);
                            errdesc = strerror(errcode);
                            errcode = fs::convertErrnoToDeviceError(errcode);
                            break;
                        }
                    }
                    plist_mem_free(key);
                    key = NULL;
                }
            } while (val);
            plist_mem_free(iter);
        } else {
            errcode = -1;
            errdesc = "Could not create dict iterator";
            printlog("Could not create dict iterator\n");
        }
        plist_t empty_dict = plist_new_dict();
        mobilebackup2_error_t err = mobilebackup2_send_status_response(m_mobilebackup2, errcode, errdesc, empty_dict);
        plist_free(empty_dict);
        if (err != MOBILEBACKUP2_E_SUCCESS) {
            printlog("Could not send status response, error %d\n", err);
        }
    }
    
    void handleRemoveFiles(IDeviceBackup *pThis, plist_t message, char *dlmsg, const std::string& outputPath)
    {
        setOverallProgressFromMessage(pThis, message, dlmsg);
        plist_t removes = plist_array_get_item(message, 1);
        uint32_t cnt = plist_array_get_size(removes);
        PRINT_VERBOSE(1, "Removing %d file%s\n", cnt, (cnt == 1) ? "" : "s");
        uint32_t ii = 0;
        int errcode = 0;
        const char *errdesc = NULL;
        for (ii = 0; ii < cnt; ii++) {
            plist_t val = plist_array_get_item(removes, ii);
            if (plist_get_node_type(val) == PLIST_STRING) {
                char *str = NULL;
                plist_get_string_val(val, &str);
                if (str) {
                    const char *checkfile = strchr(str, '/');
                    int suppress_warning = 0;
                    if (checkfile) {
                        if (strcmp(checkfile+1, "Manifest.mbdx") == 0) {
                            suppress_warning = 1;
                        }
                    }
                    std::string newpath = combinePath(outputPath, str);
                    plist_mem_free(str);
                    int res = 0;
                    if (existsDirectory(newpath)) {
                        res = fs::deleteDirectoryRecursively(newpath);
                    } else {
                        res = fs::deleteFile(newpath);
                    }
                    if (res != 0 && res != ENOENT)
                    {
                        if (!suppress_warning)
                            printlog("Could not remove '%s'\n", newpath.c_str());
                        errcode = fs::convertErrnoToDeviceError(res);
                        errdesc = strerror(res);
                    }
                    else
                    {
                        printlog("Removed: %s\n", newpath.c_str());
                    }
                }
            }
        }
        plist_t empty_dict = plist_new_dict();
        mobilebackup2_error_t err = mobilebackup2_send_status_response(m_mobilebackup2, errcode, errdesc, empty_dict);
        plist_free(empty_dict);
        if (err != MOBILEBACKUP2_E_SUCCESS) {
            printlog("Could not send status response, error %d\n", err);
        }
    }
    
    void handleCopyItem(IDeviceBackup *pThis, plist_t message, char *dlmsg, const std::string& outputPath)
    {
        plist_t srcpath = plist_array_get_item(message, 1);
        plist_t dstpath = plist_array_get_item(message, 2);
        int errcode = 0;
        const char *errdesc = NULL;
        if ((plist_get_node_type(srcpath) == PLIST_STRING) && (plist_get_node_type(dstpath) == PLIST_STRING)) {
            char *src = NULL;
            char *dst = NULL;
            plist_get_string_val(srcpath, &src);
            plist_get_string_val(dstpath, &dst);
            if (src && dst) {
                std::string oldpath = combinePath(outputPath, src);
                std::string newpath = combinePath(outputPath, dst);

                PRINT_VERBOSE(1, "Copying '%s' to '%s'\n", src, dst);

                /* check that src exists */
                if (existsDirectory(oldpath)) {
                    copyDirectory(oldpath, newpath);
                } else if (existsFile(oldpath)) {
                    copyFile(oldpath, newpath);
                }
            }
            plist_mem_free(src);
            plist_mem_free(dst);
        }
        plist_t empty_dict = plist_new_dict();
        mobilebackup2_error_t err = mobilebackup2_send_status_response(m_mobilebackup2, errcode, errdesc, empty_dict);
        plist_free(empty_dict);
        if (err != MOBILEBACKUP2_E_SUCCESS) {
            printlog("Could not send status response, error %d\n", err);
        }
    }
    
    void handleProcessMessage(plist_t message, int& operation_ok, int& result_code, const std::string outputDir)
    {
#ifndef NDEBUG
        writePlistFile(message, combinePath(outputDir, "dbg", "error.plist"), PLIST_FORMAT_XML);
#endif
        plist_t node_tmp = plist_array_get_item(message, 1);
        if (!PLIST_IS_DICT(node_tmp)) {
            printlog("Unknown message received!\n");
        }
        plist_t nn;
        int error_code = -1;
        nn = plist_dict_get_item(node_tmp, "ErrorCode");
        if (nn && PLIST_IS_UINT(nn)) {
            uint64_t ec = 0;
            plist_get_uint_val(nn, &ec);
            error_code = (uint32_t)ec;
            if (error_code == 0) {
                operation_ok = 1;
                result_code = 0;
            } else {
                result_code = -error_code;
            }
        }
        nn = plist_dict_get_item(node_tmp, "ErrorDescription");
        const char *str = NULL;
        if (nn && PLIST_IS_STRING(nn)) {
            str = plist_get_string_ptr(nn, NULL);
        }
        if (error_code != 0) {
            if (str) {
                printlog("ErrorCode %d: %s\n", error_code, str);
            } else {
                printlog("ErrorCode %d: (Unknown)\n", error_code);
            }
        }
        nn = plist_dict_get_item(node_tmp, "Content");
        if (nn && PLIST_IS_STRING(nn)) {
            str = plist_get_string_ptr(nn, NULL);
            PRINT_VERBOSE(1, "Content:\n");
            printlog("%s", str);
        }
    }
    
    void mb2_multi_status_add_file_error(plist_t status_dict, const char *path, int error_code, const char *error_message)
    {
        if (!status_dict) return;
        plist_t filedict = plist_new_dict();
        plist_dict_set_item(filedict, "DLFileErrorString", plist_new_string(error_message));
        plist_dict_set_item(filedict, "DLFileErrorCode", plist_new_uint(error_code));
        plist_dict_set_item(status_dict, path, filedict);
    }

    void mobilebackup_afc_get_file_contents(afc_client_t afc, const char *filename, char **data, uint64_t *size)
    {
        if (!afc || !data || !size) {
            return;
        }

        char **fileinfo = NULL;
        uint32_t fsize = 0;

        afc_get_file_info(afc, filename, &fileinfo);
        if (!fileinfo) {
            return;
        }
        int i;
        for (i = 0; fileinfo[i]; i+=2) {
            if (!strcmp(fileinfo[i], "st_size")) {
                fsize = atol(fileinfo[i+1]);
                break;
            }
        }
        afc_dictionary_free(fileinfo);

        if (fsize == 0) {
            return;
        }

        uint64_t f = 0;
        afc_file_open(afc, filename, AFC_FOPEN_RDONLY, &f);
        if (!f) {
            return;
        }
        char *buf = (char*)malloc((uint32_t)fsize);
        uint32_t done = 0;
        while (done < fsize) {
            uint32_t bread = 0;
            afc_file_read(afc, f, buf+done, 65536, &bread);
            if (bread > 0) {
                done += bread;
            } else {
                break;
            }
        }
        if (done == fsize) {
            *size = fsize;
            *data = buf;
        } else {
            free(buf);
        }
        afc_file_close(afc, f);
    }
    
    plist_t newInfoPlist(const char* udid, idevice_t device, afc_client_t afc)
    {
        /* gather data from lockdown */
        plist_t value_node = NULL;
        plist_t root_node = NULL;
        plist_t itunes_settings = NULL;
        plist_t min_itunes_version = NULL;
        std::string udid_uppercase;

        lockdownd_client_t lockdown = NULL;
        if (lockdownd_client_new_with_handshake(device, &lockdown, CLIENT_ID) != LOCKDOWN_E_SUCCESS) {
            return NULL;
        }

        plist_t ret = plist_new_dict();

        /* get basic device information in one go */
        lockdownd_get_value(lockdown, NULL, NULL, &root_node);

        /* get iTunes settings */
        lockdownd_get_value(lockdown, "com.apple.iTunes", NULL, &itunes_settings);

        /* get minimum iTunes version */
        lockdownd_get_value(lockdown, "com.apple.mobile.iTunes", "MinITunesVersion", &min_itunes_version);

        lockdownd_client_free(lockdown);

        /* get a list of installed user applications */
        plist_t app_dict = plist_new_dict();
        plist_t installed_apps = plist_new_array();
        instproxy_client_t ip = NULL;
        if (instproxy_client_start_service(device, &ip, CLIENT_ID) == INSTPROXY_E_SUCCESS) {
            plist_t client_opts = instproxy_client_options_new();
            instproxy_client_options_add(client_opts, "ApplicationType", "User", NULL);
            instproxy_client_options_set_return_attributes(client_opts, "CFBundleIdentifier", "ApplicationSINF", "iTunesMetadata", NULL);

            plist_t apps = NULL;
            instproxy_browse(ip, client_opts, &apps);

            sbservices_client_t sbs = NULL;
            if (sbservices_client_start_service(device, &sbs, CLIENT_ID) != SBSERVICES_E_SUCCESS) {
                printlog("Couldn't establish sbservices connection. Continuing anyway.\n");
            }

            if (apps && PLIST_IS_ARRAY(apps)) {
                uint32_t app_count = plist_array_get_size(apps);
                uint32_t i;
                for (i = 0; i < app_count; i++) {
                    plist_t app_entry = plist_array_get_item(apps, i);
                    plist_t bundle_id = plist_dict_get_item(app_entry, "CFBundleIdentifier");
                    if (bundle_id) {
                        char *bundle_id_str = NULL;
                        plist_array_append_item(installed_apps, plist_copy(bundle_id));

                        plist_get_string_val(bundle_id, &bundle_id_str);
                        plist_t sinf = plist_dict_get_item(app_entry, "ApplicationSINF");
                        plist_t meta = plist_dict_get_item(app_entry, "iTunesMetadata");
                        if (sinf && meta) {
                            plist_t adict = plist_new_dict();
                            plist_dict_set_item(adict, "ApplicationSINF", plist_copy(sinf));
                            if (sbs) {
                                char *pngdata = NULL;
                                uint64_t pngsize = 0;
                                sbservices_get_icon_pngdata(sbs, bundle_id_str, &pngdata, &pngsize);
                                if (pngdata) {
                                    plist_dict_set_item(adict, "PlaceholderIcon", plist_new_data(pngdata, pngsize));
                                    plist_mem_free(pngdata);
                                }
                            }
                            plist_dict_set_item(adict, "iTunesMetadata", plist_copy(meta));
                            plist_dict_set_item(app_dict, bundle_id_str, adict);
                        }
                        plist_mem_free(bundle_id_str);
                    }
                }
            }
            plist_free(apps);

            if (sbs) {
                sbservices_client_free(sbs);
            }

            instproxy_client_options_free(client_opts);

            instproxy_client_free(ip);
        }

        /* Applications */
        plist_dict_set_item(ret, "Applications", app_dict);

        /* set fields we understand */
        value_node = plist_dict_get_item(root_node, "BuildVersion");
        plist_dict_set_item(ret, "Build Version", plist_copy(value_node));

        value_node = plist_dict_get_item(root_node, "DeviceName");
        plist_dict_set_item(ret, "Device Name", plist_copy(value_node));
        plist_dict_set_item(ret, "Display Name", plist_copy(value_node));

        std::string uuid = makeUuid();
        plist_dict_set_item(ret, "GUID", plist_new_string(uuid.c_str()));

        value_node = plist_dict_get_item(root_node, "IntegratedCircuitCardIdentity");
        if (value_node)
            plist_dict_set_item(ret, "ICCID", plist_copy(value_node));

        value_node = plist_dict_get_item(root_node, "InternationalMobileEquipmentIdentity");
        if (value_node)
            plist_dict_set_item(ret, "IMEI", plist_copy(value_node));

        /* Installed Applications */
        plist_dict_set_item(ret, "Installed Applications", installed_apps);

        plist_dict_set_item(ret, "Last Backup Date", plist_new_date(time(NULL) - MAC_EPOCH, 0));

        value_node = plist_dict_get_item(root_node, "MobileEquipmentIdentifier");
        if (value_node)
            plist_dict_set_item(ret, "MEID", plist_copy(value_node));

        value_node = plist_dict_get_item(root_node, "PhoneNumber");
        if (value_node && (plist_get_node_type(value_node) == PLIST_STRING)) {
            plist_dict_set_item(ret, "Phone Number", plist_copy(value_node));
        }

        /* FIXME Product Name */

        value_node = plist_dict_get_item(root_node, "ProductType");
        plist_dict_set_item(ret, "Product Type", plist_copy(value_node));

        value_node = plist_dict_get_item(root_node, "ProductVersion");
        plist_dict_set_item(ret, "Product Version", plist_copy(value_node));

        value_node = plist_dict_get_item(root_node, "SerialNumber");
        plist_dict_set_item(ret, "Serial Number", plist_copy(value_node));

        /* FIXME Sync Settings? */

        value_node = plist_dict_get_item(root_node, "UniqueDeviceID");
        plist_dict_set_item(ret, "Target Identifier", plist_new_string(udid));

        plist_dict_set_item(ret, "Target Type", plist_new_string("Device"));

        /* uppercase */
        udid_uppercase = toUpper((char*)udid);
        plist_dict_set_item(ret, "Unique Identifier", plist_new_string(udid_uppercase.c_str()));

        char *data_buf = NULL;
        uint64_t data_size = 0;
        mobilebackup_afc_get_file_contents(afc, "/Books/iBooksData2.plist", &data_buf, &data_size);
        if (data_buf) {
            plist_dict_set_item(ret, "iBooks Data 2", plist_new_data(data_buf, data_size));
            free(data_buf);
        }

        plist_t files = plist_new_dict();
        const char *itunesfiles[] = {
            "ApertureAlbumPrefs",
            "IC-Info.sidb",
            "IC-Info.sidv",
            "PhotosFolderAlbums",
            "PhotosFolderName",
            "PhotosFolderPrefs",
            "VoiceMemos.plist",
            "iPhotoAlbumPrefs",
            "iTunesApplicationIDs",
            "iTunesPrefs",
            "iTunesPrefs.plist",
            NULL
        };
        int i = 0;
        for (i = 0; itunesfiles[i]; i++) {
            data_buf = NULL;
            data_size = 0;
            char *fname = (char*)malloc(strlen("/iTunes_Control/iTunes/") + strlen(itunesfiles[i]) + 1);
            strcpy(fname, "/iTunes_Control/iTunes/");
            strcat(fname, itunesfiles[i]);
            mobilebackup_afc_get_file_contents(afc, fname, &data_buf, &data_size);
            free(fname);
            if (data_buf) {
                plist_dict_set_item(files, itunesfiles[i], plist_new_data(data_buf, data_size));
                free(data_buf);
            }
        }
        plist_dict_set_item(ret, "iTunes Files", files);

        plist_dict_set_item(ret, "iTunes Settings", itunes_settings ? plist_copy(itunes_settings) : plist_new_dict());

        /* since we usually don't have iTunes, let's get the minimum required iTunes version from the device */
        if (min_itunes_version) {
            plist_dict_set_item(ret, "iTunes Version", plist_copy(min_itunes_version));
        } else {
            plist_dict_set_item(ret, "iTunes Version", plist_new_string("10.0.1"));
        }

        plist_free(itunes_settings);
        plist_free(min_itunes_version);
        plist_free(root_node);

        return ret;
    }
    
    bool willEncrypt()
    {
        uint8_t willEncrypt = 0;
        plist_t node = NULL;
        lockdownd_get_value(m_client, "com.apple.mobile.backup", "WillEncrypt", &node);
        if (node)
        {
            if (PLIST_IS_BOOLEAN(node))
            {
                plist_get_bool_val(node, &willEncrypt);
            }
            plist_free(node);
            node = NULL;
        }

        return willEncrypt != 0;
    }

    int getProductVersion()
    {
        int deviceVersion = 0;
        plist_t node = NULL;
        lockdownd_get_value(m_client, NULL, "ProductVersion", &node);
        if (node)
        {
            const char *productVersion = NULL;
            if (PLIST_IS_STRING(node))
            {
                productVersion = plist_get_string_ptr(node, NULL);
            }
            
            if (productVersion)
            {
                int vers[3] = { 0, 0, 0 };
                if (sscanf(productVersion, "%d.%d.%d", &vers[0], &vers[1], &vers[2]) >= 2) {
                    deviceVersion = DEVICE_VERSION(vers[0], vers[1], vers[2]);
                }
            }
            
            plist_free(node);
            node = NULL;
        }

        return deviceVersion;
    }
    
    inline mobilebackup2_error_t sendHelloMessage(double& remote_version)
    {
        /* send Hello message */
        double local_versions[2] = {2.0, 2.1};
        return mobilebackup2_version_exchange(m_mobilebackup2, local_versions, 2, &remote_version);
    }
    
    bool backup(IDeviceBackup *pThis, const std::string& outputPath)
    {
        /* start notification_proxy */
        
        char* sourceUdid = NULL;
        mobilebackup2_error_t err = MOBILEBACKUP2_E_SUCCESS;
        int isFullBackup = 0;
        
        // uint64_t lockfile = 0;
        int result_code = -1;

        lockdownd_service_descriptor_t service = NULL;
        lockdownd_error_t ldret = lockdownd_start_service(m_client, NP_SERVICE_NAME, &service);
        if ((ldret == LOCKDOWN_E_SUCCESS) && service && service->port)
        {
            np_client_new(m_device, service, &m_np);
            np_set_notify_callback(m_np, notifyCallback, (void *)pThis);
            const char *noties[5] = {
                NP_SYNC_CANCEL_REQUEST,
                NP_SYNC_SUSPEND_REQUEST,
                NP_SYNC_RESUME_REQUEST,
                NP_BACKUP_DOMAIN_CHANGED,
                NULL
            };
            np_observe_notifications(m_np, noties);
        }
        else
        {
            printlog("ERROR: Could not start service %s: %s\n", NP_SERVICE_NAME, lockdownd_strerror(ldret));
            return false;
        }
        
        freeService(service);
        
        /* start AFC, we need this for the lock file */
        ldret = lockdownd_start_service(m_client, AFC_SERVICE_NAME, &service);
        if ((ldret != LOCKDOWN_E_SUCCESS) || NULL == service || 0 == service->port)
        {
            freeService(service);
            printlog("ERROR: Could not start service %s: %s\n", AFC_SERVICE_NAME, lockdownd_strerror(ldret));
            return false;
        }

        afc_client_new(m_device, service, &m_afc);
        freeService(service);

        /* start mobilebackup service and retrieve port */
        ldret = lockdownd_start_service_with_escrow_bag(m_client, MOBILEBACKUP2_SERVICE_NAME, &service);
        freeClient();
        if ((ldret != LOCKDOWN_E_SUCCESS) || NULL == service || 0 == service->port)
        {
            freeService(service);
            return false;
        }
        
        PRINT_VERBOSE(1, "Started \"%s\" service on port %d.\n", MOBILEBACKUP2_SERVICE_NAME, service->port);
        mobilebackup2_client_new(m_device, service, &m_mobilebackup2);

        freeService(service);

        double remote_version = 0.0;
        err = sendHelloMessage(remote_version);
        if (err != MOBILEBACKUP2_E_SUCCESS)
        {
            printlog("Could not perform backup protocol version exchange, error code %d\n", err);
            return false;
        }

        PRINT_VERBOSE(1, "Negotiated Protocol Version %.1f\n", remote_version);

        /* check abort conditions */
        
        /*
        if (pThis->isCanclled())
        {
            PRINT_VERBOSE(1, "Aborting as requested by user...\n");
            skipBackup = true;
            goto checkpoint;
        }
         */

        /* verify existing Info.plist */
        std::string info_path;
        /* backup directory must contain an Info.plist */
        info_path = combinePath(outputPath, m_udid, "Info.plist");
        plist_t infoPlist = NULL;
        if (!info_path.empty() && existsFile(info_path))
        {
            /// PRINT_VERBOSE(1, "Reading Info.plist from backup.\n");
            readPlistFile(&infoPlist, info_path);

            if (!infoPlist)
            {
                printlog("Could not read Info.plist\n");
                isFullBackup = 1;
            }
        } else {
            isFullBackup = 1;
        }

        doPostNotification(m_device, NP_SYNC_WILL_START);
        afc_file_open(m_afc, "/com.apple.itunes.lock_sync", AFC_FOPEN_RW, &m_lockfile);
        
        if (m_lockfile)
        {
            afc_error_t aerr;
            doPostNotification(m_device, NP_SYNC_LOCK_REQUEST);
            int i = 0;
            for (; i < LOCK_ATTEMPTS; i++)
            {
                aerr = afc_file_lock(m_afc, m_lockfile, AFC_LOCK_EX);
                if (aerr == AFC_E_SUCCESS)
                {
                    doPostNotification(m_device, NP_SYNC_DID_START);
                    break;
                }
                else if (aerr == AFC_E_OP_WOULD_BLOCK)
                {
                    usleep(LOCK_WAIT);
                    continue;
                }
                else
                {
                    printlog("ERROR: could not lock file! error code: %d\n", aerr);
                    afc_file_close(m_afc, m_lockfile);
                    m_lockfile = 0;
                    /// cmd = CMD_LEAVE;
                }
            }
            if (i == LOCK_ATTEMPTS) {
                printlog("ERROR: timeout while locking for sync\n");
                afc_file_close(m_afc, m_lockfile);
                m_lockfile = 0;
                // skipBackup = true;
                return false;
            }
        }

        PRINT_VERBOSE(1, "Starting backup...\n");

        /* make sure backup device sub-directory exists */
        std::string devBackupDir = combinePath(outputPath, m_udid);
        fs::makeDirectory(devBackupDir, 0755);
        
        /* TODO: check domain com.apple.mobile.backup key RequiresEncrypt and WillEncrypt with lockdown */
        /* TODO: verify battery on AC enough battery remaining */

        /* re-create Info.plist (Device infos, IC-Info.sidb, photos, app_ids, iTunesPrefs) */
        if (infoPlist)
        {
            plist_free(infoPlist);
            infoPlist = NULL;
        }
        infoPlist = newInfoPlist(m_udid.c_str(), m_device, m_afc);
        if (!infoPlist)
        {
            printlog("Failed to generate Info.plist - aborting\n");
            // skipBackup = true;
            return false;
        }
        
        fs::deleteFile(info_path);
        writePlistFile(infoPlist, info_path.c_str(), PLIST_FORMAT_XML);
        
        plist_free(infoPlist);
        infoPlist = NULL;

        plist_t opts = NULL;
        if (pThis->forceFullBackup())
        {
            /// PRINT_VERBOSE(1, "Enforcing full backup from device.\n");
            opts = plist_new_dict();
            plist_dict_set_item(opts, "ForceFullBackup", plist_new_bool(1));
        }
        /* request backup from device with manifest from last backup */
        /*
        if (willEncrypt) {
            /// PRINT_VERBOSE(1, "Backup will be encrypted.\n");
        } else {
            /// PRINT_VERBOSE(1, "Backup will be unencrypted.\n");
        }
         */
        PRINT_VERBOSE(1, "Requesting backup from device...\n");
        err = mobilebackup2_send_request(m_mobilebackup2, "Backup", m_udid.c_str(), sourceUdid, opts);
        if (opts)
            plist_free(opts);
        if (err == MOBILEBACKUP2_E_SUCCESS)
        {
            if (isFullBackup)
            {
                PRINT_VERBOSE(1, "Full backup mode.\n");
            }
            else
            {
                PRINT_VERBOSE(1, "Incremental backup mode.\n");
            }
        }
        else
        {
            if (err == MOBILEBACKUP2_E_BAD_VERSION) {
                printlog("ERROR: Could not start backup process: backup protocol version mismatch!\n");
            } else if (err == MOBILEBACKUP2_E_REPLY_NOT_OK) {
                printlog("ERROR: Could not start backup process: device refused to start the backup process.\n");
            } else {
                printlog("ERROR: Could not start backup process: unspecified error occurred\n");
            }
            // skipBackup = true;
            return false;
        }
        
        /* reset operation success status */
        
        plist_t message = NULL;
        char *dlmsg = NULL;
        
        mobilebackup2_error_t mberr = MOBILEBACKUP2_E_SUCCESS;
        int operation_ok = 0;
        int file_count = 0;
        int progress_finished = 0;

        /* process series of DLMessage* operations */
        do {
            mberr = mobilebackup2_receive_message(m_mobilebackup2, &message, &dlmsg);
            if (mberr == MOBILEBACKUP2_E_RECEIVE_TIMEOUT) {
                PRINT_VERBOSE(2, "Device is not ready yet, retrying...\n");
                // goto files_out;
            } else if (mberr != MOBILEBACKUP2_E_SUCCESS) {
                PRINT_VERBOSE(0, "ERROR: Could not receive from mobilebackup2 (%d)\n", mberr);
                pThis->cancel();
                // goto files_out;
            }
            else
            {
#ifndef NDEBUG
                static unsigned int fileIndex = 0;
                fileIndex++;
                char fileName[32] = { 0 };
                sprintf(fileName, "message_%04u.plist", fileIndex);
#ifdef _WIN32
                std::string filePath = combinePath(outputPath, "dbg", "new", fileName);
#else
                std::string filePath = combinePath(outputPath, "dbg", "org", fileName);
#endif
                writePlistFile(message, filePath, PLIST_FORMAT_XML);
#endif
                
                if (!strcmp(dlmsg, "DLMessageDownloadFiles")) {
                    /* device wants to download files from the computer */
                    setOverallProgressFromMessage(pThis, message, dlmsg);
                    handleSendFiles(message, outputPath.c_str());
                } else if (!strcmp(dlmsg, "DLMessageUploadFiles")) {
                    /* device wants to send files to the computer */
                    setOverallProgressFromMessage(pThis, message, dlmsg);
                    file_count += handleReceiveFiles(pThis, message, outputPath.c_str());
                } else if (!strcmp(dlmsg, "DLMessageGetFreeDiskSpace")) {
                    /* device wants to know how much disk space is available on the computer */
                    uint64_t freespace = 0;
                    int res = calcFreeSpace(outputPath, freespace);
                    plist_t freespace_item = plist_new_uint(freespace);
                    mobilebackup2_send_status_response(m_mobilebackup2, res, NULL, freespace_item);
                    plist_free(freespace_item);
                } else if (!strcmp(dlmsg, "DLMessagePurgeDiskSpace")) {
                    /* device wants to purge disk space on the host - not supported */
                    plist_t empty_dict = plist_new_dict();
                    err = mobilebackup2_send_status_response(m_mobilebackup2, -1, "Operation not supported", empty_dict);
                    plist_free(empty_dict);
                } else if (!strcmp(dlmsg, "DLContentsOfDirectory")) {
                    /* list directory contents */
                    handleListDirectory(message, outputPath.c_str());
                } else if (!strcmp(dlmsg, "DLMessageCreateDirectory")) {
                    /* make a directory */
                    handleMakeDirectory(message, outputPath.c_str());
                } else if (!strcmp(dlmsg, "DLMessageMoveFiles") || !strcmp(dlmsg, "DLMessageMoveItems")) {
                    /* perform a series of rename operations */
                    handleMoveFiles(pThis, message, dlmsg, outputPath);
                } else if (!strcmp(dlmsg, "DLMessageRemoveFiles") || !strcmp(dlmsg, "DLMessageRemoveItems")) {
                    handleRemoveFiles(pThis, message, dlmsg, outputPath);
                } else if (!strcmp(dlmsg, "DLMessageCopyItem")) {
                    handleCopyItem(pThis, message, dlmsg, outputPath);
                } else if (!strcmp(dlmsg, "DLMessageDisconnect")) {
                    break;
                } else if (!strcmp(dlmsg, "DLMessageProcessMessage")) {
                    handleProcessMessage(message, operation_ok, result_code, outputPath);
                    break;
                }

                /* print status */
                
                if ((pThis->getOverallProgress() > 0) && !progress_finished) {
                    if (pThis->getOverallProgress() >= 100.0f) {
                        progress_finished = 1;
                    }
                    // print_progress_real(pThis->getOverallProgress(), 0);
                    PRINT_VERBOSE(1, " Finished\n");
                }
            }

            plist_free(message);
            message = NULL;
            plist_mem_free(dlmsg);
            dlmsg = NULL;

            if (pThis->isCanclled())
            {
                break;
            }
        } while (1);
        
        plist_free(message);
        plist_mem_free(dlmsg);

        /* report operation status to user */
        bool res = false;
        PRINT_VERBOSE(1, "Received %d files from device.\n", file_count);
        if (operation_ok && checkSnapshotState(outputPath, m_udid, "finished")) {
            PRINT_VERBOSE(1, "Backup Successful.\n");
            res = true;
        } else {
            if (pThis->isCanclled()) {
                PRINT_VERBOSE(1, "Backup Aborted.\n");
            } else {
                PRINT_VERBOSE(1, "Backup Failed (Error Code %d).\n", -result_code);
            }
        }

        return res;
    }
    
    inline void freeService(lockdownd_service_descriptor_t& service)
    {
        if (service != NULL)
        {
            lockdownd_service_descriptor_free(service);
            service = NULL;
        }
    }
    
    void unlockAfc()
    {
        if (m_lockfile)
        {
            afc_file_lock(m_afc, m_lockfile, AFC_LOCK_UN);
            afc_file_close(m_afc, m_lockfile);
            m_lockfile = 0;
            
            doPostNotification(m_device, NP_SYNC_DID_FINISH);
        }
    }

    idevice_t getDevice() const
    {
        return m_device;
    }
    
    lockdownd_client_t getClient() const
    {
        return m_client;
    }
    
    
private:
    idevice_t m_device;
    afc_client_t m_afc;
    uint64_t m_lockfile;
    np_client_t m_np;
    lockdownd_client_t m_client;
    mobilebackup2_client_t m_mobilebackup2;
    std::string m_udid;
    std::map<std::string, size_t> m_files;
};

IDeviceBackup::IDeviceBackup(const DeviceInfo& deviceInfo, const std::string& outputPath) : m_deviceInfo(deviceInfo), m_outputPath(outputPath), m_quitFlag(0), m_overallProgress(0)
{
}

bool IDeviceBackup::queryDevices(std::vector<DeviceInfo> &devices)
{
    idevice_info_t *devList = NULL;
    int numberOfDevices = 0;
    
    if (idevice_get_device_list_extended(&devList, &numberOfDevices) < 0)
    {
        printlog("ERROR: Unable to retrieve device list!\n");
        return false;
    }
    devices.clear();
    for (int idx = 0; devList[idx] != NULL; idx++)
    {
        std::vector<DeviceInfo>::iterator it = devices.emplace(devices.end());
        it->setUdid(devList[idx]->udid);
        it->setUsb(devList[idx]->conn_type == CONNECTION_USBMUXD);
        
        std::string value;
        if (queryDeviceName(it->getUdid(), value))
        {
            it->setName(value);
        }
        
        IDeviceBackupClient dbc(it->getUdid());
        
        lockdownd_error_t err = dbc.initWithHandShake();
        dbc.updateDeviceInfo(*it, err);
        if (LOCKDOWN_E_SUCCESS == err)
        {
            if (dbc.queryAppContainer(BUNDLEID_WECHAT, value))
            {
                it->setWechatPath(value);
            }
        }
    }
    idevice_device_list_extended_free(devList);
    
    return true;
}

bool IDeviceBackup::queryDeviceName(const std::string& udid, std::string& name)
{
    if (udid.empty())
    {
        return false;
    }

    IDeviceBackupClient dbc(udid);
    if (!dbc.init())
    {
        return false;
    }
    
    return dbc.queryDeviceName(name);
}

bool IDeviceBackup::queryWechatPath(const std::string& udid, std::string& wechatPath)
{
    if (udid.empty())
    {
        return false;
    }
    
    IDeviceBackupClient dbc(udid);
    if (dbc.initWithHandShake() != LOCKDOWN_E_SUCCESS)
    {
        return false;
    }
    
    bool res = dbc.queryAppContainer(BUNDLEID_WECHAT, wechatPath);
    return res;
}

bool IDeviceBackup::forceFullBackup() const
{
    return false;
}

void IDeviceBackup::cancel()
{
    ++m_quitFlag;
}

bool IDeviceBackup::isCanclled() const
{
    return m_quitFlag > 0u;
}

int IDeviceBackup::getErrCode() const
{
    return m_errCode;
}

const std::string IDeviceBackup::getErrMsg() const
{
    return m_errMsg;
}

void IDeviceBackup::setError(int errCode, const std::string& errMsg)
{
    m_errCode = errCode;
    m_errMsg = errMsg;
}

bool IDeviceBackup::filter(const std::string& srcPath, const std::string& destPath) const
{
    return ((srcPath.find(m_deviceInfo.getWechatUuid()) == std::string::npos) && (srcPath.find("/Containers/Shared/AppGroup/") == std::string::npos));
}
bool IDeviceBackup::backup()
{
    // Verify if backup directory exists
    if (!existsDirectory(m_outputPath))
    {
        printlog("ERROR: Backup directory does not exist: %s\n", m_outputPath.c_str());
        return false;
    }
    
    IDeviceBackupClient dbc(m_deviceInfo.getUdid());
    
    dbc.loadFiles(m_outputPath, m_deviceInfo.getUdid());
    
    if (dbc.initWithHandShake() != LOCKDOWN_E_SUCCESS)
    {
        return false;
    }

    return dbc.backup(this, m_outputPath);
}

double IDeviceBackup::getOverallProgress() const
{
    return (double)m_overallProgress;
}

void IDeviceBackup::setOverallProgress(double progress)
{
    if (progress > 0.0)
        m_overallProgress = progress;
}
