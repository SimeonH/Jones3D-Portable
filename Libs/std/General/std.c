#include <stdio.h>

#include "std.h"
#include "stdMemory.h"
#include "stdPlatform.h"
#include "stdUtil.h"

#include <j3dcore/j3dhook.h>
#include <std/RTI/symbols.h>
#include <std/Win95/stdConsole.h>

static char aPrintBuffer[2048]     = { 0 };
static char aFilePrintBuffer[2048] = { 0 };
static bool bStdStartup = false;

static int16_t word_183962C;
static int16_t word_1839630;
static int16_t word_183962E;

void std_InstallHooks(void)
{
    J3D_HOOKFUNC(stdStartup);
    J3D_HOOKFUNC(stdShutdown);
    J3D_HOOKFUNC(stdPrintf);
    J3D_HOOKFUNC(stdConsolePrintf);
    J3D_HOOKFUNC(stdFileFromPath);
    J3D_HOOKFUNC(stdCalcBitPos);
    J3D_HOOKFUNC(stdFileOpen);
    J3D_HOOKFUNC(stdFileClose);
    J3D_HOOKFUNC(stdFileRead);
    J3D_HOOKFUNC(stdFileWrite);
    J3D_HOOKFUNC(stdFileGets);
    J3D_HOOKFUNC(stdFileGetws);
    J3D_HOOKFUNC(stdFileEof);
    J3D_HOOKFUNC(stdFileTell);
    J3D_HOOKFUNC(stdFileSize);
    J3D_HOOKFUNC(stdFilePrintf);
    J3D_HOOKFUNC(stdFileSeek);

#ifdef _WIN32
    // Hook on stdio_open, stdio_close & stdio_flush due to stdio_services using stdFile* functions which might have conflicting FILE* object
    // Note on debug version this should be skipped
    J3DHookFunction(stdio_open_ADDR, (void*)stdFileOpen);
    J3DHookFunction(stdio_close_ADDR, (void*)stdFileClose);
    J3DHookFunction(stdio_flush_ADDR, (void*)stdFileFlush);
#endif
}

const char std_g_aEmptyString[8];
char std_g_genBuffer[2048];
tHostServices* std_g_pHS;

void J3DAPI stdStartup(tHostServices* pHS)
{
    int16_t v1;

    std_g_pHS = pHS; // TODO: Should probably be moved within if scope

    if ( !bStdStartup )
    {
        // TODO: Should probably do success check
        stdMemory_Startup();
        stdMemory_Open();

        // TODO: Remove when all round and rand float functions are replaced && updated
        // TODO: consider setting FE_DOWNWARD for round functions (lrintf); i.e.: fesetround(FE_DOWNWARD) to mimic same behavior as OG
        short  tmp = 0;
#ifdef _MSC_VER
        __asm  fnstcw word ptr[tmp];
#else
        __asm__ volatile("fnstcw %0" : "=m"(tmp));
#endif
        word_183962C = tmp;

        // The following flags control in binary (Indy3D.exe) FPU rounding control flags for frndint (floorf, ceilf) and fistp (lrintf)
        // and should not have any effect in the reimplementation since these vars aren't used.
        v1 = (0xB00 | word_183962C & 0xFF); // round up CF for in binary ceilf function
        word_1839630 = v1;
        v1 = (0x700 | word_183962C & 0xFF); // Round down CF for in binary floorf & lrintf
        word_183962E = v1;
        bStdStartup = true;
    }
}

void stdShutdown(void)
{
    if ( bStdStartup )
    {
        stdMemory_Close();
        stdMemory_Shutdown();
        bStdStartup = false;
    }
}

void J3DAPI stdPrintf(tPrintfFunc pfPrint, const char* pFilePath, unsigned int linenum, const char* format, ...)
{
    if ( pfPrint )
    {
        va_list args;
        va_start(args, format);

        size_t fnIdx = 0;
        bool bFnFound = false;
        for ( size_t i = 0; pFilePath[i]; ++i )
        {
            if ( pFilePath[i] == '\\' )
            {
                bFnFound = true;
                fnIdx = i;
            }
        }

        if ( bFnFound )
        {
            ++fnIdx;
        }

        // Format string: <filename>(linenum): format
        int fnSize = _snprintf(aPrintBuffer, sizeof(aPrintBuffer), &pFilePath[fnIdx]);
        size_t remain = sizeof(aPrintBuffer) - fnSize;
        int nWritten = _snprintf(&aPrintBuffer[fnSize], remain, "(%d): ", linenum);
        vsnprintf_s(&aPrintBuffer[nWritten + fnSize], STD_ARRAYLEN(aPrintBuffer) - (nWritten + fnSize), remain - nWritten, format, args);
        va_end(args);

        // Write to output function
        pfPrint(aPrintBuffer);
    }
}

