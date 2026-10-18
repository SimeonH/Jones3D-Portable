#include "std.h"
#include "stdMemory.h"
#include <j3dcore/j3dhook.h>
#include <std/RTI/symbols.h>
#include <std/General/stdUtil.h>

#include <stdlib.h>
#include <string.h>

#define STDMEMORY_HEADERMAGIC   0x12345678
#define STD_MEMORY_FREE_PATTERN 0xDD

static tMemoryBlock stdMemory_aZoneData[8456];
//#define bStartup J3D_DECL_FAR_VAR(bStartup, int)
//#define bOpen J3D_DECL_FAR_VAR(bOpen, int)

static bool bStartup = false;
static bool bOpen    = false;

void stdMemory_InstallHooks(void)
{
    J3D_HOOKFUNC(stdMemory_Startup);
    J3D_HOOKFUNC(stdMemory_Shutdown);
    J3D_HOOKFUNC(stdMemory_Open);
    J3D_HOOKFUNC(stdMemory_Close);
    J3D_HOOKFUNC(stdMemory_Malloc);
    J3D_HOOKFUNC(stdMemory_Free);
    J3D_HOOKFUNC(stdMemory_Realloc);
    J3D_HOOKFUNC(stdMemory_BlockMalloc);
    J3D_HOOKFUNC(stdMemory_BlockFree);
    J3D_HOOKFUNC(stdMemory_BlockRealloc);
    J3D_HOOKFUNC(stdMemory_BlockAlloc);
}

tMemoryState stdMemory_g_curState;

// Small memory blocks are allocated from zones of STDMEMORY_ZONESIZE bytes.
// Each block in zone starts with header, zone memory is terminated with header of size 0.
// Large blocks are allocated directly and have header with block size and NULL zone.

#define STDMEMORY_ZONESIZE        0x7C00
#define STDMEMORY_MAXZONEBLOCK    0x1000
#define STDMEMORY_BLOCKUSED       0x8000
#define STDMEMORY_BLOCKSIZEMASK   0x7FFF

typedef struct sMemoryZoneBlockHeader
{
    uint16_t size;     // Size of block incl. header, STDMEMORY_BLOCKUSED bit set when block is in use
    uint16_t prevSize; // Size of previous block
    tMemoryBlock* pZone;
} tMemoryZoneBlockHeader;
J3D_ASSERT_SIZE32(tMemoryZoneBlockHeader, 8);

// Zone fields: header.size - pointer to zone memory, header.pHeap - largest free block, size - size of largest free block (excl. header)
#define STDMEMORY_ZONEMEM(pZone)     (*(tMemoryZoneBlockHeader**)&(pZone)->header.size)
#define STDMEMORY_ZONELARGEST(pZone) (*(tMemoryZoneBlockHeader**)&(pZone)->header.pHeap)

#define STDMEMORY_NEXTBLOCK(pBlock, size) ((tMemoryZoneBlockHeader*)((uint8_t*)(pBlock) + (size)))

