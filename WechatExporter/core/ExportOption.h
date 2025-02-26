//
//  ExportOption.h
//  WechatExporter
//
//  Created by Matthew on 2022/7/10.
//  Copyright © 2022 Matthew. All rights reserved.
//

#ifndef ExportOption_h
#define ExportOption_h

enum SessionParsingOption
{
    SPO_IGNORE_AVATAR = 1 << 0,
    SPO_IGNORE_AUDIO = 1 << 1,
    SPO_IGNORE_IMAGE = 1 << 2,
    SPO_IGNORE_VIDEO = 1 << 3,
    SPO_IGNORE_EMOJI = 1 << 4,
    SPO_IGNORE_FILE = 1 << 5,
    SPO_IGNORE_CARD = 1 << 6,
    SPO_IGNORE_SHARING = 1 << 7,
    SPO_IGNORE_HTML_ENC = 1 << 8,
    SPO_TEXT_MODE = 0xFFFF,
    SPO_PDF_MODE = 1 << 16,
    SPO_DOC_MODE = 1 << 17,
    
    SPO_USING_REMOTE_EMOJI = 1 << 19,
    SPO_DESC = 1 << 20,
    SPO_ICON_IN_SESSION = 1 << 21,     // Put Head Icon and Emoji files in the folder of session
    SPO_SYNC_LOADING = 1 << 22,
    SPO_SUPPORT_FILTER = 1 << 23,
    SPO_INCLUDING_SUBSCRIPTION = 1 << 24,
    
    SPO_OVERWRITE_EXISTING_FIlE = 1 << 26,
    SPO_EXP_GROUP_MEMBERS = 1 << 27,
    SPO_EXP_CONTACTS = 1 << 28,
    SPO_OUTPUT_DBG_LOGS = 1 << 29,
    SPO_INCREMENTAL_EXP = 1 << 30,

    SPO_END
};

enum EXPORT_OPTION : uint64_t
{
    EO_INCREMENTAL_EXP = 1ull << 0,
    EO_OUTPUT_DBG_LOGS = 1ull << 1,
    EO_OVERWRT_EXISTING_FIlE = 1ull << 2,
    EO_FILTER_BY_NAME = 1ull << 3,
    EO_EXP_GROUP_MEMBERS = 1ull << 4,
    EO_EXP_CONTACTS = 1ull << 5,

    EO_TEXT_MODE = 1ull << 12,
    EO_PDF_MODE = 1ull << 13,
    EO_DOC_MODE = 1ull << 14,
    
    EO_TIME_DESC = 1ull << 20,
    EO_USING_REMOTE_EMOJI = 1ull << 21,

    // Sync
    // Async
    //  onscroll
    //  normal pager
    //
    
    EO_LOADING_ONSCRLL = 1ull << 28,

    EO_PAGER_NORMAL = 1ull << 29,
    EO_PAGER_YEAR = 1ull << 30,
    EO_PAGER_MONTH = 1ull << 31,
    
    EO_ASYNC_MASK = 0xFull << 28,  // bits
    EO_PAGER_MASK = 0x7ull << 29,  // bits
    
    EO_SUPPORT_FILTER = 1ull << 40,
    EO_INCLUDING_SUBSCRIPTION = 1ull << 41,

    EO_END,
    // EO_LARGR_VALUE =
};

class ExportOption
{
public:
    ExportOption() : m_options(0)
    {
    }
    
    ExportOption(uint64_t options) : m_options(options)
    {
    }
    
    ExportOption& operator=(uint64_t options)
    {
        m_options = options;
        return *this;
    }
    
    void setTextMode(bool textMode = true)
    {
        if (textMode)
            m_options |= EO_TEXT_MODE;
        else
            m_options &= ~EO_TEXT_MODE;
    }
    
    bool isHtmlMode() const
    {
        return (m_options & EO_TEXT_MODE) == 0 && (m_options & EO_PDF_MODE) == 0;
    }
    
    bool isTextMode() const
    {
        return (m_options & EO_TEXT_MODE) == EO_TEXT_MODE;
    }

    void setPdfMode(bool pdfMode = true)
    {
        setTextMode(!pdfMode);  // html mode
        if (pdfMode)
            m_options |= EO_PDF_MODE;
        else
            m_options &= ~EO_PDF_MODE;
    }
    
    bool isPdfMode() const
    {
        return (m_options & EO_PDF_MODE) == EO_PDF_MODE;
    }

    void setOrder(bool asc = true)
    {
        if (asc)
            m_options &= ~EO_TIME_DESC;
        else
            m_options |= EO_TIME_DESC;
    }
    
    bool isDesc() const
    {
        return (m_options & EO_TIME_DESC) == EO_TIME_DESC;
    }

    void saveFilesInSessionFolder(bool flag = true)
    {
    }
    
    void filterByName()
    {
        m_options |= EO_FILTER_BY_NAME;
    }
    
    bool isFilteredByName() const
    {
        return (m_options & EO_FILTER_BY_NAME) == EO_FILTER_BY_NAME;
    }

    void setIncrementalExporting(bool incrementalExporting)
    {
        if (incrementalExporting)
            m_options |= EO_INCREMENTAL_EXP;
        else
            m_options &= ~EO_INCREMENTAL_EXP;
    }
    
    bool isIncrementalExporting() const
    {
        return (m_options & EO_INCREMENTAL_EXP) == EO_INCREMENTAL_EXP;
    }

    void supportsFilter(bool supportsFilter = true)
    {
        if (supportsFilter)
            m_options |= EO_SUPPORT_FILTER;
        else
            m_options &= ~EO_SUPPORT_FILTER;
    }
    
