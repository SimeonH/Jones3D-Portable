#ifndef J3DCORE_J3DWIN32COMPAT_H
#define J3DCORE_J3DWIN32COMPAT_H
// Win32 and DirectX type definitions for non-Windows builds.
//
// The shared game code still uses Win32 types in its interfaces (window handles, DirectPlay ids, ...).
// Only types are defined here, the platform code implements the functionality on top of SDL/POSIX.
// DirectX structs embedded in shared game types are reduced to the members the game reads.
#ifdef _WIN32
#error "j3dwin32compat.h is only for non-Windows builds"
#endif

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include "j3dcrtcompat.h" // MSVC CRT extensions and the standard headers windows.h used to pull in

#include <signal.h>
#include <unistd.h>

#define WINAPI
#define CALLBACK
#define PASCAL
#define APIENTRY
#define __stdcall
#define __cdecl

#define MAX_PATH 260

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

// Registry roots, the settings are stored in a file (see wuRegistryFile.c)
#define HKEY_CLASSES_ROOT                ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER                ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE               ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS                       ((HKEY)(uintptr_t)0x80000003)
#define HKEY_PERFORMANCE_DATA            ((HKEY)(uintptr_t)0x80000004)
#define HKEY_CURRENT_CONFIG              ((HKEY)(uintptr_t)0x80000005)
#define HKEY_CURRENT_USER_LOCAL_SETTINGS ((HKEY)(uintptr_t)0x80000007)

// Console text attributes
#define FOREGROUND_BLUE      0x0001
#define FOREGROUND_GREEN     0x0002
#define FOREGROUND_RED       0x0004
#define FOREGROUND_INTENSITY 0x0008

typedef int BOOL;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef int INT;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef float FLOAT;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef int32_t HRESULT;

typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef DWORD* LPDWORD;
typedef WORD* LPWORD;
typedef BYTE* LPBYTE;
typedef BYTE* PBYTE;
typedef LONG* LPLONG;
typedef BOOL* LPBOOL;

// Handles are opaque pointers. HWND holds the SDL_Window* of the game window.
typedef void* HANDLE;
typedef void* HWND;
typedef void* HDC;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HKEY;
typedef void* HFONT;
typedef void* HBITMAP;
typedef void* HGDIOBJ;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HMENU;
typedef void* HBRUSH;
typedef void* HIMAGELIST;

#ifndef TRUE
#   define TRUE 1
#endif
#ifndef FALSE
#   define FALSE 0
#endif

#define S_OK    ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL  ((HRESULT)0x80004005)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) < 0)

#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))

typedef struct _GUID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID;
typedef GUID* LPGUID;
typedef const GUID* REFGUID;

typedef struct tagRECT
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *LPPOINT;

typedef struct tagSIZE
{
    LONG cx;
    LONG cy;
} SIZE, *LPSIZE;

