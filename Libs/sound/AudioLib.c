#include "AudioLib.h"
#include <j3dcore/j3dhook.h>
#include <sound/RTI/symbols.h>

#include <std/General/stdUtil.h>

#include <stdbool.h>
#include <string.h>

static const int16_t AudioLib_aStepTable[89] = {
      7,
      8,
      9,
      10,
      11,
      12,
      13,
      14,
      16,
      17,
      19,
      21,
      23,
      25,
      28,
      31,
      34,
      37,
      41,
      45,
      50,
      55,
      60,
      66,
      73,
      80,
      88,
      97,
      107,
      118,
      130,
      143,
      157,
      173,
      190,
      209,
      230,
      253,
      279,
      307,
      337,
      371,
      408,
      449,
      494,
      544,
      598,
      658,
      724,
      796,
      876,
      963,
      1060,
      1166,
      1282,
      1411,
      1552,
      1707,
      1878,
      2066,
      2272,
      2499,
      2749,
      3024,
      3327,
      3660,
      4026,
      4428,
      4871,
      5358,
      5894,
      6484,
      7132,
      7845,
      8630,
      9493,
      10442,
      11487,
      12635,
      13899,
      15289,
      16818,
      18500,
      20350,
      22385,
      24623,
      27086,
      29794,
      32767
    };
static const uint8_t AudioLib_aStepBits[89] = {
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      4u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      5u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      6u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u,
      7u
    };
static const int8_t AudioLib_aIndex2Bit[2] = { -1, 4 };
static const int8_t AudioLib_aIndex3Bit[4] = { -1, -1, 2, 6 };
static const int8_t AudioLib_aIndex4Bit[8] = { -1, -1, -1, -1, 1, 2, 4, 6 };
static const int8_t AudioLib_aIndex5Bit[16] = { -1, -1, -1, -1, -1, -1, -1, -1, 1, 1, 1, 2, 2, 4, 5, 6 };
static const int8_t AudioLib_aIndex6Bit[32] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1,  1,  1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  5,  5,  6,  6
};
static const int8_t AudioLib_aIndex7Bit[64] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,
     2,  2,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6
};

// Index adjust tables per number of code bits
static const int8_t* const AudioLib_aIndexTableTable[8] = {
    NULL,
    NULL,
    AudioLib_aIndex2Bit,
    AudioLib_aIndex3Bit,
    AudioLib_aIndex4Bit,
    AudioLib_aIndex5Bit,
    AudioLib_aIndex6Bit,
    AudioLib_aIndex7Bit
};

// Delta table per step index, built on first use (originally AudioLib_aDeltaTable + AudioLib_word_14E4928)
static uint16_t AudioLib_aDeltaTables[89][64];
static int AudioLib_bDeltaTableInitialized;

void AudioLib_InstallHooks(void)
{
    // Uncomment only lines for functions that have full definition and doesn't call original function (non-thunk functions)

    J3D_HOOKFUNC(AudioLib_ParseWaveFileHeader);
    J3D_HOOKFUNC(AudioLib_Compress);
    J3D_HOOKFUNC(AudioLib_ResetCompressor);
    J3D_HOOKFUNC(AudioLib_Uncompress);
    J3D_HOOKFUNC(AudioLib_GetMouthPosition);
    J3D_HOOKFUNC(AudioLib_GenerateLipSynchBlock);
    J3D_HOOKFUNC(AudioLib_CompressBlock);
    J3D_HOOKFUNC(AudioLib_UncompressBlock);
    J3D_HOOKFUNC(AudioLib_WVSMCompressBlock);
    J3D_HOOKFUNC(AudioLib_WVSMUncompressBlock);
}


static inline uint16_t AudioLib_SwapU16(uint16_t val)
{
    return (uint16_t)((val >> 8) | (val << 8));
}

static inline uint32_t AudioLib_SwapU32(uint32_t val)
{
    return (((val & 0xFF00) | (val << 16)) << 8) | (((val & 0xFF0000) | (val >> 16)) >> 8);
}

static inline uint16_t AudioLib_ReadU16(const uint8_t* p)
{
    return *(const uint16_t*)p;
}

static inline uint32_t AudioLib_ReadU32(const uint8_t* p)
{
    return *(const uint32_t*)p;
}

