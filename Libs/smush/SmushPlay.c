#include "SmushPlay.h"
#include "codecs/Blocky16Api.h"

#include <sound/AudioLib.h>
#include <sound/Driver.h>
#include <std/General/stdPlatform.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// SMUSH (SANM) movie file layout:
//   'SANM' size
//     'SHDR' size: version (u16), numFrames (u32), unknown (u16), width (u16), height (u16), unknown (u16), usecPerFrame (u32), flags (u16), ...
//     'FLHD' size: 'Wave' sub-header (freq, channels) and 'Bl16' sub-headers
//     'FRME' size: 'Bl16' video and 'Wave' VIMA compressed audio chunks
// Note, all tags and sizes are big endian.

#define SMUSHPLAY_TAG(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define SMUSHPLAY_DEFAULT_USEC_PER_FRAME 66667 // 15 fps

static tSoundDevice SmushPlay_pDSound;
static size_t SmushPlay_volume = 127;

static uint32_t SmushPlay_ReadBE32(const uint8_t* p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

static uint32_t SmushPlay_ReadLE32(const uint8_t* p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t SmushPlay_ReadLE16(const uint8_t* p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

int J3DAPI SmushPlay_SysStartup(HWND hwnd, tSoundDevice pDSound)
{
    (void)hwnd;
    SmushPlay_pDSound = pDSound;
    return 1;
}

void SmushPlay_SysShutdown(void)
{
    SmushPlay_pDSound = NULL;
}

void J3DAPI SmushPlay_SetGlobalVolume(size_t volume)
{
    SmushPlay_volume = volume > 127 ? 127 : volume;
}

static uint8_t* SmushPlay_LoadFile(const char* pFilename, size_t* pSize)
{
    // Game paths use backslashes and are case-insensitive
    char aPath[1024];
    stdPlatform_NativePath(pFilename, aPath, sizeof(aPath));

    FILE* pFile = NULL;
    if ( fopen_s(&pFile, aPath, "rb") != 0 || !pFile )
    {
        return NULL;
    }

    fseek(pFile, 0, SEEK_END);
    long size = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    uint8_t* pData = NULL;
    if ( size > 0 && (pData = (uint8_t*)malloc(size)) != NULL )
    {
        if ( fread(pData, 1, size, pFile) != (size_t)size )
        {
            free(pData);
            pData = NULL;
        }
    }

    fclose(pFile);
    *pSize = (size_t)size;
    return pData;
}

// Finds next sub-chunk in chunk data, returns false when no more chunks
static bool SmushPlay_NextChunk(const uint8_t** ppCur, const uint8_t* pEnd, uint32_t* pTag, uint32_t* pSize, const uint8_t** ppData)
{
    const uint8_t* pCur = *ppCur;
    if ( pCur + 8 > pEnd )
    {
        return false;
    }

    *pTag  = SmushPlay_ReadBE32(pCur);
    *pSize = SmushPlay_ReadBE32(pCur + 4);
    *ppData = pCur + 8;
    if ( *ppData + *pSize > pEnd )
    {
        return false;
    }

    *ppCur = *ppData + *pSize + (*pSize & 1); // Chunks are padded to even size
    return true;
}

// Returns pointer to VIMA data and number of decompressed samples (per channel) of Wave chunk
static const uint8_t* SmushPlay_GetWaveData(const uint8_t* pData, uint32_t size, uint32_t* pNumSamples)
{
    if ( size < 12 )
    {
        return NULL;
    }

    int32_t numSamples = (int32_t)SmushPlay_ReadBE32(pData);
    if ( numSamples < 0 )
    {
        // Extended header: -1, unknown, numSamples
        *pNumSamples = SmushPlay_ReadBE32(pData + 8);
        return pData + 12;
    }

    *pNumSamples = (uint32_t)numSamples;
    return pData + 4;
}

// Decodes whole audio track of the movie
static int16_t* SmushPlay_DecodeAudio(const uint8_t* pFrames, const uint8_t* pEnd, unsigned int numChannels, size_t* pNumSamples)
{
    // Count samples
    size_t totalSamples = 0;
    const uint8_t* pCur = pFrames;
    uint32_t tag, size;
    const uint8_t* pChunk;
    while ( SmushPlay_NextChunk(&pCur, pEnd, &tag, &size, &pChunk) )
    {
        if ( tag != SMUSHPLAY_TAG('F', 'R', 'M', 'E') )
        {
            continue;
        }

        const uint8_t* pSub = pChunk;
        uint32_t subTag, subSize;
        const uint8_t* pSubData;
        while ( SmushPlay_NextChunk(&pSub, pChunk + size, &subTag, &subSize, &pSubData) )
        {
            uint32_t numSamples;
            if ( subTag == SMUSHPLAY_TAG('W', 'a', 'v', 'e') && SmushPlay_GetWaveData(pSubData, subSize, &numSamples) )
            {
                totalSamples += numSamples;
            }
        }
    }

    if ( !totalSamples )
    {
        return NULL;
    }

    // Some extra space as the decoder writes in whole frames
    int16_t* aSamples = (int16_t*)calloc(totalSamples * numChannels + 64, sizeof(int16_t));
    if ( !aSamples )
    {
        return NULL;
    }

    // Decode
    size_t curSample = 0;
    pCur = pFrames;
    while ( SmushPlay_NextChunk(&pCur, pEnd, &tag, &size, &pChunk) )
    {
        if ( tag != SMUSHPLAY_TAG('F', 'R', 'M', 'E') )
        {
            continue;
        }

        const uint8_t* pSub = pChunk;
        uint32_t subTag, subSize;
        const uint8_t* pSubData;
        while ( SmushPlay_NextChunk(&pSub, pChunk + size, &subTag, &subSize, &pSubData) )
        {
            uint32_t numSamples;
            const uint8_t* pVima;
            if ( subTag == SMUSHPLAY_TAG('W', 'a', 'v', 'e') && (pVima = SmushPlay_GetWaveData(pSubData, subSize, &numSamples)) != NULL )
            {
                // VIMA is the same ADPCM format as AudioLib compressed data
                tAudioCompressorState state = { 0 };
                AudioLib_Uncompress(&state, (uint8_t*)&aSamples[curSample * numChannels], pVima, numSamples * numChannels * sizeof(int16_t));
                curSample += numSamples;
            }
        }
    }

    *pNumSamples = totalSamples;
    return aSamples;
}

// Movie audio: a DirectSound buffer on Windows, a sound driver (OpenAL) buffer elsewhere

#ifdef _WIN32

typedef LPDIRECTSOUNDBUFFER SmushAudioBuffer;

static SmushAudioBuffer SmushPlay_CreateAudioBuffer(const int16_t* aSamples, size_t numSamples, unsigned int freq, unsigned int numChannels)
{
    if ( !SmushPlay_pDSound || !aSamples || !numSamples )
    {
        return NULL;
    }

    WAVEFORMATEX wavefmt = { 0 };
    wavefmt.wFormatTag      = WAVE_FORMAT_PCM;
    wavefmt.nChannels       = (WORD)numChannels;
    wavefmt.nSamplesPerSec  = freq;
    wavefmt.wBitsPerSample  = 16;
    wavefmt.nBlockAlign     = (WORD)(numChannels * sizeof(int16_t));
    wavefmt.nAvgBytesPerSec = freq * wavefmt.nBlockAlign;

    DWORD dataSize = (DWORD)(numSamples * wavefmt.nBlockAlign);

    DSBUFFERDESC dsbufd = { 0 };
    dsbufd.dwSize        = sizeof(DSBUFFERDESC);
    dsbufd.dwFlags       = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
    dsbufd.dwBufferBytes = dataSize;
    dsbufd.lpwfxFormat   = &wavefmt;

    LPDIRECTSOUNDBUFFER pBuffer = NULL;
    if ( IDirectSound_CreateSoundBuffer(SmushPlay_pDSound, &dsbufd, &pBuffer, NULL) != DS_OK || !pBuffer )
    {
        return NULL;
    }

    LPVOID pAudio1, pAudio2;
    DWORD audioBytes1, audioBytes2;
    if ( IDirectSoundBuffer_Lock(pBuffer, 0, dataSize, &pAudio1, &audioBytes1, &pAudio2, &audioBytes2, 0) != DS_OK )
    {
        IDirectSoundBuffer_Release(pBuffer);
        return NULL;
    }

    memcpy(pAudio1, aSamples, audioBytes1);
    if ( pAudio2 )
    {
        memcpy(pAudio2, (const uint8_t*)aSamples + audioBytes1, audioBytes2);
    }

    IDirectSoundBuffer_Unlock(pBuffer, pAudio1, audioBytes1, pAudio2, audioBytes2);

    // Volume 0 - 127 to hundredths of decibel attenuation
    LONG dsVolume = SmushPlay_volume ? (LONG)(2000.0 * log10((double)SmushPlay_volume / 127.0)) : DSBVOLUME_MIN;
    IDirectSoundBuffer_SetVolume(pBuffer, dsVolume);
    return pBuffer;
}

static void SmushPlay_PlayAudio(SmushAudioBuffer pBuffer)
{
    IDirectSoundBuffer_Play(pBuffer, 0, 0, 0);
}

// Returns the play position in bytes or -1 when not playing
static int64_t SmushPlay_GetAudioPosition(SmushAudioBuffer pBuffer)
{
    DWORD playPos = 0, writePos = 0, status = 0;
    if ( IDirectSoundBuffer_GetStatus(pBuffer, &status) == DS_OK && (status & DSBSTATUS_PLAYING) != 0
        && IDirectSoundBuffer_GetCurrentPosition(pBuffer, &playPos, &writePos) == DS_OK )
    {
        return playPos;
    }

    return -1;
}

static void SmushPlay_ReleaseAudioBuffer(SmushAudioBuffer pBuffer)
{
    IDirectSoundBuffer_Stop(pBuffer);
    IDirectSoundBuffer_Release(pBuffer);
}

static double SmushPlay_GetTime(void)
{
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

static void SmushPlay_Sleep(void)
{
    Sleep(1);
}

#else // !_WIN32

typedef struct
{
    tSoundBuffer pBuffer;
    int16_t* aSamples;
    size_t dataSize;
} SmushAudio;
typedef SmushAudio* SmushAudioBuffer;

static SmushAudioBuffer SmushPlay_CreateAudioBuffer(const int16_t* aSamples, size_t numSamples, unsigned int freq, unsigned int numChannels)
{
    if ( !aSamples || !numSamples )
    {
        return NULL;
    }

    SmushAudio* pAudio = (SmushAudio*)calloc(1, sizeof(SmushAudio));
    if ( !pAudio )
    {
        return NULL;
    }

    pAudio->dataSize = numSamples * numChannels * sizeof(int16_t);

    // Created when played, the driver starts the buffer on creation
    tSoundChannelFlag flags = SOUND_CHANNEL_GLOBALFOCUS | SOUND_CHANNEL_USE3DCAPS;
    pAudio->pBuffer = SoundDriver_CreateAndPlay(freq, 16, numChannels, (const uint8_t*)aSamples, pAudio->dataSize, &flags, NULL);
    if ( !pAudio->pBuffer )
    {
        free(pAudio);
        return NULL;
    }

    SoundDriver_Stop(pAudio->pBuffer);
    SoundDriver_SetVolume(pAudio->pBuffer, (float)SmushPlay_volume / 127.0f);
    return pAudio;
}

static void SmushPlay_PlayAudio(SmushAudioBuffer pAudio)
{
    SoundDriver_Play(pAudio->pBuffer, /*bLoop=*/0);
}

// Returns the play position in bytes or -1 when not playing
static int64_t SmushPlay_GetAudioPosition(SmushAudioBuffer pAudio)
{
    if ( (SoundDriver_GetStatusAndCaps(pAudio->pBuffer) & SOUND_CHANNEL_PLAYING) == 0 )
    {
        return -1;
    }

    return (int64_t)SoundDriver_GetCurrentPosition(pAudio->pBuffer);
}

static void SmushPlay_ReleaseAudioBuffer(SmushAudioBuffer pAudio)
{
    SoundDriver_Stop(pAudio->pBuffer);
    SoundDriver_Release(pAudio->pBuffer);
    free(pAudio);
}

static double SmushPlay_GetTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void SmushPlay_Sleep(void)
{
    struct timespec ts = { 0, 1000000 };
    nanosleep(&ts, NULL);
}

#endif // _WIN32

int J3DAPI SmushPlay_PlayMovie(const char* pFilename, int a1, int a3, int a4, int a5, int width, int height, int a8, int a9, int(__cdecl* pBlit)(const SmushBitmap* pBitmap, int a2), int a11, int a12, int a13)
{
    (void)a1;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)width;
    (void)height;
    (void)a8;
    (void)a9;
    (void)a11;
    (void)a12;
    (void)a13;

    if ( !pFilename || !pBlit )
    {
        return 1;
    }

    size_t fileSize = 0;
    uint8_t* pFileData = SmushPlay_LoadFile(pFilename, &fileSize);
    if ( !pFileData )
    {
        return 1;
    }

    const uint8_t* pEnd = pFileData + fileSize;
    int result = 1;
    void* pDecoder = NULL;
    uint8_t* pFrameBuf = NULL;
    int16_t* aSamples = NULL;
    SmushAudioBuffer pAudioBuffer = NULL;

    // Parse header
    if ( fileSize < 16 || SmushPlay_ReadBE32(pFileData) != SMUSHPLAY_TAG('S', 'A', 'N', 'M') )
    {
        goto cleanup;
    }

    const uint8_t* pCur = pFileData + 8;
    uint32_t tag, size;
    const uint8_t* pChunk;
    if ( !SmushPlay_NextChunk(&pCur, pEnd, &tag, &size, &pChunk) || tag != SMUSHPLAY_TAG('S', 'H', 'D', 'R') || size < 20 )
    {
        goto cleanup;
    }

    uint32_t numFrames    = SmushPlay_ReadLE32(pChunk + 2);
    int movieWidth        = SmushPlay_ReadLE16(pChunk + 8);
    int movieHeight       = SmushPlay_ReadLE16(pChunk + 10);
    uint32_t usecPerFrame = SmushPlay_ReadLE32(pChunk + 14);
    if ( !usecPerFrame )
    {
        usecPerFrame = SMUSHPLAY_DEFAULT_USEC_PER_FRAME;
    }

    unsigned int audioFreq = 22050;
    unsigned int audioChannels = 2;
    if ( !SmushPlay_NextChunk(&pCur, pEnd, &tag, &size, &pChunk) || tag != SMUSHPLAY_TAG('F', 'L', 'H', 'D') )
    {
        goto cleanup;
    }

    // Frames header
    for ( uint32_t pos = 0; pos + 8 <= size; )
    {
        uint32_t subTag = SmushPlay_ReadBE32(pChunk + pos);
        if ( subTag == SMUSHPLAY_TAG('W', 'a', 'v', 'e') && pos + 16 <= size )
        {
            audioFreq     = SmushPlay_ReadLE32(pChunk + pos + 8);
            audioChannels = SmushPlay_ReadLE32(pChunk + pos + 12);
            pos += 20;
        }
        else
        {
            pos += SmushPlay_ReadBE32(pChunk + pos + 4) + 8;
        }
    }

    if ( audioChannels < 1 || audioChannels > 2 )
    {
        audioChannels = 2;
    }

    const uint8_t* pFrames = pCur;

    // Decode audio and start playback
    size_t numSamples = 0;
    aSamples = SmushPlay_DecodeAudio(pFrames, pEnd, audioChannels, &numSamples);
    pAudioBuffer = SmushPlay_CreateAudioBuffer(aSamples, numSamples, audioFreq, audioChannels);

    pDecoder  = Blocky16_Create(movieWidth, movieHeight);
    pFrameBuf = (uint8_t*)calloc((size_t)movieWidth * movieHeight, sizeof(uint16_t));
    if ( !pDecoder || !pFrameBuf )
    {
        goto cleanup;
    }

    SmushBitmap bitmap = { 0 };
    bitmap.pPixels   = pFrameBuf;
    bitmap.width     = movieWidth;
    bitmap.height    = movieHeight;
    bitmap.pixelSize = 2;
    bitmap.bpp       = 16;
    bitmap.pitch     = movieWidth * 2;
    bitmap.format.redBPP        = 5;
    bitmap.format.greenBPP      = 6;
    bitmap.format.blueBPP       = 5;
    bitmap.format.redPosShift   = 11;
    bitmap.format.greenPosShift = 5;
    bitmap.format.bluePosShift  = 0;
    bitmap.format.redPosShiftRight   = 3;
    bitmap.format.greenPosShiftRight = 2;
    bitmap.format.bluePosShiftRight  = 3;

    double startTime = SmushPlay_GetTime();
    if ( pAudioBuffer )
    {
        SmushPlay_PlayAudio(pAudioBuffer);
    }

    result = 0;
    uint32_t frameNum = 0;
    while ( frameNum < numFrames && SmushPlay_NextChunk(&pCur, pEnd, &tag, &size, &pChunk) )
    {
        if ( tag != SMUSHPLAY_TAG('F', 'R', 'M', 'E') )
        {
            continue;
        }

        // Decode video
        const uint8_t* pSub = pChunk;
        uint32_t subTag, subSize;
        const uint8_t* pSubData;
        while ( SmushPlay_NextChunk(&pSub, pChunk + size, &subTag, &subSize, &pSubData) )
        {
            if ( subTag == SMUSHPLAY_TAG('B', 'l', '1', '6') && subSize >= 560 )
            {
                Blocky16_Decode(pDecoder, pFrameBuf, pSubData);
            }
        }

        // Wait for frame time, sync to audio when playing
        double frameTime = (double)frameNum * usecPerFrame / 1000000.0;
        for ( ;; )
        {
            double curTime;
            int64_t playPos = pAudioBuffer ? SmushPlay_GetAudioPosition(pAudioBuffer) : -1;
            if ( playPos >= 0 )
            {
                curTime = (double)playPos / (double)(audioFreq * audioChannels * sizeof(int16_t));
            }
            else
            {
                curTime = SmushPlay_GetTime() - startTime;
            }

            if ( curTime >= frameTime )
            {
                break;
            }

            SmushPlay_Sleep();
        }

        // Display frame, blit returns non-zero to stop playback
        if ( pBlit(&bitmap, frameNum) )
        {
            break;
        }

        ++frameNum;
    }

cleanup:
    if ( pAudioBuffer )
    {
        SmushPlay_ReleaseAudioBuffer(pAudioBuffer);
    }

    if ( pDecoder )
    {
        Blocky16_Destroy(pDecoder);
    }

    free(pFrameBuf);
    free(aSamples);
    free(pFileData);
    return result;
}
