// Microsoft CRT extensions for non-Windows builds, see j3dcrtcompat.h
#ifndef _WIN32
#include "j3dcrtcompat.h"

#include <errno.h>
#include <stdbool.h>

#ifndef STRUNCATE
#define STRUNCATE 80 // MSVC value
#endif

// MSVC CRT linear congruential generator
static unsigned int j3d_randSeed = 1;

int j3d_rand(void)
{
    j3d_randSeed = j3d_randSeed * 214013u + 2531011u;
    return (int)((j3d_randSeed >> 16) & 0x7FFF);
}

void j3d_srand(unsigned int seed)
{
    j3d_randSeed = seed;
}

size_t strnlen_s(const char* str, size_t maxSize)
{
    return str ? strnlen(str, maxSize) : 0;
}

errno_t strcpy_s(char* pDest, rsize_t destSize, const char* pSrc)
{
    return strncpy_s(pDest, destSize, pSrc, _TRUNCATE);
}

// Copies at most count chars and null-terminates. Fails with an empty pDest if it doesn't fit, unless count is _TRUNCATE.
errno_t strncpy_s(char* pDest, rsize_t destSize, const char* pSrc, rsize_t count)
{
    if ( !pDest || destSize == 0 ) {
        return EINVAL;
    }

    if ( !pSrc )
    {
        pDest[0] = 0;
        return EINVAL;
    }

    size_t len = strnlen(pSrc, count == _TRUNCATE ? destSize : count);
    if ( len >= destSize )
    {
        if ( count != _TRUNCATE )
        {
            pDest[0] = 0;
            return ERANGE;
        }

        len = destSize - 1;
        memcpy(pDest, pSrc, len);
        pDest[len] = 0;
        return STRUNCATE;
    }

    memcpy(pDest, pSrc, len);
    pDest[len] = 0;
    return 0;
}

errno_t strcat_s(char* pDest, rsize_t destSize, const char* pSrc)
{
    return strncat_s(pDest, destSize, pSrc, _TRUNCATE);
}

errno_t strncat_s(char* pDest, rsize_t destSize, const char* pSrc, rsize_t count)
{
    if ( !pDest || destSize == 0 ) {
        return EINVAL;
    }

    size_t dstLen = strnlen(pDest, destSize);
    if ( dstLen == destSize )
    {
        pDest[0] = 0; // not terminated
        return EINVAL;
    }

    errno_t err = strncpy_s(pDest + dstLen, destSize - dstLen, pSrc, count);
    if ( err != 0 && err != STRUNCATE ) {
        pDest[0] = 0;
    }

    return err;
}

errno_t wcsncpy_s(wchar_t* pDest, rsize_t destSize, const wchar_t* pSrc, rsize_t count)
{
    if ( !pDest || destSize == 0 ) {
        return EINVAL;
    }

    if ( !pSrc )
    {
        pDest[0] = 0;
        return EINVAL;
    }

    size_t len = wcsnlen(pSrc, count == _TRUNCATE ? destSize : count);
    if ( len >= destSize )
    {
        if ( count != _TRUNCATE )
        {
            pDest[0] = 0;
            return ERANGE;
        }

        len = destSize - 1;
        wmemcpy(pDest, pSrc, len);
        pDest[len] = 0;
        return STRUNCATE;
    }

    wmemcpy(pDest, pSrc, len);
    pDest[len] = 0;
    return 0;
}

// Returns -1 on truncation like MSVC
int vsnprintf_s(char* pBuffer, size_t bufferSize, size_t count, const char* pFormat, va_list args)
{
    if ( !pBuffer || bufferSize == 0 ) {
        return -1;
    }

    size_t maxLen = count == _TRUNCATE || count >= bufferSize ? bufferSize - 1 : count;
    int len = vsnprintf(pBuffer, maxLen + 1, pFormat, args);
    if ( len < 0 || (size_t)len > maxLen )
    {
        pBuffer[maxLen] = 0;
        return -1;
    }

    return len;
}

int _vsnwprintf_s(wchar_t* pBuffer, size_t bufferSize, size_t count, const wchar_t* pFormat, va_list args)
{
    if ( !pBuffer || bufferSize == 0 ) {
        return -1;
    }

    size_t maxLen = count == _TRUNCATE || count >= bufferSize ? bufferSize - 1 : count;
    int len = vswprintf(pBuffer, maxLen + 1, pFormat, args);
    if ( len < 0 )
    {
        pBuffer[maxLen] = 0;
        return -1;
    }

    return len;
}

