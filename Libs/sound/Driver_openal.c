// Sound driver on OpenAL Soft (non-Windows builds), see Driver.c for the DirectSound implementation.
// There's no 3D sound hardware mode: sounds are 2D sources mixed by the game's listener sound mix
// (volume, pan, pitch in SoundDriver_Update3DSound), like the DirectSound driver without 3D caps.
#include "Driver.h"

#include <rdroid/Math/rdVector.h>
#include <sound/AudioLib.h>
#include <std/General/stdMath.h>
#include <std/General/stdPlatform.h>
#include <std/General/stdUtil.h>

#include <AL/al.h>
#include <AL/alc.h>

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define SOUNDDRIVER_FARVOLUMETHRESHOLD 0.01f

// AL buffer shared by duplicated sound buffers
typedef struct sSoundDriverData
{
    ALuint buffer;
    uint32_t refCount;
    uint32_t sampleRate;
} SoundDriverData;

struct sSoundDriverBuffer
{
    ALuint source;
    SoundDriverData* pData;
    float volume;    // as set, 0 - 1
    float pan;       // -1 - 1
    float frequency; // Hz
    int bLoop;
};

struct sSoundDriverDevice
{
    ALCdevice* pDevice;
    ALCcontext* pContext;
};

static float SoundDriver_maxVolume = 1.0f;

static struct sSoundDriverDevice SoundDriver_device;
static tSoundDevice SoundDriver_pDevice = NULL;

static int SoundDriver_aPanTable[256];

static tPrintfFunc SoundDriver_pfLogError;
static SoundCalcListenerSoundMixFunc SoundDriver_pfCalcListenerSoundMix;
static SoundDriverGetSoundBufferDataFunc SoundDriver_pfGetSoundBufferData;

static rdVector3 SoundDriver_curListenerPos;
static float SoundDriver_frontOrientATan;

static const int SoundDriver_aDBTable[256] = {
    -9640, -4810, -4210, -3850, -3600, -3410, -3250, -3120, -3000, -2900, -2810, -2730, -2650, -2580, -2520, -2460,
    -2400, -2350, -2300, -2250, -2210, -2160, -2120, -2080, -2050, -2010, -1980, -1950, -1910, -1880, -1850, -1830,
    -1800, -1770, -1750, -1720, -1700, -1670, -1650, -1630, -1600, -1580, -1560, -1540, -1520, -1500, -1480, -1460,
    -1450, -1430, -1410, -1390, -1380, -1360, -1340, -1330, -1310, -1300, -1280, -1270, -1250, -1240, -1220, -1210,
    -1200, -1180, -1170, -1160, -1140, -1130, -1120, -1110, -1090, -1080, -1070, -1060, -1050, -1040, -1020, -1010,
    -1000,  -990,  -980,  -970,  -960,  -950,  -940,  -930,  -920,  -910,  -900,  -890,  -880,  -870,  -860,  -850,
     -840,  -830,  -830,  -820,  -810,  -800,  -790,  -780,  -770,  -770,  -760,  -750,  -740,  -730,  -730,  -720,
     -710,  -700,  -690,  -690,  -680,  -670,  -660,  -660,  -650,  -640,  -640,  -630,  -620,  -610,  -610,  -600,
     -590,  -590,  -580,  -570,  -570,  -560,  -550,  -550,  -540,  -530,  -530,  -520,  -520,  -510,  -500,  -500,
     -490,  -490,  -480,  -470,  -470,  -460,  -460,  -450,  -440,  -440,  -430,  -430,  -420,  -420,  -410,  -410,
     -400,  -390,  -390,  -380,  -380,  -370,  -370,  -360,  -360,  -350,  -350,  -340,  -340,  -330,  -330,  -320,
     -320,  -310,  -310,  -300,  -300,  -290,  -290,  -280,  -280,  -270,  -270,  -260,  -260,  -260,  -250,  -250,
     -240,  -240,  -230,  -230,  -220,  -220,  -210,  -210,  -210,  -200,  -200,  -190,  -190,  -180,  -180,  -180,
     -170,  -170,  -160,  -160,  -160,  -150,  -150,  -140,  -140,  -140,  -130,  -130,  -120,  -120,  -120,  -110,
     -110,  -100,  -100,  -100,   -90,   -90,   -80,   -80,   -80,   -70,   -70,   -70,   -60,   -60,   -50,   -50,
      -50,   -40,   -40,   -40,   -30,   -30,   -30,   -20,   -20,   -20,   -10,   -10,   -10,   -10,   -10,   -10
};