// Returns pointer to snd data
const uint8_t* J3DAPI AudioLib_ParseWaveFileHeader(const uint8_t* pData, int* pType, uint32_t* pSampleRate, uint32_t* pBitsPerSample, uint32_t* pNumChannels, uint32_t* pExtraInfo, uint32_t* pSoundDataSize, uint32_t* pExtraDataOffset, uint32_t* pSoundDataOffset)
{
    if ( pType )
    {
        *pType = 0;
    }

    if ( !pData )
    {
        return NULL;
    }

    if ( pExtraDataOffset )
    {
        *pExtraDataOffset = 0;
    }

    if ( pSoundDataOffset )
    {
        *pSoundDataOffset = 0;
    }

    // Skip MCMP compression map header
    bool bMcmp = memcmp(pData, "MCMP", 4) == 0;
    if ( bMcmp )
    {
        uint32_t numEntries = AudioLib_SwapU16(AudioLib_ReadU16(pData + 4));
        pData = pData + AudioLib_SwapU16(AudioLib_ReadU16(pData + numEntries * 9 + 6)) + numEntries * 9 + 8;
    }

    if ( memcmp(pData, "INDYWV", 6) == 0 )
    {
        if ( pType )
        {
            *pType = 6;
        }

        if ( pSampleRate )
        {
            *pSampleRate = AudioLib_ReadU32(pData + 6);
        }

        if ( pBitsPerSample )
        {
            *pBitsPerSample = AudioLib_ReadU32(pData + 10);
        }

        if ( pNumChannels )
        {
            *pNumChannels = AudioLib_ReadU32(pData + 14);
        }

        // Note, both extra info and sound data size are read from the same field
        if ( pExtraInfo )
        {
            *pExtraInfo = AudioLib_ReadU32(pData + 18);
        }

        if ( pSoundDataSize )
        {
            *pSoundDataSize = AudioLib_ReadU32(pData + 18);
        }

        if ( pExtraDataOffset && pSoundDataOffset )
        {
            uint32_t extraSize = AudioLib_ReadU32(pData + 22);
            *pSoundDataOffset  = extraSize;
            if ( extraSize )
            {
                *pExtraDataOffset = (uint32_t)(pData + 26);
                return pData + extraSize + 26;
            }

            *pExtraDataOffset = 0;
        }

        return pData + 26;
    }

    if ( memcmp(pData, "RIFF", 4) == 0 )
    {
        uint32_t riffSize = AudioLib_ReadU32(pData + 4);
        if ( memcmp(pData + 8, "WAVEfmt ", 8) != 0 || *(const int16_t*)(pData + 20) != 1 ) // PCM only
        {
            return NULL;
        }

        if ( pType )
        {
            *pType = bMcmp + 3;
        }

        if ( pSampleRate )
        {
            *pSampleRate = AudioLib_ReadU32(pData + 24);
        }

        if ( pBitsPerSample )
        {
            *pBitsPerSample = *(const int16_t*)(pData + 34);
        }

        if ( pNumChannels )
        {
            *pNumChannels = *(const int16_t*)(pData + 22);
        }

        if ( pExtraInfo )
        {
            *pExtraInfo = 1;
        }

        // Find data chunk
        const uint8_t* pChunkSize = pData + 16;
        uint32_t chunkSize        = AudioLib_ReadU32(pChunkSize);
        bool bData;
        do
        {
            pChunkSize = pChunkSize + chunkSize + 8;
            chunkSize  = AudioLib_ReadU32(pChunkSize);
            if ( memcmp(pChunkSize - 4, "sync", 4) == 0 && pExtraDataOffset && pSoundDataOffset )
            {
                *pExtraDataOffset = (uint32_t)(pChunkSize + 4);
                *pSoundDataOffset = chunkSize;
            }

            if ( pChunkSize >= pData + riffSize + 8 )
            {
                return NULL;
            }

            bData = memcmp(pChunkSize - 4, "data", 4) == 0;
        } while ( !bData );

        if ( pChunkSize < pData + riffSize + 8 )
        {
            if ( pSoundDataSize )
            {
                *pSoundDataSize = chunkSize;
            }

            return pChunkSize + 4;
        }

        return NULL;
    }

    if ( memcmp(pData, "iMUS", 4) == 0 )
    {
        uint32_t frmtSize = AudioLib_SwapU32(AudioLib_ReadU32(pData + 12));
        if ( memcmp(pData + 16, "FRMT", 4) != 0 )
        {
            return NULL;
        }

        if ( pType )
        {
            *pType = bMcmp + 1;
        }

        const uint8_t* pFrmt = pData + 32;
        if ( pBitsPerSample )
        {
            *pBitsPerSample = AudioLib_SwapU32(AudioLib_ReadU32(pFrmt));
        }

        if ( pSampleRate )
        {
            *pSampleRate = AudioLib_SwapU32(AudioLib_ReadU32(pData + 36));
        }

        if ( pNumChannels )
        {
            *pNumChannels = AudioLib_SwapU32(AudioLib_ReadU32(pData + 40));
        }

        if ( pExtraInfo )
        {
            *pExtraInfo = 1;
        }

        if ( pSoundDataSize )
        {
            *pSoundDataSize = AudioLib_SwapU32(AudioLib_ReadU32(pFrmt + frmtSize - 12));
        }

        return pFrmt + frmtSize - 8;
    }

    if ( bMcmp || memcmp(pData, "FORM", 4) != 0 )
    {
        return NULL;
    }

    uint32_t formSize = AudioLib_ReadU32(pData + 4);
    if ( memcmp(pData + 8, "AIFFCOMM", 8) != 0 )
    {
        return NULL;
    }

    if ( pType )
    {
        *pType = 5;
    }

    const uint8_t* pChunk = pData + 16;
    uint32_t chunkSize    = AudioLib_SwapU32(AudioLib_ReadU32(pChunk));
    if ( pSampleRate )
    {
        // Note, extended float sample rate is approximated from its exponent byte
        if ( pData[29] == 14 )
        {
            *pSampleRate = 44100;
        }
        else
        {
            *pSampleRate = pData[29] != 12 ? 22050 : 11025;
        }
    }

    if ( pBitsPerSample )
    {
        *pBitsPerSample = pData[27];
    }

    if ( pNumChannels )
    {
        *pNumChannels = pData[21];
    }

    if ( pExtraInfo )
    {
        *pExtraInfo = 0;
    }

    // Find SSND chunk
    const uint8_t* pFormEnd = pData + AudioLib_SwapU32(formSize) + 8;
    do
    {
        uint32_t nextSize = AudioLib_ReadU32(pChunk + chunkSize + 8);
        pChunk    = pChunk + chunkSize + 8;
        chunkSize = AudioLib_SwapU32(nextSize);
        if ( pFormEnd <= pChunk )
        {
            return NULL;
        }
    } while ( memcmp(pChunk - 4, "SSND", 4) != 0 );

    if ( pChunk < pFormEnd )
    {
        if ( pSoundDataSize )
        {
            *pSoundDataSize = chunkSize - 8;
        }

        return pChunk + 12;
    }

    return NULL;
}

