#ifndef SOUND_DRIVER_H
#define SOUND_DRIVER_H
#include <j3dcore/j3d.h>
#include <sound/types.h>
#include <sound/RTI/addresses.h>


J3D_EXTERN_C_START

int J3DAPI SoundDriver_Open(int bNoSound3D, int bGlobalFocus, HWND hwnd, tSoundDevice pDirectSound, SoundCalcListenerSoundMixFunc pfCalcListenerSoundMix, SoundDriverGetSoundBufferDataFunc pfGetSoundBufferData, tPrintfFunc pfLogError);
void SoundDriver_Close(void);

void J3DAPI SoundDriver_SetMaxVolume(float volume);
tSoundBuffer J3DAPI SoundDriver_CreateAndPlay(size_t samplesPerSec, size_t nBitsPerSample, size_t numChannels, const uint8_t* pSoundData, size_t dataSize, tSoundChannelFlag* pFlags, tSoundBuffer pSBuffer);
void J3DAPI SoundDriver_Release(tSoundBuffer pDSBuf);
void J3DAPI SoundDriver_Play(tSoundBuffer pDSBuf, int bLoop);
void J3DAPI SoundDriver_Stop(tSoundBuffer pDSBuf);

void J3DAPI SoundDriver_SetVolume(tSoundBuffer pDSBuf, float volume);
float J3DAPI SoundDriver_GetVolume(tSoundBuffer pDSBuf);

void J3DAPI SoundDriver_SetPan(tSoundBuffer pDSoundBuf, float pan);
float J3DAPI SoundDriver_GetPan(tSoundBuffer pDSBuf); // Added: From debug

void J3DAPI SoundDriver_SetFrequency(tSoundBuffer pDSBuf, float freq);
float J3DAPI SoundDriver_GetFrequency(tSoundBuffer pDSBuf);

size_t J3DAPI SoundDriver_GetCurrentPosition(tSoundBuffer pDSoundBuf);
tSoundChannelFlag J3DAPI SoundDriver_GetStatusAndCaps(tSoundBuffer pDSBuffer);

void J3DAPI SoundDriver_SetListenerPosition(const rdVector3* pPos, const rdVector3* pVelocity, const rdVector3* pTopOrient, const rdVector3* pFrontOrient);
void SoundDriver_ListenerCommitDeferred(void);

void J3DAPI SoundDriver_SetPosAndVelocity(tSoundBuffer pBuffer, float x, float y, float z, float velX, float velY, float velZ, float minDistance, float maxDistance);

int J3DAPI SoundDriver_Update3DSound(tSoundBuffer* ppDSBuf, float x, float y, float z, float volume, float pitch, tSoundChannelFlag* pChannelFlags, float minRadius, float maxRadius, SoundEnvFlags envflags);

void J3DAPI SoundDriver_SetGlobals(float distanceFactor, float minDistance, float maxDistance, float rolloffFactor, float dopplerFactor);

int SoundDriver_Has3DHW(void);
int SoundDriver_Use3DCaps(void); // Added: From debug version
tSoundDevice SoundDriver_GetDSound(void);

tSoundDevice J3DAPI SoundDriver_CreateDirectSound(int bNoSound3D);
void J3DAPI SoundDriver_ReleaseDirectSound(tSoundDevice pDSound);

void SoundDriver_CreateListener(void);
void SoundDriver_ReleaseListener(void);

void J3DAPI SoundDriver_SetOuputFormat(tSoundDevice pDSound, uint32_t nSamplesPerSec, uint32_t nBitsPerSample, uint32_t numChannels);

tSoundBuffer J3DAPI SoundDriver_GetPrimaryDSBuffer(tSoundDevice pDSound);
void J3DAPI SoundDriver_ReleasePrimaryDSBuffer();

int J3DAPI SoundDriver_GetDecibelVolume(float volume);
int J3DAPI SoundDriver_GetIntPan(float pan);

float J3DAPI SoundDriver_GetVolumeFromDecibels(int dB);
float J3DAPI SoundDriver_GetPanFromTable(int pan); // Added From debug

uint32_t SoundDriver_GetTimeMsec(void);
void J3DAPI SoundDriver_Sleep(unsigned int dwMilliseconds);

// Helper hooking functions
void Driver_InstallHooks(void);

J3D_EXTERN_C_END
#endif // SOUND_DRIVER_H
