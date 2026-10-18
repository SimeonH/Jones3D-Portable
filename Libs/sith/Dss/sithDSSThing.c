#include "sithDSSThing.h"
#include <j3dcore/j3dhook.h>

#include <rdroid/Math/rdVector.h>

#include <rdroid/Engine/rdPuppet.h>
#include <rdroid/Engine/rdThing.h>
#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Primitives/rdPolyline.h>

#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithComm.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Dss/sithDSS.h>
#include <sith/Dss/sithMulti.h>
#include <sith/Engine/sithPathMove.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/Main/sithMain.h>
#include <sith/RTI/symbols.h>
#include <sith/World/sithActor.h>
#include <sith/World/sithItem.h>
#include <sith/World/sithModel.h>
#include <sith/World/sithSector.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithSurface.h>
#include <sith/World/sithTemplate.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>

#include <sound/Sound.h>

#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/Win95/stdComm.h>

#include <string.h>


// Copies name as zero padded 64 byte string, same as the original strncpy(pDest, pSrc, 63) + null termination
static void sithDSSThing_CopyName(char pDest[64], const char* pSrc)
{
    memset(pDest, 0, 64);
    memcpy(pDest, pSrc, strnlen(pSrc, 63));
}

void sithDSSThing_InstallHooks(void)
{

    // Uncomment only lines for functions that have full definition and doesn't call original function (non-thunk functions)

    J3D_HOOKFUNC(sithDSSThing_Pos);
    J3D_HOOKFUNC(sithDSSThing_ProcessPos);
    J3D_HOOKFUNC(sithDSSThing_MovePos);
    J3D_HOOKFUNC(sithDSSThing_ProcessMovePos);
    J3D_HOOKFUNC(sithDSSThing_UpdateState);
    J3D_HOOKFUNC(sithDSSThing_ProcessStateUpdate);
    J3D_HOOKFUNC(sithDSSThing_PlaySound);
    J3D_HOOKFUNC(sithDSSThing_ProcessPlaySound);
    J3D_HOOKFUNC(sithDSSThing_PlaySoundMode);
    J3D_HOOKFUNC(sithDSSThing_ProcessPlaySoundMode);
    J3D_HOOKFUNC(sithDSSThing_PlayKey);
    J3D_HOOKFUNC(sithDSSThing_ProcessPlayKey);
    J3D_HOOKFUNC(sithDSSThing_PlayKeyMode);
    J3D_HOOKFUNC(sithDSSThing_ProcessPlayKeyMode);
    J3D_HOOKFUNC(sithDSSThing_SetModel);
    J3D_HOOKFUNC(sithDSSThing_ProcessSetModel);
    J3D_HOOKFUNC(sithDSSThing_StopKey);
    J3D_HOOKFUNC(sithDSSThing_ProcessStopKey);
    J3D_HOOKFUNC(sithDSSThing_StopSound);
    J3D_HOOKFUNC(sithDSSThing_ProcessStopSound);
    J3D_HOOKFUNC(sithDSSThing_Fire);
    J3D_HOOKFUNC(sithDSSThing_ProcessFire);
    J3D_HOOKFUNC(sithDSSThing_Death);
    J3D_HOOKFUNC(sithDSSThing_ProcessDeath);
    J3D_HOOKFUNC(sithDSSThing_DamageThing);
    J3D_HOOKFUNC(sithDSSThing_ProcessDamage);
    J3D_HOOKFUNC(sithDSSThing_FullDescription);
    J3D_HOOKFUNC(sithDSSThing_ProcessFullDescription);
    J3D_HOOKFUNC(sithDSSThing_PathMove);
    J3D_HOOKFUNC(sithDSSThing_ProcessPathMove);
    J3D_HOOKFUNC(sithDSSThing_Attachment);
    J3D_HOOKFUNC(sithDSSThing_ProcessAttachment);
    J3D_HOOKFUNC(sithDSSThing_Take);
    J3D_HOOKFUNC(sithDSSThing_ProcessTake);
    J3D_HOOKFUNC(sithDSSThing_CreateThing);
    J3D_HOOKFUNC(sithDSSThing_ProcessCreateThing);
    J3D_HOOKFUNC(sithDSSThing_DestroyThing);
    J3D_HOOKFUNC(sithDSSThing_ProcessDestroyThing);
    J3D_HOOKFUNC(sithDSSThing_MoveToPos);
}


int J3DAPI sithDSSThing_Pos(const SithThing* pThing, DPID toID, unsigned int dpFlags)
{
    if ( !pThing || pThing->type == SITH_THING_FREE || !pThing->pInSector )
    {
        SITHLOG_STATUS("Asked to send state for invalid thing pointer.\n");
        return 0;
    }

    SITHDSS_STARTOUT(SITHDSS_POS);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHUINT16(pThing->attach.flags);

    SITH_ASSERTREL(pThing->pInSector);
    SITHDSS_PUSHUINT16(sithSector_GetSectorIndex(pThing->pInSector));
    SITHDSS_PUSHVEC3(&pThing->pos);

    rdVector3 pyr;
    rdMatrix_ExtractAngles34(&pThing->orient, &pyr);
    SITHDSS_PUSHVEC3(&pyr);

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        SITHDSS_PUSHUINT32(pThing->moveInfo.physics.flags);
        SITHDSS_PUSHVEC3(&pThing->moveInfo.physics.velocity);
        SITHDSS_PUSHVEC3(&pThing->moveInfo.physics.angularVelocity);
    }

    if ( pThing->type == SITH_THING_PLAYER )
    {
        SITHDSS_PUSHFLOAT(pThing->thingInfo.actorInfo.headPYR.pitch);
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, toID, SITHMESSAGE_STREAM_ALL, dpFlags);
}

int J3DAPI sithDSSThing_ProcessPos(const SithMessage* pMsg)
{
    if ( !sithWorld_g_pCurrentWorld )
    {
        return 0;
    }

    SITHDSS_STARTIN(pMsg);

    int guid = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Bad thing guid %d in DSS::Pos.\n", guid);
        return 0;
    }

    if ( pThing->type == SITH_THING_FREE || !pThing->pInSector )
    {
        SITHLOG_ERROR("Bad thing guid %d in DSS::Pos.\n", guid);
        return 0;
    }

    SithAttachFlag attachFlags = SITHDSS_POPUINT16();
    if ( !attachFlags && pThing->attach.flags )
    {
        sithThing_DetachThing(pThing);
    }

    int secIdx = SITHDSS_POPUINT16();
    SithSector* pSector = sithSector_GetSectorEx(sithWorld_g_pCurrentWorld, secIdx);
    if ( !pSector )
    {
        SITHLOG_ERROR("Bad sector index %d in DSS::Pos.\n", secIdx);
        return 0;
    }

    rdVector3 pos;
    SITHDSS_POPVEC3(&pos);

    rdVector3 pyr;
    SITHDSS_POPVEC3(&pyr);
    rdMatrix_BuildRotate34(&pThing->orient, &pyr);

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        pThing->moveInfo.physics.flags = SITHDSS_POPUINT32();
        SITHDSS_POPVEC3(&pThing->moveInfo.physics.velocity);
        SITHDSS_POPVEC3(&pThing->moveInfo.physics.angularVelocity);
        sithDSSThing_MoveToPos(pThing, &pos, pSector);
    }
    else
    {
        rdVector_Copy3(&pThing->pos, &pos);
        sithThing_SetSector(pThing, pSector, 0);
    }

    if ( pThing->type == SITH_THING_PLAYER )
    {
        rdVector3 headPYR;
        headPYR.pitch = SITHDSS_POPFLOAT();
        headPYR.yaw   = 0.0f;
        headPYR.roll  = 0.0f;
        sithActor_SetHeadPYR(pThing, &headPYR);
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_MovePos(const SithThing* pThing, DPID idTo, unsigned int sendFlags)
{
    if ( !pThing || pThing->type == SITH_THING_FREE || !pThing->pInSector )
    {
        SITHLOG_STATUS("Asked to send state for invalid thing pointer.\n");
        return 0;
    }

    SITHDSS_STARTOUT(SITHDSS_MOVEPOS);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHUINT16(sithSector_GetSectorIndex(pThing->pInSector));
    SITHDSS_PUSHVEC3(&pThing->pos);

    rdVector3 pyr;
    rdMatrix_ExtractAngles34(&pThing->orient, &pyr);
    SITHDSS_PUSHVEC3(&pyr);

    SITHDSS_PUSHUINT32(pThing->moveStatus);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, SITHMESSAGE_STREAM_ALL, sendFlags);
}