void* J3DAPI stdMemory_BlockMalloc(size_t size)
{
    if ( size > STDMEMORY_MAXZONEBLOCK )
    {
        return stdMemory_BlockAlloc(size);
    }

    size_t alignedSize = (size + 3) & ~3u;

    // Find zone with enough free space
    tMemoryBlock* pZone = &stdMemory_aZoneData[0];
    size_t zoneNum = 0;
    for ( ; zoneNum < STD_ARRAYLEN(stdMemory_aZoneData); ++zoneNum, ++pZone )
    {
        if ( !pZone->bAlloced || alignedSize <= pZone->size )
        {
            break;
        }
    }

    if ( zoneNum == STD_ARRAYLEN(stdMemory_aZoneData) || !pZone->bAlloced )
    {
        // Allocate new zone
        pZone   = &stdMemory_aZoneData[0];
        zoneNum = 0;
        for ( ; zoneNum < STD_ARRAYLEN(stdMemory_aZoneData); ++zoneNum, ++pZone )
        {
            if ( !pZone->bAlloced || !STDMEMORY_ZONEMEM(pZone) )
            {
                break;
            }
        }

        if ( zoneNum == STD_ARRAYLEN(stdMemory_aZoneData) )
        {
            return stdMemory_BlockAlloc(alignedSize);
        }

        tMemoryZoneBlockHeader* pZoneMem = (tMemoryZoneBlockHeader*)malloc(STDMEMORY_ZONESIZE);
        if ( !pZoneMem )
        {
            return stdMemory_BlockAlloc(alignedSize);
        }

        STDMEMORY_ZONELARGEST(pZone) = pZoneMem;
        STDMEMORY_ZONEMEM(pZone)     = pZoneMem;
        pZone->size     = STDMEMORY_ZONESIZE - 16;
        pZone->bAlloced = 1;

        pZoneMem->size     = STDMEMORY_ZONESIZE - 8;
        pZoneMem->prevSize = 0;
        pZoneMem->pZone    = pZone;

        // Terminator block
        tMemoryZoneBlockHeader* pEnd = STDMEMORY_NEXTBLOCK(pZoneMem, STDMEMORY_ZONESIZE - 8);
        pEnd->size     = 0;
        pEnd->prevSize = STDMEMORY_ZONESIZE - 8;
        pEnd->pZone    = pZone;
    }

    // Find best fitting free block
    uint16_t blockSize = (uint16_t)(alignedSize + sizeof(tMemoryZoneBlockHeader));
    tMemoryZoneBlockHeader* pBest = NULL;
    uint16_t bestRemainder = STDMEMORY_ZONESIZE;
    for ( tMemoryZoneBlockHeader* pBlock = STDMEMORY_ZONEMEM(pZone); pBlock->size; )
    {
        uint16_t curSize = pBlock->size;
        if ( (curSize & STDMEMORY_BLOCKUSED) == 0 )
        {
            // Note, unsigned 16 bit arithmetic, too small blocks wrap around
            if ( (uint16_t)(curSize - blockSize) < bestRemainder )
            {
                pBest         = pBlock;
                bestRemainder = (uint16_t)(curSize - blockSize);
            }
        }
        else
        {
            curSize &= STDMEMORY_BLOCKSIZEMASK;
        }

        pBlock = STDMEMORY_NEXTBLOCK(pBlock, curSize);
    }

    STD_ASSERTREL(pBest);

    // Split block
    uint16_t freeSize = pBest->size;
    pBest->size = blockSize | STDMEMORY_BLOCKUSED;
    if ( freeSize != blockSize )
    {
        tMemoryZoneBlockHeader* pRemainder = STDMEMORY_NEXTBLOCK(pBest, blockSize);
        pRemainder->prevSize = blockSize;
        pRemainder->size     = freeSize - blockSize;
        STDMEMORY_NEXTBLOCK(pBest, freeSize)->prevSize = freeSize - blockSize;
    }

    pBest->pZone = pZone;

    // Update zone's largest free block
    if ( pBest == STDMEMORY_ZONELARGEST(pZone) )
    {
        tMemoryZoneBlockHeader* pLargest = NULL;
        uint16_t largestSize = sizeof(tMemoryZoneBlockHeader);
        for ( tMemoryZoneBlockHeader* pBlock = STDMEMORY_ZONEMEM(pZone); pBlock->size; )
        {
            uint16_t curSize = pBlock->size;
            if ( (curSize & STDMEMORY_BLOCKUSED) == 0 )
            {
                if ( largestSize < curSize )
                {
                    largestSize = curSize;
                    pLargest    = pBlock;
                }
            }
            else
            {
                curSize &= STDMEMORY_BLOCKSIZEMASK;
            }

            pBlock = STDMEMORY_NEXTBLOCK(pBlock, curSize);
        }

        STDMEMORY_ZONELARGEST(pZone) = pLargest;
        if ( largestSize > sizeof(tMemoryZoneBlockHeader) )
        {
            pZone->size = (uint16_t)(largestSize - sizeof(tMemoryZoneBlockHeader));
        }
        else
        {
            pZone->size = 0;
        }
    }

    return &pBest[1];
}