int J3DAPI AudioLib_Compress(tAudioCompressorState* pCompressorState, uint8_t* pOutBuffer, const uint8_t* pInBuffer, int size, unsigned int numChannels)
{
    if ( numChannels > 2 )
    {
        // ADPCM, numChannels - 2 is actual number of channels
        unsigned int numChans = numChannels - 2;

        uint8_t header = pCompressorState->unknown0;
        if ( numChans > 1 )
        {
            header = ~header; // Stereo flag
        }

        *pOutBuffer = header;
        *(uint16_t*)(pOutBuffer + 1) = AudioLib_SwapU16((uint16_t)pCompressorState->unknown2);
        uint8_t* pOut = pOutBuffer + 3;
        if ( numChans > 1 )
        {
            *pOut = pCompressorState->unknown1;
            *(uint16_t*)(pOutBuffer + 4) = AudioLib_SwapU16((uint16_t)pCompressorState->unknown3);
            pOut = pOutBuffer + 6;
        }

        return AudioLib_CompressBlock(pCompressorState, pOut, (int16_t*)pInBuffer, (unsigned int)size / (numChans * 2), numChans, 1, 1) + numChans * 3;
    }

    // WVSM
    static const uint8_t aWvsmHeader[10] = { 0xE4, 0x11, 0x11, 0x64, 0x22, 0x22, 'W', 'V', 'S', 'M' };
    memcpy(pOutBuffer, aWvsmHeader, sizeof(aWvsmHeader));

    uint8_t* pOut = pOutBuffer + 10;
    while ( size )
    {
        unsigned int remaining = 0;
        if ( (unsigned int)size > 0x1000 )
        {
            remaining = size - 0x1000;
            size      = 0x1000;
        }

        pOut += AudioLib_WVSMCompressBlock(pOut, pInBuffer, size, NULL);
        pInBuffer += 0x1000;
        size = remaining;
    }

    // Note, the size of header is added twice to the returned size
    return (pOut - pOutBuffer) + 10;
}

void J3DAPI AudioLib_ResetCompressor(tAudioCompressorState* pState)
{
    pState->unknown1 = 0;
    pState->unknown0 = 0;
    pState->unknown3 = 0;
    pState->unknown2 = 0;
}

void J3DAPI AudioLib_Uncompress(tAudioCompressorState* pCompressorState, uint8_t* pOutSndData, const uint8_t* pCompressedData, unsigned int size)
{
    unsigned int numChannels = 1;
    pCompressorState->unknown0 = *pCompressedData;
    if ( (int8_t)*pCompressedData < 0 )
    {
        pCompressorState->unknown0 = ~*pCompressedData;
        numChannels = 2;
    }

    const uint8_t* pIn = pCompressedData + 3;
    pCompressorState->unknown2 = (int16_t)AudioLib_SwapU16(AudioLib_ReadU16(pCompressedData + 1));
    if ( numChannels > 1 )
    {
        pCompressorState->unknown1 = *pIn;
        pIn = pCompressedData + 6;
        pCompressorState->unknown3 = (int16_t)AudioLib_SwapU16(AudioLib_ReadU16(pCompressedData + 4));
    }

    if ( numChannels == 2 && pCompressorState->unknown2 == 0x1111 && pCompressorState->unknown1 == 0x64 && pCompressorState->unknown3 == 0x2222 && memcmp(pIn, "WVSM", 4) == 0 )
    {
        pIn += 4;
        while ( size )
        {
            unsigned int remaining = 0;
            if ( size > 0x1000 )
            {
                remaining = size - 0x1000;
                size      = 0x1000;
            }

            pIn += AudioLib_WVSMUncompressBlock(pOutSndData, pIn, size);
            pOutSndData += 0x1000;
            size = remaining;
        }

        return;
    }

    AudioLib_UncompressBlock(pCompressorState, pOutSndData, pIn, size / (numChannels * 2), numChannels, 1);
}