int stdConsolePrintf(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    vsnprintf_s(std_g_genBuffer, STD_ARRAYLEN(std_g_genBuffer), STD_ARRAYLEN(std_g_genBuffer) - 1, pFormat, args);
    va_end(args);

    stdConsole_WriteConsole(std_g_genBuffer, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    return sizeof(std_g_genBuffer);
}

const char* J3DAPI stdFileFromPath(const char* pPath)
{
    char* pFilename = strrchr(pPath, '\\');
    if ( pFilename )
    {
        return pFilename + 1;
    }

    return pPath;
}

int J3DAPI stdCalcBitPos(int bit)
{
    int pos = 0;
    while ( bit > 1 )
    {
        bit >>= 1;
        pos++;
    }

    return pos;
}

// TODO: Functions below might all needed to be move to stdPlatform

tFileHandle J3DAPI stdFileOpen(const char* pFilename, const char* mode)
{
#ifndef _WIN32
    char aPath[1024];
    pFilename = stdPlatform_NativePath(pFilename, aPath, sizeof(aPath));
#endif

    FILE* fh;
    if ( fopen_s(&fh, pFilename, mode) != 0 ) {
        fh = NULL; // Make sure it's null
    }
    return (tFileHandle)fh;
}

int J3DAPI stdFileClose(tFileHandle fh)
{
    fclose((FILE*)fh);
    return 0;
}

size_t J3DAPI stdFileRead(tFileHandle fh, void* pOutData, size_t size)
{
    return fread(pOutData, 1u, size, (FILE*)fh);
}

size_t J3DAPI stdFileWrite(tFileHandle fh, const void* pData, size_t size)
{
    return fwrite(pData, 1u, size, (FILE*)fh);
}

int J3DAPI stdFileFlush(tFileHandle fh)
{
    return fflush((FILE*)fh);
}

char* J3DAPI stdFileGets(tFileHandle fh, char* pStr, size_t size)
{
    return fgets(pStr, size, (FILE*)fh);
}

#ifndef _WIN32
// Windows-1252 code points of bytes 0x80 - 0x9F, the rest maps to the same Unicode code point
static const uint16_t std_aCp1252High[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};
#endif

wchar_t* J3DAPI stdFileGetws(tFileHandle fh, wchar_t* pOutStr, size_t size)
{
#ifdef _WIN32
    return fgetws(pOutStr, size, (FILE*)fh);
#else
    // Like MSVC's text mode fgetws on an ANSI file: bytes are widened with the Windows-1252 code page
    if ( size == 0 ) {
        return NULL;
    }

    size_t len = 0;
    int c = 0;
    while ( len < size - 1 && (c = fgetc((FILE*)fh)) != EOF )
    {
        if ( c == '\r' ) {
            continue; // text mode
        }

        pOutStr[len++] = (c >= 0x80 && c < 0xA0) ? (wchar_t)std_aCp1252High[c - 0x80] : (wchar_t)c;
        if ( c == '\n' ) {
            break;
        }
    }

    if ( len == 0 && c == EOF ) {
        return NULL;
    }

    pOutStr[len] = 0;
    return pOutStr;
#endif
}

int J3DAPI stdFileEof(tFileHandle fh)
{
    return feof((FILE*)fh);
}

int J3DAPI stdFileTell(tFileHandle fh)
{
    return ftell((FILE*)fh);
}

size_t J3DAPI stdFileSize(const char* pFilePath)
{
    tFileHandle fh = stdFileOpen(pFilePath, "rb");
    if ( !fh ) {
        return 0;
    }

    stdFileSeek(fh, 0, 2);
    int size = stdFileTell(fh);
    stdFileClose(fh);
    return size;
}

int stdFilePrintf(tFileHandle fh, const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    int size = vsnprintf_s(aFilePrintBuffer, sizeof(aFilePrintBuffer), sizeof(aFilePrintBuffer) - 1, pFormat, args);
    va_end(args);

    stdFileWrite(fh, aFilePrintBuffer, size);
    return 0;
}

int J3DAPI stdFileSeek(tFileHandle fh, int offset, int origin)
{
    return fseek((FILE*)fh, offset, origin);
}