double J3DAPI SoundDriver_CalcDistToListener(float x1, float y1, float z1, float x2, float y2, float z2);
void J3DAPI SoundDriver_CalcListeneSoundMix(float x, float y, float z, float volume, float pitch, float* newVolume, float* pan, float* newPitch, float minRadius, float maxRadius, SoundEnvFlags envflags);
void SoundDriver_InitPanTable(void);

void Driver_InstallHooks(void)
{
}

static void SoundDriver_LogError(const char* pFormat, ...)
{
    if ( SoundDriver_pfLogError )
    {
        char aText[512];
        va_list args;
        va_start(args, pFormat);
        vsnprintf(aText, sizeof(aText), pFormat, args);
        va_end(args);
        SoundDriver_pfLogError("%s", aText);
    }
}

static bool SoundDriver_CheckAL(const char* pWhat)
{
    ALenum err = alGetError();
    if ( err != AL_NO_ERROR )
    {
        SoundDriver_LogError("SoundDriver: OpenAL error 0x%X: %s\n", err, pWhat);
        return false;
    }

    return true;
}

int J3DAPI SoundDriver_Open(int bNoSound3D, int bGlobalFocus, HWND hwnd, tSoundDevice pDirectSound, SoundCalcListenerSoundMixFunc pfCalcListenerSoundMix, SoundDriverGetSoundBufferDataFunc pfGetSoundBufferData, tPrintfFunc pfLogError)
{
    J3D_UNUSED(bGlobalFocus);
    J3D_UNUSED(hwnd);
    J3D_UNUSED(pDirectSound);

    SoundDriver_pfLogError = pfLogError;
    SoundDriver_InitPanTable();

    SoundDriver_pDevice = SoundDriver_CreateDirectSound(bNoSound3D);

    SoundDriver_pfCalcListenerSoundMix = pfCalcListenerSoundMix;
    SoundDriver_pfGetSoundBufferData   = pfGetSoundBufferData;
    return SoundDriver_pDevice != NULL;
}

void SoundDriver_Close(void)
{
    SoundDriver_ReleaseDirectSound(SoundDriver_pDevice);
    SoundDriver_pDevice = NULL;
}

void J3DAPI SoundDriver_SetMaxVolume(float volume)
{
    SoundDriver_maxVolume = volume;
}

static ALenum SoundDriver_GetALFormat(size_t nBitsPerSample, size_t numChannels)
{
    if ( nBitsPerSample == 8 ) {
        return numChannels == 2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
    }

    return numChannels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
}

static void SoundDriver_ReleaseData(SoundDriverData* pData)
{
    if ( pData && --pData->refCount == 0 )
    {
        alDeleteBuffers(1, &pData->buffer);
        free(pData);
    }
}

