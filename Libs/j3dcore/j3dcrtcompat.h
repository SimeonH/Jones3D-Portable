#ifndef J3DCORE_J3DCRTCOMPAT_H
#define J3DCORE_J3DCRTCOMPAT_H
// Microsoft CRT extensions ("secure" _s functions and friends) used by the game code, for non-Windows builds.
// Semantics follow the MSVC CRT; see j3dcrtcompat.c.
#ifdef _WIN32
#error "j3dcrtcompat.h is only for non-Windows builds"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t rsize_t;
typedef int errno_t;

#define _TRUNCATE ((size_t)-1)

#define __int64 int64_t

#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _snwprintf swprintf
#define _stricmp strcasecmp
#define _strcmpi strcasecmp
#define _strnicmp strncasecmp
#define _wcsicmp wcscasecmp
#define _wcsnicmp wcsncasecmp
#define strtok_s strtok_r
#define sprintf_s snprintf

// MSVC rand(): same generator and range (0 - 0x7FFF) as on Windows, the game code scales rand() by 1/32767
int j3d_rand(void);
void j3d_srand(unsigned int seed);
#ifndef __cplusplus
#   undef RAND_MAX
#   define RAND_MAX 0x7FFF
#   define rand j3d_rand
#   define srand j3d_srand
#endif

size_t strnlen_s(const char* str, size_t maxSize);
errno_t strcpy_s(char* pDest, rsize_t destSize, const char* pSrc);
errno_t strncpy_s(char* pDest, rsize_t destSize, const char* pSrc, rsize_t count);
errno_t strcat_s(char* pDest, rsize_t destSize, const char* pSrc);
errno_t strncat_s(char* pDest, rsize_t destSize, const char* pSrc, rsize_t count);
errno_t wcsncpy_s(wchar_t* pDest, rsize_t destSize, const wchar_t* pSrc, rsize_t count);

int vsnprintf_s(char* pBuffer, size_t bufferSize, size_t count, const char* pFormat, va_list args);
int _vsnwprintf_s(wchar_t* pBuffer, size_t bufferSize, size_t count, const wchar_t* pFormat, va_list args);

// For %s, %c and %[ conversions the pointer argument is followed by the buffer size (rsize_t)
int vsscanf_s(const char* pStr, const char* pFormat, va_list args);
int sscanf_s(const char* pStr, const char* pFormat, ...);

errno_t fopen_s(FILE** ppFile, const char* pFilename, const char* pMode);

char* _strlwr(char* pStr);
char* _strupr(char* pStr);

#ifdef __cplusplus
}
#endif

#endif // J3DCORE_J3DCRTCOMPAT_H