int J3DAPI AudioLib_GetMouthPosition(uint8_t* pData, int a2, uint8_t* pMouthPosX, uint8_t* pMouthPosY)
{
    uint8_t mouthPosX = 0;
    uint8_t mouthPosY = 0;
    int bError        = 0;

    if ( memcmp(pData, "SYNC", 4) == 0 && (a2 & 0xFFF00000) == 0 )
    {
        // Binary search entry by time
        const uint32_t* aEntries = (const uint32_t*)(pData + 8);
        int numEntries = *(const int*)(pData + 4);
        uint32_t time  = (a2 & 0xFFFFFFF0) * 0x1000;

        int hi  = numEntries - 1;
        int idx = 0;
        if ( aEntries[numEntries - 1] < time )
        {
            time = 0;
            idx  = hi;
        }

        int lo = 0;
        if ( time )
        {
            for ( ;; )
            {
                int curHi = hi;
                idx = (curHi - lo) / 2 + lo;
                if ( lo + 1 >= curHi || time == (aEntries[idx] & 0xFFFF0000) )
                {
                    break;
                }

                hi = idx;
                if ( aEntries[idx] < time )
                {
                    lo = idx;
                    hi = curHi;
                }
            }
        }

        mouthPosX = (uint8_t)(aEntries[idx] >> 8) & 0x7F;
        mouthPosY = (uint8_t)aEntries[idx] & 0x7F;
    }
    else
    {
        bError = 1;
    }

    if ( pMouthPosX )
    {
        *pMouthPosX = mouthPosX;
    }

    if ( pMouthPosY )
    {
        *pMouthPosY = mouthPosY;
    }

    return bError;
}

int J3DAPI AudioLib_GenerateLipSynchBlock(uint8_t* pOutData, const uint8_t* pSndData, unsigned int a3, char a4, char a5, int sampleRate, int bitsPerSample, int numChannels, int a9, int sndDataSize)
{
    if ( !sndDataSize || !pSndData || bitsPerSample != 16 || numChannels != 1 )
    {
        return 0;
    }

    if ( a9 )
    {
        pSndData = pSndData + 1;
    }

    uint32_t* pOutEntry = (uint32_t*)(pOutData + 8);
    memcpy(pOutData, "SYNC", 4);
    pOutData[4] = 0;

    // Build masks of mouth positions bits
    unsigned int maskX = 0, maskY = 0;
    uint8_t valX = (uint8_t)(a4 - 1);
    uint8_t valY = (uint8_t)(a5 - 1);
    for ( int i = 0; i < 7; ++i )
    {
        maskX <<= 1;
        maskY <<= 1;
        if ( valX )
        {
            maskX |= 1;
        }

        if ( valY )
        {
            maskY |= 1;
        }

        valX >>= 1;
        valY >>= 1;
    }

    // Calc max amplitude and number of zero crossings
    int numCrossings = 0;
    int maxAmp       = 0;
    unsigned int sumAmp = 0;
    if ( sndDataSize > 2 )
    {
        const uint8_t* pSample = pSndData;
        for ( unsigned int count = (unsigned int)(sndDataSize - 1) >> 1; count; --count )
        {
            int16_t sample     = *(const int16_t*)(pSample + 2);
            int16_t prevSample = *(const int16_t*)pSample;
            pSample += 2;

            int amp = sample < 0 ? -sample : sample;
            if ( sample >= 0 && prevSample < 0 )
            {
                ++numCrossings;
            }

            if ( maxAmp < amp )
            {
                maxAmp = amp;
            }

            sumAmp += amp;
        }
    }

    char aDebug[200];
    STD_FORMAT(aDebug, "MaxAmp: %d  Avg: %d", maxAmp, sumAmp / ((unsigned int)sndDataSize >> 1));

    int aMaxAmps[15];
    for ( size_t i = 0; i < STD_ARRAYLEN(aMaxAmps); ++i )
    {
        aMaxAmps[i] = maxAmp;
    }

    unsigned int chunkSize      = (unsigned int)(sampleRate * 2) / a3;
    unsigned int totalCrossings = chunkSize * numCrossings;

    int curMaxAmpIdx       = 0;
    unsigned int msecTime  = 0;
    int processedSize      = 0;
    unsigned int prevPosX  = 0;
    unsigned int prevPosY  = 0;
    unsigned int lastPosX  = 0xFFFFFFFF;
    unsigned int lastPosY  = 0xFFFFFFFF;

    for ( unsigned int nextSize = chunkSize; (int)nextSize <= sndDataSize; nextSize = chunkSize + processedSize )
    {
        int chunkSumAmp = 0;
        int chunkMaxAmp = 0;
        int chunkCrossings = 0;
        if ( (int)chunkSize > 2 )
        {
            const uint8_t* pSample = pSndData;
            for ( unsigned int count = (chunkSize - 1) >> 1; count; --count )
            {
                int16_t sample     = *(const int16_t*)(pSample + 2);
                int16_t prevSample = *(const int16_t*)pSample;
                pSample += 2;

                int amp = sample < 0 ? -sample : sample;
                if ( sample >= 0 && prevSample < 0 )
                {
                    ++chunkCrossings;
                }

                if ( chunkMaxAmp < amp )
                {
                    chunkMaxAmp = amp;
                }

                chunkSumAmp += amp;
            }
        }

        // Average of last 15 max amplitudes
        int avgMaxAmp = 0;
        for ( int i = curMaxAmpIdx; i < curMaxAmpIdx + 15; ++i )
        {
            avgMaxAmp += aMaxAmps[i % 15];
        }

        avgMaxAmp /= 15;
        if ( avgMaxAmp == 0 )
        {
            avgMaxAmp = 1;
        }

        if ( avgMaxAmp < maxAmp / 4 )
        {
            avgMaxAmp = maxAmp / 4;
        }

        aMaxAmps[curMaxAmpIdx] = chunkMaxAmp;

        int openness = (chunkMaxAmp * 60) / avgMaxAmp;
        int width    = (chunkCrossings * 50) / (int)(totalCrossings / (unsigned int)sndDataSize + 1);
        if ( openness < 25 )
        {
            width = ((width - 37) * openness + 925) / 25;
        }

        if ( width > 99 )
        {
            width = 100;
        }

        unsigned int posX = (width * 127) / 100;
        if ( openness > 99 )
        {
            openness = 100;
        }

        unsigned int posY = (openness * 127) / 100;
        if ( chunkSumAmp / ((int)chunkSize >> 1) < (chunkMaxAmp * 30) / 100 )
        {
            posX = (int)prevPosX < 48 ? 0 : prevPosX - 16;
            posY = (int)prevPosY < 16 ? 0 : prevPosY - 16;
        }

        unsigned int maskedPosX = posX & maskX;
        unsigned int maskedPosY = posY & maskY;
        if ( maskedPosX != lastPosX || maskedPosY != lastPosY )
        {
            *pOutEntry++ = (msecTime & 0xFFFF0000) | ((maskedPosX & 0x7F) << 8) | (maskedPosY & 0x7F);
            lastPosX = maskedPosX;
            lastPosY = maskedPosY;
        }

        msecTime += (int)(0x3E8000 / (uint64_t)a3);
        processedSize += chunkSize;
        pSndData += chunkSize;
        curMaxAmpIdx = (curMaxAmpIdx + 1) % 15;
        prevPosX = posX;
        prevPosY = posY;
    }

    *pOutEntry = msecTime & 0xFFFF0000;

    int numEntries = (((uint8_t*)pOutEntry + 4 - pOutData) >> 2) - 2;
    *(int*)(pOutData + 4) = numEntries;
    return numEntries * 4 + 8;
}

