#ifndef WECHAT_EXPORTER_PLIST_COMPAT_H
#define WECHAT_EXPORTER_PLIST_COMPAT_H

#include <plist/plist.h>

namespace wechat_plist_detail {
// libplist 2.3 added an optional detected-format output parameter.
template <typename Result>
inline void fromMemory(Result (*parse)(const char *, uint32_t, plist_t *),
                       const char *data, uint32_t size, plist_t *node)
{
    parse(data, size, node);
}

template <typename Result, typename Format>
inline void fromMemory(Result (*parse)(const char *, uint32_t, plist_t *, Format *),
                       const char *data, uint32_t size, plist_t *node)
{
    parse(data, size, node, nullptr);
}
}

inline void parsePlistMemory(const char *data, uint32_t size, plist_t *node)
{
    wechat_plist_detail::fromMemory(&plist_from_memory, data, size, node);
}

#endif
