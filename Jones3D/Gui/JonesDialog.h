#ifndef JONES3D_JONESDIALOG_H
#define JONES3D_JONESDIALOG_H
#include <Jones3D/types.h>
#include <Jones3D/RTI/addresses.h>
#include <j3dcore/j3d.h>
#include <rdroid/types.h>
#include <sith/types.h>
#include <std/types.h>

J3D_EXTERN_C_START

#ifdef _WIN32
int J3DAPI JonesDialog_InitCursorTracking(int* pAlreadyInited);
#endif
#ifdef _WIN32
int J3DAPI JonesDialog_TrackCursor(int bReDraw);
#endif
#ifdef _WIN32
int J3DAPI JonesDialog_ResetCursorTracking();
#endif
#ifdef _WIN32
int J3DAPI JonesDialog_CopyRectToDrawBuffer(GDIDIBSectionInfo* pInfo, const PPOINT lpSourceOrigin, LPRECT pDstRect);
#endif
#ifdef _WIN32
int J3DAPI JonesDialog_RestoreBackground(HDC hdc, HWND hwnd, LPPOINT a3, LPRECT a4);
#endif
void J3DAPI JonesDialog_SetGameState(JonesDialogGameState* pState, int bWindowModeSupported);
void J3DAPI JonesDialog_ResetGameState(JonesDialogGameState* pState, int bWindowModeSupported);
#ifdef _WIN32
int J3DAPI JonesDialog_AllocScreenDBISection(int* pbSkippedAllocation);
#endif
#ifdef _WIN32
int J3DAPI JonesDialog_AllocOffScreenGDISection(GDIDIBSectionInfo* pInfo, HDC hdc, int b24bpp, int* pBSkipedAllocation);
#endif
#ifdef _WIN32
void J3DAPI JonesDialog_FreeScreenDIBSection();
#endif
#ifdef _WIN32
int J3DAPI JonesDialog_AllocOffScreenDIBSection(int* pBSkipedAllocation);
#endif
#ifdef _WIN32
int J3DAPI JonesDialog_FreeOffScreenDIBSection();
#endif

// Helper hooking functions
void JonesDialog_InstallHooks(void);

J3D_EXTERN_C_END
#endif // JONES3D_JONESDIALOG_H