// ADPCM compression
int J3DAPI AudioLib_CompressBlock(tAudioCompressorState* pCompressorState, uint8_t* pOutBuffer, int16_t* a3, int a4, unsigned int numChannels, int a6, int bStateInitialized)
{
    if ( !bStateInitialized )
    {
        AudioLib_ResetCompressor(pCompressorState);
    }

    uint8_t* pOut         = pOutBuffer;
    unsigned int bitPos   = 0;
    unsigned int bitBuf   = 0;
    uint8_t* aIndices     = &pCompressorState->unknown0;
    int16_t* aPredictions = &pCompressorState->unknown2;

    for ( unsigned int ch = 0; ch < numChannels; ++ch )
    {
        int index     = (int8_t)aIndices[ch];
        int predicted = aPredictions[ch];

        const uint16_t* pIn = (const uint16_t*)a3 + ch;
        for ( int n = a4; n; --n )
        {
            int stepAccum = 0;
            uint8_t code  = 0;
            uint8_t sign  = 0;
            int step      = (uint16_t)AudioLib_aStepTable[index];

            // Note, big endian sample is not sign extended
            int sample = a6 ? (int16_t)*pIn : AudioLib_SwapU16(*pIn);
            int diff   = sample - predicted;

            uint8_t numBits  = AudioLib_aStepBits[index];
            int signMask     = 1 << (numBits - 1);
            uint8_t escape   = (uint8_t)signMask - 1;
            if ( diff < 0 )
            {
                sign = (uint8_t)signMask;
                diff = -diff;
            }

            int bitVal = signMask >> 1;
            for ( int i = numBits - 1; i; --i )
            {
                if ( diff >= step )
                {
                    diff -= step;
                    code |= (uint8_t)bitVal;
                    stepAccum += step;
                }

                step >>= 1;
                bitVal >>= 1;
            }

            if ( code )
            {
                stepAccum += step;
            }

            // Write code bits
            uint8_t freeBits = 8 - (bitPos & 7);
            bitBuf = (bitBuf << numBits) | (uint16_t)(sign | code);
            bitPos += numBits;
            if ( numBits >= freeBits )
            {
                *pOut++ = (uint8_t)((uint16_t)bitBuf >> (uint16_t)(numBits - freeBits));
            }

            if ( code == escape )
            {
                // Write raw sample
                predicted = a6 ? (int16_t)*pIn : AudioLib_SwapU16(*pIn);

                unsigned int shift = bitPos & 7;
                uint16_t val = (uint16_t)(((bitBuf & 0xFF) << 8) | ((predicted >> 8) & 0xFF));
                *pOut++ = (uint8_t)(val >> shift);

                bitBuf = (((predicted >> 8) & 0xFF) << 8) | (predicted & 0xFF);
                *pOut++ = (uint8_t)((uint16_t)bitBuf >> shift);
            }
            else
            {
                if ( sign )
                {
                    stepAccum = -stepAccum;
                }

                predicted += stepAccum;
                if ( predicted < -0x8000 )
                {
                    predicted = -0x8000;
                }
                else if ( predicted > 0x7FFF )
                {
                    predicted = 0x7FFF;
                }
            }

            index += AudioLib_aIndexTableTable[numBits][code];
            if ( index < 0 )
            {
                index = 0;
            }

            if ( index > 88 )
            {
                index = 88;
            }

            pIn += numChannels;
        }

        aIndices[ch]     = (uint8_t)index;
        aPredictions[ch] = (int16_t)predicted;
    }

    // Flush remaining bits
    if ( (bitPos & 7) != 0 )
    {
        *pOut++ = (uint8_t)(bitBuf << (8 - (bitPos & 7)));
    }

    return pOut - pOutBuffer;
}


