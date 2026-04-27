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
#include <cerrno>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// sprintf_s(buf, size, fmt, ...) shim.
// The MSVC form always takes an explicit size as the 2nd argument.
// On Linux we map to snprintf, discarding the size arg (buf is always
// a local array whose sizeof is identical to the passed size in this codebase).
// We use a helper macro to absorb the size argument cleanly.
#define _sprintf_s_discard_size(buf, size, fmt, ...) snprintf(buf, size, fmt, ##__VA_ARGS__)
#define sprintf_s(buf, size, ...) _sprintf_s_discard_size(buf, size, __VA_ARGS__)

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

// fopen_s shim
inline int fopen_s(FILE** f, const char* name, const char* mode) {
    *f = fopen(name, mode);
    return (*f == nullptr) ? errno : 0;
}

// strncpy_s shim
inline int strncpy_s(char* dest, size_t destsz, const char* src, size_t count) {
    size_t n = (count == (size_t)-1 || count >= destsz) ? destsz - 1 : count;
    strncpy(dest, src, n);
    dest[n] = '\0';
    return 0;
}

// strcpy_s shim
inline int strcpy_s(char* dest, size_t destsz, const char* src) {
    strncpy(dest, src, destsz - 1);
    dest[destsz - 1] = '\0';
    return 0;
}

// strcat_s shim
inline int strcat_s(char* dest, size_t destsz, const char* src) {
    size_t dlen = strlen(dest);
    if (dlen >= destsz) return -1;
    strncpy(dest + dlen, src, destsz - dlen - 1);
    dest[destsz - 1] = '\0';
    return 0;
}

// sscanf_s shim — on Linux sscanf is safe enough for our use cases
#define sscanf_s sscanf

#endif // !_MSC_VER

#endif // _PLATFORM_COMPAT_H_