typedef struct _SYSTEMTIME
{
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

typedef struct _FILETIME
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *LPFILETIME;

typedef union _LARGE_INTEGER
{
    struct
    {
        DWORD LowPart;
        LONG HighPart;
    };
    LONGLONG QuadPart;
} LARGE_INTEGER;

typedef struct tagMSG
{
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *LPMSG;

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagMINMAXINFO
{
    POINT ptReserved;
    POINT ptMaxSize;
    POINT ptMaxPosition;
    POINT ptMinTrackSize;
    POINT ptMaxTrackSize;
} MINMAXINFO, *LPMINMAXINFO;

// Window messages, the SDL wkernel translates SDL events to these
#define WM_DESTROY        0x0002
#define WM_SIZE           0x0005
#define WM_ACTIVATE       0x0006
#define WM_SETFOCUS       0x0007
#define WM_KILLFOCUS      0x0008
#define WM_PAINT          0x000F
#define WM_CLOSE          0x0010
#define WM_QUIT           0x0012
#define WM_ACTIVATEAPP    0x001C
#define WM_GETMINMAXINFO  0x0024
#define WM_POWERBROADCAST 0x0218
#define WM_KEYDOWN        0x0100
#define WM_KEYUP          0x0101
#define WM_CHAR           0x0102
#define WM_SYSKEYDOWN     0x0104
#define WM_SYSKEYUP       0x0105
#define WM_SYSCOMMAND     0x0112
#define WM_TIMER          0x0113
#define WM_MOUSEMOVE      0x0200
#define WM_LBUTTONDOWN    0x0201
#define WM_LBUTTONUP      0x0202
#define WM_RBUTTONDOWN    0x0204
#define WM_RBUTTONUP      0x0205
#define WM_USER           0x0400

#define WA_INACTIVE    0
#define WA_ACTIVE      1
#define WA_CLICKACTIVE 2

#define SC_KEYMENU    0xF100
#define SC_SCREENSAVE 0xF140

#define PBT_APMQUERYSUSPEND    0x0000
#define PBT_APMSUSPEND         0x0004
#define PBT_APMRESUMECRITICAL  0x0006
#define PBT_APMRESUMESUSPEND   0x0007

#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002

// Window styles, see wkernel_SetWindowStyle
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_VISIBLE          0x10000000L
#define WS_CAPTION          0x00C00000L
#define WS_BORDER           0x00800000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

#define SW_HIDE 0
#define SW_SHOW 5

#define MB_OK              0x00000000L
#define MB_ICONERROR       0x00000010L
#define MB_ICONWARNING     0x00000030L
#define MB_ICONINFORMATION 0x00000040L

// Virtual key codes (wParam of WM_KEYDOWN), letters and digits are their ASCII codes
#define VK_BACK     0x08
#define VK_TAB      0x09
#define VK_RETURN   0x0D
#define VK_SHIFT    0x10
#define VK_CONTROL  0x11
#define VK_MENU     0x12
#define VK_PAUSE    0x13
#define VK_CAPITAL  0x14
#define VK_ESCAPE   0x1B
#define VK_SPACE    0x20
#define VK_PRIOR    0x21
#define VK_NEXT     0x22
#define VK_END      0x23
#define VK_HOME     0x24
#define VK_LEFT     0x25
#define VK_UP       0x26
#define VK_RIGHT    0x27
#define VK_DOWN     0x28
#define VK_SNAPSHOT 0x2C
#define VK_INSERT   0x2D
#define VK_DELETE   0x2E
#define VK_F1       0x70
#define VK_F2       0x71
#define VK_F3       0x72
#define VK_F4       0x73
#define VK_F5       0x74
#define VK_F6       0x75
#define VK_F7       0x76
#define VK_F8       0x77
#define VK_F9       0x78
#define VK_F10      0x79
#define VK_F11      0x7A
#define VK_F12      0x7B

// Win32 file functions used by the game code, implemented on POSIX with game path translation (Libs/std/SDL/stdWin32File.c)
typedef struct _SECURITY_ATTRIBUTES
{
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_NORMAL    0x00000080

typedef struct _WIN32_FIND_DATAA
{
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, WIN32_FIND_DATA, *LPWIN32_FIND_DATAA;

DWORD SearchPathA(LPCSTR lpPath, LPCSTR lpFileName, LPCSTR lpExtension, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart);
DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
BOOL SetCurrentDirectoryA(LPCSTR lpPathName);
BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
BOOL FindClose(HANDLE hFindFile);

#define SearchPath          SearchPathA
#define GetCurrentDirectory GetCurrentDirectoryA
#define SetCurrentDirectory SetCurrentDirectoryA
#define CreateDirectory     CreateDirectoryA
#define FindFirstFile       FindFirstFileA
#define FindNextFile        FindNextFileA

#define KF_EXTENDED 0x0100
#define KF_ALTDOWN  0x2000
#define KF_REPEAT   0x4000
#define KF_UP       0x8000

static inline void DebugBreak(void)
{
    raise(SIGTRAP);
}

static inline void OutputDebugStringA(LPCSTR pText)
{
    fputs(pText, stderr);
}
#define OutputDebugString OutputDebugStringA

static inline BOOL GetComputerNameA(LPSTR pBuffer, LPDWORD pSize)
{
    if ( gethostname(pBuffer, *pSize) != 0 ) {
        return FALSE;
    }
    pBuffer[*pSize - 1] = 0;
    *pSize = (DWORD)strlen(pBuffer);
    return TRUE;
}
#define GetComputerName GetComputerNameA

static inline void Sleep(DWORD msec)
{
    struct timespec ts = { (time_t)(msec / 1000), (long)(msec % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static inline void GetLocalTime(SYSTEMTIME* pTime)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm t;
    localtime_r(&ts.tv_sec, &t);
    pTime->wYear         = (WORD)(t.tm_year + 1900);
    pTime->wMonth        = (WORD)(t.tm_mon + 1);
    pTime->wDayOfWeek    = (WORD)t.tm_wday;
    pTime->wDay          = (WORD)t.tm_mday;
    pTime->wHour         = (WORD)t.tm_hour;
    pTime->wMinute       = (WORD)t.tm_min;
    pTime->wSecond       = (WORD)t.tm_sec;
    pTime->wMilliseconds = (WORD)(ts.tv_nsec / 1000000);
}

// BMP file format

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD
{
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
} BITMAPINFO;

#define BI_RGB       0
#define BI_BITFIELDS 3

// DirectX

typedef DWORD D3DCOLOR;

// d3dtypes.h color macros
#define RGBA_MAKE(r, g, b, a) ((D3DCOLOR)(((DWORD)(a) << 24) | ((DWORD)(r) << 16) | ((DWORD)(g) << 8) | (DWORD)(b)))
#define D3DRGB(r, g, b) (0xff000000u | ((DWORD)((r) * 255) << 16) | ((DWORD)((g) * 255) << 8) | (DWORD)((b) * 255))
#define D3DRGBA(r, g, b, a) (((DWORD)((a) * 255) << 24) | ((DWORD)((r) * 255) << 16) | ((DWORD)((g) * 255) << 8) | (DWORD)((b) * 255))

typedef DWORD DPID, *LPDPID; // DirectPlay player id
#define DPID_ALLPLAYERS   0
#define DPSEND_GUARANTEED 0x00000001
#define DPERR_GENERIC     E_FAIL

#include "j3ddikeys.h"

typedef struct _DDCOLORKEY
{
    DWORD dwColorSpaceLowValue;
    DWORD dwColorSpaceHighValue;
} DDCOLORKEY, *LPDDCOLORKEY;

typedef struct _DDPIXELFORMAT
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwRGBAlphaBitMask;
} DDPIXELFORMAT, *LPDDPIXELFORMAT;

// Only the D3D device description members the game reads
typedef struct _D3DDeviceDesc
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwDeviceRenderBitDepth;
    DWORD dwDeviceZBufferBitDepth;
    DWORD dwMaxVertexCount;
} D3DDEVICEDESC, *LPD3DDEVICEDESC;

#define D3DDD_DEVICERENDERBITDEPTH  0x00000080L
#define D3DDD_DEVICEZBUFFERBITDEPTH 0x00000100L

#define DDBD_1  0x00004000L
#define DDBD_2  0x00002000L
#define DDBD_4  0x00001000L
#define DDBD_8  0x00000800L
#define DDBD_16 0x00000400L
#define DDBD_24 0x00000200L
#define DDBD_32 0x00000100L

// Only the DirectDraw caps members the game reads
typedef struct _DDCAPS_DX6
{
    DWORD dwSize;
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwVidMemTotal;
    DWORD dwVidMemFree;
} DDCAPS_DX6, DDCAPS;

#define DDCAPS2_CANRENDERWINDOWED 0x00080000L

#endif // J3DCORE_J3DWIN32COMPAT_H