    bool isSupportingFilter() const
    {
        return (m_options & EO_SUPPORT_FILTER) == EO_SUPPORT_FILTER;
    }

    void outputDebugLogs(bool outputDebugLogs)
    {
        if (outputDebugLogs)
            m_options |= EO_OUTPUT_DBG_LOGS;
        else
            m_options &= ~EO_OUTPUT_DBG_LOGS;
    }
    
    bool isOutputtingDebugLogs() const
    {
        return (m_options & EO_OUTPUT_DBG_LOGS) == EO_OUTPUT_DBG_LOGS;
    }

    void useRemoteEmoji(bool useEmojiUrl)
    {
        if (useEmojiUrl)
            m_options |= EO_USING_REMOTE_EMOJI;
        else
            m_options &= ~EO_USING_REMOTE_EMOJI;
    }
    
    bool isUsingRemoteEmoji() const
    {
        return (m_options & EO_USING_REMOTE_EMOJI) == EO_USING_REMOTE_EMOJI;
    }
    
    void includesSubscription()
    {
        m_options |= EO_INCLUDING_SUBSCRIPTION;
    }
    
    bool isIncludingSubscription() const
    {
        return (m_options & EO_INCLUDING_SUBSCRIPTION) == EO_INCLUDING_SUBSCRIPTION;
    }
    
    bool isSyncLoading() const
    {
        return (m_options & EO_ASYNC_MASK) == 0;
    }
    
    bool isAsyncLoading() const
    {
        return (m_options | EO_ASYNC_MASK) != 0;
    }
    
    void setSyncLoading()
    {
        m_options &= ~EO_ASYNC_MASK;
    }

    void setLoadingDataOnScroll(bool loadingDataOnScroll = true)
    {
        if (loadingDataOnScroll)
            m_options |= EO_LOADING_ONSCRLL;
        else
            m_options &= ~EO_LOADING_ONSCRLL;
    }
    
    bool getLoadingDataOnScroll() const
    {
        return (m_options & EO_LOADING_ONSCRLL) == EO_LOADING_ONSCRLL;
    }
    
    bool hasPager() const
    {
        return (m_options & EO_PAGER_MASK) != 0;
    }
    
    void setPager()
    {
        m_options |= EO_PAGER_NORMAL;
        m_options &= ~EO_PAGER_YEAR;
        m_options &= ~EO_PAGER_MONTH;
    }

    
    void setPagerByYear()
    {
        m_options &= ~EO_PAGER_NORMAL;
        m_options |= EO_PAGER_YEAR;
        m_options &= ~EO_PAGER_MONTH;
    }

    bool isPagerByYear() const
    {
        return (m_options & EO_PAGER_YEAR) == EO_PAGER_YEAR;
    }
    
    void setPagerByMonth()
    {
        m_options &= ~EO_PAGER_NORMAL;
        m_options &= ~EO_PAGER_YEAR;
        m_options |= EO_PAGER_MONTH;
    }

    bool isPagerByMonth() const
    {
        return (m_options & EO_PAGER_MONTH) == EO_PAGER_MONTH;
    }
    
    
    operator uint64_t() const
    {
        return m_options;
    }
    
    void fromSessionParsingOptions(int options)
    {
        m_options = 0;
        
        if ((options & SPO_TEXT_MODE) == SPO_TEXT_MODE)
        {
            m_options |= EO_TEXT_MODE;
        }
        if ((options & SPO_PDF_MODE) == SPO_PDF_MODE)
        {
            m_options |= EO_PDF_MODE;
        }
        if ((options & SPO_DOC_MODE) == SPO_DOC_MODE)
        {
            m_options |= EO_DOC_MODE;
        }
        
        if ((options & SPO_USING_REMOTE_EMOJI) == SPO_USING_REMOTE_EMOJI)
        {
            m_options |= EO_USING_REMOTE_EMOJI;
        }
        if ((options & SPO_DESC) == SPO_DESC)
        {
            m_options |= EO_TIME_DESC;
        }
        if ((options & SPO_SYNC_LOADING) == SPO_SYNC_LOADING)
        {
            m_options &= ~EO_ASYNC_MASK;
        }
        if ((options & SPO_SUPPORT_FILTER) == SPO_SUPPORT_FILTER)
        {
            m_options |= EO_SUPPORT_FILTER;
        }
        
        if ((options & SPO_INCLUDING_SUBSCRIPTION) == SPO_INCLUDING_SUBSCRIPTION)
        {
            m_options |= EO_INCLUDING_SUBSCRIPTION;
        }
        if ((options & SPO_OVERWRITE_EXISTING_FIlE) == SPO_OVERWRITE_EXISTING_FIlE)
        {
            m_options |= EO_OVERWRT_EXISTING_FIlE;
        }
        if ((options & SPO_EXP_GROUP_MEMBERS) == SPO_EXP_GROUP_MEMBERS)
        {
            m_options |= EO_EXP_GROUP_MEMBERS;
        }
        if ((options & SPO_EXP_CONTACTS) == SPO_EXP_CONTACTS)
        {
            m_options |= EO_EXP_CONTACTS;
        }
        if ((options & SPO_OUTPUT_DBG_LOGS) == SPO_OUTPUT_DBG_LOGS)
        {
            m_options |= EO_OUTPUT_DBG_LOGS;
        }
        if ((options & SPO_INCREMENTAL_EXP) == SPO_INCREMENTAL_EXP)
        {
            m_options |= EO_INCREMENTAL_EXP;
        }
    }
    
private:
    uint64_t m_options;
};


#endif /* ExportOption_h */