void J3DAPI stdMemory_BlockFree(void* pMemory)
{
    tMemoryZoneBlockHeader* pBlock = (tMemoryZoneBlockHeader*)pMemory - 1;
    STD_ASSERTREL(pMemory);

    tMemoryBlock* pZone = pBlock->pZone;
    if ( !pZone )
    {
        // Large block
        free(pBlock);
        return;
    }

    if ( (pBlock->size & STDMEMORY_BLOCKUSED) == 0 || (uint16_t)(pZone - stdMemory_aZoneData) >= STD_ARRAYLEN(stdMemory_aZoneData) )
    {
        STDLOG_WARNING("Attempting to dispose a bogus or already-disposed-of block!");
        return;
    }

    // Merge with next and previous free blocks
    unsigned int size = pBlock->size & STDMEMORY_BLOCKSIZEMASK;
    tMemoryZoneBlockHeader* pNext = STDMEMORY_NEXTBLOCK(pBlock, size);
    if ( (pNext->size & STDMEMORY_BLOCKUSED) == 0 )
    {
        size += pNext->size;
    }

    tMemoryZoneBlockHeader* pPrev = (tMemoryZoneBlockHeader*)((uint8_t*)pBlock - pBlock->prevSize);
    if ( (pPrev->size & STDMEMORY_BLOCKUSED) == 0 )
    {
        size += pPrev->size;
        pBlock = pPrev;
    }

    pBlock->size = (uint16_t)size;
    STDMEMORY_NEXTBLOCK(pBlock, size & 0xFFFF)->prevSize = (uint16_t)size;

    size_t freeSize = (size & 0xFFFF) - sizeof(tMemoryZoneBlockHeader);
    if ( pZone->size < freeSize )
    {
        if ( (uint16_t)size == STDMEMORY_ZONESIZE - 8 )
        {
            // Whole zone is free, release zone memory
            // Note, bAlloced flag is kept and zone is reused when zone memory is NULL
            free(STDMEMORY_ZONEMEM(pZone));
            STDMEMORY_ZONEMEM(pZone)     = NULL;
            pZone->size                  = 0;
            STDMEMORY_ZONELARGEST(pZone) = NULL;
            return;
        }

        pZone->size = freeSize;
        STDMEMORY_ZONELARGEST(pZone) = pBlock;
    }
}

void* J3DAPI stdMemory_BlockRealloc(void* pData, size_t size)
{
    if ( !pData )
    {
        return stdMemory_BlockMalloc(size);
    }

    if ( !size )
    {
        stdMemory_BlockFree(pData);
        return NULL;
    }

    void* pNewData = stdMemory_BlockMalloc(size);

    // Get size of old block incl. header
    const tMemoryZoneBlockHeader* pBlock = (const tMemoryZoneBlockHeader*)pData - 1;
    size_t oldSize;
    if ( !pBlock->pZone )
    {
        oldSize = ((const tMemoryBlockHeader*)pBlock)->size;
    }
    else
    {
        oldSize = pBlock->size & STDMEMORY_BLOCKSIZEMASK;
    }

    if ( !pNewData )
    {
        return NULL;
    }

    if ( oldSize - sizeof(tMemoryZoneBlockHeader) <= size )
    {
        size = oldSize - sizeof(tMemoryZoneBlockHeader);
    }

    memcpy(pNewData, pData, size);
    stdMemory_BlockFree(pData);
    return pNewData;
}

void* J3DAPI stdMemory_BlockAlloc(size_t size)
{
    tMemoryBlockHeader* pHeader = (tMemoryBlockHeader*)malloc(size + sizeof(tMemoryBlockHeader));
    if ( !pHeader )
    {
        return NULL;
    }

    pHeader->size  = size + sizeof(tMemoryBlockHeader);
    pHeader->pHeap = NULL;
    return &pHeader[1];
}

int stdMemory_Startup(void)
{
    STD_ASSERTREL(bStartup == false);
    memset(&stdMemory_g_curState, 0, sizeof(stdMemory_g_curState));
    bStartup = true;
    return 1;
}

void stdMemory_Shutdown(void)
{
    STD_ASSERTREL(bStartup == true);
    // TODO: Should we close the system here?
    bStartup = false;
}

int stdMemory_Open(void)
{
    STD_ASSERTREL(bStartup == true);
    if ( bOpen )
    {
        STDLOG_ERROR("Warning: System already open!\n");
        return 0;
    }
    else
    {
        bOpen = true;
        return 1;
    }
}

void stdMemory_Close(void)
{
    if ( bOpen )
    {
        STDLOG_ERROR("Warning: System already closed!\n");
        return;
    }

    STDLOG_STATUS("Maximum dynamic memory used: %d bytes.\n", stdMemory_g_curState.maxBytes);
    if ( stdMemory_g_curState.totalAllocs || stdMemory_g_curState.totalBytes )
    {
        STDLOG_ERROR("UNFREED MEMORY ALERT!\n\n");
        std_g_pHS->pErrorPrint("File\tLine\tSize\tNumber\n\n");

        tMemoryHeader* pCur = stdMemory_g_curState.header.pNext;
        size_t count = 0;
        while ( pCur && count < 100 )
        {
            std_g_pHS->pErrorPrint("%s\t%d\t%d\t%d\n", pCur->pFilename, pCur->line, pCur->size, pCur->number);
            pCur = pCur->pNext;;
            ++count;
        }
    }

    bOpen = false;
}

