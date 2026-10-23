#ifndef WKERNEL_WKERNEL_H
#define WKERNEL_WKERNEL_H
#include <j3dcore/j3d.h>
#include <wkernel/types.h>
#include <wkernel/RTI/addresses.h>

// TODO: move module to std

J3D_EXTERN_C_START

int J3DAPI wkernel_Run(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd, LPCSTR lpWindowName); // Note, process proc must be set before
WKERNELPROC J3DAPI wkernel_SetProcessProc(WKERNELPROC pfProc);
WKERNELSTARTUPPROC J3DAPI wkernel_SetStartupCallback(WKERNELSTARTUPPROC pfOnStartup);
WKERNELSHUTDOWNPROC J3DAPI wkernel_SetShutdownCallback(WKERNELSHUTDOWNPROC pfOnShutdown);
void J3DAPI wkernel_SetWindowStyle(LONG dwNewLong);

BOOL J3DAPI wkernel_SetWindowSize(int width, int height);
void J3DAPI wkernel_SetWindowProc(WKERNELWNDPROC pfProc); // Window process
int wkernel_PeekProcessEvents(void);
int wkernel_ProcessEvents(void);

// Added: Module which holds the game resources (icons), on Windows Indy3D.exe loaded as data file
void J3DAPI wkernel_SetResourceInstance(HINSTANCE hInstance);
HINSTANCE wkernel_GetResourceInstance(void);

// Added: Win32 window functions for the game window, which is an SDL window (HWND is its SDL_Window*).
// Messages are delivered to the window procedure set with wkernel_SetWindowProc.
UINT_PTR wkernel_SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse);
BOOL wkernel_KillTimer(HWND hWnd, UINT_PTR uIDEvent);
BOOL wkernel_PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT wkernel_DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL wkernel_ShowWindow(HWND hWnd, int nCmdShow);

// Helper hooking functions
void wkernel_InstallHooks(void);

J3D_EXTERN_C_END
#endif // WKERNEL_WKERNEL_H