// ADPCM decompress
void J3DAPI AudioLib_UncompressBlock(tAudioCompressorState* pCompressorState, uint8_t* pOutData, const uint8_t* pInData, int size, unsigned int numChannels, int bStateInited)
{
    if ( !AudioLib_bDeltaTableInitialized )
    {
        for ( unsigned int code = 0; code < 64; ++code )
        {
            for ( size_t i = 0; i < 89; ++i )
            {
                uint16_t step  = (uint16_t)AudioLib_aStepTable[i];
                uint16_t delta = 0;
                for ( unsigned int mask = 0x20; mask; mask >>= 1 )
                {
                    if ( (code & mask) != 0 )
                    {
                        delta += step;
                    }

                    step >>= 1;
                }

                AudioLib_aDeltaTables[i][code] = delta;
            }
        }

        AudioLib_bDeltaTableInitialized = 1;
    }

    if ( !bStateInited )
    {
        AudioLib_ResetCompressor(pCompressorState);
    }

    uint8_t* aIndices     = &pCompressorState->unknown0;
    int16_t* aPredictions = &pCompressorState->unknown2;

    // Note, bit position and buffer are shared between channels
    int bitPos      = 0;
    uint16_t bitBuf = (uint16_t)((pInData[0] << 8) | pInData[1]);
    const uint8_t* pIn = pInData + 2;

    for ( unsigned int ch = 0; ch < numChannels; ++ch )
    {
        int index     = (int8_t)aIndices[ch];
        int predicted = aPredictions[ch];
        uint8_t* pOut = pOutData + ch * 2;

        for ( int n = size; n; --n )
        {
            uint8_t numBits      = AudioLib_aStepBits[index];
            unsigned int signMask = 1u << (numBits - 1);
            uint8_t escape       = (uint8_t)signMask - 1;

            bitPos += numBits;
            unsigned int code = ((uint32_t)bitBuf >> (16 - bitPos)) & (signMask | escape);
            if ( bitPos > 7 )
            {
                bitPos -= 8;
                bitBuf = (uint16_t)((bitBuf << 8) | *pIn++);
            }

            if ( (code & signMask) != 0 )
            {
                code ^= signMask;
            }
            else
            {
                signMask = 0;
            }

            if ( (uint8_t)code == escape )
            {
                // Read raw sample
                uint8_t hi   = (uint8_t)(((uint32_t)bitBuf << bitPos) >> 8);
                uint16_t buf = (uint16_t)((bitBuf << 8) | *pIn++);
                uint8_t lo   = (uint8_t)(buf >> (8 - bitPos));
                predicted    = (int16_t)((hi << 8) | lo);

                bitBuf = (uint16_t)((buf << 8) | *pIn++);
            }
            else
            {
                unsigned int delta = AudioLib_aDeltaTables[index][code << (7 - numBits)];
                if ( (code & 0xFFFF) != 0 )
                {
                    delta += (uint32_t)(int32_t)AudioLib_aStepTable[index] >> (numBits - 1);
                }

                if ( (signMask & 0xFFFF) != 0 )
                {
                    predicted = predicted - delta;
                    if ( predicted <= -0x8000 )
                    {
                        predicted = -0x8000;
                    }
                }
                else
                {
                    predicted = delta + predicted;
                    if ( predicted >= 0x7FFF )
                    {
                        predicted = 0x7FFF;
                    }
                }
            }

            *(int16_t*)pOut = (int16_t)predicted;
            pOut += numChannels * 2;

            index += AudioLib_aIndexTableTable[numBits][code];
            if ( index <= 0 )
            {
                index = 0;
            }
            else if ( index >= 88 )
            {
                index = 88;
            }
        }

        aIndices[ch]     = (uint8_t)index;
        aPredictions[ch] = (int16_t)predicted;
    }
}