// vsscanf with MSVC's buffer size arguments: each conversion is scanned separately with sscanf,
// %s, %c and %[ consume the extra size argument and are limited to it.
int vsscanf_s(const char* pStr, const char* pFormat, va_list args)
{
    int numAssigned = 0;
    const char* pIn = pStr;
    const char* pFmt = pFormat;

    while ( *pFmt )
    {
        // Literal text and whitespace up to the next conversion
        if ( *pFmt != '%' || pFmt[1] == '%' )
        {
            if ( isspace((unsigned char)*pFmt) )
            {
                while ( isspace((unsigned char)*pIn) ) {
                    ++pIn;
                }
                ++pFmt;
                continue;
            }

            char c = *pFmt;
            pFmt += (c == '%') ? 2 : 1;
            if ( *pIn != c ) {
                break;
            }
            ++pIn;
            continue;
        }

        // Parse the conversion spec: %[*][width][length]conv
        char aSpec[64];
        size_t specLen = 0;
        aSpec[specLen++] = *pFmt++; // '%'

        bool bSuppress = false;
        if ( *pFmt == '*' )
        {
            bSuppress = true;
            aSpec[specLen++] = *pFmt++;
        }

        size_t width = 0;
        while ( isdigit((unsigned char)*pFmt) ) {
            width = width * 10 + (size_t)(*pFmt++ - '0');
        }

        char aLength[3] = { 0 };
        size_t lenLen = 0;
        while ( strchr("hlLjztq", *pFmt) && *pFmt && lenLen < 2 ) {
            aLength[lenLen++] = *pFmt++;
        }

        char conv = *pFmt;
        if ( !conv ) {
            break;
        }

        // Scan set
        char aSet[256] = { 0 };
        if ( conv == '[' )
        {
            size_t setLen = 0;
            aSet[setLen++] = *pFmt++;
            if ( *pFmt == '^' ) {
                aSet[setLen++] = *pFmt++;
            }
            if ( *pFmt == ']' ) {
                aSet[setLen++] = *pFmt++;
            }
            while ( *pFmt && *pFmt != ']' && setLen < sizeof(aSet) - 2 ) {
                aSet[setLen++] = *pFmt++;
            }
            if ( *pFmt == ']' ) {
                aSet[setLen++] = *pFmt++;
            }
        }
        else
        {
            ++pFmt;
        }

        void* pArg = NULL;
        if ( !bSuppress && conv != 'n' ) {
            pArg = va_arg(args, void*);
        }

        // String conversions take the buffer size and are limited to it
        if ( !bSuppress && (conv == 's' || conv == 'c' || conv == '[') )
        {
            size_t bufSize = va_arg(args, rsize_t);
            size_t maxWidth = conv == 'c' ? bufSize : (bufSize ? bufSize - 1 : 0);
            if ( conv == 'c' && width == 0 ) {
                width = 1;
            }
            if ( width == 0 || width > maxWidth ) {
                width = maxWidth;
            }
            if ( width == 0 ) {
                break;
            }
        }

        if ( width ) {
            specLen += (size_t)snprintf(&aSpec[specLen], sizeof(aSpec) - specLen, "%zu", width);
        }
        specLen += (size_t)snprintf(&aSpec[specLen], sizeof(aSpec) - specLen, "%s", aLength);
        if ( conv == '[' ) {
            specLen += (size_t)snprintf(&aSpec[specLen], sizeof(aSpec) - specLen, "%s", aSet);
        }
        else {
            aSpec[specLen++] = conv;
        }
        snprintf(&aSpec[specLen], sizeof(aSpec) - specLen, "%%n");

        if ( conv == 'n' )
        {
            if ( !bSuppress ) {
                *va_arg(args, int*) = (int)(pIn - pStr);
            }
            continue;
        }

        // Input failure before the first conversion is EOF
        if ( *pIn == 0 && conv != 'c' ) {
            return numAssigned == 0 ? EOF : numAssigned;
        }

        int consumed = -1;
        int res = bSuppress ? sscanf(pIn, aSpec, &consumed) : sscanf(pIn, aSpec, pArg, &consumed);
        if ( consumed < 0 || (!bSuppress && res != 1) ) {
            break;
        }

        pIn += consumed;
        if ( !bSuppress ) {
            ++numAssigned;
        }
    }

    return numAssigned;
}

int sscanf_s(const char* pStr, const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    int res = vsscanf_s(pStr, pFormat, args);
    va_end(args);
    return res;
}

errno_t fopen_s(FILE** ppFile, const char* pFilename, const char* pMode)
{
    if ( !ppFile ) {
        return EINVAL;
    }

    *ppFile = fopen(pFilename, pMode);
    return *ppFile ? 0 : errno;
}

char* _strlwr(char* pStr)
{
    for ( char* p = pStr; p && *p; ++p ) {
        *p = (char)tolower((unsigned char)*p);
    }
    return pStr;
}

char* _strupr(char* pStr)
{
    for ( char* p = pStr; p && *p; ++p ) {
        *p = (char)toupper((unsigned char)*p);
    }
    return pStr;
}

#endif // !_WIN32
