#ifndef SMUSHPLAY_H
#define SMUSHPLAY_H

#include <j3dcore/j3d.h>
#include <std/types.h>
#include <sound/types.h>

J3D_EXTERN_C_START

typedef struct sSmushColorFormat
{
    int redBPP;
    int greenBPP;
    int blueBPP;
    int redPosShift;
    int greenPosShift;
    int bluePosShift;
    int redPosShiftRight;
    int greenPosShiftRight;
    int bluePosShiftRight;
    int alphaBPP;
    int alphaPosShift;
    int alphaPosShiftRight;
} SmushColorFormat;

typedef struct sSmushBitmap
{
    void* pPixels;
    uint32_t width;
    uint32_t height;
    int pixelSize;
    int bpp;
    size_t pitch;
    int unknown6;
    int unknown7;
    tColorMode colorMode;
    SmushColorFormat format;
    int unknown21;
    int unknown22;
} SmushBitmap;


// SMUSH (SANM) movie player.
// Video is decoded with Blocky16 codec ported from ScummVM, audio (VIMA) with AudioLib.

int J3DAPI SmushPlay_SysStartup(HWND hwnd, tSoundDevice pDSound);
void SmushPlay_SysShutdown(void);
void J3DAPI SmushPlay_SetGlobalVolume(size_t volume); // 0 - 127

// Plays movie, blocks until movie ends or pBlit returns non-zero.
// pBlit is called for every frame with decoded RGB565 frame bitmap.
int J3DAPI SmushPlay_PlayMovie(const char* pFilename, int a1, int a3, int a4, int a5, int width, int height, int a8, int a9, int(__cdecl* pBlit)(const SmushBitmap* pBitmap, int a2), int a11, int a12, int a13);

J3D_EXTERN_C_END


#endif // SMUSHPLAY_H