tSoundBuffer J3DAPI SoundDriver_CreateAndPlay(size_t samplesPerSec, size_t nBitsPerSample, size_t numChannels, const uint8_t* pSoundData, size_t dataSize, tSoundChannelFlag* pFlags, tSoundBuffer pSBuffer)
{
    if ( !SoundDriver_pDevice ) {
        return NULL;
    }

    tSoundBuffer pBuf = (tSoundBuffer)calloc(1, sizeof(struct sSoundDriverBuffer));
    if ( !pBuf ) {
        return NULL;
    }

    if ( pSBuffer && pSBuffer->pData )
    {
        // Duplicate: share the sound data
        pBuf->pData = pSBuffer->pData;
        ++pBuf->pData->refCount;
    }
    else
    {
        pBuf->pData = (SoundDriverData*)calloc(1, sizeof(SoundDriverData));
        if ( !pBuf->pData )
        {
            free(pBuf);
            return NULL;
        }

        pBuf->pData->refCount   = 1;
        pBuf->pData->sampleRate = (uint32_t)samplesPerSec;

        alGenBuffers(1, &pBuf->pData->buffer);
        if ( (*pFlags & SOUND_CHANNEL_COMPRESSED) != 0 )
        {
            size_t uncompressedSize = ((const tAudioCompressedData*)pSoundData)->uncompressedSize;
            uint8_t* pUncompressed  = (uint8_t*)malloc(uncompressedSize);
            if ( pUncompressed )
            {
                tAudioCompressorState compressor;
                AudioLib_ResetCompressor(&compressor);
                AudioLib_Uncompress(&compressor, pUncompressed, ((const tAudioCompressedData*)pSoundData)->compressedData, uncompressedSize);
                alBufferData(pBuf->pData->buffer, SoundDriver_GetALFormat(nBitsPerSample, numChannels), pUncompressed, (ALsizei)uncompressedSize, (ALsizei)samplesPerSec);
                free(pUncompressed);
            }
            else
            {
                SoundDriver_LogError("SoundDriver: SoundDriver_CreateAndPlay Failed to allocate memory to uncompress data.");
            }
        }
        else
        {
            alBufferData(pBuf->pData->buffer, SoundDriver_GetALFormat(nBitsPerSample, numChannels), pSoundData, (ALsizei)dataSize, (ALsizei)samplesPerSec);
        }

        if ( !SoundDriver_CheckAL("creating buffer") )
        {
            SoundDriver_ReleaseData(pBuf->pData);
            free(pBuf);
            return NULL;
        }
    }

    alGenSources(1, &pBuf->source);
    if ( !SoundDriver_CheckAL("creating source") )
    {
        SoundDriver_ReleaseData(pBuf->pData);
        free(pBuf);
        return NULL;
    }

    alSourcei(pBuf->source, AL_BUFFER, (ALint)pBuf->pData->buffer);
    alSourcei(pBuf->source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(pBuf->source, AL_ROLLOFF_FACTOR, 0.0f);
    alSource3f(pBuf->source, AL_POSITION, 0.0f, 0.0f, -1.0f);

    pBuf->frequency = (float)pBuf->pData->sampleRate;
    pBuf->pan       = 0.0f;

    // Software mixed 2D sound
    *pFlags &= ~(SOUND_CHANNEL_HWMIXING | SOUND_CHANNEL_3DSOUND);
    *pFlags |= SOUND_CHANNEL_SWMIXING | SOUND_CHANNEL_GLOBALFOCUS;

    // Restarted sounds start silent, the sound mix sets the volume (SoundDriver_Update3DSound)
    SoundDriver_SetVolume(pBuf, (*pFlags & SOUND_CHANNEL_RESTART) != 0 ? 0.0f : 1.0f);

    if ( (*pFlags & SOUND_CHANNEL_PLAYING) != 0 ) {
        SoundDriver_Play(pBuf, (*pFlags & (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING)) == (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING));
    }

    return pBuf;
}

void J3DAPI SoundDriver_Release(tSoundBuffer pDSBuf)
{
    if ( pDSBuf )
    {
        alSourceStop(pDSBuf->source);
        alDeleteSources(1, &pDSBuf->source);
        SoundDriver_ReleaseData(pDSBuf->pData);
        free(pDSBuf);
    }
}

void J3DAPI SoundDriver_Play(tSoundBuffer pDSBuf, int bLoop)
{
    if ( pDSBuf )
    {
        pDSBuf->bLoop = bLoop;
        alSourcei(pDSBuf->source, AL_LOOPING, bLoop ? AL_TRUE : AL_FALSE);

        // DirectSound Play continues from the current position
        ALint state = 0;
        alGetSourcei(pDSBuf->source, AL_SOURCE_STATE, &state);
        if ( state != AL_PLAYING ) {
            alSourcePlay(pDSBuf->source);
        }

        SoundDriver_CheckAL("playing");
    }
}

void J3DAPI SoundDriver_Stop(tSoundBuffer pDSBuf)
{
    if ( pDSBuf ) {
        alSourcePause(pDSBuf->source); // DirectSound Stop keeps the play position
    }
}

void J3DAPI SoundDriver_SetVolume(tSoundBuffer pDSBuf, float volume)
{
    if ( pDSBuf )
    {
        // Same attenuation steps as DirectSound (hundredths of decibel)
        int dB = SoundDriver_GetDecibelVolume(volume);
        pDSBuf->volume = volume;
        alSourcef(pDSBuf->source, AL_GAIN, dB <= -9600 ? 0.0f : powf(10.0f, (float)dB / 2000.0f));
    }
}

float J3DAPI SoundDriver_GetVolume(tSoundBuffer pDSBuf)
{
    if ( !pDSBuf ) {
        return 1.0f;
    }

    return SoundDriver_GetVolumeFromDecibels(SoundDriver_GetDecibelVolume(pDSBuf->volume));
}

void J3DAPI SoundDriver_SetPan(tSoundBuffer pDSoundBuf, float pan)
{
    if ( pDSoundBuf )
    {
        pan = STDMATH_CLAMP(pan, -1.0f, 1.0f);
        pDSoundBuf->pan = pan;

        // Pan a mono source by its direction around the listener, stereo sources aren't panned by OpenAL
        float angle = pan * (float)M_PI_2;
        alSource3f(pDSoundBuf->source, AL_POSITION, sinf(angle), 0.0f, -cosf(angle));
    }
}

float J3DAPI SoundDriver_GetPan(tSoundBuffer pDSBuf)
{
    if ( !pDSBuf ) {
        return 0.0f;
    }

    return SoundDriver_GetPanFromTable(SoundDriver_GetIntPan(pDSBuf->pan));
}

void J3DAPI SoundDriver_SetFrequency(tSoundBuffer pDSBuf, float freq)
{
    if ( pDSBuf && pDSBuf->pData->sampleRate )
    {
        pDSBuf->frequency = (float)(int32_t)freq;
        alSourcef(pDSBuf->source, AL_PITCH, STDMATH_CLAMP(pDSBuf->frequency / (float)pDSBuf->pData->sampleRate, 0.01f, 8.0f));
    }
}

float J3DAPI SoundDriver_GetFrequency(tSoundBuffer pDSBuf)
{
    if ( !pDSBuf ) {
        return 22050.0f;
    }

    return pDSBuf->frequency;
}

size_t J3DAPI SoundDriver_GetCurrentPosition(tSoundBuffer pDSoundBuf)
{
    if ( !pDSoundBuf ) {
        return 0;
    }

    ALint offset = 0;
    alGetSourcei(pDSoundBuf->source, AL_BYTE_OFFSET, &offset);
    return (size_t)offset;
}

tSoundChannelFlag J3DAPI SoundDriver_GetStatusAndCaps(tSoundBuffer pDSBuffer)
{
    if ( !pDSBuffer ) {
        return (tSoundChannelFlag)0;
    }

    tSoundChannelFlag flags = SOUND_CHANNEL_SWMIXING | SOUND_CHANNEL_GLOBALFOCUS;

    ALint state = 0;
    alGetSourcei(pDSBuffer->source, AL_SOURCE_STATE, &state);
    if ( state == AL_PLAYING )
    {
        flags |= SOUND_CHANNEL_PLAYING;
        if ( pDSBuffer->bLoop ) {
            flags |= SOUND_CHANNEL_LOOP;
        }
    }

    return flags;
}

void J3DAPI SoundDriver_SetListenerPosition(const rdVector3* pPos, const rdVector3* pVelocity, const rdVector3* pTopOrient, const rdVector3* pFrontOrient)
{
    J3D_UNUSED(pVelocity);

    if ( pPos ) {
        rdVector_Copy3(&SoundDriver_curListenerPos, pPos);
    }

    if ( !SoundDriver_pfCalcListenerSoundMix && pTopOrient && pFrontOrient )
    {
        if ( pFrontOrient->x == 0.0f ) {
            SoundDriver_frontOrientATan = 0.0f;
        }
        else
        {
            SoundDriver_frontOrientATan = atan2f(pFrontOrient->z, pFrontOrient->x);
            if ( errno == EDOM ) {
                SoundDriver_frontOrientATan = 0.0f;
            }
        }
    }
}

void SoundDriver_ListenerCommitDeferred(void)
{
}

void J3DAPI SoundDriver_SetPosAndVelocity(tSoundBuffer pBuffer, float x, float y, float z, float velX, float velY, float velZ, float minDistance, float maxDistance)
{
    // Only used with 3D sound hardware
    J3D_UNUSED(pBuffer);
    J3D_UNUSED(x);
    J3D_UNUSED(y);
    J3D_UNUSED(z);
    J3D_UNUSED(velX);
    J3D_UNUSED(velY);
    J3D_UNUSED(velZ);
    J3D_UNUSED(minDistance);
    J3D_UNUSED(maxDistance);
}

int J3DAPI SoundDriver_Update3DSound(tSoundBuffer* ppDSBuf, float x, float y, float z, float volume, float pitch, tSoundChannelFlag* pChannelFlags, float minRadius, float maxRadius, SoundEnvFlags envflags)
{
    tSoundBuffer pDSBuf = NULL;
    if ( ppDSBuf )
    {
        pDSBuf = *ppDSBuf;
    }

    bool b3DControl = false;
    if ( (*pChannelFlags & SOUND_CHANNEL_USE3DCAPS) != 0 )
    {
        b3DControl = true;
    }
    else if ( SoundDriver_Use3DCaps() && (*pChannelFlags & SOUND_CHANNEL_UNKNOWN_8000000) == 0 )
    {
        float distSquare = (SoundDriver_curListenerPos.x - x) * (SoundDriver_curListenerPos.x - x)
            + (SoundDriver_curListenerPos.y - y) * (SoundDriver_curListenerPos.y - y)
            + (SoundDriver_curListenerPos.z - z) * (SoundDriver_curListenerPos.z - z);
        b3DControl = distSquare <= 0.25f;
    }

    if ( (*pChannelFlags & SOUND_CHANNEL_DISABLEHWMIXING) != 0 )
    {
        b3DControl = false;
    }

    float newVolume = volume;
    float pan       = 0.0f;
    float newPitch  = pitch;
    if ( !b3DControl )
    {
        SoundDriver_CalcListeneSoundMix(x, y, z, volume, pitch, &newVolume, &pan, &newPitch, minRadius, maxRadius, envflags);
        if ( newVolume < SOUNDDRIVER_FARVOLUMETHRESHOLD
            && (*pChannelFlags & (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING)) != (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING)
        #ifdef J3D_QOL_IMPROVEMENTS
            && (*pChannelFlags & (SOUND_CHANNEL_FAR)) != (SOUND_CHANNEL_FAR)  // Fixed: Added check for channel being played far away. 
                                                                              //        This prevents removing a looping sound in the Sound_Update at far distance that is out of hearing range.
        #endif
            )
        {
            return 0;
        }
    }

    // There's no 3D hardware mixing (SoundDriver_Use3DCaps), the sound is always mixed by the listener sound mix

    // Note: No need to check for null pDSBuf because all underlying functions do that
    if ( (*pChannelFlags & SOUND_CHANNEL_HWMIXING) == 0 || (*pChannelFlags & SOUND_CHANNEL_DISABLEHWMIXING) != 0 )
    {
        float newVol = newVolume * SoundDriver_maxVolume;
        SoundDriver_SetVolume(pDSBuf, newVol);
        SoundDriver_SetPan(pDSBuf, pan);
        if ( newPitch != 1.0f )
        {
            float freq = SoundDriver_GetFrequency(pDSBuf);
            freq *= newPitch / pitch;
            SoundDriver_SetFrequency(pDSBuf, freq);
        }
    }

    if ( (*pChannelFlags & SOUND_CHANNEL_FAR) != 0 )
    {
        if ( newVolume > SOUNDDRIVER_FARVOLUMETHRESHOLD )
        {
            *pChannelFlags |= SOUND_CHANNEL_RESTART;
            return 1;
        }
    }

#ifdef J3D_QOL_IMPROVEMENTS
    // Fixed: Always stop currently looping far sound when volume drops below SOUNDDRIVER_FARVOLUMETHESHOLD.
    //        Originally, far marked looping sound was not stopped due to else if branch here.
    if ( (*pChannelFlags & (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING)) == (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING)
        && (*pChannelFlags & SOUND_CHANNEL_3DSOUND) == SOUND_CHANNEL_3DSOUND
        && newVolume < SOUNDDRIVER_FARVOLUMETHRESHOLD )
    #else

    else if ( (*pChannelFlags & (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING)) == (SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING)
        && (*pChannelFlags & SOUND_CHANNEL_3DSOUND) == SOUND_CHANNEL_3DSOUND
        && newVolume < SOUNDDRIVER_FARVOLUMETHRESHOLD )
    #endif
    {
        SoundDriver_Stop(pDSBuf);
        if ( pDSBuf )
        {
            SoundDriver_Release(pDSBuf);
            *ppDSBuf = NULL;
        }

        *pChannelFlags &= ~(SOUND_CHANNEL_LOOP | SOUND_CHANNEL_PLAYING);
        *pChannelFlags |= SOUND_CHANNEL_FAR;
    }

    return 1;
}

void J3DAPI SoundDriver_SetGlobals(float distanceFactor, float minDistance, float maxDistance, float rolloffFactor, float dopplerFactor)
{
    // Only used with 3D sound hardware
    J3D_UNUSED(distanceFactor);
    J3D_UNUSED(minDistance);
    J3D_UNUSED(maxDistance);
    J3D_UNUSED(rolloffFactor);
    J3D_UNUSED(dopplerFactor);
}

int SoundDriver_Has3DHW(void)
{
    return 0;
}

int SoundDriver_Use3DCaps(void)
{
    return 0;
}

tSoundDevice SoundDriver_GetDSound(void)
{
    return SoundDriver_pDevice;
}

tSoundDevice J3DAPI SoundDriver_CreateDirectSound(int bNoSound3D)
{
    J3D_UNUSED(bNoSound3D);
    if ( SoundDriver_device.pDevice ) {
        return &SoundDriver_device;
    }

    SoundDriver_device.pDevice = alcOpenDevice(NULL);
    if ( !SoundDriver_device.pDevice )
    {
        SoundDriver_LogError("SoundDriver: Failed to open the OpenAL device.\n");
        return NULL;
    }

    SoundDriver_device.pContext = alcCreateContext(SoundDriver_device.pDevice, NULL);
    if ( !SoundDriver_device.pContext || !alcMakeContextCurrent(SoundDriver_device.pContext) )
    {
        SoundDriver_LogError("SoundDriver: Failed to create the OpenAL context.\n");
        if ( SoundDriver_device.pContext ) {
            alcDestroyContext(SoundDriver_device.pContext);
        }

        alcCloseDevice(SoundDriver_device.pDevice);
        memset(&SoundDriver_device, 0, sizeof(SoundDriver_device));
        return NULL;
    }

    alDistanceModel(AL_NONE); // the game attenuates by itself
    return &SoundDriver_device;
}

void J3DAPI SoundDriver_ReleaseDirectSound(tSoundDevice pDSound)
{
    if ( pDSound && pDSound->pDevice )
    {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(pDSound->pContext);
        alcCloseDevice(pDSound->pDevice);
        memset(pDSound, 0, sizeof(*pDSound));
    }
}

void SoundDriver_CreateListener(void)
{
}

void SoundDriver_ReleaseListener(void)
{
}

void J3DAPI SoundDriver_SetOuputFormat(tSoundDevice pDSound, uint32_t nSamplesPerSec, uint32_t nBitsPerSample, uint32_t numChannels)
{
    J3D_UNUSED(pDSound);
    J3D_UNUSED(nSamplesPerSec);
    J3D_UNUSED(nBitsPerSample);
    J3D_UNUSED(numChannels);
}

tSoundBuffer J3DAPI SoundDriver_GetPrimaryDSBuffer(tSoundDevice pDSound)
{
    J3D_UNUSED(pDSound);
    return NULL;
}

void J3DAPI SoundDriver_ReleasePrimaryDSBuffer()
{
}

int J3DAPI SoundDriver_GetDecibelVolume(float volume)
{
    int idx = (int)(volume * 255.0f);
    if ( idx < 0 )
    {
        idx = 0;
    }

    if ( idx < STD_ARRAYLEN(SoundDriver_aDBTable) )
    {
        return SoundDriver_aDBTable[idx];
    }

    return 0;
}

int J3DAPI SoundDriver_GetIntPan(float pan)
{
    int idx = (int)(pan * 128.0f + 128.0f);
    if ( idx < 0 )
    {
        idx = 0;
    }

    if ( idx < STD_ARRAYLEN(SoundDriver_aPanTable) )
    {
        return SoundDriver_aPanTable[idx];
    }

    return 0;
}

float J3DAPI SoundDriver_GetVolumeFromDecibels(int dB)
{
    int v1;
    int idx = STD_ARRAYLEN(SoundDriver_aDBTable);
    do
    {
        v1 = idx--;
    } while ( v1 && SoundDriver_aDBTable[idx] > dB );

    return (float)idx * 1.0f / 255.0f;
}

float J3DAPI SoundDriver_GetPanFromTable(int pan)
{
    int v1;
    int idx = STD_ARRAYLEN(SoundDriver_aPanTable);
    do
    {
        v1 = idx--;
    } while ( v1 && SoundDriver_aPanTable[idx] > pan );

    return (float)(idx - 128) * 1.0f / 128.0f;
}

void SoundDriver_InitPanTable(void)
{
    int i;

    for ( i = 0; i < (STD_ARRAYLEN(SoundDriver_aPanTable) / 2); ++i )
    {
        SoundDriver_aPanTable[i + 128] = -SoundDriver_aDBTable[255 - 2 * i];
        SoundDriver_aPanTable[127 - i] = SoundDriver_aDBTable[255 - 2 * i];
    }
}

void J3DAPI SoundDriver_CalcListeneSoundMix(float x, float y, float z, float volume, float pitch, float* newVolume, float* pan, float* newPitch, float minRadius, float maxRadius, SoundEnvFlags envflags)
{
    if ( SoundDriver_pfCalcListenerSoundMix )
    {
        SoundSpatialInfo spatialInfo = { 0 };
        spatialInfo.pos.x     = x;
        spatialInfo.pos.y     = y;
        spatialInfo.pos.z     = z;
        spatialInfo.volume    = volume;
        spatialInfo.pitch     = pitch;
        spatialInfo.minRadius = minRadius;
        spatialInfo.maxRadius = maxRadius;
        spatialInfo.flags     = envflags;

        if ( SoundDriver_curListenerPos.x < 100000.0f )
        {
            SoundDriver_pfCalcListenerSoundMix(&spatialInfo, newVolume, pan, newPitch);
        }
        else
        {
            if ( newVolume )
            {
                *newVolume = 0.0f;
            }

            if ( pan )
            {
                *pan = 0.0f;
            }
        }
    }
    else
    {
        if ( !newVolume )
        {
            return;
        }

        *newVolume = volume;
        if ( pan )
        {
            *pan = 0.0f;
        }

        if ( newPitch )
        {
            *newPitch = 1.0f;
        }

        double dist = SoundDriver_CalcDistToListener(x, y, z, SoundDriver_curListenerPos.x, SoundDriver_curListenerPos.y, SoundDriver_curListenerPos.z);
        if ( dist > 500.0 )
        {
            *newVolume = 0.0f;
            return;
        }

        if ( dist >= 200.0 )
        {
            *newVolume = 0.0f;
        }
        else
        {
            if ( dist < 30.0 )
            {
                *newVolume = 1.0f;
                return;
            }

            if ( dist >= 200.0 ) // this makes no sense
            {
                dist = 200.0f;
            }

            *newVolume = (float)((*newVolume - 0.0) * (170.0 - (dist - 30.0)) / 170.0 + 0.0); // 0.0f looks like to be some constant vars
        }

        if ( pan )
        {
            float dx = x - SoundDriver_curListenerPos.x;
            float dz = z - SoundDriver_curListenerPos.z;
            if ( dx <= -0.5f || dx >= 0.5f )
            {
                if ( dz <= -0.5f || dz >= 0.5f )
                {
                    float atan = atan2f(dz, dx);
                    if ( errno == EDOM )
                    {
                        atan = 0.0f;
                    }

                    *pan = -sinf(atan - SoundDriver_frontOrientATan);
                }
                else if ( dx <= 0.0f )
                {
                    *pan = -1.0f;
                }
                else
                {
                    *pan = 1.0f;
                }
            }
        }
    }
}

double J3DAPI SoundDriver_CalcDistToListener(float x1, float y1, float z1, float x2, float y2, float z2)
{
    float dx  = fabsf(x2 - x1);
    float dz  = fabsf(z2 - z1);
    float hdx = (dx > (double)dz ? dz : dx) / 2.0f;
    float dy  = fabsf(y2 - y1);
    return dx + dz - hdx + dy;
}

uint32_t SoundDriver_GetTimeMsec(void)
{
    return stdPlatform_GetTimeMsec();
}

void J3DAPI SoundDriver_Sleep(unsigned int dwMilliseconds)
{
    Sleep(dwMilliseconds);
}