int J3DAPI sithDSSThing_ProcessMovePos(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Bad thing guid %d in DSS::Pos.\n", guid);
        return 0;
    }

    if ( pThing->type == SITH_THING_FREE || !pThing->pInSector )
    {
        SITHLOG_ERROR("Bad thing guid %d in DSS::Pos.\n", guid);
        return 0;
    }

    int secIdx = SITHDSS_POPUINT16();
    SithSector* pSector = sithSector_GetSectorEx(sithWorld_g_pCurrentWorld, secIdx);
    if ( !pSector )
    {
        SITHLOG_ERROR("Bad sector index %d in DSS::Pos.\n", secIdx);
        return 0;
    }

    SITHDSS_POPVEC3(&pThing->pos);
    sithThing_SetSector(pThing, pSector, 0);

    rdVector3 pyr;
    SITHDSS_POPVEC3(&pyr);
    rdMatrix_BuildRotate34(&pThing->orient, &pyr);

    switch ( (SithThingMoveStatus)SITHDSS_POPUINT32() )
    {
        case SITHPLAYERMOVE_JUMP_READY:
            sithPlayerActions_JumpStart(pThing);
            break;

        case SITHPLAYERMOVE_PULLINGUP:
            sithPlayerActions_ClimbPullUp(pThing);
            break;

        case SITHPLAYERMOVE_PULLINGUP_1M:
            sithPlayerActions_ClimbOn1m(pThing);
            break;

        case SITHPLAYERMOVE_STRAFING_LEFT:
            sithPlayerActions_StrafeLeft(pThing);
            break;

        case SITHPLAYERMOVE_STRAFING_RIGHT:
            sithPlayerActions_StrafeRight(pThing);
            break;

        case SITHPLAYERMOVE_UNKNOWN_60:
            sithPlayerActions_JumpLeft(pThing);
            break;

        case SITHPLAYERMOVE_JUMPBACK:
            sithPlayerActions_HopBack(pThing);
            break;

        default:
            break;
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_UpdateState(const SithThing* pThing, DPID toID, unsigned int outstream)
{
    if ( !pThing || pThing->type == SITH_THING_FREE || !pThing->pInSector || !sithThing_ValidateThingPointer(sithWorld_g_pCurrentWorld, pThing) )
    {
        SITHLOG_STATUS("Asked to send state for invalid thing pointer.\n");
        return 0;
    }

    SITHDSS_STARTOUT(SITHDSS_THINGSTATE);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHUINT32(pThing->unknownFlags);
    SITHDSS_PUSHUINT32(pThing->msecLifeLeft);

    SITH_ASSERTREL(pThing->pInSector);
    SITHDSS_PUSHUINT16(sithSector_GetSectorIndex(pThing->pInSector));
    SITHDSS_PUSHUINT16(pThing->collide.type);
    SITHDSS_PUSHVEC3(&pThing->pos);
    SITHDSS_PUSHUINT32(pThing->flags);

    if ( pThing->pPuppetClass )
    {
        SITHDSS_PUSHUINT16(pThing->pPuppetState->armedMode);
        SITHDSS_PUSHUINT16(pThing->pPuppetState->moveMode);
    }

    SITHDSS_PUSHFLOAT(pThing->light.color.red);
    SITHDSS_PUSHFLOAT(pThing->light.color.green);
    SITHDSS_PUSHFLOAT(pThing->light.color.blue);

    switch ( pThing->type )
    {
        case SITH_THING_ACTOR:
        case SITH_THING_CORPSE:
        case SITH_THING_PLAYER:
            SITHDSS_PUSHUINT32(pThing->thingInfo.actorInfo.flags);
            break;

        case SITH_THING_WEAPON:
            SITHDSS_PUSHUINT32(pThing->thingInfo.weaponInfo.flags);
            break;

        case SITH_THING_ITEM:
            SITHDSS_PUSHUINT32(pThing->thingInfo.itemInfo.flags);
            if ( (pThing->thingInfo.itemInfo.flags & SITH_ITEM_BACKPACK) != 0 )
            {
                int16_t numItems = (int16_t)pThing->thingInfo.itemInfo.numItems;
                SITHDSS_PUSHINT16(numItems);
                for ( int i = 0; i < numItems; ++i )
                {
                    SITHDSS_PUSHINT16(pThing->thingInfo.itemInfo.aBackpackItems[i].typeId);
                    SITHDSS_PUSHFLOAT(pThing->thingInfo.itemInfo.aBackpackItems[i].amount);
                }
            }
            break;

        default:
            break;
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        SITHDSS_PUSHUINT32(pThing->moveInfo.physics.flags);
    }

    if ( !pThing->pSwapList )
    {
        SITHDSS_PUSHINT16(-1);
    }
    else
    {
        int16_t numEntries = 0;
        for ( const SithThingSwapEntry* pEntry = pThing->pSwapList; pEntry; pEntry = pEntry->pNextEntry )
        {
            ++numEntries;
        }

        SITHDSS_PUSHINT16(numEntries);
        for ( const SithThingSwapEntry* pEntry = pThing->pSwapList; pEntry; pEntry = pEntry->pNextEntry )
        {
            SITHDSS_PUSHINT32(pEntry->entryNum);
            SITHDSS_PUSHINT32(pEntry->meshNum);

            char aModelName[64];
            sithDSSThing_CopyName(aModelName, pEntry->pSrcModel->aName);
            SITHDSS_PUSHSTRING(aModelName, sizeof(aModelName));

            SITHDSS_PUSHINT32(pEntry->srcMeshNum);
        }
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, toID, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessStateUpdate(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Thing guid %x not found.\n", guid);
        return 0;
    }

    if ( pThing->type == SITH_THING_FREE || !pThing->pInSector )
    {
        SITHLOG_STATUS("Warning - got state update for non-existant thing index %d.\n", guid);
        return 0;
    }

    pThing->unknownFlags = SITHDSS_POPUINT32();
    pThing->msecLifeLeft = SITHDSS_POPUINT32();

    int secIdx = SITHDSS_POPUINT16();
    SithSector* pSector = sithSector_GetSectorEx(sithWorld_g_pCurrentWorld, secIdx);
    if ( !pSector )
    {
        SITHLOG_ERROR("Bad sector index %d in DSS::Pos.\n", secIdx);
        return 0;
    }

    pThing->collide.type = SITHDSS_POPUINT16();
    SITHDSS_POPVEC3(&pThing->pos);
    sithThing_SetSector(pThing, pSector, 0);

    // Remote flag is kept as it is
    SithThingFlag flags = SITHDSS_POPUINT32();
    if ( (pThing->flags & SITH_TF_REMOTE) != 0 )
    {
        pThing->flags = flags | SITH_TF_REMOTE;
    }
    else
    {
        pThing->flags = flags & ~SITH_TF_REMOTE;
    }

    if ( pThing->pPuppetClass )
    {
        uint16_t armedMode          = SITHDSS_POPUINT16();
        SithPuppetMoveMode moveMode = SITHDSS_POPUINT16();
        sithPuppet_SetArmedMode(pThing, armedMode);
        sithPuppet_SetMoveMode(pThing, moveMode);
    }

    pThing->light.color.red   = SITHDSS_POPFLOAT();
    pThing->light.color.green = SITHDSS_POPFLOAT();
    pThing->light.color.blue  = SITHDSS_POPFLOAT();
    pThing->light.color.alpha = 0.0f;

    switch ( pThing->type )
    {
        case SITH_THING_ACTOR:
        case SITH_THING_CORPSE:
        case SITH_THING_PLAYER:
            pThing->thingInfo.actorInfo.flags = SITHDSS_POPUINT32();
            break;

        case SITH_THING_WEAPON:
            pThing->thingInfo.weaponInfo.flags = SITHDSS_POPUINT32();
            break;

        case SITH_THING_ITEM:
            pThing->thingInfo.itemInfo.flags = SITHDSS_POPUINT32();
            if ( (pThing->thingInfo.itemInfo.flags & SITH_ITEM_BACKPACK) != 0 )
            {
                // Note, only the lower 16 bits of numItems and typeId are set
                int16_t numItems = SITHDSS_POPINT16();
                *(int16_t*)&pThing->thingInfo.itemInfo.numItems = numItems;
                for ( int i = 0; i < numItems; ++i )
                {
                    *(int16_t*)&pThing->thingInfo.itemInfo.aBackpackItems[i].typeId = SITHDSS_POPINT16();
                    pThing->thingInfo.itemInfo.aBackpackItems[i].amount = SITHDSS_POPFLOAT();
                }
            }
            break;

        default:
            break;
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        pThing->moveInfo.physics.flags = SITHDSS_POPUINT32();
    }

    if ( pThing->pSwapList )
    {
        sithThing_ResetSwapList(pThing);
    }

    int numEntries = SITHDSS_POPINT16();
    if ( numEntries != -1 )
    {
        for ( ; numEntries > 0; --numEntries )
        {
            SITHDSS_POPINT32(); // entryNum, not used
            int meshNum = SITHDSS_POPINT32();

            char aModelName[64];
            SITHDSS_POPSTRING(aModelName, sizeof(aModelName));
            aModelName[STD_ARRAYLEN(aModelName) - 1] = 0;

            int srcMeshNum = SITHDSS_POPINT32();

            rdModel3* pModel = sithModel_Load(aModelName, 1);
            if ( pModel )
            {
                sithThing_AddSwapEntry(pThing, meshNum, pModel, srcMeshNum);
            }
        }
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_PlaySound(const SithThing* pThing, const rdVector3* pPos, tSoundHandle hSnd, float volume, float pan, SoundPlayFlag playflags, int guid, DPID idTo, unsigned int outstream)
{
    SITH_ASSERTREL(hSnd);

    SITHDSS_STARTOUT(SITHDSS_PLAYSOUND);

    SITHDSS_PUSHUINT32(playflags);
    SITHDSS_PUSHFLOAT(volume);
    SITHDSS_PUSHFLOAT(pan);
    SITHDSS_PUSHUINT16(Sound_GetSoundIndex(hSnd));

    if ( (playflags & SOUNDPLAY_THING_POS) != 0 )
    {
        SITH_ASSERTREL(pThing);
        SITHDSS_PUSHINT32(pThing->guid);
    }
    else if ( (playflags & SOUNDPLAY_ABSOLUTE_POS) != 0 )
    {
        SITHDSS_PUSHVEC3(pPos);
    }

    SITHDSS_PUSHINT32(guid);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 0);
}

int J3DAPI sithDSSThing_ProcessPlaySound(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    SoundPlayFlag playflags = SITHDSS_POPUINT32();
    float volume            = SITHDSS_POPFLOAT();
    float pan               = SITHDSS_POPFLOAT();

    uint16_t sndIdx   = SITHDSS_POPUINT16();
    tSoundHandle hSnd = Sound_GetSoundHandle(sndIdx);
    if ( !hSnd )
    {
        SITHLOG_ERROR("Sound index %d not valid in DSS::PlaySound.\n", sndIdx);
        return 0;
    }

    // Note, the trailing sound guid is not used
    if ( (playflags & SOUNDPLAY_THING_POS) != 0 )
    {
        int guid          = SITHDSS_POPINT32();
        SithThing* pThing = sithThing_GetGuidThing(guid);
        if ( !pThing )
        {
            SITHLOG_ERROR("Thing guid %d not found in DSS::PlaySound (sound=%s)\n", guid, Sound_GetSoundFilename(hSnd));
            return 0;
        }

        sithSoundMixer_PlaySoundThing(hSnd, pThing, 1.0f, volume, pan, playflags);
        return 1;
    }

    if ( (playflags & SOUNDPLAY_ABSOLUTE_POS) != 0 )
    {
        rdVector3 pos;
        SITHDSS_POPVEC3(&pos);
        sithSoundMixer_PlaySoundPos(hSnd, &pos, NULL, 1.0f, volume, pan, playflags);
        return 1;
    }

    sithSoundMixer_PlaySound(hSnd, volume, pan, playflags);

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_PlaySoundMode(const SithThing* pThing, int16_t mode, int channel, float a4)
{
    SITH_ASSERTREL(pThing);

    SITHDSS_STARTOUT(SITHDSS_PLAYSOUNDMODE);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHINT32(channel);
    SITHDSS_PUSHFLOAT(a4);
    SITHDSS_PUSHINT16(mode);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, SITHMESSAGE_SENDTOJOINEDPLAYERS, SITHMESSAGE_STREAM_ALL, 0);
}

int J3DAPI sithDSSThing_ProcessPlaySoundMode(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid          = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Thing guid %d not found in DSS::PlaySoundMode\n", guid);
        return 0;
    }

    SITHDSS_POPINT32(); // channel, not used
    float selectionRatio    = SITHDSS_POPFLOAT();
    SithSoundClassMode mode = SITHDSS_POPUINT16();

    if ( selectionRatio < 0.0f )
    {
        sithSoundClass_PlayModeRandom(pThing, mode);
    }
    else
    {
        sithSoundClass_PlayMode(pThing, mode, selectionRatio);
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_PlayKey(const SithThing* pThing, const rdKeyframe* pKey, rdKeyframeFlags flags, int16_t lo, unsigned int trackGUID, DPID idTo, unsigned int outstream)
{
    SITH_ASSERTREL(pThing && pKey);

    SITHDSS_STARTOUT(SITHDSS_PLAYKEY);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHINT32(pKey->idx);
    SITHDSS_PUSHINT16(lo);
    SITHDSS_PUSHUINT32(flags);
    SITHDSS_PUSHUINT32(trackGUID);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 0);
}

int J3DAPI sithDSSThing_ProcessPlayKey(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid          = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Got DSS::PlayKey message for thing GUID %d - not found.\n", guid);
        return 0;
    }

    if ( !pThing->renderData.pPuppet )
    {
        SITHLOG_ERROR("Got DSS::PlayKey message for thing %s - no puppet.\n", pThing->aName);
        return 0;
    }

    int keyIdx       = SITHDSS_POPINT32();
    rdKeyframe* pKey = sithPuppet_GetKeyframeByIndex(keyIdx);
    if ( !pKey )
    {
        SITHLOG_ERROR("Key %d not found in DSS::PlayKey.\n", keyIdx);
        return 0;
    }

    uint16_t lowPriority  = SITHDSS_POPUINT16();
    rdKeyframeFlags flags = SITHDSS_POPUINT32();
    uint32_t trackGUID    = SITHDSS_POPUINT32();

    int trackNum = sithPuppet_PlayKey(pThing->renderData.pPuppet, pKey, lowPriority, lowPriority + 2, flags, NULL);
    if ( trackNum >= 0 )
    {
        pThing->renderData.pPuppet->aTracks[trackNum].guid = trackGUID;
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_PlayKeyMode(const SithThing* pThing, SithPuppetSubMode mode, unsigned int trackGUID, DPID idTo, unsigned int outstream)
{
    SITH_ASSERTREL(pThing);

    SITHDSS_STARTOUT(SITHDSS_PLAYKEYMODE);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHUINT32(trackGUID);
    SITHDSS_PUSHUINT16(mode);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 0);
}

int J3DAPI sithDSSThing_ProcessPlayKeyMode(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid          = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Got DSS::PlayKey message for thing GUID %d - not found.\n", guid);
        return 0;
    }

    if ( !pThing->renderData.pPuppet )
    {
        SITHLOG_ERROR("Got DSS::PlayKeyMode message for thing %s - no puppet.\n", pThing->aName);
        return 0;
    }

    uint32_t trackGUID        = SITHDSS_POPUINT32();
    SithPuppetSubMode submode = SITHDSS_POPUINT16();

    int trackNum = sithPuppet_PlayMode(pThing, submode, NULL);
    if ( trackNum >= 0 )
    {
        pThing->renderData.pPuppet->aTracks[trackNum].guid = trackGUID;
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_SetModel(const SithThing* pThing, DPID idTo)
{
    SITH_ASSERTREL(pThing);
    if ( !pThing || pThing->renderData.type != RD_THING_MODEL3 || !pThing->renderData.data.pModel3 )
    {
        SITHLOG_STATUS("Asked to send state for invalid thing pointer.\n");
        return 0;
    }

    SITHDSS_STARTOUT(SITHDSS_SETMODEL);

    SITHDSS_PUSHINT32(pThing->guid);

    char aModelName[64];
    sithDSSThing_CopyName(aModelName, pThing->renderData.data.pModel3->aName);
    SITHDSS_PUSHSTRING(aModelName, sizeof(aModelName));

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, SITHMESSAGE_STREAM_ALL, 1u);
}

int J3DAPI sithDSSThing_ProcessSetModel(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid          = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Got DSS::PlayKey message for thing GUID %d - not found.\n", guid);
        return 0;
    }

    char aModelName[64];
    SITHDSS_POPSTRING(aModelName, sizeof(aModelName));
    aModelName[STD_ARRAYLEN(aModelName) - 1] = 0;

    rdModel3* pModel = sithModel_Load(aModelName, 1);
    if ( !pModel )
    {
        return 0;
    }

    SITHLOG_STATUS("Changing %s to use model %s.\n", pThing->aName, pModel->aName);
    sithThing_SetThingModel(pThing, pModel);

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_StopKey(const SithThing* pThing, int trackGUID, float fadeTime, DPID idTo, unsigned int outstream)
{
    SITH_ASSERTREL(pThing);

    SITHDSS_STARTOUT(SITHDSS_STOPKEY);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHINT32(trackGUID);
    SITHDSS_PUSHFLOAT(fadeTime);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessStopKey(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid          = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing || !pThing->renderData.pPuppet )
    {
        SITHLOG_ERROR("Bad thing guid %d in DSS::StopKey.\n", guid);
        return 0;
    }

    rdPuppet* pPuppet = pThing->renderData.pPuppet;
    uint32_t trackGUID = SITHDSS_POPUINT32();
    float fadeTime     = SITHDSS_POPFLOAT();

    size_t trackNum = 0;
    for ( ; trackNum < STD_ARRAYLEN(pPuppet->aTracks); ++trackNum )
    {
        if ( pPuppet->aTracks[trackNum].guid == trackGUID )
        {
            break;
        }
    }

    if ( trackNum < STD_ARRAYLEN(pPuppet->aTracks) )
    {
        sithPuppet_StopKey(pPuppet, trackNum, fadeTime);
        return 1;
    }

    SITHLOG_ERROR("Track with guid %x not found.\n", trackGUID);

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_StopSound(tSoundChannelHandle hChannel, float secFadeTime, DPID idTo, unsigned int outstream)
{
    SITHDSS_STARTOUT(SITHDSS_STOPSOUND);

    SITHDSS_PUSHINT32(hChannel ? Sound_GetChannelGUID(hChannel) : -1);
    SITHDSS_PUSHFLOAT(secFadeTime);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessStopSound(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid          = SITHDSS_POPINT32();
    float secFadeTime = SITHDSS_POPFLOAT();

    tSoundChannelHandle hChannel = sithSoundMixer_GetChannelHandle(guid);
    if ( !hChannel )
    {
        SITHLOG_ERROR("Could not find a channel with GUID %d.\n", guid);
        return 1;
    }

    if ( secFadeTime <= 0.0f )
    {
        sithSoundMixer_StopSound(hChannel);
    }
    else
    {
        sithSoundMixer_FadeVolume(hChannel, 0.0f, secFadeTime);
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_Fire(const SithThing* pShooter, const SithThing* pProjectileTemplate, const rdVector3* fireDir, const rdVector3* firePos, tSoundHandle hFireSnd, uint16_t puppetSubmode, float extra, int16_t projectileFlags, float sedRapidFireTime, int projectileGUID, DPID idTo, unsigned int outstream)
{
    SITH_ASSERTREL(pShooter && fireDir && firePos);

    SITHDSS_STARTOUT(SITHDSS_FIRE);

    SITHDSS_PUSHINT32(pShooter->guid);
    SITHDSS_PUSHINT16(projectileFlags);
    SITHDSS_PUSHUINT16(sithTemplate_GetTemplateIndex(pProjectileTemplate));
    SITHDSS_PUSHUINT16(hFireSnd ? Sound_GetSoundIndex(hFireSnd) : (uint16_t)-1);
    SITHDSS_PUSHUINT16(puppetSubmode);
    SITHDSS_PUSHVEC3(firePos);
    SITHDSS_PUSHVEC3(fireDir);
    SITHDSS_PUSHFLOAT(extra);
    SITHDSS_PUSHFLOAT(sedRapidFireTime);
    SITHDSS_PUSHINT32(projectileGUID);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 0);
}

int J3DAPI sithDSSThing_ProcessFire(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid            = SITHDSS_POPINT32();
    SithThing* pShooter = sithThing_GetGuidThing(guid);
    if ( !pShooter )
    {
        SITHLOG_ERROR("Bad shooter guid %d in DSS:Fire.\n", guid);
        return 0;
    }

    SithFireProjectileFlags projectileFlags = SITHDSS_POPUINT16();
    const SithThing* pTemplate              = sithTemplate_GetTemplateByIndex(SITHDSS_POPUINT16());
    tSoundHandle hFireSnd                   = Sound_GetSoundHandle(SITHDSS_POPUINT16());
    SithPuppetSubMode submode               = SITHDSS_POPUINT16();

    rdVector3 firePos, fireDir;
    SITHDSS_POPVEC3(&firePos);
    SITHDSS_POPVEC3(&fireDir);

    float extra          = SITHDSS_POPFLOAT();
    float secRapidFire   = SITHDSS_POPFLOAT();
    int projectileGUID   = SITHDSS_POPINT32();

    SithThing* pProjectile = sithWeapon_WeaponFireProjectile(pShooter, pTemplate, &fireDir, &firePos, hFireSnd, submode, extra, projectileFlags, secRapidFire);
    if ( pProjectile )
    {
        pProjectile->guid = projectileGUID;
        pProjectile->flags |= SITH_TF_REMOTE;
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_Death(const SithThing* pThing, const SithThing* pKiller, char bKillPlayer, DPID to, unsigned int outstream)
{
    SITHDSS_STARTOUT(SITHDSS_DEATH);

    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHINT32(pKiller ? pKiller->guid : -1);
    SITHDSS_PUSHINT8(bKillPlayer);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, to, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessDeath(const SithMessage* pMsg)
{
    SITH_ASSERTREL(pMsg);

    SITHDSS_STARTIN(pMsg);

    int guid         = SITHDSS_POPINT32();
    int killerGuid   = SITHDSS_POPINT32();
    int8_t bKillPlayer = SITHDSS_POPINT8();

    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Got DSS::Death message for thing guid %d I don't have locally.\n", guid);
        return 0;
    }

    SithThing* pKiller = sithThing_GetGuidThing(killerGuid);
    switch ( pThing->type )
    {
        case SITH_THING_FREE:
            SITHLOG_ERROR("Got death message from unused thing %d.\n", pThing->idx);
            break; // Falls to unhandled type log

        case SITH_THING_PLAYER:
            if ( bKillPlayer == 1 )
            {
                sithPlayer_KillPlayer(pThing);
                return 1;
            }
            // Fallthrough

        case SITH_THING_ACTOR:
            sithActor_KillActor(pThing, pKiller, 0);
            return 1;

        default:
            break;
    }

    SITHLOG_ERROR("Got death message for unhandled thing type %d, index %d.\n", pThing->type, pThing->idx);

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_DamageThing(const SithThing* pVictim, const SithThing* pPurpetrator, float damage, SithDamageType hitType, DPID idTo, unsigned int outstream)
{
    SITH_ASSERTREL(pVictim);

    SITHDSS_STARTOUT(SITHDSS_DAMAGE);

    SITHDSS_PUSHINT32(pVictim->guid);
    SITHDSS_PUSHINT32(pPurpetrator ? pPurpetrator->guid : -1);
    SITHDSS_PUSHFLOAT(damage);
    SITHDSS_PUSHUINT16(hitType);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessDamage(const SithMessage* pMsg)
{
    SITH_ASSERTREL(pMsg);

    if ( sithMessage_GetSenderID() != sithMulti_g_serverId )
    {
        return 0;
    }

    SITHDSS_STARTIN(pMsg);

    int guid           = SITHDSS_POPINT32();
    int purpetratorGuid = SITHDSS_POPINT32();

    SithThing* pVictim = sithThing_GetGuidThing(guid);
    if ( !pVictim )
    {
        SITHLOG_ERROR("Got DSS::Death message for thing guid %d I don't have locally.\n", guid);
        return 0;
    }

    SithThing* pPurpetrator = sithThing_GetGuidThing(purpetratorGuid);
    if ( !pPurpetrator )
    {
        pPurpetrator = pVictim;
    }

    float damage           = SITHDSS_POPFLOAT();
    SithDamageType hitType = SITHDSS_POPUINT16();
    sithThing_DamageThing(pVictim, pPurpetrator, damage, hitType);

    SITHDSS_ENDIN;
    return 1;
}

static const char sithDSSThing_aPolylineCogEndName[] = "+plcogend";

int J3DAPI sithDSSThing_FullDescription(const SithThing* pThing, DPID idTo, unsigned int outstream)
{
    SITHDSS_STARTOUT(SITHDSS_THINGFULLDESC);

    SITHDSS_PUSHUINT16(pThing->idx);
    SITHDSS_PUSHUINT16(pThing->type);

    if ( pThing->type != SITH_THING_FREE )
    {
        SITHDSS_PUSHUINT16(sithTemplate_GetTemplateIndex(pThing->pTemplate));
        SITHDSS_PUSHUINT32(pThing->signature);
        SITHDSS_PUSHINT32(pThing->guid);

        SITHDSS_PUSHVEC3(&pThing->pos);
        SITHDSS_PUSHVEC3(&pThing->forceMoveStartPos);
        SITHDSS_PUSHVEC3(&pThing->orient.rvec);
        SITHDSS_PUSHVEC3(&pThing->orient.lvec);
        SITHDSS_PUSHVEC3(&pThing->orient.uvec);

        SITHDSS_PUSHUINT32(pThing->moveStatus);
        SITHDSS_PUSHUINT16(sithSector_GetSectorIndex(pThing->pInSector));
        SITHDSS_PUSHUINT32(pThing->flags);

        SITHDSS_PUSHUINT32(pThing->msecLifeLeft);
        SITHDSS_PUSHUINT32(pThing->msecTimerTime);
        SITHDSS_PUSHUINT32(pThing->msecNextPulseTime);
        SITHDSS_PUSHUINT32(pThing->msecPulseInterval);
        SITHDSS_PUSHFLOAT(pThing->userval);

        SITHDSS_PUSHUINT16(pThing->collide.type);
        SITHDSS_PUSHFLOAT(pThing->collide.size);
        SITHDSS_PUSHFLOAT(pThing->collide.movesize);

        SITHDSS_PUSHVEC4(&pThing->light.color);
        SITHDSS_PUSHFLOAT(pThing->light.minRadius);
        SITHDSS_PUSHFLOAT(pThing->light.maxRadius);

        SITHDSS_PUSHUINT32(pThing->unknownFlags);

        if ( (pThing->flags & SITH_TF_COGLINKED) != 0 )
        {
            SITHDSS_PUSHINT16(pThing->pCog ? pThing->pCog->idx : -1);
            SITHDSS_PUSHINT16(pThing->pCaptureCog ? pThing->pCaptureCog->idx : -1);
        }

        switch ( pThing->type )
        {
            case SITH_THING_ACTOR:
            case SITH_THING_CORPSE:
            case SITH_THING_PLAYER:
            {
                const SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
                SITHDSS_PUSHUINT32(pActor->flags);
                SITHDSS_PUSHFLOAT(pActor->health);
                SITHDSS_PUSHFLOAT(pActor->maxHealth);
                SITHDSS_PUSHFLOAT(pActor->extraSpeed);
                SITHDSS_PUSHVEC3(&pActor->headPYR);
                SITHDSS_PUSHFLOAT(pActor->headLightIntensity.x);
                SITHDSS_PUSHFLOAT(pActor->headLightIntensity.y);
                SITHDSS_PUSHFLOAT(pActor->headLightIntensity.z);
                SITHDSS_PUSHINT32(pThing->aiState);
                SITHDSS_PUSHINT32(pThing->aiArmedModeState);
                SITHDSS_PUSHUINT8(pActor->bForceMovePlay);
                SITHDSS_PUSHUINT8(pActor->bControlsDisabled);
                SITHDSS_PUSHUINT32(pActor->endurance.msecUnderwater);
                SITHDSS_PUSHFLOAT(pActor->maxThrust);
                SITHDSS_PUSHFLOAT(pActor->maxRotVelocity);
                SITHDSS_PUSHFLOAT(pActor->minHeadPitch);
                SITHDSS_PUSHFLOAT(pActor->maxHeadPitch);
                SITHDSS_PUSHFLOAT(pActor->maxHeadYaw);
                SITHDSS_PUSHFLOAT(pActor->maxHeadVelocity);
                SITHDSS_PUSHUINT16(sithThing_GetThingIndex(pActor->pThingMeshAttached));
                SITHDSS_PUSHINT32(pActor->attachMeshNum);
                SITHDSS_PUSHINT16(pActor->pPlayer ? (int16_t)(pActor->pPlayer - sithPlayer_g_aPlayers) : -1);
            } break;

            case SITH_THING_WEAPON:
                SITHDSS_PUSHUINT32(pThing->thingInfo.weaponInfo.flags);
                SITHDSS_PUSHUINT16(pThing->thingInfo.weaponInfo.numRicochets);
                break;

            case SITH_THING_EXPLOSION:
                SITHDSS_PUSHUINT32(pThing->thingInfo.explosionInfo.flags);
                break;

            case SITH_THING_SPRITE:
                SITHDSS_PUSHFLOAT(pThing->thingInfo.spriteInfo.width);
                SITHDSS_PUSHFLOAT(pThing->thingInfo.spriteInfo.height);
                SITHDSS_PUSHFLOAT(pThing->thingInfo.spriteInfo.alpha);
                SITHDSS_PUSHFLOAT(pThing->thingInfo.spriteInfo.rollAngle);
                SITHDSS_PUSHUINT16(sithThing_GetThingIndex(pThing->thingInfo.spriteInfo.pThingMeshAttached));
                SITHDSS_PUSHINT32(pThing->thingInfo.spriteInfo.attachMeshNum);
                break;

            case SITH_THING_POLYLINE:
            {
                SITH_ASSERTREL(pThing->renderData.data.pPolyline);
                rdPolyline* pPolyline = pThing->renderData.data.pPolyline;

                if ( strcmp(pThing->aName, sithDSSThing_aPolylineCogEndName) == 0 )
                {
                    SITHDSS_PUSHUINT16(sithThing_GetThingIndex(pThing->pParent));
                    pPolyline->face.extraLight.alpha = pThing->alpha;
                }

                char aMatName[64];
                sithDSSThing_CopyName(aMatName, pPolyline->face.pMaterial->aName);
                SITHDSS_PUSHSTRING(aMatName, sizeof(aMatName));

                SITHDSS_PUSHFLOAT(pPolyline->length);
                SITHDSS_PUSHFLOAT(pPolyline->baseRadius);
                SITHDSS_PUSHFLOAT(pPolyline->tipRadius);
                SITHDSS_PUSHUINT16(pPolyline->geoMode);
                SITHDSS_PUSHUINT16(pPolyline->lightMode);
                SITHDSS_PUSHVEC4(&pPolyline->face.extraLight);
            } break;

            default:
                break;
        }

        if ( pThing->moveType == SITH_MT_PHYSICS )
        {
            const SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;
            SITHDSS_PUSHUINT32(pPhysics->flags);
            SITHDSS_PUSHVEC3(&pPhysics->velocity);
            SITHDSS_PUSHVEC3(&pPhysics->angularVelocity);
            SITHDSS_PUSHVEC3(&pPhysics->thrust);
            SITHDSS_PUSHVEC3(&pPhysics->rotThrust);
            SITHDSS_PUSHFLOAT(pPhysics->maxRotationVelocity);
            SITHDSS_PUSHFLOAT(pPhysics->maxVelocity);
            SITHDSS_PUSHFLOAT(pPhysics->orientSpeed);
            SITHDSS_PUSHFLOAT(pPhysics->buoyancy);
            SITHDSS_PUSHFLOAT(pPhysics->height);
            SITHDSS_PUSHFLOAT(pPhysics->mass);
        }
        else if ( pThing->moveType == SITH_MT_PATH )
        {
            const SithPathMoveInfo* pPath = &pThing->moveInfo.pathMovement;
            SITHDSS_PUSHUINT16(pPath->mode);
            SITHDSS_PUSHVEC3(&pPath->vecDeltaPos);
            SITHDSS_PUSHFLOAT(pPath->moveTimeRemaining);
            SITHDSS_PUSHFLOAT(pPath->moveVel);
            SITHDSS_PUSHFLOAT(pPath->rotDeltaTime);
            SITHDSS_PUSHVEC3(&pPath->rotOffset);
            SITHDSS_PUSHVEC3(&pPath->goalPYR);
            SITHDSS_PUSHFLOAT(pPath->rotDelta);
            SITHDSS_PUSHUINT16(pPath->numBlockedMoves);
            SITHDSS_PUSHUINT16(pPath->currentFrame);
            SITHDSS_PUSHUINT16(pPath->nextFrame);
            SITHDSS_PUSHUINT16(pPath->goalFrame);
            SITHDSS_PUSHMAT34(&pPath->curOrient);
            SITHDSS_PUSHVEC3(&pPath->rotateToPYR);

            SITHDSS_PUSHUINT16(pPath->numFrames);
            for ( size_t i = 0; i < pPath->numFrames; ++i )
            {
                SITHDSS_PUSHVEC3(&pPath->aFrames[i].pos);
                SITHDSS_PUSHVEC3(&pPath->aFrames[i].pyr);
            }
        }

        if ( !pThing->pSwapList )
        {
            SITHDSS_PUSHINT16(-1);
        }
        else
        {
            int16_t numEntries = 0;
            for ( const SithThingSwapEntry* pEntry = pThing->pSwapList; pEntry; pEntry = pEntry->pNextEntry )
            {
                ++numEntries;
            }

            SITHDSS_PUSHINT16(numEntries);
            for ( const SithThingSwapEntry* pEntry = pThing->pSwapList; pEntry; pEntry = pEntry->pNextEntry )
            {
                SITHDSS_PUSHINT32(pEntry->entryNum);
                SITHDSS_PUSHINT32(pEntry->meshNum);
                SITHDSS_PUSHINT16(sithModel_GetModelIndex(pEntry->pSrcModel));
                SITHDSS_PUSHINT32(pEntry->srcMeshNum);
            }
        }

        SITHDSS_PUSHINT32(pThing->numSwapEntries);

        // Note, renderData.data is a union so for non-model things e.g. sprites the model index is -1 but
        // insertOffset is still written from the wrong render data struct. When the message is processed the
        // insertOffset is skipped for model index -1 which makes the rest of the message (alpha) misaligned.
        if ( pThing->renderData.data.pModel3 )
        {
            SITHDSS_PUSHINT16(sithModel_GetModelIndex(pThing->renderData.data.pModel3));
            SITHDSS_PUSHVEC3(&pThing->renderData.data.pModel3->insertOffset);
        }
        else
        {
            SITHDSS_PUSHINT16(-1);
        }

        if ( pThing->type == SITH_THING_ACTOR || pThing->type == SITH_THING_PLAYER )
        {
            const SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
            SITHDSS_PUSHFLOAT(pActor->secWeaponActivationStartTime);
            SITHDSS_PUSHFLOAT(pActor->secTimeLastRapidFired);
            SITHDSS_PUSHFLOAT(pActor->secWeaponActivationWaitEndTime);
            SITHDSS_PUSHFLOAT(pActor->secWeaponActivationWaitTime);
            SITHDSS_PUSHFLOAT(pActor->secAimWaitEndTime);
            SITHDSS_PUSHFLOAT(pActor->secWeaponSwapTime);
            SITHDSS_PUSHINT32(pActor->selectedWeaponID);
            SITHDSS_PUSHINT32(pActor->curWeaponID);
            SITHDSS_PUSHINT32(pActor->deselectedWeaponID);
            SITHDSS_PUSHINT32(pActor->weaponSwapRefNum);
            SITHDSS_PUSHVEC3(&pActor->vecUnknown0);
            SITHDSS_PUSHVEC3(&pActor->fireOffset);
            SITHDSS_PUSHVEC4(&pActor->voiceInfo.voiceColor.top);
            SITHDSS_PUSHINT32(pActor->voiceInfo.hSndChannel);
            SITHDSS_PUSHINT32(pActor->voiceInfo.voiceHeadInfo.headMeshNum);
            SITHDSS_PUSHINT32(pActor->voiceInfo.voiceHeadInfo.headSwapRefNum);
            for ( size_t i = 0; i < STD_ARRAYLEN(pActor->voiceInfo.voiceHeadInfo.apSoundHeadModels); ++i )
            {
                SITHDSS_PUSHINT16(sithModel_GetModelIndex(pActor->voiceInfo.voiceHeadInfo.apSoundHeadModels[i]));
            }
        }

        SITHDSS_PUSHFLOAT(pThing->alpha);
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessFullDescription(const SithMessage* pMsg)
{
    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    SITH_ASSERTREL(pMsg);

    if ( stdComm_IsGameActive() && (sithMain_g_sith_mode.subModeFlags & 8) == 0 )
    {
        SITHLOG_ERROR("Received DSS::ThingFullDesc message when not in sync mode.\n");
        return 0;
    }

    SITHDSS_STARTIN(pMsg);

    int thingIdx = SITHDSS_POPUINT16();
    if ( thingIdx >= (int)sithWorld_g_pCurrentWorld->numThings )
    {
        SITHLOG_ERROR("FullDescription received for thing id %d, out of range.\n", thingIdx);
        return 0;
    }

    SithThing* pThing = &pWorld->aThings[thingIdx];
    if ( pThing->type != SITH_THING_FREE )
    {
        sithThing_RemoveThing(pWorld, pThing);
    }

    if ( thingIdx > pWorld->lastThingIdx )
    {
        pWorld->lastThingIdx = thingIdx;
    }

    SithThingType type = SITHDSS_POPUINT16();
    if ( type == SITH_THING_FREE )
    {
        return 1;
    }

    sithThing_Reset(pThing);

    size_t templateIdx         = SITHDSS_POPUINT16();
    const SithThing* pTemplate = sithTemplate_GetTemplateByIndex(templateIdx);
    if ( pTemplate )
    {
        // Fixed: Use the returned template. Original used pWorld->aThingTemplates[templateIdx] which is out of bounds for static world template index.
        sithThing_SetThingBasedOn(pThing, pTemplate);
    }

    pThing->type      = type;
    pThing->signature = SITHDSS_POPUINT32();
    pThing->guid      = SITHDSS_POPINT32();

    SITHDSS_POPVEC3(&pThing->pos);
    SITHDSS_POPVEC3(&pThing->forceMoveStartPos);
    SITHDSS_POPVEC3(&pThing->orient.rvec);
    SITHDSS_POPVEC3(&pThing->orient.lvec);
    SITHDSS_POPVEC3(&pThing->orient.uvec);

    pThing->moveStatus = SITHDSS_POPUINT32();

    SithSector* pSector = sithSector_GetSectorEx(sithWorld_g_pCurrentWorld, SITHDSS_POPUINT16());
    if ( pSector )
    {
        sithThing_SetSector(pThing, pSector, 1);
    }

    pThing->flags             = SITHDSS_POPUINT32();
    pThing->msecLifeLeft      = SITHDSS_POPUINT32();
    pThing->msecTimerTime     = SITHDSS_POPUINT32();
    pThing->msecNextPulseTime = SITHDSS_POPUINT32();
    pThing->msecPulseInterval = SITHDSS_POPUINT32();
    pThing->userval           = SITHDSS_POPFLOAT();

    pThing->collide.type     = SITHDSS_POPUINT16();
    pThing->collide.size     = SITHDSS_POPFLOAT();
    pThing->collide.movesize = SITHDSS_POPFLOAT();

    SITHDSS_POPVEC4(&pThing->light.color);
    pThing->light.minRadius = SITHDSS_POPFLOAT();
    pThing->light.maxRadius = SITHDSS_POPFLOAT();

    pThing->unknownFlags = SITHDSS_POPUINT32();

    if ( (pThing->flags & SITH_TF_COGLINKED) != 0 )
    {
        pThing->pCog        = sithCog_GetCogByIndex(SITHDSS_POPUINT16());
        pThing->pCaptureCog = sithCog_GetCogByIndex(SITHDSS_POPUINT16());
    }

    switch ( pThing->type )
    {
        case SITH_THING_ACTOR:
        case SITH_THING_CORPSE:
        case SITH_THING_PLAYER:
        {
            SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
            pActor->flags      = SITHDSS_POPUINT32();
            pActor->health     = SITHDSS_POPFLOAT();
            pActor->maxHealth  = SITHDSS_POPFLOAT();
            pActor->extraSpeed = SITHDSS_POPFLOAT();
            SITHDSS_POPVEC3(&pActor->headPYR);
            pActor->headLightIntensity.x = SITHDSS_POPFLOAT();
            pActor->headLightIntensity.y = SITHDSS_POPFLOAT();
            pActor->headLightIntensity.z = SITHDSS_POPFLOAT();
            pActor->headLightIntensity.w = 0.0f;

            pThing->aiState          = SITHDSS_POPINT32();
            pThing->aiArmedModeState = SITHDSS_POPINT32();

            pActor->bForceMovePlay              = SITHDSS_POPUINT8();
            pActor->bControlsDisabled           = SITHDSS_POPUINT8();
            pActor->endurance.msecUnderwater    = SITHDSS_POPUINT32();
            pActor->maxThrust                   = SITHDSS_POPFLOAT();
            pActor->maxRotVelocity              = SITHDSS_POPFLOAT();
            pActor->minHeadPitch                = SITHDSS_POPFLOAT();
            pActor->maxHeadPitch                = SITHDSS_POPFLOAT();
            pActor->maxHeadYaw                  = SITHDSS_POPFLOAT();
            pActor->maxHeadVelocity             = SITHDSS_POPFLOAT();

            int meshThingIdx = SITHDSS_POPUINT16();
            if ( meshThingIdx < (int)sithWorld_g_pCurrentWorld->numThings )
            {
                pActor->pThingMeshAttached = &sithWorld_g_pCurrentWorld->aThings[meshThingIdx];
            }

            pActor->attachMeshNum = SITHDSS_POPINT32();

            size_t playerNum = SITHDSS_POPUINT16();
            if ( playerNum < STD_ARRAYLEN(sithPlayer_g_aPlayers) )
            {
                pActor->pPlayer = &sithPlayer_g_aPlayers[playerNum];
            }
        } break;

        case SITH_THING_WEAPON:
            pThing->thingInfo.weaponInfo.flags        = SITHDSS_POPUINT32();
            pThing->thingInfo.weaponInfo.numRicochets = SITHDSS_POPUINT16();
            break;

        case SITH_THING_EXPLOSION:
            pThing->thingInfo.explosionInfo.flags = SITHDSS_POPUINT32();
            break;

        case SITH_THING_SPRITE:
        {
            pThing->thingInfo.spriteInfo.width     = SITHDSS_POPFLOAT();
            pThing->thingInfo.spriteInfo.height    = SITHDSS_POPFLOAT();
            pThing->thingInfo.spriteInfo.alpha     = SITHDSS_POPFLOAT();
            pThing->thingInfo.spriteInfo.rollAngle = SITHDSS_POPFLOAT();

            int meshThingIdx = SITHDSS_POPUINT16();
            if ( meshThingIdx < (int)sithWorld_g_pCurrentWorld->numThings )
            {
                pThing->thingInfo.spriteInfo.pThingMeshAttached = &sithWorld_g_pCurrentWorld->aThings[meshThingIdx];
            }

            pThing->thingInfo.spriteInfo.attachMeshNum = SITHDSS_POPINT32();
        } break;

        case SITH_THING_POLYLINE:
        {
            if ( strcmp(pThing->aName, sithDSSThing_aPolylineCogEndName) == 0 )
            {
                int parentIdx = SITHDSS_POPUINT16();
                if ( parentIdx < (int)sithWorld_g_pCurrentWorld->numThings )
                {
                    pThing->pParent = &sithWorld_g_pCurrentWorld->aThings[parentIdx];
                }
            }

            char aMatName[64];
            SITHDSS_POPSTRING(aMatName, sizeof(aMatName));
            aMatName[STD_ARRAYLEN(aMatName) - 1] = 0;

            float length              = SITHDSS_POPFLOAT();
            float baseRadius          = SITHDSS_POPFLOAT();
            float tipRadius           = SITHDSS_POPFLOAT();
            rdGeometryMode geoMode    = SITHDSS_POPUINT16();
            rdLightMode lightMode     = SITHDSS_POPUINT16();

            rdVector4 color;
            SITHDSS_POPVEC4(&color);

            rdPolyline* pPolyline = rdPolyline_New(pThing->aName, aMatName, aMatName, length, baseRadius, tipRadius, geoMode, lightMode, &color);
            if ( pPolyline )
            {
                rdThing_NewEntry(&pThing->renderData, NULL);
                rdThing_SetPolyline(&pThing->renderData, pPolyline);
            }

            if ( strcmp(pThing->aName, sithDSSThing_aPolylineCogEndName) == 0 )
            {
                pThing->alpha = color.alpha;
            }
        } break;

        default:
            break;
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;
        pPhysics->flags = SITHDSS_POPUINT32();
        SITHDSS_POPVEC3(&pPhysics->velocity);
        SITHDSS_POPVEC3(&pPhysics->angularVelocity);
        SITHDSS_POPVEC3(&pPhysics->thrust);
        SITHDSS_POPVEC3(&pPhysics->rotThrust);
        pPhysics->maxRotationVelocity = SITHDSS_POPFLOAT();
        pPhysics->maxVelocity         = SITHDSS_POPFLOAT();
        pPhysics->orientSpeed         = SITHDSS_POPFLOAT();
        pPhysics->buoyancy            = SITHDSS_POPFLOAT();
        pPhysics->height              = SITHDSS_POPFLOAT();
        pPhysics->mass                = SITHDSS_POPFLOAT();
    }
    else if ( pThing->moveType == SITH_MT_PATH )
    {
        SithPathMoveInfo* pPath = &pThing->moveInfo.pathMovement;
        pPath->mode = SITHDSS_POPINT16();
        SITHDSS_POPVEC3(&pPath->vecDeltaPos);
        pPath->moveTimeRemaining = SITHDSS_POPFLOAT();
        pPath->moveVel           = SITHDSS_POPFLOAT();
        pPath->rotDeltaTime      = SITHDSS_POPFLOAT();
        SITHDSS_POPVEC3(&pPath->rotOffset);
        SITHDSS_POPVEC3(&pPath->goalPYR);
        pPath->rotDelta        = SITHDSS_POPFLOAT();
        pPath->numBlockedMoves = SITHDSS_POPINT16();
        pPath->currentFrame    = SITHDSS_POPINT16();
        pPath->nextFrame       = SITHDSS_POPINT16();
        pPath->goalFrame       = SITHDSS_POPINT16();
        SITHDSS_POPMAT34(&pPath->curOrient);
        SITHDSS_POPVEC3(&pPath->rotateToPYR);

        // Note, the allocation result is not checked
        pPath->numFrames = SITHDSS_POPINT16();
        if ( pPath->numFrames )
        {
            pPath->sizeFrames = pPath->numFrames;
            pPath->aFrames    = (SithPathFrame*)STDMALLOC(sizeof(SithPathFrame) * pPath->numFrames);
        }

        for ( size_t i = 0; i < pPath->numFrames; ++i )
        {
            SITHDSS_POPVEC3(&pPath->aFrames[i].pos);
            SITHDSS_POPVEC3(&pPath->aFrames[i].pyr);
        }
    }

    sithThing_Initialize(sithWorld_g_pCurrentWorld, pThing, 0);

    if ( pThing->pSwapList )
    {
        sithThing_ResetSwapList(pThing);
    }

    uint16_t numEntries = SITHDSS_POPUINT16();
    if ( numEntries != (uint16_t)-1 )
    {
        for ( int i = (int16_t)numEntries; i > 0; --i )
        {
            int entryNum   = SITHDSS_POPINT32();
            int meshNum    = SITHDSS_POPINT32();
            uint16_t modelIdx = SITHDSS_POPUINT16();
            int srcMeshNum = SITHDSS_POPINT32();

            rdModel3* pModel = sithModel_GetModelByIndex(modelIdx);
            if ( pModel )
            {
                pThing->numSwapEntries = entryNum;
                sithThing_AddSwapEntry(pThing, meshNum, pModel, srcMeshNum);
            }
        }
    }

    pThing->numSwapEntries = SITHDSS_POPINT32();

    uint16_t modelIdx = SITHDSS_POPUINT16();
    if ( modelIdx != (uint16_t)-1 )
    {
        rdModel3* pModel = sithModel_GetModelByIndex(modelIdx);
        if ( pModel && pThing->renderData.data.pModel3 != pModel )
        {
            sithThing_SetThingModel(pThing, pModel);
        }

        SITHDSS_POPVEC3(&pThing->renderData.data.pModel3->insertOffset);
    }

    if ( pThing->type == SITH_THING_ACTOR || pThing->type == SITH_THING_PLAYER )
    {
        SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
        pActor->secWeaponActivationStartTime   = SITHDSS_POPFLOAT();
        pActor->secTimeLastRapidFired          = SITHDSS_POPFLOAT();
        pActor->secWeaponActivationWaitEndTime = SITHDSS_POPFLOAT();
        pActor->secWeaponActivationWaitTime    = SITHDSS_POPFLOAT();
        pActor->secAimWaitEndTime              = SITHDSS_POPFLOAT();
        pActor->secWeaponSwapTime              = SITHDSS_POPFLOAT();
        pActor->selectedWeaponID               = SITHDSS_POPINT32();
        pActor->curWeaponID                    = SITHDSS_POPINT32();
        pActor->deselectedWeaponID             = SITHDSS_POPINT32();
        pActor->weaponSwapRefNum               = SITHDSS_POPINT32();
        SITHDSS_POPVEC3(&pActor->vecUnknown0);
        SITHDSS_POPVEC3(&pActor->fireOffset);
        SITHDSS_POPVEC4(&pActor->voiceInfo.voiceColor.top);
        pActor->voiceInfo.hSndChannel                  = SITHDSS_POPINT32();
        pActor->voiceInfo.voiceHeadInfo.headMeshNum    = SITHDSS_POPINT32();
        pActor->voiceInfo.voiceHeadInfo.headSwapRefNum = SITHDSS_POPINT32();
        for ( size_t i = 0; i < STD_ARRAYLEN(pActor->voiceInfo.voiceHeadInfo.apSoundHeadModels); ++i )
        {
            pActor->voiceInfo.voiceHeadInfo.apSoundHeadModels[i] = sithModel_GetModelByIndex(SITHDSS_POPUINT16());
        }
    }

    if ( strcmp(pThing->aName, sithDSSThing_aPolylineCogEndName) != 0 )
    {
        pThing->alpha = SITHDSS_POPFLOAT();
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_PathMove(const SithThing* pThing, int16_t frame, float speed, int moveType, DPID idTO, unsigned int outstream)
{
    if ( !pThing || pThing->moveType != SITH_MT_PATH || pThing->type == SITH_THING_FREE || !pThing->pInSector )
    {
        return 0;
    }

    SITHDSS_STARTOUT(SITHDSS_PATHMOVE);

    SITHDSS_PUSHINT32(moveType);
    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHUINT32(sithMain_g_frameNumber);
    SITHDSS_PUSHUINT16(sithSector_GetSectorIndex(pThing->pInSector));
    SITHDSS_PUSHVEC3(&pThing->pos);

    rdVector3 pyr;
    rdMatrix_ExtractAngles34(&pThing->orient, &pyr);
    SITHDSS_PUSHVEC3(&pyr);

    SITHDSS_PUSHINT16(frame);
    SITHDSS_PUSHFLOAT(speed);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTO, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessPathMove(const SithMessage* pMsg)
{
    SITH_ASSERTREL(pMsg);

    SITHDSS_STARTIN(pMsg);

    int moveType = SITHDSS_POPINT32();
    int guid     = SITHDSS_POPINT32();

    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( pThing && pThing->moveType == SITH_MT_PATH )
    {
        size_t frameNum = SITHDSS_POPUINT32();
        if ( frameNum < (size_t)pThing->moveFrame )
        {
            SITHLOG_STATUS("Received old move command: Frame %d < frame %d.  Ignored.\n", frameNum, pThing->moveFrame);
            return 1;
        }

        pThing->moveFrame = frameNum;

        SithSector* pSector = sithSector_GetSectorEx(sithWorld_g_pCurrentWorld, SITHDSS_POPUINT16());
        if ( pSector )
        {
            SITHDSS_POPVEC3(&pThing->pos);
            sithThing_SetSector(pThing, pSector, 0);

            rdVector3 pyr;
            SITHDSS_POPVEC3(&pyr);
            rdMatrix_BuildRotate34(&pThing->orient, &pyr);

            uint16_t frame = SITHDSS_POPUINT16();
            float speed    = SITHDSS_POPFLOAT();
            switch ( moveType )
            {
                case 0:
                    sithPathMove_MoveToFrame(pThing, frame, speed);
                    break;

                case 1:
                    sithPathMove_SkipToFrame(pThing, frame, speed);
                    break;

                case 2:
                    sithPathMove_Finish(pThing);
                    break;

                default:
                    break;
            }

            return 1;
        }
    }

    SITHLOG_ERROR("Bad format in DSS::PathMove message, GUID=%x.\n", guid);

    SITHDSS_ENDIN;
    return 0;
}

int J3DAPI sithDSSThing_Attachment(const SithThing* pThing, DPID idTo, unsigned int outstream, unsigned int sendFlags)
{
    SITHDSS_STARTOUT(SITHDSS_ATTACHMENT);
    SITHDSS_PUSHINT32(pThing->guid);
    SITHDSS_PUSHUINT16(pThing->attach.flags);

    /**(uint32_t*)sithMulti_g_message.data = pThing->guid;
    *(uint16_t*)&sithMulti_g_message.data[4] = pThing->attach.flags;*/
    //uint8_t* pCurOut = &sithMulti_g_message.data[6];

    if ( (pThing->attach.flags & SITH_ATTACH_SURFACE) != 0 || (pThing->attach.flags & SITH_ATTACH_CLIMBSURFACE) != 0 )
    {
        SITHDSS_PUSHINT16(sithSurface_GetSurfaceIndex(pThing->attach.attachedToStructure.pSurfaceAttached));
        //*(uint16_t*)pCurOut = sithSurface_GetSurfaceIndex(pThing->attach.attachedToStructure.pSurfaceAttached);
        //pCurOut = &sithMulti_g_message.data[8];
    }
    else if ( (pThing->attach.flags & (SITH_ATTACH_THING | SITH_ATTACH_THINGFACE)) != 0 )
    {
        SITHDSS_PUSHINT32(pThing->attach.attachedToStructure.pThingAttached->guid);
       /**(uint32_t*)pCurOut = pThing->attach.attachedToStructure.pThingAttached->guid;
       pCurOut = &sithMulti_g_message.data[10];*/

        if ( (pThing->attach.flags & SITH_ATTACH_THINGFACE) != 0 )
        {
            SithThing* pThingAttached = pThing->attach.attachedToStructure.pThingAttached;
            if ( pThingAttached && pThingAttached->renderData.data.pModel3 )
            {
                // Note: This is buggy as the attached pFace can be from any of the model's mesh and not from the first one.
                const rdModel3Mesh* pMesh = pThing->attach.attachedToStructure.pThingAttached->renderData.data.pModel3->aGeos[0].aMeshes;
                int16_t faceIdx = pThing->attach.pFace - pMesh->aFaces;

                // Fixed: Added
                if ( faceIdx >= 0 && faceIdx < pMesh->numFaces ) {
                    SITHDSS_PUSHINT16(faceIdx);
                }
                else
                {
                    // TODO: This is quick fix, a better solution would be to pack mesh index and face index in int16.
                    //       Note, the vanilla engine doesn't handle -1 index on load and can crash the game.
                    //       But we don't care as it could crash anyway since the index is invalid.
                    SITHDSS_PUSHINT16(-1);
                }

                /**(uint16_t*)pCurOut = pThing->attach.pFace - pThing->attach.attachedToStructure.pThingAttached->renderData.data.pModel3->aGeos[0].aMeshes->aFaces;
                pCurOut = &sithMulti_g_message.data[12];*/
            }
            else
            {
                SITHDSS_PUSHINT16(-1);
                /**(uint16_t*)pCurOut = -1;
                pCurOut = &sithMulti_g_message.data[12];*/
            }
        }
        else
        {
            SITHDSS_PUSHVEC3(&pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing);
            /*memcpy(pCurOut, &pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing, 12u);
            pCurOut = &sithMulti_g_message.data[22];*/
        }
    }

    else if ( (pThing->attach.flags & SITH_ATTACH_THINGCLIMBWHIP) != 0 )
    {
        SITHDSS_PUSHFLOAT(pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing.z);
        /**(float*)pCurOut = pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing.z;
        pCurOut = &sithMulti_g_message.data[10];*/
    }

    /*sithMulti_g_message.type = SITHDSS_ATTACHMENT;
    sithMulti_g_message.length = pCurOut - sithMulti_g_message.data;*/
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, sendFlags);
}

int J3DAPI sithDSSThing_ProcessAttachment(const SithMessage* pMsg)
{
    SITH_ASSERTREL(pMsg);
    SITHDSS_STARTIN(pMsg);

    //guid = *(uint32_t*)pMsg->data;
    int guid = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("sithDSSThing_ProcessAttachment: Cannot find GUID for thing index %d.\n", guid);
        return 0;
    }

    //attachflags = *(uint16_t*)&pMsg->data[4];
    SithAttachFlag attachflags = SITHDSS_POPUINT16();

    // uint8_t* pCurIn = &pMsg->data[6];
    if ( (attachflags & (SITH_ATTACH_CLIMBSURFACE | SITH_ATTACH_SURFACE)) != 0 )
    {
        //surfIdx = *(uint16_t*)pCurIn;
        int surfIdx = SITHDSS_POPINT16();
        SithSurface* pSurf = sithSurface_GetSurfaceEx(sithWorld_g_pCurrentWorld, surfIdx);
        if ( !pSurf )
        {
            SITHLOG_ERROR("sithDSSThing_ProcessAttachment: Invalid surface index %d.\n", surfIdx);
            SITHDSS_ENDIN;
            return 0;
        }

        pThing->attach.flags = attachflags;
        pThing->attach.pFace = &pSurf->face;
        pThing->attach.attachedToStructure.pSurfaceAttached = pSurf;
        rdVector_Copy3(&pThing->attach.attachedFaceFirstVert, &sithWorld_g_pCurrentWorld->aVertices[*pSurf->face.aVertices]);
    }
    else if ( (attachflags & (SITH_ATTACH_THING | SITH_ATTACH_THINGFACE)) != 0 )
    {
        /*guidAttach = *(uint32_t*)pCurIn;
        pCurIn = &pMsg->data[10];*/

        int guidAttach = SITHDSS_POPINT32();
        SithThing* pThingAttached = sithThing_GetGuidThing(guidAttach);
        if ( !pThingAttached )
        {
            SITHLOG_ERROR("sithDSSThing_ProcessAttachment: Cannot find GUID for thing index %d.\n", guidAttach);
            SITHDSS_ENDIN;
            return 0;
        }

        pThing->attach.attachedToStructure.pThingAttached = pThingAttached;
        pThing->attach.flags = attachflags;
        sithThing_AddThingToAttachedThings(pThing, pThingAttached);

        if ( (attachflags & SITH_ATTACH_THINGFACE) != 0 )
        {
            int faceIdx = SITHDSS_POPINT16();

            // Fixed: Add null check and index bound check
            if ( pThingAttached->renderData.data.pModel3 && faceIdx >= 0 && faceIdx < pThingAttached->renderData.data.pModel3->aGeos[0].aMeshes->numFaces )
            {
                rdFace* pFaceAttached = &pThingAttached->renderData.data.pModel3->aGeos[0].aMeshes->aFaces[faceIdx];
                rdVector_Copy3(&pThing->attach.attachedFaceFirstVert, &pThingAttached->renderData.data.pModel3->aGeos[0].aMeshes->apVertices[*pFaceAttached->aVertices]);
                pThing->attach.pFace = pFaceAttached;
            }
            else // Added
            {
                pThing->attach.flags &= ~SITH_ATTACH_THINGFACE;
                if ( pThing->attach.flags == 0 ) {
                    pThing->attach.flags |= SITH_ATTACH_THING;
                }
            }
        }
        else
        {
            SITHDSS_POPVEC3(&pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing);
            //rdVector_Copy3(&pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing, pCurIn, sizeof(pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing));
        }
    }
    else if ( (attachflags & SITH_ATTACH_THINGCLIMBWHIP) != 0 )
    {
        pThing->attach.flags = attachflags;
        pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing.z = SITHDSS_POPFLOAT();
        //pThing->attach.vecUnknownMaybeLocalPositionOnTheAttachedThing.z = *(float*)pCurIn;
    }
    else if ( pThing->attach.flags )
    {
        sithThing_DetachThing(pThing);
    }

    SITHDSS_ENDIN;
    return 1;
}


int J3DAPI sithDSSThing_Take(const SithThing* pItem, const SithThing* pThing, unsigned int outstream)
{
    SITHDSS_STARTOUT(SITHDSS_TAKE);

    SITHDSS_PUSHINT32(pItem->guid);
    SITHDSS_PUSHINT32(pThing->guid);

    SITHDSS_ENDOUT;
    if ( stdComm_IsGameHost() )
    {
        return sithDSSThing_ProcessTake(&sithMulti_g_message);
    }

    return sithMessage_SendMessage(&sithMulti_g_message, sithMulti_g_serverId, outstream, 1u);
}

int J3DAPI sithDSSThing_ProcessTake(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int itemGuid     = SITHDSS_POPINT32();
    SithThing* pItem = sithThing_GetGuidThing(itemGuid);
    if ( !pItem && stdComm_IsGameHost() )
    {
        sithDSSThing_DestroyThing(itemGuid, sithMessage_GetSenderID());
        return 0;
    }

    SithThing* pTaker = sithThing_GetGuidThing(SITHDSS_POPINT32());
    if ( pItem && pTaker )
    {
        if ( pMsg->type == SITHDSS_TAKE )
        {
            if ( !stdComm_IsGameHost() )
            {
                return 0;
            }

            if ( pItem->type != SITH_THING_ITEM )
            {
                return 1;
            }

            if ( (pItem->flags & (SITH_TF_DISABLED | SITH_TF_DESTROYED)) != 0 )
            {
                return 1;
            }

            // Forward message to all players as take confirmation
            ((SithMessage*)pMsg)->type = SITHDSS_TAKE2;
            sithMessage_SendMessage((SithMessage*)pMsg, SITHMESSAGE_SENDTOJOINEDPLAYERS, SITHMESSAGE_STREAM_NET, 1u);
        }

        sithItem_SetItemTaken(pItem, pTaker, 1);
        return 1;
    }

    SITHLOG_ERROR("Bad thing or taker guid in DSS::Take.\n");
    stdComm_IsGameHost();

    SITHDSS_ENDIN;
    return 0;
}

int J3DAPI sithDSSThing_CreateThing(const SithThing* pTemplate, const SithThing* pNewThing, const SithThing* pMarker, const SithSector* pSector, const rdVector3* pos, const rdVector3* pyr, unsigned int outstream, unsigned int sendFlags)
{
    SITHDSS_STARTOUT(SITHDSS_CREATETHING);

    SITHDSS_PUSHUINT16(sithTemplate_GetTemplateIndex(pTemplate));
    if ( pMarker )
    {
        SITHDSS_PUSHINT32(pMarker->guid);
    }
    else
    {
        SITH_ASSERTREL(pSector && pos && pyr);
        SITHDSS_PUSHINT32(-1);
        SITHDSS_PUSHINT32(sithSector_GetSectorIndex(pSector));
        SITHDSS_PUSHVEC3(pos);
        SITHDSS_PUSHVEC3(pyr);
    }

    SITHDSS_PUSHINT32(pNewThing->guid);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, SITHMESSAGE_SENDTOJOINEDPLAYERS, outstream, sendFlags);
}

int J3DAPI sithDSSThing_ProcessCreateThing(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int templateIdx            = SITHDSS_POPUINT16();
    const SithThing* pTemplate = sithTemplate_GetTemplateByIndex(templateIdx);
    if ( !pTemplate )
    {
        SITHLOG_ERROR("Bad template index %d in DSS::CreateThing message.\n", templateIdx);
        return 0;
    }

    SithThing* pNewThing = NULL;
    int markerGuid       = SITHDSS_POPINT32();
    if ( markerGuid < 0 )
    {
        int secIdx          = SITHDSS_POPINT32();
        SithSector* pSector = sithSector_GetSectorEx(sithWorld_g_pCurrentWorld, secIdx);
        if ( !pSector )
        {
            SITHLOG_ERROR("Bad sector %d in DSS::CreateThing, template %s.\n", secIdx, pTemplate->aName);
            return 0;
        }

        rdVector3 pos, pyr;
        SITHDSS_POPVEC3(&pos);
        SITHDSS_POPVEC3(&pyr);

        rdMatrix34 orient;
        rdMatrix_BuildRotate34(&orient, &pyr);
        pNewThing = sithThing_CreateThingAtPos(pTemplate, &pos, &orient, pSector, NULL);
    }
    else
    {
        SithThing* pMarker = sithThing_GetGuidThing(markerGuid);
        if ( !pMarker )
        {
            SITHLOG_ERROR("Cannot find GUID %d to create template %s.\n", markerGuid, pTemplate->aName);
            return 0;
        }

        pNewThing = sithThing_CreateThing(pTemplate, pMarker);
    }

    if ( !pNewThing )
    {
        SITHLOG_ERROR("Failed to create thing of type %s from network instruction.\n", pTemplate->aName);
        return 0;
    }

    pNewThing->guid = SITHDSS_POPINT32();
    pNewThing->flags |= SITH_TF_REMOTE;

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSSThing_DestroyThing(int guid, DPID idTo)
{
    SITHDSS_STARTOUT(SITHDSS_DESTROYTHING);
    SITHDSS_PUSHINT32(guid);
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, SITHMESSAGE_STREAM_ALL, 1u);
}

int J3DAPI sithDSSThing_ProcessDestroyThing(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int guid          = SITHDSS_POPINT32();
    SithThing* pThing = sithThing_GetGuidThing(guid);
    if ( !pThing )
    {
        SITHLOG_ERROR("Bad thing guid %d in DSS::DestroyThing.\n", guid);
        return 0;
    }

    sithThing_DestroyThing(pThing);

    SITHDSS_ENDIN;
    return 1;
}

void J3DAPI sithDSSThing_MoveToPos(SithThing* pThing, const rdVector3* pPos, SithSector* pSector)
{
    SITH_ASSERTREL(pThing && pThing->moveType == SITH_MT_PHYSICS && pSector && pPos);

    // Predict position after 0.25 sec and if it's close enough to the received position, adjust velocity
    // to move thing to the position within the next 0.25 sec, otherwise snap the thing to the position.
    rdVector3 dpos;
    dpos.x = pThing->moveInfo.physics.velocity.x * 0.25f + pPos->x - pThing->pos.x;
    dpos.y = pThing->moveInfo.physics.velocity.y * 0.25f + pPos->y - pThing->pos.y;
    dpos.z = pThing->moveInfo.physics.velocity.z * 0.25f + pPos->z - pThing->pos.z;

    float dist = rdVector_Len3(&dpos);
    if ( dist != 0.0f && dist < 0.5f )
    {
        pThing->moveInfo.physics.velocity.x = dpos.x * 4.0f;
        pThing->moveInfo.physics.velocity.y = dpos.y * 4.0f;
        pThing->moveInfo.physics.velocity.z = dpos.z * 4.0f;
        return;
    }

    rdVector_Copy3(&pThing->pos, pPos);
    sithThing_SetSector(pThing, pSector, 0);
}