int J3DAPI AudioLib_WVSMCompressBlock(uint8_t* pOutBuffer, const uint8_t* pInBuffer, int blockSize, FILE* pFile)
{
    const uint8_t* pInBuffer_1;
    int size_1;
    unsigned int sampleCount;
    int curLeftSamp;
    const uint8_t* v8;
    int i;
    int curRightSamp;
    int j;
    int kRight_1;
    int* pLeft;
    int kLeft;
    int* pRight;
    int rsavedBits;
    int lsavedBits_1;
    int numSlots;
    int* pRight_1;
    int* pLeft_1;
    int v21;
    int leftOffset;
    char kRight_2;
    int* v24;
    int v25;
    int* v26;
    int v27;
    const int16_t* pCurIn;
    uint8_t* pCurOut;
    int curLeftSample;
    const int16_t* pRightIn;
    int leftSampleOffseted;
    int leftSampeCompressed;
    int v34;
    uint8_t* v35;
    int curRightSampe;
    uint8_t* pRightOut;
    int rightSampleOffseted;
    int rightSampleCompressed;
    int v40;
    uint8_t* v41;
    int compressedSize;
    int kRight;
    int v44;
    unsigned int kLeft_1;
    int lsavedBits;
    int v47;
    int rightOffset;
    int leftBits[17];
    int rightBits[17];
    int blockSizea;

    pInBuffer_1 = pInBuffer;

    memset(leftBits, 0, sizeof(leftBits));

    size_1 = blockSize >> 1;
    blockSizea = size_1;

    memset(rightBits, 0, sizeof(rightBits));

    if ( size_1 > 0 )
    {
        int outOfBoundsLeft = 0;
        int outOfBoundsRight = 0;
        sampleCount = (unsigned int)(size_1 + 1) >> 1;

        // Note in debug version of Indy3D.exe the loop is the same only the loop range is different.
        // In debug version the loop range is defined as counter = 0; while(counter < size_1) {... counter += 2; }
        // This change still causes the rightBits or leftBits to be rad out of bounds, 
        // so the change must be somewhere else in order for the debug version to run normally
        do
        {
            curLeftSamp = *(int16_t*)pInBuffer_1;
            v8 = pInBuffer_1 + 2;
            if ( curLeftSamp < 0 )
            {
                curLeftSamp = -curLeftSamp;
            }

            for ( i = 0; curLeftSamp; ++i ) // [ADD] Fixed init i value (orig. i = 1) which caused out of bounds access
            {
                curLeftSamp >>= 1;
            }

            pInBuffer_1 = v8 + 2;

            if ( i > 16 ) // [ADD] bound check fix
            {
                i = 16;
                outOfBoundsLeft++;
            }
            ++leftBits[i];

            curRightSamp = *((int16_t*)pInBuffer_1 - 1);
            if ( curRightSamp < 0 )
            {
                curRightSamp = -curRightSamp;
            }

            for ( j = 0; curRightSamp; ++j ) // [ADD] Fixed init j value (orig. j = 1)
            {
                curRightSamp >>= 1;
            }

            --sampleCount;

            if ( j > 16 ) // [ADD] bound check fix
            {
                j = 16;
                outOfBoundsRight++;
            }
            ++rightBits[j];
        } while ( sampleCount );

        if ( outOfBoundsLeft || outOfBoundsRight )
        {
            // TODO: make separate log function var for audio lib
          /*  if (Sound_pHS) {
                Sound_pHS->logWarning("AudioLib_WVSMCompressBlock: %d left bits out of bounds, %d right bits out of bounds\n", outOfBoundsLeft, outOfBoundsRight);
            }*/
        }
    }

    kRight_1 = 8;

    pLeft = &leftBits[16];
    for ( kLeft = 8; kLeft > 0; --kLeft )
    {
        if ( *pLeft )
        {
            break;
        }

        --pLeft;
    }

    v44 = kLeft;

    pRight = &rightBits[16];
    do
    {
        if ( *pRight )
        {
            break;
        }

        --kRight_1;
        --pRight;
    } while ( kRight_1 > 0 );

    rsavedBits = 0;
    lsavedBits_1 = 0;

    kRight = kRight_1;
    numSlots = 192;
    pRight_1 = &rightBits[kRight_1 + 8];
    pLeft_1 = &leftBits[kLeft + 8];
    do
    {
        if ( kLeft <= 0 )
        {
            if ( kRight <= 0 )
            {
                break;
            }

            v21 = 0;
        }

        else if ( kRight <= 0 )
        {
            v21 = 1;
        }
        else
        {
            v21 = *pLeft_1 < *pRight_1;
            kLeft = v44;
        }

        if ( v21 )
        {
            if ( *pLeft_1 > numSlots )
            {
                break;
            }

            numSlots -= *pLeft_1;
            --kLeft;
            --pLeft_1;
            v44 = kLeft;
            ++lsavedBits_1;
        }
        else
        {
            if ( *pRight_1 > numSlots )
            {
                break;
            }

            numSlots -= *pRight_1--;
            --kRight;
            ++rsavedBits;
        }
    } while ( numSlots );

    lsavedBits = lsavedBits_1;
    if ( kLeft )
    {
        leftOffset = 1 << (kLeft - 1);
        v47 = leftOffset;
    }
    else
    {
        v47 = 0;
        leftOffset = 0;
    }

    kRight_2 = kRight;
    if ( kRight )
    {
        rightOffset = 1 << (kRight - 1);
    }
    else
    {
        rightOffset = 0;
    }

    if ( pFile )
    {
        fprintf(pFile, "L = %4d:", kLeft);
        v24 = leftBits;
        v25 = 16;
        do
        {
            fprintf(pFile, "%4d,", *v24++);
            --v25;
        } while ( v25 );

        fprintf(pFile, "%4d\n", leftBits[16]);

        fprintf(pFile, "R = %4d:", kRight);
        v26 = rightBits;
        v27 = 16;
        do
        {
            fprintf(pFile, "%4d,", *v26++);
            --v27;
        } while ( v27 );

        fprintf(pFile, "%4d\n", rightBits[16]);

        fprintf(pFile, "%d escape slots used, %dL/%dR bits saved\n\n", 192 - numSlots, lsavedBits, rsavedBits);
        kLeft |= (v44 & 0xff);
        leftOffset = v47;
        kRight_2 = kRight;
    }

    pCurIn = (const int16_t*)pInBuffer;
    *pOutBuffer = 0;
    pOutBuffer[2] = kRight_2 + 16 * kLeft;      // assign sample expander kRight_2 | (kLeft << 4)
    pCurOut = pOutBuffer + 3;

    // compressing block
    if ( blockSizea > 0 )
    {
        kLeft_1 = (unsigned int)(blockSizea + 1) >> 1;// TODO: make new int var
        while ( 1 )
        {
            curLeftSample = *pCurIn;
            pRightIn = pCurIn + 1;
            if ( curLeftSample >= 0 )
            {
                leftSampleOffseted = leftOffset + curLeftSample;
            }
            else
            {
                leftSampleOffseted = curLeftSample - leftOffset;
            }

            if ( leftSampleOffseted < -32767 )
            {
                leftSampleOffseted = -32767;
            }

            if ( leftSampleOffseted > 32767 )
            {
                leftSampleOffseted = 32767;
            }

            leftSampeCompressed = leftSampleOffseted >> kLeft;
            v34 = leftSampeCompressed << kLeft; // leftSampleOffseted could be used instead
            if ( leftSampeCompressed > 127 || leftSampeCompressed < -127 )
            {
                *pCurOut = 0x80;
                v35 = pCurOut + 1;
                *v35 = (v34 >> 8) & 0xff;
                pCurOut = v35 + 1;
                *pCurOut = v34;
            }
            else
            {
                *pCurOut = leftSampeCompressed;
            }

            curRightSampe = *pRightIn;
            pRightOut = pCurOut + 1;
            pCurIn = pRightIn + 1;
            if ( curRightSampe >= 0 )
            {
                rightSampleOffseted = rightOffset + curRightSampe;
            }
            else
            {
                rightSampleOffseted = curRightSampe - rightOffset;
            }

            if ( rightSampleOffseted < -32767 )
            {
                rightSampleOffseted = -32767;
            }

            if ( rightSampleOffseted > 32767 )
            {
                rightSampleOffseted = 32767;
            }

            rightSampleCompressed = rightSampleOffseted >> kRight;
            v40 = rightSampleCompressed << kRight;// rightSampleOffseted could be used instead
            if ( rightSampleCompressed > 127 || rightSampleCompressed < -127 )
            {
                *pRightOut = 0x80;
                v41 = pRightOut + 1;
                *v41 = (v40 >> 8) & 0xff;
                pRightOut = v41 + 1;
                *pRightOut = v40;
            }
            else
            {
                *pRightOut = rightSampleCompressed;
            }

            pCurOut = pRightOut + 1;
            if ( !--kLeft_1 )
            {
                break;
            }

            leftOffset = v47;
        }
    }

    compressedSize = pCurOut - pOutBuffer;

    // assign be compressed size to the begin of out block
    *pOutBuffer = (uint16_t)((uint16_t)pCurOut - (uint16_t)pOutBuffer - 2) >> 8;
    pOutBuffer[1] = (uint8_t)pCurOut - (uint8_t)pOutBuffer - 2;

    return compressedSize;
}