void* J3DAPI stdMemory_Malloc(size_t size, const char* pFilename, size_t line)
{
    size_t totalBytes;
    tMemoryHeader* pData;

    pData = (tMemoryHeader*)std_g_pHS->pMalloc(size + sizeof(tMemoryHeap));
    if ( !pData )
    {
        return 0;
    }

    pData->number = stdMemory_g_curState.totalAllocs;
    pData->id = (uint32_t)(uintptr_t)pData;
    pData->size = size;
    pData->pFilename = pFilename;
    pData->line = line;
    pData->pNext = stdMemory_g_curState.header.pNext;
    if ( pData->pNext )
    {
        pData->pNext->pPrev = pData;
    }

    pData->pPrev = &stdMemory_g_curState.header;
    stdMemory_g_curState.header.pNext = pData;

    memset(&pData[1], 0xCC, size);

    pData->magic = STDMEMORY_HEADERMAGIC;
    *(int*)((char*)&pData[1].number + size) = STDMEMORY_HEADERMAGIC;
    stdMemory_g_curState.totalBytes += size;

    if ( stdMemory_g_curState.maxBytes <= stdMemory_g_curState.totalBytes )
    {
        totalBytes = stdMemory_g_curState.totalBytes;
    }
    else
    {
        totalBytes = stdMemory_g_curState.maxBytes;
    }

    stdMemory_g_curState.maxBytes = totalBytes;
    ++stdMemory_g_curState.totalAllocs;
    return &pData[1];
}

void J3DAPI stdMemory_Free(void* pBytes)
{
    tMemoryHeader* pHeader = (tMemoryHeader*)((char*)pBytes - sizeof(tMemoryHeader));
    STD_ASSERTREL((pHeader != NULL) && ((uint32_t)(uintptr_t)pHeader == pHeader->id));
    STD_ASSERTREL(pHeader->magic == STDMEMORY_HEADERMAGIC);
    STD_ASSERTREL(*(uint32_t*)((uint8_t*)pBytes + pHeader->size) == STDMEMORY_HEADERMAGIC);

    memset(pBytes, STD_MEMORY_FREE_PATTERN, pHeader->size);
    if ( pHeader->pNext )
    {
        pHeader->pNext->pPrev = pHeader->pPrev;
    }

    STD_ASSERTREL(pHeader->pPrev);
    pHeader->pPrev->pNext = pHeader->pNext;
    stdMemory_g_curState.totalBytes -= pHeader->size;
    --stdMemory_g_curState.totalAllocs;
    std_g_pHS->pFree(pHeader);
}

void* J3DAPI stdMemory_Realloc(void* pBytes, size_t size, const char* pFilename, size_t line)
{
    size_t totalBytes;
    tMemoryHeap* pHeap;
    tMemoryHeader* pHeader;
    size_t v8;

    if ( !pBytes )
    {
        return stdMemory_Malloc(size, pFilename, line);
    }

    if ( size )
    {
        pHeader = (tMemoryHeader*)((char*)pBytes - sizeof(tMemoryHeader));
        STD_ASSERTREL((pHeader != NULL) && ((uint32_t)(uintptr_t)pHeader == pHeader->id));
        v8 = pHeader->size;
        pHeap = (tMemoryHeap*)std_g_pHS->pRealloc(pHeader, size + sizeof(tMemoryHeap));
        if ( pHeap )
        {
            pHeap->header.id = (uint32_t)(uintptr_t)pHeap;
            pHeap->header.size = size;
            pHeap->header.pFilename = pFilename;
            pHeap->header.line = line;
            if ( pHeap->header.pNext )
            {
                pHeap->header.pNext->pPrev = &pHeap->header;
            }

            if ( pHeap->header.pPrev )
            {
                pHeap->header.pPrev->pNext = &pHeap->header;
            }

            pHeap->header.magic = STDMEMORY_HEADERMAGIC;
            *(void**)((char*)&pHeap->pMemory + size) = (void*)STDMEMORY_HEADERMAGIC;
            stdMemory_g_curState.totalBytes += size - v8;
            if ( stdMemory_g_curState.maxBytes <= stdMemory_g_curState.totalBytes )
            {
                totalBytes = stdMemory_g_curState.totalBytes;
            }
            else
            {
                totalBytes = stdMemory_g_curState.maxBytes;
            }

            stdMemory_g_curState.maxBytes = totalBytes;
            return &pHeap->pMemory;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        stdMemory_Free(pBytes);
        return 0;
    }
}
