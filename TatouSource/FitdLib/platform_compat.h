///////////////////////////////////////////////////////////////////////////////
// Platform compatibility shims for non-MSVC compilers
///////////////////////////////////////////////////////////////////////////////

#ifndef _PLATFORM_COMPAT_H_
#define _PLATFORM_COMPAT_H_

#ifndef _MSC_VER

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// sprintf_s shim: ignore buffer size, forward to snprintf
#define sprintf_s(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

// _stricmp / _strnicmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp

// mkdir on Linux takes a mode argument
#define _mkdir(path) mkdir(path, 0755)

// ctime_s shim
inline int ctime_s(char* buf, size_t bufsz, const time_t* timer) {
    const char* result = ctime(timer);
    if (!result) return -1;
    strncpy(buf, result, bufsz - 1);
    buf[bufsz - 1] = '\0';
    return 0;
}

#endif // !_MSC_VER

#endif // _PLATFORM_COMPAT_H_