int J3DAPI AudioLib_WVSMUncompressBlock(uint8_t* pOutBuffer, const uint8_t* pInBuffer, int blockSize)
{
    uint8_t shifts = pInBuffer[2]; // Hi nibble left channel shift, lo nibble right channel shift
    int numSamples = blockSize >> 1;
    const uint8_t* pIn = pInBuffer + 3;

    for ( int i = 0; i < numSamples; i += 2 )
    {
        // Left sample
        if ( *pIn == 0x80 )
        {
            *(uint16_t*)pOutBuffer = (uint16_t)(pIn[2] + pIn[1] * 0x100);
            pIn += 3;
        }
        else
        {
            *(int16_t*)pOutBuffer = (int16_t)((int)(int8_t)*pIn << (shifts >> 4));
            pIn += 1;
        }

        if ( i + 1 == numSamples )
        {
            break;
        }

        // Right sample
        if ( *pIn == 0x80 )
        {
            *(uint16_t*)(pOutBuffer + 2) = (uint16_t)(pIn[2] + pIn[1] * 0x100);
            pIn += 3;
        }
        else
        {
            *(int16_t*)(pOutBuffer + 2) = (int16_t)((int)(int8_t)*pIn << (shifts & 0xF));
            pIn += 1;
        }

        pOutBuffer += 4;
    }

    return pIn - pInBuffer;
}
