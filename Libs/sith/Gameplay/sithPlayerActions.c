#include "sithPlayerActions.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdVector.h>
#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithComm.h>
#include <sith/Dss/sithDSSThing.h>
#include <sith/Devices/sithSound.h>
#include <sith/Engine/sithAnimate.h>
#include <sith/Engine/sithCamera.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithFX.h>
#include <sith/Gameplay/sithInventory.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerControls.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Gameplay/sithVehicleControls.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithSurface.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>
#include <sound/Sound.h>
#include <std/General/stdUtil.h>

#include <math.h>
#include <stdbool.h>
#include <string.h>

int sithPlayerActions_climbPupTrackNum;
static unsigned int sithPlayerActions_msecCurActivateTime;

#define SITHPLAYERACTIONS_ACTIVATEWAIT 500u // Min msec between two activations

#define SITHPLAYERACTIONS_ITEM_CHALK    47
#define SITHPLAYERACTIONS_ITEM_PATCHKIT 56
#define SITHPLAYERACTIONS_ITEM_RAFT     57

void sithPlayerActions_InstallHooks(void)
{
    J3D_HOOKFUNC(sithPlayerActions_Activate);
    J3D_HOOKFUNC(sithPlayerActions_Jump);
    J3D_HOOKFUNC(sithPlayerActions_MoveToPlayerPosition);
    J3D_HOOKFUNC(sithPlayerActions_PushItem);
    J3D_HOOKFUNC(sithPlayerActions_PullItem);
    J3D_HOOKFUNC(sithPlayerActions_ClimbPullUp);
    J3D_HOOKFUNC(sithPlayerActions_ClimbMove);
    J3D_HOOKFUNC(sithPlayerActions_UnmountWall);
    J3D_HOOKFUNC(sithPlayerActions_ClimbOn1m);
    J3D_HOOKFUNC(sithPlayerActions_ClimbOn2m);
    J3D_HOOKFUNC(sithPlayerActions_LeapForward);
    J3D_HOOKFUNC(sithPlayerActions_JumpForward);
    J3D_HOOKFUNC(sithPlayerActions_JumpLeft);
    J3D_HOOKFUNC(sithPlayerActions_JumpRight);
    J3D_HOOKFUNC(sithPlayerActions_JumpRollBack);
    J3D_HOOKFUNC(sithPlayerActions_JumpRollForward);
    J3D_HOOKFUNC(sithPlayerActions_StrafeLeft);
    J3D_HOOKFUNC(sithPlayerActions_StrafeRight);
    J3D_HOOKFUNC(sithPlayerActions_HopBack);
    J3D_HOOKFUNC(sithPlayerActions_Stand2Crawl);
    J3D_HOOKFUNC(sithPlayerActions_Crawl2Stand);
    J3D_HOOKFUNC(sithPlayerActions_JumpStart);
    J3D_HOOKFUNC(sithPlayerActions_HasActiveWeapon);
    J3D_HOOKFUNC(sithPlayerActions_ClimbToClimb);
    J3D_HOOKFUNC(sithPlayerActions_ClimbToHang);
    J3D_HOOKFUNC(sithPlayerActions_CanMoveToPos);
    J3D_HOOKFUNC(sithPlayerActions_SearchForCollision);
    J3D_HOOKFUNC(sithPlayerActions_CanClimbOn1m);
    J3D_HOOKFUNC(sithPlayerActions_CanClimbOn2m);
    J3D_HOOKFUNC(sithPlayerActions_SearchClimbSurface);
    J3D_HOOKFUNC(sithPlayerActions_ActorGrabLedge);
    J3D_HOOKFUNC(sithPlayerActions_ClimbDownToIdle);
    J3D_HOOKFUNC(sithPlayerActions_CanPullUp);
    J3D_HOOKFUNC(sithPlayerActions_GetFaceTopZ);
    J3D_HOOKFUNC(sithPlayerActions_StartInvisibility);
    J3D_HOOKFUNC(sithPlayerActions_EndInvisibility);
    J3D_HOOKFUNC(sithPlayerActions_IsInvisible);
    J3D_HOOKFUNC(sithPlayerActions_StartJewelFlying);
    J3D_HOOKFUNC(sithPlayerActions_EnableJewelFlying);
    J3D_HOOKFUNC(sithPlayerActions_DisableJewelFlying);
    J3D_HOOKFUNC(sithPlayerActions_MoveToClimbSector);
    J3D_HOOKFUNC(sithPlayerActions_Crawl);
}

int sithPlayerActions_g_jewelFlyingPuppetTrackNum = -1;
SithSurface* sithPlayerActions_g_pCurCollidedSurface;
rdFace* sithPlayerActions_g_pCurCollidedModelFace;
rdModel3* sithPlayerActions_g_pCurCollidedModel;
SithThing* sithPlayerActions_g_pPlasma;
int sithPlayerActions_g_bJewelFlying;
int sithPlayerActions_g_bPlayerInvisible;
SithSurface* sithPlayerActions_g_pChalkWriteSurf;
rdVector3 sithPlayerActions_g_posChalkMarkWrite;
SithSector* sithPlayerActions_g_pChalkWriteSector;

static inline float sithPlayerActions_DotZ(const rdVector3* pVec)
{
    return pVec->x * rdroid_g_zVector3.x + pVec->y * rdroid_g_zVector3.y + pVec->z * rdroid_g_zVector3.z;
}

static inline bool sithPlayerActions_IsZeroVector(const rdVector3* pVec)
{
    return pVec->x == 0.0f && pVec->y == 0.0f && pVec->z == 0.0f;
}

static inline void sithPlayerActions_SetForceMoveStartPos(SithThing* pThing)
{
    rdVector_Copy3(&pThing->forceMoveStartPos, &pThing->pos);
}

// Returns true when thing can be activated by player
static inline bool sithPlayerActions_IsActivatableThing(const SithThing* pPlayer, const SithThing* pThing)
{
    return pThing->type != SITH_THING_ITEM
        && pThing->type != SITH_THING_WEAPON
        && (pPlayer->controlType != SITH_CT_AI || pThing->type != SITH_THING_PLAYER);
}

// Aligns thing orientation to look in direction of pLook vector, keeping the up vector vertical
static void sithPlayerActions_AlignOrient(SithThing* pThing, const rdVector3* pLook)
{
    rdMatrix34* pOrient = &pThing->orient;
    pOrient->lvec.x = pLook->x;
    pOrient->rvec.z = 0.0f;
    pOrient->lvec.y = pLook->y;
    pOrient->lvec.z = pLook->z;
    pOrient->rvec.x = pOrient->lvec.y - 0.0f;
    pOrient->rvec.y = 0.0f - pOrient->lvec.x;
    rdVector_Normalize3Acc(&pOrient->rvec);

    pOrient->uvec.x = pOrient->lvec.z * pOrient->rvec.y - pOrient->lvec.y * pOrient->rvec.z;
    pOrient->uvec.y = pOrient->rvec.z * pOrient->lvec.x - pOrient->rvec.x * pOrient->lvec.z;
    pOrient->uvec.z = pOrient->lvec.y * pOrient->rvec.x - pOrient->rvec.y * pOrient->lvec.x;
}

int J3DAPI sithPlayerActions_Activate(SithThing* pPlayer)
{
    int bActivated = 0;
    if ( sithTime_g_clockTime < sithPlayerActions_msecCurActivateTime + SITHPLAYERACTIONS_ACTIVATEWAIT )
    {
        return 0;
    }

    // Can't activate while holding a gun
    unsigned int curWeapon = (unsigned int)pPlayer->thingInfo.actorInfo.curWeaponID;
    if ( curWeapon > 0 && curWeapon < 13 )
    {
        return 0;
    }

    sithPlayerActions_msecCurActivateTime = sithTime_g_clockTime;

    rdMatrix34 orient = pPlayer->orient;
    rdVector3 eyePos;
    rdVector_Copy3(&eyePos, &pPlayer->pos);
    if ( !sithCollision_FindSectorInRadius(pPlayer->pInSector, &pPlayer->pos, &eyePos, 0.0f) )
    {
        SITHLOG_ERROR("Activate: eyepoint outside of sector?\n");
        return 0;
    }

    if ( pPlayer == sithPlayer_g_pLocalPlayerThing
        && sithInventory_GetCurrentItem(pPlayer) == SITHPLAYERACTIONS_ITEM_CHALK
        && pPlayer->pInSector
        && (pPlayer->pInSector->flags & SITH_SECTOR_UNDERWATER) == 0
        && sithInventory_GetCurrentWeapon(pPlayer) == SITHWEAPON_NO_WEAPON
        && !sithWeapon_IsMountingWeapon(pPlayer)
        && pPlayer->moveStatus == SITHPLAYERMOVE_STILL )
    {
        // Write chalk mark on the wall in front of player
        eyePos.z = eyePos.z - -0.039999999f;
        SithSector* pSector = sithCollision_FindSectorInRadius(pPlayer->pInSector, &pPlayer->pos, &eyePos, 0.0f);
        if ( !pSector )
        {
            return 0;
        }

        SithSurface* pWallSurf = NULL;
        float wallDist         = 0.0f;
        sithCollision_SearchForCollisions(pSector, pPlayer, &eyePos, &pPlayer->orient.lvec, pPlayer->collide.movesize - -0.079999998f, 0.0099999998f, 3);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                SithSurface* pSurface = pCollision->pSurfaceCollided;
                if ( (pSurface->flags & SITH_SURFACE_COGLINKED) == 0 && (!pSurface->pAdjoin || (pSurface->pAdjoin->flags & SITH_ADJOIN_NOPLAYERMOVE) == 0) )
                {
                    wallDist  = pCollision->distance;
                    pWallSurf = pSurface;
                    break;
                }
            }
            else if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
            {
                break;
            }
        }

        sithCollision_DecreaseStackLevel();
        if ( !pWallSurf )
        {
            sithSoundClass_PlayVoiceModeRandom(pPlayer, (SithSoundClassMode)0x66);
            return 0;
        }

        if ( pWallSurf->pAdjoin )
        {
            SithSurfaceAdjoinFlag adjoinFlags = pWallSurf->pAdjoin->flags;
            if ( (adjoinFlags & SITH_ADJOIN_NOPLAYERMOVE) != 0 || (adjoinFlags & SITH_ADJOIN_MOVE) == 0 )
            {
                sithSoundClass_PlayVoiceModeRandom(pPlayer, (SithSoundClassMode)0x66);
                return 0;
            }
        }

        if ( (pWallSurf->flags & SITH_SURFACE_WATER) != 0 )
        {
            sithSoundClass_PlayVoiceModeRandom(pPlayer, (SithSoundClassMode)0x66);
            return 0;
        }

        float dist = wallDist + pPlayer->collide.movesize;
        rdVector3 markPos;
        markPos.x = pPlayer->orient.lvec.x * dist + pPlayer->pos.x;
        markPos.z = pPlayer->pos.z + pPlayer->orient.lvec.z * dist + pPlayer->collide.movesize;
        markPos.y = pPlayer->pos.y + pPlayer->orient.lvec.y * dist;
        pSector = sithCollision_FindSectorInRadius(pPlayer->pInSector, &pPlayer->pos, &markPos, 0.0f);
        if ( !pSector )
        {
            return 0;
        }

        pPlayer->moveStatus = SITHPLAYERMOVE_ACTIVATING;
        pPlayer->thingInfo.actorInfo.bControlsDisabled = 1;
        rdVector_Copy3(&sithPlayerActions_g_posChalkMarkWrite, &markPos);
        sithPlayerActions_g_pChalkWriteSurf   = pWallSurf;
        sithPlayerActions_g_pChalkWriteSector = pSector;

        sithCog_ThingSendMessage(pPlayer, pPlayer, SITHCOG_MSG_USER6);
        sithPhysics_ResetThingMovement(pPlayer);
        sithPuppet_PlayMode(pPlayer, SITHPUPPETSUBMODE_ACTIVATE, sithPlayerControls_PuppetCallback);
        return 1;
    }

    if ( sithInventory_GetCurrentItem(pPlayer) != SITHPLAYERACTIONS_ITEM_PATCHKIT )
    {
        // Activate thing or surface in front of player
        rdVector_Copy3(&eyePos, &pPlayer->pos);
        SithSector* pSector = sithCollision_FindSectorInRadius(pPlayer->pInSector, &pPlayer->pos, &eyePos, 0.0f);
        sithCollision_SearchForCollisions(pSector, pPlayer, &eyePos, &orient.lvec, pPlayer->collide.movesize - -0.039999999f, 0.050000001f, 2);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_TRACKSHOTS) != 0 )
                {
                    SITHLOG_STATUS("Activate hit surface %d.\n", sithSurface_GetSurfaceIndex(pCollision->pSurfaceCollided));
                }

                if ( (pCollision->pSurfaceCollided->flags & SITH_SURFACE_COGLINKED) != 0 && curWeapon < 14 )
                {
                    sithCog_SurfaceSendMessage(pCollision->pSurfaceCollided, pPlayer, SITHCOG_MSG_ACTIVATE);
                    bActivated = 1;
                    break;
                }
            }
            else if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
            {
                SithThing* pHitThing = pCollision->pThingCollided;
                if ( !sithPlayerActions_IsActivatableThing(pPlayer, pHitThing) )
                {
                    continue;
                }

                if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_TRACKSHOTS) != 0 )
                {
                    SITHLOG_STATUS("Activate hit thing %s.\n", pHitThing->aName);
                }

                if ( (pHitThing->flags & SITH_TF_COGLINKED) != 0 && (pHitThing->flags & SITH_TF_MOVABLE) == 0 )
                {
                    sithCog_ThingSendMessage(pHitThing, pPlayer, SITHCOG_MSG_ACTIVATE);
                    bActivated = 1;
                    if ( curWeapon <= 13 )
                    {
                        break;
                    }

                    // Imp stands are activated with imp jewel weapon
                    if ( pHitThing->renderData.type == RD_THING_MODEL3 && pHitThing->renderData.data.pModel3 )
                    {
                        if ( !_strcmpi(pHitThing->renderData.data.pModel3->aName, "imp_stands.3do") )
                        {
                            SithInventoryType* pWeaponType = sithInventory_GetType(curWeapon);
                            if ( pWeaponType->pCog )
                            {
                                sithCog_SendMessage(pWeaponType->pCog, SITHCOG_MSG_CHANGED, SITHCOG_SYM_REF_THING, pPlayer->idx, SITHCOG_SYM_REF_THING, pHitThing->idx, 0);
                            }
                        }

                        break;
                    }
                }
            }
        }

        sithCollision_DecreaseStackLevel();
        if ( bActivated )
        {
            return bActivated;
        }

        // Activate thing or surface below player
        rdVector3 probePos;
        probePos.x = pPlayer->orient.lvec.x * 0.035f + pPlayer->pos.x;
        probePos.y = pPlayer->orient.lvec.y * 0.035f + pPlayer->pos.y;
        probePos.z = pPlayer->orient.lvec.z * 0.035f + pPlayer->pos.z;
        pSector = sithCollision_FindSectorInRadius(pPlayer->pInSector, &pPlayer->pos, &probePos, 0.0f);
        if ( pSector )
        {
            rdVector3 downDir;
            rdVector_Neg3(&downDir, &rdroid_g_zVector3);
            sithCollision_SearchForCollisions(pSector, pPlayer, &probePos, &downDir, 0.11f, 0.039999999f, 2);
            for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
            {
                if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
                {
                    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_TRACKSHOTS) != 0 )
                    {
                        SITHLOG_STATUS("Activate down hit surface %d.\n", sithSurface_GetSurfaceIndex(pCollision->pSurfaceCollided));
                    }

                    if ( (pCollision->pSurfaceCollided->flags & SITH_SURFACE_COGLINKED) != 0 )
                    {
                        sithCog_SurfaceSendMessage(pCollision->pSurfaceCollided, pPlayer, SITHCOG_MSG_ACTIVATE);
                        bActivated = 1;
                        break;
                    }
                }
                else if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
                {
                    SithThing* pHitThing = pCollision->pThingCollided;
                    if ( !sithPlayerActions_IsActivatableThing(pPlayer, pHitThing) )
                    {
                        continue;
                    }

                    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_TRACKSHOTS) != 0 )
                    {
                        SITHLOG_STATUS("Activate down hit thing %s.\n", pHitThing->aName);
                    }

                    if ( (pHitThing->flags & SITH_TF_COGLINKED) != 0 && (pHitThing->flags & SITH_TF_MOVABLE) == 0 )
                    {
                        sithCog_ThingSendMessage(pHitThing, pPlayer, SITHCOG_MSG_ACTIVATE);
                        bActivated = 1;
                        break;
                    }
                }
            }

            sithCollision_DecreaseStackLevel();
        }
    }

    if ( bActivated || pPlayer != sithPlayer_g_pLocalPlayerThing || !pPlayer->pInSector || (pPlayer->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0 )
    {
        return bActivated;
    }

    int curItem = sithInventory_GetCurrentItem(pPlayer);
    if ( curItem == SITHPLAYERACTIONS_ITEM_RAFT )
    {
        if ( sithInventory_GetCurrentWeapon(pPlayer) != SITHWEAPON_NO_WEAPON
            || sithWeapon_IsMountingWeapon(pPlayer)
            || (pPlayer->moveInfo.physics.flags & (SITH_PF_MINECAR | SITH_PF_RAFT | SITH_PF_JEEP | SITH_PF_UNKNOWN_8000000)) != 0 )
        {
            return 0;
        }

        // Inflate raft, player must stand on flat floor
        if ( (pPlayer->attach.flags & SITH_ATTACH_SURFACE) == 0 )
        {
            return 0;
        }

        SithSurface* pFloor = pPlayer->attach.attachedToStructure.pSurfaceAttached;
        if ( (pFloor->flags & SITH_SURFACE_ISFLOOR) == 0 || pFloor->face.normal.z <= 0.99959999f )
        {
            return 0;
        }

        rdVector3 waterPos;
        waterPos.x = pPlayer->orient.lvec.x * 0.1f + pPlayer->pos.x;
        waterPos.y = pPlayer->orient.lvec.y * 0.1f + pPlayer->pos.y;
        waterPos.z = pPlayer->orient.lvec.z * 0.1f + pPlayer->pos.z;
        SithSector* pWaterSector = sithCollision_FindSectorInRadius(pPlayer->pInSector, &pPlayer->pos, &waterPos, 0.001f);
        if ( !pWaterSector )
        {
            return 0;
        }

        // Search for water surface in front of player
        rdVector3 downDir;
        rdVector_Neg3(&downDir, &rdroid_g_zVector3);

        bool bFoundWater = false;
        float waterDist  = sithPhysics_GetThingHeight(pPlayer) - -0.059999999f;
        sithCollision_SearchForCollisions(pWaterSector, NULL, &waterPos, &downDir, waterDist, 0.07f, 1);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            if ( (pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0 )
            {
                SithSurface* pSurface = pCollision->pSurfaceCollided;
                if ( pSurface && pSurface->pAdjoin && (pSurface->pAdjoin->pAdjoinSector->flags & SITH_SECTOR_UNDERWATER) != 0 )
                {
                    waterDist   = pCollision->distance;
                    bFoundWater = true;
                    break;
                }

                continue;
            }

            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0
                && (pCollision->type & SITHCOLLISION_FACE) != 0
                && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_WATER) != 0 )
            {
                float waterDepth = 0.07f + pCollision->distance;
                if ( waterDepth - sithPhysics_GetThingHeight(pPlayer) >= 0.039999999f
                    && waterDepth - sithPhysics_GetThingHeight(pPlayer) <= 0.059999999f
                    && pCollision->pSurfaceCollided->face.normal.z > 0.99984002f )
                {
                    waterDist   = 0.07f + pCollision->distance;
                    bFoundWater = true;
                }
            }

            break;
        }

        sithCollision_DecreaseStackLevel();
        if ( !bFoundWater )
        {
            return 0;
        }

        // Find raft player
        size_t playerNum;
        SithThing* pRaftThing = NULL;
        for ( playerNum = 0; playerNum < sithPlayer_g_numPlayers; ++playerNum )
        {
            const char* pName = sithPlayer_g_aPlayers[playerNum].pThing->aName;
            if ( !strcmp(pName, "raftplayer") || !strcmp(pName, "shirtraftplayer") )
            {
                pRaftThing = sithPlayer_g_aPlayers[playerNum].pThing;
                break;
            }
        }

        if ( !pRaftThing )
        {
            return 0;
        }

        rdVector3 raftPos;
        rdVector_Copy3(&raftPos, &waterPos);
        raftPos.z = raftPos.z - (waterDist - sithPhysics_GetThingHeight(pRaftThing));
        SithSector* pRaftSector = sithCollision_FindSectorInRadius(pWaterSector, &waterPos, &raftPos, 0.0f);
        if ( !pRaftSector )
        {
            SITHLOG_ERROR("This is really bad, trying to board raft in non-existent sector..\n");
        }

        // Raft is placed sideways to the water
        rdMatrix34 raftOrient;
        rdVector_Neg3(&raftOrient.rvec, &pPlayer->orient.lvec);
        rdVector_Copy3(&raftOrient.lvec, &pPlayer->orient.rvec);
        rdVector_Copy3(&raftOrient.uvec, &pPlayer->orient.uvec);

        // Note, original sends exited message when raft sector is the same as player sector
        SithSector* pSameSector = pRaftSector == pPlayer->pInSector ? pWaterSector : NULL;
        if ( pSameSector )
        {
            sithCog_SectorSendMessage(pPlayer->pInSector, pPlayer, SITHCOG_MSG_EXITED);
        }

        if ( sithPlayer_g_bGuybrush )
        {
            sithPlayer_ToggleGuybrush();
        }

        sithPlayer_HidePlayer(sithPlayer_g_playerNum);
        sithThing_ExitSector(pRaftThing);

        *pRaftThing->thingInfo.actorInfo.pPlayer = *pPlayer->thingInfo.actorInfo.pPlayer;
        pRaftThing->thingInfo.actorInfo.pPlayer->pThing = pRaftThing;
        pRaftThing->thingInfo.actorInfo.health = pPlayer->thingInfo.actorInfo.health;

        sithPlayer_SetLocalPlayer(playerNum);
        sithPlayer_ShowPlayer(playerNum, 0);
        sithThing_SetPositionAndOrient(pRaftThing, &raftPos, &raftOrient);
        sithThing_EnterSector(pRaftThing, pRaftSector, 1, pSameSector == NULL);

        sithPuppet_SetMoveMode(pRaftThing, SITHPUPPET_MOVEMODE_NORMAL);
        sithPhysics_FindFloor(pRaftThing, 1);
        sithCamera_SetCameraFocus(&sithCamera_g_aCameras[1], pRaftThing, NULL);

        pRaftThing->moveStatus = SITHPLAYERMOVE_RAFT_BOARDING;
        sithPuppet_PlayMode(pRaftThing, SITHPUPPETSUBMODE_HOPBACK, sithVehicleControls_PuppetCallback);
        pRaftThing->thingInfo.actorInfo.bControlsDisabled = 1;
        sithInventory_SetSwimmingInventory(pRaftThing, 0);
        pRaftThing->alpha = 1.0f;
        sithInventory_SetInventoryDisabled(pRaftThing, SITHPLAYERACTIONS_ITEM_PATCHKIT, 0);
        sithVehicleControls_SendMessageUser0(pRaftThing);

        if ( pRaftThing->pCog )
        {
            sithCog_SendMessage(pRaftThing->pCog, SITHCOG_MSG_BOARDED, SITHCOG_SYM_REF_THING, pRaftThing->idx, SITHCOG_SYM_REF_THING, pPlayer->idx, 0);
        }

        if ( (pPlayer->thingInfo.actorInfo.flags & SITH_AF_INVULNERABLE) != 0 )
        {
            pRaftThing->thingInfo.actorInfo.flags |= SITH_AF_INVULNERABLE;
        }

        if ( pRaftThing->thingInfo.actorInfo.endurance.raftLeakDamage )
        {
            pRaftThing->thingInfo.actorInfo.endurance.raftLeakDamage = 1;
        }

        sithCog_ThingSendMessageEx(pRaftThing, NULL, SITHCOG_MSG_CALLBACK, 0, 16, 0, 0);
        return 1;
    }

    if ( curItem == SITHPLAYERACTIONS_ITEM_PATCHKIT
        && (pPlayer->moveInfo.physics.flags & SITH_PF_RAFT) != 0
        && pPlayer->thingInfo.actorInfo.endurance.raftLeakDamage )
    {
        // Patch raft
        pPlayer->thingInfo.actorInfo.endurance.raftLeakDamage = 0;
        sithInventory_SetInventory(pPlayer, SITHPLAYERACTIONS_ITEM_PATCHKIT, sithInventory_GetInventory(pPlayer, SITHPLAYERACTIONS_ITEM_PATCHKIT) - 1.0f);
        sithSoundClass_PlayModeFirst(pPlayer, SITHSOUNDCLASS_ACTIVATE);
        sithSoundClass_StopMode(pPlayer, SITHSOUNDCLASS_JUMPWOOD);
        return 1;
    }

    return bActivated;
}

// Returns jump sound mode depending on the surface or thing player is standing on
static SithSoundClassMode sithPlayerActions_GetJumpSoundMode(const SithThing* pThing)
{
    SithAttachFlag attachFlags = pThing->attach.flags;
    if ( (attachFlags & (SITH_ATTACH_THING | SITH_ATTACH_THINGFACE)) != 0 )
    {
        SithThingFlag flags = pThing->attach.attachedToStructure.pThingAttached->flags;
        if ( (flags & SITH_TF_METAL) != 0 )
        {
            return SITHSOUNDCLASS_JUMPMETAL;
        }

        if ( (flags & SITH_TF_EARTH) != 0 )
        {
            return SITHSOUNDCLASS_JUMPEARTH;
        }

        if ( (flags & SITH_TF_SNOW) != 0 )
        {
            return SITHSOUNDCLASS_JUMPSNOW;
        }

        return (flags & SITH_TF_WOOD) != 0 ? SITHSOUNDCLASS_JUMPWOOD : SITHSOUNDCLASS_JUMP;
    }

    SithSurfaceFlag flags = pThing->attach.attachedToStructure.pSurfaceAttached->flags;
    if ( (flags & 0xE0DF0080) == 0 )
    {
        return SITHSOUNDCLASS_JUMP;
    }

    if ( (flags & SITH_SURFACE_METAL) != 0 )
    {
        return SITHSOUNDCLASS_JUMPMETAL;
    }

    if ( (flags & (SITH_SURFACE_WATER | SITH_SURFACE_SHALLOWWATER)) != 0 )
    {
        return SITHSOUNDCLASS_JUMPWATER;
    }

    if ( (flags & SITH_SURFACE_EARTH) != 0 )
    {
        return SITHSOUNDCLASS_JUMPEARTH;
    }

    if ( (flags & SITH_SURFACE_SNOW) != 0 )
    {
        return SITHSOUNDCLASS_JUMPSNOW;
    }

    if ( (flags & SITH_SURFACE_WOOD) != 0 )
    {
        return SITHSOUNDCLASS_JUMPWOOD;
    }

    if ( (flags & SITH_SURFACE_ECHO) != 0 )
    {
        return SITHSOUNDCLASS_JUMPHARDECHO;
    }

    if ( (flags & SITH_SURFACE_WOODECHO) != 0 )
    {
        return SITHSOUNDCLASS_JUMPWOODECHO;
    }

    if ( (flags & SITH_SURFACE_EARTHECHO) != 0 )
    {
        return SITHSOUNDCLASS_JUMPEARTHECHO;
    }

    return (flags & SITH_SURFACE_AETHERIUM) != 0 ? SITHSOUNDCLASS_JUMPAET : SITHSOUNDCLASS_JUMP;
}

void J3DAPI sithPlayerActions_Jump(SithThing* pThing, float a2, int jumpDirection)
{
    (void)a2; // Unused in v1.0
    if ( pThing->type != SITH_THING_ACTOR && pThing->type != SITH_THING_PLAYER )
    {
        return;
    }

    if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0 )
    {
        return;
    }

    // Note, for unknown jump directions original uses uninitialized direction vector
    rdVector3 dir = { 0 };
    switch ( jumpDirection )
    {
        case 1:
        case 99:
            rdVector_Set3(&dir, 0.0f, 1.0f, 0.0f);
            break;

        case 2:
            rdVector_Set3(&dir, 0.0f, -1.0f, 0.0f);
            break;

        case 3:
            rdVector_Set3(&dir, -1.0f, 0.0f, 0.0f);
            break;

        case 4:
            rdVector_Set3(&dir, 1.0f, 0.0f, 0.0f);
            break;

        default: // 0, -1
            break;
    }

    rdVector3 vel;
    rdVector_Set3(&vel, 0.0f, 0.0f, 2.4000001f);
    if ( jumpDirection != 0 )
    {
        rdVector3 jumpDir;
        rdMatrix_TransformVector34(&jumpDir, &dir, &pThing->orient);
        vel.x = jumpDir.x + vel.x;
        vel.y = jumpDir.y + vel.y;
        vel.z = jumpDir.z + vel.z;
    }

    rdVector_Normalize3Acc(&vel);
    if ( !pThing->attach.flags )
    {
        return;
    }

    SithSoundClassMode soundMode = sithPlayerActions_GetJumpSoundMode(pThing);
    tSoundChannelHandle hChannel = sithSoundClass_PlayModeRandom(pThing, soundMode);
    int guid = hChannel ? Sound_GetChannelGUID(hChannel) : -1;
    if ( hChannel && sithMessage_g_outputstream )
    {
        sithDSSThing_PlaySoundMode(pThing, (int16_t)soundMode, guid, -1.0f);
    }

    sithThing_DetachThing(pThing);
    sithPlayerActions_SetForceMoveStartPos(pThing);
    pThing->moveInfo.physics.angularVelocity.y = 0.0f;

    if ( jumpDirection == 0 )
    {
        pThing->moveStatus = SITHPLAYERMOVE_JUMPUP;
        sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_JUMPUP, NULL);
    }
    else if ( jumpDirection == 1 )
    {
        pThing->moveStatus = SITHPLAYERMOVE_JUMPFWD;
        pThing->collide.movesize = 0.059999999f;
        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_JUMPFWD, NULL);
    }
    else if ( jumpDirection == -1 )
    {
        pThing->moveStatus = SITHPLAYERMOVE_JUMPBACK;
        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_HOPBACK, NULL);
        sithPlayerActions_HopBack(pThing);
    }
    else
    {
        rdVector_Copy3(&pThing->moveInfo.physics.velocity, &vel);
    }

    if ( sithMessage_g_outputstream )
    {
        sithThing_SyncThing(pThing, SITHTHING_SYNC_POS);
    }
}

void J3DAPI sithPlayerActions_MoveToPlayerPosition(SithThing* pThing, unsigned int playerNum)
{
    if ( playerNum >= sithPlayer_g_numPlayers )
    {
        return;
    }

    SithPlayer* pPlayer = &sithPlayer_g_aPlayers[playerNum];
    if ( (pPlayer->flags & SITH_PLAYER_PLACED) != 0 )
    {
        pThing->orient = pPlayer->orient;
        rdVector_Copy3(&pThing->pos, &pThing->orient.dvec);
        rdVector_Copy3(&pThing->orient.dvec, &rdroid_g_zeroVector3);
        sithThing_SetSector(pThing, pPlayer->pInSector, 0);
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        sithPhysics_ResetThingMovement(pThing);
        pThing->moveInfo.physics.flags &= ~SITH_PF_ALIGNED;
        sithPhysics_FindFloor(pThing, 1);
    }
}

void J3DAPI sithPlayerActions_PushItem(SithThing* pThing, SithThing* pItem, rdVector3* pDirection)
{
    float movesize = pThing->collide.movesize * 0.5f;
    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    pThing->moveStatus = SITHPLAYERMOVE_PUSHING;
    pThing->collide.movesize = movesize;
    sithPlayerActions_SetForceMoveStartPos(pThing);

    int trackNum = sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_PUSHITEM, NULL);
    if ( trackNum >= 0 )
    {
        sithAnimate_PushItem(pThing, pItem, pDirection, trackNum);
        sithPlayerControls_g_bCutsceneMode = 1;
    }
    else if ( trackNum == -1 )
    {
        pThing->thingInfo.actorInfo.bForceMovePlay = 0;
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        pThing->collide.movesize = pThing->collide.movesize + pThing->collide.movesize;
    }
}

void J3DAPI sithPlayerActions_PullItem(SithThing* pThing, SithThing* pItem, rdVector3* pDirection)
{
    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    pThing->moveStatus = SITHPLAYERMOVE_PULLING;
    sithPlayerActions_SetForceMoveStartPos(pThing);

    rdVector3 pullDir;
    rdVector_Neg3(&pullDir, pDirection);
    pThing->collide.movesize = pThing->collide.movesize * 0.5f;

    int trackNum = sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_PULLITEM, NULL);
    if ( trackNum >= 0 )
    {
        sithPlayerControls_g_bCutsceneMode = 1;
        sithAnimate_PullItem(pThing, pItem, &pullDir, trackNum);
    }
    else if ( trackNum == -1 )
    {
        pThing->thingInfo.actorInfo.bForceMovePlay = 0;
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        pThing->collide.movesize = pThing->collide.movesize + pThing->collide.movesize;
    }
}

SithInventoryItem* J3DAPI sithPlayerActions_ClimbPullUp(SithThing* pThing)
{
    float movesize = pThing->collide.movesize * 0.25f;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    pThing->moveStatus = SITHPLAYERMOVE_PULLINGUP;
    pThing->collide.movesize = movesize;
    pThing->moveInfo.physics.flags = (pThing->moveInfo.physics.flags & ~SITH_PF_ALIGNED) | SITH_PF_FLOORSTICK;

    sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_CLIMBPULLINGUP, NULL);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBONTO);
    sithInventory_SetSwimmingInventory(pThing, 1);
    return NULL; // Original returns leftover return value of sithInventory_SetSwimmingInventory which is never used
}

void J3DAPI sithPlayerActions_ClimbMove(SithThing* pThing, SithSurface* pSurf, int direction)
{
    float movesize = pThing->collide.movesize;
    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    pThing->collide.movesize = movesize * 0.25f;

    switch ( direction )
    {
        case 1:
            pThing->moveStatus = SITHPLAYERMOVE_CLIMBING_UP;
            sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLUP, NULL);
            break;

        case 2:
            pThing->moveStatus = SITHPLAYERMOVE_CLIMBING_DOWN;
            sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLDOWN, NULL);
            break;

        case 3:
            pThing->moveStatus = SITHPLAYERMOVE_CLIMBING_RIGHT;
            sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLRIGHT, NULL);
            sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBHORIZ);
            break;

        case 4:
            pThing->moveStatus = SITHPLAYERMOVE_CLIMBING_LEFT;
            sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLLEFT, NULL);
            sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBHORIZ);
            break;

        default:
            break;
    }

    sithThing_AttachThingToClimbSurface(pThing, pSurf);
}

void J3DAPI sithPlayerActions_UnmountWall(SithThing* pThing, int trackNum)
{
    (void)trackNum; // Unused in v1.0
    pThing->moveStatus = SITHPLAYERMOVE_FALLING;
    pThing->moveInfo.physics.flags = (pThing->moveInfo.physics.flags & ~SITH_PF_ALIGNED) | SITH_PF_FLOORSTICK;
    pThing->thingInfo.actorInfo.bControlsDisabled = 0;
    pThing->thingInfo.actorInfo.bForceMovePlay    = 0;
    sithPhysics_ResetThingMovement(pThing);
    sithPuppet_RemoveAllTracks(pThing);

    // Push slightly away from the wall
    rdVector3 vel;
    rdVector_Set3(&vel, 0.0f, -0.075000003f, 0.0099999998f);
    rdMatrix_TransformVector34(&pThing->moveInfo.physics.velocity, &vel, &pThing->orient);

    sithThing_DetachThing(pThing);
    sithPhysics_FindFloor(pThing, 0);
    if ( pThing->attach.flags )
    {
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_STAND, NULL);
    }

    sithPhysics_SetThingLook(pThing, &rdroid_g_zVector3, 0.0f);
    sithInventory_SetSwimmingInventory(pThing, 1);
}

void J3DAPI sithPlayerActions_ClimbOn1m(SithThing* pThing)
{
    if ( pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        return;
    }

    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    pThing->moveStatus = SITHPLAYERMOVE_PULLINGUP_1M;
    if ( (pThing->moveInfo.physics.flags & SITH_PF_ONWATERSURFACE) != 0 )
    {
        pThing->moveInfo.physics.flags &= ~SITH_PF_ONWATERSURFACE;
    }

    pThing->collide.movesize = pThing->collide.movesize * 0.5f;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    sithThing_DetachThing(pThing);
    sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_MOUNT1MSTEP, NULL);
    sithInventory_SetSwimmingInventory(pThing, 0);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBONTO);
}

void J3DAPI sithPlayerActions_ClimbOn2m(SithThing* pThing)
{
    if ( pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        return;
    }

    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    pThing->moveStatus = SITHPLAYERMOVE_PULLINGUP;
    pThing->collide.movesize = 0.0099999998f;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    sithThing_DetachThing(pThing);
    sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_MOUNT2MLEDGE, NULL);
    sithInventory_SetSwimmingInventory(pThing, 0);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBONTO);
}

// Sets jump velocity in local thing coordinates, plays jump sound and detaches thing from floor
static void sithPlayerActions_StartJumpMove(SithThing* pThing, float x, float y, float z, bool bPlayRandom)
{
    rdVector3 vel;
    rdVector_Set3(&vel, x, y, z);
    rdMatrix_TransformVector34(&pThing->moveInfo.physics.velocity, &vel, &pThing->orient);

    if ( pThing->attach.flags )
    {
        SithSoundClassMode soundMode = sithPlayerActions_GetJumpSoundMode(pThing);
        if ( bPlayRandom )
        {
            sithSoundClass_PlayModeRandom(pThing, soundMode);
        }
        else
        {
            sithSoundClass_PlayModeFirst(pThing, soundMode);
        }
    }

    sithThing_DetachThing(pThing);
}

void J3DAPI sithPlayerActions_LeapForward(SithThing* pThing)
{
    sithPlayerActions_StartJumpMove(pThing, 0.0f, 0.77f, 0.75f, false);
}

void J3DAPI sithPlayerActions_JumpForward(SithThing* pThing)
{
    rdVector3 vel;
    rdVector_Set3(&vel, 0.0f, 0.40000001f, 0.75f);
    rdVector3 thingVel;
    rdMatrix_TransformVector34(&thingVel, &vel, &pThing->orient);
    pThing->collide.movesize = 0.079999998f;
    rdVector_Copy3(&pThing->moveInfo.physics.velocity, &thingVel);

    if ( pThing->attach.flags )
    {
        sithSoundClass_PlayModeFirst(pThing, sithPlayerActions_GetJumpSoundMode(pThing));
    }

    sithThing_DetachThing(pThing);
}

void J3DAPI sithPlayerActions_JumpLeft(SithThing* pThing)
{
    pThing->moveInfo.physics.angularVelocity.y = 0.0f;
    sithPlayerActions_StartJumpMove(pThing, -0.75f, 0.0f, 0.40000001f, false);
}

void J3DAPI sithPlayerActions_JumpRight(SithThing* pThing)
{
    pThing->moveInfo.physics.angularVelocity.y = 0.0f;
    sithPlayerActions_StartJumpMove(pThing, 0.75f, 0.0f, 0.40000001f, false);
}

void J3DAPI sithPlayerActions_JumpRollBack(SithThing* pThing)
{
    pThing->moveInfo.physics.angularVelocity.y = 0.0f;
    sithPlayerActions_StartJumpMove(pThing, 0.0f, -0.75f, 0.40000001f, false);
}

void J3DAPI sithPlayerActions_JumpRollForward(SithThing* pThing)
{
    pThing->moveInfo.physics.angularVelocity.y = 0.0f;
    sithPlayerActions_StartJumpMove(pThing, 0.0f, 0.75f, 0.40000001f, false);
}

static void sithPlayerActions_Strafe(SithThing* pThing, float dir, SithThingMoveStatus moveStatus, SithPuppetSubMode submode)
{
    if ( pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        return;
    }

    rdVector3 strafePos;
    strafePos.x = dir * pThing->orient.rvec.x * 0.1f + pThing->pos.x;
    strafePos.y = dir * pThing->orient.rvec.y * 0.1f + pThing->pos.y;
    strafePos.z = dir * pThing->orient.rvec.z * 0.1f + pThing->pos.z;

    int bOtherFloor;
    if ( sithPlayerActions_CanMoveToPos(pThing, &strafePos, &bOtherFloor) == 1 )
    {
        pThing->thingInfo.actorInfo.bForceMovePlay = 1;
        sithPlayerActions_SetForceMoveStartPos(pThing);
        pThing->moveStatus = moveStatus;
        sithPuppet_PlayForceMoveMode(pThing, submode, NULL);
    }
}

void J3DAPI sithPlayerActions_StrafeLeft(SithThing* pThing)
{
    sithPlayerActions_Strafe(pThing, -1.0f, SITHPLAYERMOVE_STRAFING_LEFT, SITHPUPPETSUBMODE_STRAFELEFT);
}

void J3DAPI sithPlayerActions_StrafeRight(SithThing* pThing)
{
    sithPlayerActions_Strafe(pThing, 1.0f, SITHPLAYERMOVE_STRAFING_RIGHT, SITHPUPPETSUBMODE_STRAFERIGHT);
}

void J3DAPI sithPlayerActions_HopBack(SithThing* pThing)
{
    sithPlayerActions_StartJumpMove(pThing, 0.0f, -0.34999999f, 0.60000002f, true);
}

void J3DAPI sithPlayerActions_Stand2Crawl(SithThing* pThing)
{
    sithInventory_SetSwimmingInventory(pThing, 0);
    pThing->collide.movesize = 0.02f;
    pThing->moveStatus = SITHPLAYERMOVE_STAND_TO_CRAWL;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_CRAWL);
    sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_STAND2CRAWL, NULL);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBDOWNONTO);
}

void J3DAPI sithPlayerActions_Crawl2Stand(SithThing* pThing)
{
    // Check there's enough room to stand up
    rdVector3 upDir;
    rdVector_Copy3(&upDir, &rdroid_g_zVector3);

    bool bCanStand = true;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &upDir, 0.115f, pThing->collide.movesize, 0x200);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & (SITHCOLLISION_WORLD | SITHCOLLISION_THING)) != 0 )
        {
            bCanStand = false;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bCanStand )
    {
        return;
    }

    sithInventory_SetSwimmingInventory(pThing, 1);
    sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_WALK);
    sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_WALKBACK);
    pThing->collide.movesize = 0.039999999f;
    pThing->moveStatus = SITHPLAYERMOVE_CRAWL_TO_STAND;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_CRAWL2STAND, NULL);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBONTO);
}

int J3DAPI sithPlayerActions_JumpStart(SithThing* pThing)
{
    pThing->moveStatus = SITHPLAYERMOVE_JUMP_READY;
    sithPhysics_ResetThingMovement(pThing);
    int result = sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_JUMPREADY, sithPlayerControls_PuppetCallback);
    sithPuppet_g_bPlayerLeapForward = 0;
    pThing->thingInfo.actorInfo.bControlsDisabled = 1;
    return result;
}

int J3DAPI sithPlayerActions_HasActiveWeapon(SithThing* pThing)
{
    if ( sithInventory_GetCurrentWeapon(pThing) != SITHWEAPON_NO_WEAPON )
    {
        return 1;
    }

    return sithWeapon_IsMountingWeapon(pThing);
}

void J3DAPI sithPlayerActions_ClimbToClimb(SithThing* pThing, int bLedge)
{
    if ( pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        return;
    }

    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    pThing->moveStatus = SITHPLAYERMOVE_CLIMB_DOWN_TO_MOUNT;
    pThing->collide.movesize = 0.001f;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    sithThing_DetachThing(pThing);

    sithPuppet_PlayForceMoveMode(pThing, bLedge ? SITHPUPPETSUBMODE_GRABLEDGE : SITHPUPPETSUBMODE_CLIMBTOCLIMB, NULL);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBDOWNONTO);
    sithInventory_SetSwimmingInventory(pThing, 0);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBDOWNONTO);
}

void J3DAPI sithPlayerActions_ClimbToHang(SithThing* pThing)
{
    if ( pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        return;
    }

    pThing->thingInfo.actorInfo.bForceMovePlay = 1;
    pThing->moveStatus = SITHPLAYERMOVE_CLIMB_TO_HANG;
    pThing->collide.movesize = 0.001f;
    sithPlayerActions_SetForceMoveStartPos(pThing);
    sithThing_DetachThing(pThing);
    sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_CLIMBTOHANG, NULL);
    sithInventory_SetSwimmingInventory(pThing, 0);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBDOWNONTO);
}

// Returns dot product of horizontal surface normal and horizontal look direction
static float sithPlayerActions_GetHorizontalDot(const SithThing* pThing, const rdVector3* pNormal)
{
    rdVector3 normal;
    normal.x = pNormal->x;
    normal.y = pNormal->y;
    normal.z = 0.0f;
    rdVector_Normalize3Acc(&normal);

    rdVector3 look;
    look.x = pThing->orient.lvec.x;
    look.y = pThing->orient.lvec.y;
    look.z = 0.0f;
    rdVector_Normalize3Acc(&look);

    return look.z * normal.z + look.y * normal.y + look.x * normal.x;
}

int J3DAPI sithPlayerActions_CanMoveToPos(SithThing* pThing, rdVector3* pPos, int* a3)
{
    SITH_ASSERTREL(pThing);

    int result   = 0;
    float radius = pThing->collide.movesize * 0.5f;
    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, pPos, 0.0f);
    if ( !pSector )
    {
        return 2;
    }

    // Search for floor at pPos
    rdVector3 downDir;
    rdVector_Set3(&downDir, 0.0f, 0.0f, -1.0f);
    sithCollision_SearchForCollisions(pSector, pThing, pPos, &downDir, sithPhysics_GetThingHeight(pThing) - -0.115f, radius, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            SITH_ASSERTREL(pCollision->pSurfaceCollided);
            SithSurface* pSurface = pCollision->pSurfaceCollided;
            SithSurfaceFlag surfFlags = pSurface->flags;
            float dot = sithPlayerActions_DotZ(&pSurface->face.normal);
            if ( (surfFlags & SITH_SURFACE_ISFLOOR) == 0 && (surfFlags & SITH_SURFACE_LAVA) == 0 )
            {
                if ( dot >= 0.63f )
                {
                    result = 2;
                }
                else if ( pThing->moveStatus != SITHPLAYERMOVE_RUNNING )
                {
                    result = 0;
                }
                else
                {
                    result = sithPlayerActions_GetHorizontalDot(pThing, &pSurface->face.normal) <= 0.0f ? 3 : 0;
                }
            }
            else
            {
                SithThingMoveStatus moveStatus = pThing->moveStatus;
                if ( ((moveStatus == SITHPLAYERMOVE_UNKNOWN_4 || moveStatus == SITHPLAYERMOVE_UNKNOWN_5) && (surfFlags & (SITH_SURFACE_WATER | SITH_SURFACE_SHALLOWWATER)) != 0)
                    || (surfFlags & SITH_SURFACE_LAVA) != 0 )
                {
                    result = 0;
                    break;
                }

                if ( dot < 0.80000001f && dot > 0.69f )
                {
                    // Steep slope
                    if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_CONTROLSDISABLED) != 0 )
                    {
                        result = 1;
                    }
                    else if ( moveStatus != SITHPLAYERMOVE_RUNNING )
                    {
                        result = 0;
                    }
                    else
                    {
                        result = sithPlayerActions_GetHorizontalDot(pThing, &pSurface->face.normal) <= 0.0f ? 3 : 0;
                    }
                }
                else
                {
                    result = dot >= 0.69f ? 1 : 0;
                }
            }

            if ( a3 )
            {
                *a3 = (pThing->attach.flags & SITH_ATTACH_SURFACE) != 0 && pThing->attach.attachedToStructure.pSurfaceAttached == pSurface ? 0 : 1;
            }

            break;
        }

        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            SITH_ASSERTREL(pCollision->pThingCollided);
            SithThing* pHitThing = pCollision->pThingCollided;
            if ( (pHitThing->flags & SITH_TF_STANDON) == 0 && pHitThing->type != SITH_THING_ITEM )
            {
                result = 0;
                break;
            }

            result = 1;
            if ( a3 )
            {
                *a3 = (pThing->attach.flags & SITH_ATTACH_THINGFACE) != 0 && pThing->attach.attachedToStructure.pThingAttached == pHitThing ? 0 : 1;
            }

            break;
        }

        if ( (pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0
            && pCollision->pSurfaceCollided
            && (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_NOPLAYERMOVE) != 0
            && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_LAVA) != 0 )
        {
            *a3 = 0;
            result = 0;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return result;
}

double J3DAPI sithPlayerActions_SearchForCollision(SithThing* pThing, const rdVector3* pPYR, SithSurface** pHitSurf, rdFace** pHitFace, rdModel3Mesh** pHitMesh, SithThing** pHitThing)
{
    (void)pPYR; // Unused in v1.0, look vector is used instead
    if ( sithPlayerActions_HasActiveWeapon(pThing) )
    {
        return -1.0;
    }

    rdVector3 startPos;
    rdVector_Copy3(&startPos, &pThing->pos);
    startPos.z = pThing->pos.z - -0.090000004f;

    rdVector3 lookDir;
    rdVector_Copy3(&lookDir, &pThing->orient.lvec);

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return -1.0;
    }

    // Search for ledge surface or 3DO ledge face in front
    bool bFoundSurf = false;
    bool bFoundFace = false;
    SithSurface* pLedgeSurf = NULL;
    rdFace* pLedgeFace      = NULL;
    float distance          = 0.0f;
    const rdVector3* pLook  = &pThing->orient.lvec;
    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &lookDir, 0.090000004f, 0.0049999999f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            SithSurface* pSurface = pCollision->pSurfaceCollided;
            if ( !pSurface || (pSurface->flags & SITH_SURFACE_LEDGE) == 0 )
            {
                continue;
            }

            const rdVector3* pNormal = &pSurface->face.normal;
            if ( -pNormal->z * pLook->z + -pNormal->y * pLook->y + pLook->x * -pNormal->x < 0.949f )
            {
                continue;
            }

            if ( sithPlayerActions_g_pCurCollidedSurface != pSurface )
            {
                sithPlayerActions_g_pCurCollidedSurface = pSurface;
                pLedgeSurf = pSurface;
                bFoundSurf = true;
                distance   = pCollision->distance;
                if ( distance < 0.0f )
                {
                    pLedgeSurf = NULL;
                    bFoundSurf = false;
                }
            }

            break;
        }

        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            rdFace* pFace = pCollision->pFaceCollided;
            if ( !pFace || pFace == sithPlayerActions_g_pCurCollidedModelFace || (pFace->flags & RD_FF_3DO_LEDGE) == 0 )
            {
                continue;
            }

            sithPlayerActions_g_pCurCollidedModelFace = pFace;
            pLedgeFace = pFace;

            rdVector3 normal;
            rdMatrix_TransformVector34(&normal, &pFace->normal, &pCollision->pThingCollided->orient);
            if ( -pLook->x * normal.x + -pLook->y * normal.y + -pLook->z * normal.z <= 0.949f )
            {
                continue;
            }

            distance   = pCollision->distance;
            bFoundFace = true;
            if ( distance < 0.0f )
            {
                distance = 0.0f;
            }

            *pHitMesh  = pCollision->pMeshCollided;
            *pHitThing = pCollision->pThingCollided;
            sithPlayerActions_g_pCurCollidedModel     = pCollision->pThingCollided->renderData.data.pModel3;
            sithPlayerActions_g_pCurCollidedModelFace = pFace;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();

    if ( bFoundSurf || bFoundFace )
    {
        // Check there's no floor below
        float movesize = pThing->collide.movesize;
        pThing->collide.movesize = 0.02f;
        sithPhysics_ResetThingMovement(pThing);

        rdVector3 downDir;
        rdVector_Neg3(&downDir, &rdroid_g_zVector3);

        bool bBlocked = false;
        sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &downDir, 0.13f, pThing->collide.movesize, 0xA00);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                if ( (pCollision->pSurfaceCollided->flags & SITH_SURFACE_LAVA) == 0 )
                {
                    bBlocked = true;
                    break;
                }
            }
            else if ( (pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0 )
            {
                if ( (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_NOPLAYERMOVE) != 0 && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_LAVA) == 0 )
                {
                    bBlocked = true;
                    break;
                }
            }
            else if ( (pCollision->type & SITHCOLLISION_THING) != 0
                && (pCollision->pThingCollided->flags & SITH_TF_STANDON) != 0
                && pCollision->pThingCollided != *pHitThing )
            {
                bBlocked = true;
                break;
            }
        }

        sithCollision_DecreaseStackLevel();
        pThing->collide.movesize = movesize;
        if ( bBlocked )
        {
            return -1.0;
        }
    }

    if ( bFoundSurf )
    {
        *pHitSurf = pLedgeSurf;
        return distance;
    }

    if ( bFoundFace )
    {
        *pHitFace = pLedgeFace;
        return distance;
    }

    return -1.0;
}

int J3DAPI sithPlayerActions_CanClimbOn1m(SithThing* pThing)
{
    rdVector3 startPos;
    rdVector_Copy3(&startPos, &pThing->pos);
    startPos.z = (startPos.z - sithPhysics_GetThingHeight(pThing)) - -0.098000005f;

    rdVector3 lookDir;
    rdVector_Copy3(&lookDir, &pThing->orient.lvec);
    float moveDist = pThing->collide.movesize - -0.0099999998f;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    // Search for a wall or 3DO thing in front of the feet
    bool bFound = false;
    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &lookDir, moveDist, 0.0099999998f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            SithSurface* pSurface = pCollision->pSurfaceCollided;
            if ( !pSurface )
            {
                continue;
            }

            float dot = pSurface->face.normal.x * rdroid_g_zVector3.x + pSurface->face.normal.z * rdroid_g_zVector3.z + pSurface->face.normal.y * rdroid_g_zVector3.y;
            if ( dot < 0.1f && dot > -0.1f )
            {
                bFound = true;
                break;
            }
        }
        else if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            rdFace* pFace        = pCollision->pFaceCollided;
            SithThing* pHitThing = pCollision->pThingCollided;

            // Note, face is not checked for null when thing has 0x1000 flag set
            if ( ((pFace && (pHitThing->flags & SITH_TF_STANDON) != 0 && (pHitThing->flags & SITH_TF_MOUNTABLE) != 0) || (pHitThing->flags & SITH_TF_UNKNOWN_1000) != 0)
                && pFace->normal.z == 0.0f )
            {
                // Can't climb on bridges
                char aName[256];
                STD_STRCPY(aName, pHitThing->aName);
                bFound = strcmp(aName, "20mropebridge") != 0
                    && strcmp(aName, "10mbrokebridge") != 0
                    && strcmp(aName, "20mholesbridge") != 0
                    && strcmp(aName, "20mholebridge") != 0
                    && strcmp(aName, "25mholebridge") != 0;
                break;
            }
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bFound )
    {
        return 0;
    }

    // Check that there's free space above the step
    startPos.z = startPos.z - -0.115f;
    pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    int bCanClimb = 1;
    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &lookDir, 0.071000002f, 0.045000002f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        bool bBlocked;
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            bBlocked = pCollision->pSurfaceCollided != NULL;
        }
        else
        {
            bBlocked = (pCollision->type & SITHCOLLISION_THING) != 0 && pCollision->pFaceCollided;
        }

        if ( bBlocked )
        {
            bCanClimb = 0;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return bCanClimb;
}

int J3DAPI sithPlayerActions_CanClimbOn2m(SithThing* pThing)
{
    rdVector3 startPos;
    rdVector_Copy3(&startPos, &pThing->pos);
    startPos.z = (startPos.z - sithPhysics_GetThingHeight(pThing)) - -0.15000001f;

    rdVector3 lookDir;
    rdVector_Copy3(&lookDir, &pThing->orient.lvec);
    float moveDist = pThing->collide.movesize - -0.0099999998f;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    // Search for ledge or climbable crate in front
    int bFound = 0;
    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &lookDir, moveDist, 0.02f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        bool bLedge = false;
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            bLedge = pCollision->pSurfaceCollided && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_LEDGE) != 0;
        }
        else if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            bLedge = pCollision->pThingCollided && (pCollision->pThingCollided->flags & SITH_TF_CLIMBCRATE) != 0;
        }

        if ( bLedge && pCollision->distance < 0.039999999f )
        {
            bFound = 1;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();

    bool bBlocked = false;
    if ( bFound )
    {
        // Check that there's free space above the ledge
        rdVector3 abovePos;
        abovePos.x = startPos.x;
        abovePos.y = startPos.y;
        abovePos.z = startPos.z - -0.15000001f;
        pSector = sithCollision_FindSectorInRadius(pSector, &startPos, &abovePos, 0.0f);
        if ( !pSector )
        {
            return 0;
        }

        sithCollision_SearchForCollisions(pSector, pThing, &abovePos, &lookDir, pThing->collide.movesize - -0.050000001f, 0.045000002f, 0xA00);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            bool bHit;
            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                bHit = pCollision->pSurfaceCollided != NULL;
            }
            else
            {
                bHit = (pCollision->type & SITHCOLLISION_THING) != 0;
            }

            if ( bHit )
            {
                bBlocked = true;
                break;
            }
        }

        sithCollision_DecreaseStackLevel();
    }

    // Check head room
    bool bHeadRoom = true;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &rdroid_g_zVector3, 0.2f, 0.029999999f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        bool bHit;
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            bHit = pCollision->pSurfaceCollided != NULL;
        }
        else
        {
            bHit = (pCollision->type & SITHCOLLISION_THING) != 0 && pCollision->pThingCollided != pThing;
        }

        if ( bHit )
        {
            bHeadRoom = false;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bHeadRoom || (bFound && bBlocked) )
    {
        return 0;
    }

    return bFound;
}

int J3DAPI sithPlayerActions_SearchClimbSurface(SithThing* pThing)
{
    if ( sithInventory_GetCurrentWeapon(pThing) != SITHWEAPON_NO_WEAPON || sithWeapon_IsMountingWeapon(pThing) )
    {
        return 0;
    }

    // Check that nothing is behind the player
    rdVector3 lookDir, backDir;
    rdVector_Copy3(&lookDir, &pThing->orient.lvec);
    rdVector_Neg3(&backDir, &lookDir);

    bool bBlocked = false;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &backDir, 0.07f, pThing->collide.movesize, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & (SITHCOLLISION_WORLD | SITHCOLLISION_THING)) != 0 )
        {
            bBlocked = true;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( bBlocked )
    {
        return 0;
    }

    // Check that there's no floor behind the player
    rdVector3 probePos;
    probePos.x = backDir.x * 0.07f + pThing->pos.x;
    probePos.y = backDir.y * 0.07f + pThing->pos.y;
    probePos.z = backDir.z * 0.07f + pThing->pos.z;

    rdVector3 downDir;
    rdVector_Neg3(&downDir, &rdroid_g_zVector3);

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &probePos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    sithCollision_SearchForCollisions(pSector, pThing, &probePos, &downDir, 0.25f, pThing->collide.movesize, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        bool bHit;
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            bHit = (pCollision->pSurfaceCollided->flags & SITH_SURFACE_CLIMBABLE) == 0;
        }
        else
        {
            bHit = (pCollision->type & SITHCOLLISION_THING) != 0;
        }

        if ( bHit )
        {
            bBlocked = true;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( bBlocked )
    {
        return 0;
    }

    // Move probe slightly below and search for wall facing back
    downDir.x = downDir.x * 0.1f;
    downDir.y = downDir.y * 0.1f;
    downDir.z = downDir.z * 0.1f;

    rdVector3 prevPos = probePos;
    probePos.x = downDir.x + probePos.x;
    probePos.y = downDir.y + probePos.y;
    probePos.z = downDir.z + probePos.z;
    pSector = sithCollision_FindSectorInRadius(pSector, &prevPos, &probePos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    rdVector_Neg3(&backDir, &pThing->orient.lvec);

    bool bClimbable = false;
    bool bLedge     = false;
    bool bHasSlope  = false;
    sithCollision_SearchForCollisions(pSector, NULL, &probePos, &lookDir, 0.02f, 0.039999999f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            SithSurface* pSurface = pCollision->pSurfaceCollided;
            if ( !pSurface )
            {
                continue;
            }

            rdVector3 normal = pSurface->face.normal;
            if ( normal.z > 0.1f )
            {
                bHasSlope = true;
            }

            normal.z = 0.0f;
            rdVector_Normalize3Acc(&normal);
            if ( normal.z * backDir.z + normal.y * backDir.y + normal.x * backDir.x <= 0.949f )
            {
                continue;
            }

            if ( (pSurface->flags & SITH_SURFACE_LEDGE) != 0 )
            {
                bLedge = true;
                break;
            }

            if ( (pSurface->flags & SITH_SURFACE_CLIMBABLE) != 0 )
            {
                bClimbable = true;
                break;
            }
        }
        else if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            rdFace* pFace = pCollision->pFaceCollided;
            if ( !pFace || (pFace->flags & RD_FF_3DO_LEDGE) == 0 )
            {
                continue;
            }

            rdVector3 normal;
            rdMatrix_TransformVector34(&normal, &pFace->normal, &pCollision->pThingCollided->orient);
            if ( normal.z * backDir.z + normal.y * backDir.y + normal.x * backDir.x > 0.949f )
            {
                bLedge = true;
                break;
            }
        }
    }

    sithCollision_DecreaseStackLevel();

    // 1 - climbable wall, 3 - climbable wall below sloped ledge, 2 - ledge to hang on
    if ( bClimbable )
    {
        return bHasSlope ? 3 : 1;
    }

    return bLedge ? 2 : 0;
}

int J3DAPI sithPlayerActions_ActorGrabLedge(SithThing* pThing, float distance, SithSurface* pHitSurf, rdFace* pHitFace, const rdModel3Mesh* pHitMesh, SithThing* pHitThing)
{
    (void)distance; // Unused in v1.0

    // Face the ledge
    rdVector3 look;
    if ( pHitSurf )
    {
        rdVector_Neg3(&look, &pHitSurf->face.normal);
        sithPlayerActions_AlignOrient(pThing, &look);
    }
    else if ( pHitFace )
    {
        rdVector3 normal;
        rdMatrix_TransformVector34(&normal, &pHitFace->normal, &pHitThing->orient);
        rdVector_Neg3(&look, &normal);
        sithPlayerActions_AlignOrient(pThing, &look);
    }

    pThing->moveStatus = SITHPLAYERMOVE_HANGING;
    if ( pHitSurf )
    {
        rdVector3 prevPos = pThing->pos;
        rdVector3 pos     = pThing->pos;

        // Get ledge top
        const int* aVertices    = pHitSurf->face.aVertices;
        const rdVector3* pVert0 = &sithWorld_g_pCurrentWorld->aVertices[aVertices[0]];
        float topZ = pVert0->z;
        for ( size_t i = 0; i < pHitSurf->face.numVertices; ++i )
        {
            float z = sithWorld_g_pCurrentWorld->aVertices[aVertices[i]].z;
            if ( z > topZ )
            {
                topZ = z;
            }
        }

        const rdVector3* pNormal = &pHitSurf->face.normal;
        float dist = (pos.y - pVert0->y) * pNormal->y + (pos.x - pVert0->x) * pNormal->x + (topZ - pVert0->z) * pNormal->z;
        pThing->pos.x = -pThing->orient.lvec.x * 0.024f + pThing->orient.lvec.x * dist + pThing->pos.x;
        pThing->pos.y = -pThing->orient.lvec.y * 0.024f + pThing->orient.lvec.y * dist + pThing->pos.y;
        pThing->pos.z = topZ - 0.118f;

        // Note, original doesn't check for null sector
        SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &prevPos, &pThing->pos, 0.0f);
        if ( pSector != pThing->pInSector )
        {
            sithThing_ExitSector(pThing);
            sithThing_EnterSector(pThing, pSector, 1, 0);
        }

        sithThing_AttachThingToClimbSurface(pThing, pHitSurf);
        sithPlayerActions_g_pCurCollidedSurface = pHitSurf;
    }
    else if ( pHitFace )
    {
        rdVector3 prevPos = pThing->pos;
        rdVector3 pos     = pThing->pos;

        float topZ = sithPlayerActions_GetFaceTopZ(pHitThing, pHitFace, pHitMesh) + pHitThing->pos.z;
        rdMatrix34 orient = pHitThing->orient;

        rdVector3 normal;
        rdMatrix_TransformVector34(&normal, &pHitFace->normal, &orient);

        rdVector3 vert;
        rdMatrix_TransformVector34(&vert, &pHitMesh->apVertices[pHitFace->aVertices[0]], &orient);
        vert.x = pHitThing->pos.x + vert.x;
        vert.y = pHitThing->pos.y + vert.y;
        vert.z = vert.z + pHitThing->pos.z;

        float dist = (pos.x - vert.x) * normal.x + (pos.y - vert.y) * normal.y + (topZ - vert.z) * normal.z;
        pThing->pos.x = -pThing->orient.lvec.x * 0.024f + pThing->orient.lvec.x * dist + pThing->pos.x;
        pThing->pos.y = -pThing->orient.lvec.y * 0.024f + pThing->orient.lvec.y * dist + pThing->pos.y;
        pThing->pos.z = topZ - 0.118f;

        SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &prevPos, &pThing->pos, 0.0f);
        if ( pSector != pThing->pInSector )
        {
            sithThing_ExitSector(pThing);
            sithThing_EnterSector(pThing, pSector, 1, 0);
        }

        sithThing_AttachThingToThingFace(pThing, pHitThing, pHitFace, pHitMesh->apVertices, 1);
        sithPlayerActions_g_pCurCollidedModelFace = pHitFace;
        sithPlayerActions_g_pCurCollidedModel     = pHitThing->renderData.data.pModel3;
    }

    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_HANGLEDGE, NULL);
    pThing->moveInfo.physics.flags &= ~SITH_PF_FLOORSTICK;
    sithInventory_SetSwimmingInventory(pThing, 0);
    return 1;
}

int J3DAPI sithPlayerActions_ClimbDownToIdle(SithThing* pThing)
{
    float movesize = pThing->collide.movesize;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &pThing->orient.lvec, movesize, movesize, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        SithSurface* pSurface = pCollision->pSurfaceCollided;
        if ( (pCollision->type & (SITHCOLLISION_WORLD | SITHCOLLISION_ADJOINCROSS)) != 0 && pSurface && (pSurface->flags & SITH_SURFACE_CLIMBABLE) != 0 )
        {
            pThing->moveStatus = SITHPLAYERMOVE_CLIMBIDLE;
            sithPlayerActions_MoveToClimbSector(pThing, pSurface);
            sithThing_AttachThingToClimbSurface(pThing, pSurface);
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLIDLE, NULL);
            pThing->moveInfo.physics.flags &= ~SITH_PF_FLOORSTICK;
            sithInventory_SetSwimmingInventory(pThing, 1);
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return 1;
}

// Returns true if any world surface, thing or non-passable adjoin blocks the move
static bool sithPlayerActions_IsPullUpBlocked(SithSector* pSector, SithThing* pThing, const rdVector3* pPos, const rdVector3* pDir, float moveDist, float radius)
{
    bool bBlocked = false;
    sithCollision_SearchForCollisions(pSector, pThing, pPos, pDir, moveDist, radius, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        SithCollisionType type = pCollision->type;
        if ( (type & SITHCOLLISION_WORLD) != 0 )
        {
            if ( pCollision->pSurfaceCollided )
            {
                bBlocked = true;
                break;
            }
        }
        else if ( (type & SITHCOLLISION_THING) != 0
            || ((type & SITHCOLLISION_ADJOINTOUCH) != 0 && pCollision->pSurfaceCollided && (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_NOPLAYERMOVE) != 0) )
        {
            bBlocked = true;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return bBlocked;
}

int J3DAPI sithPlayerActions_CanPullUp(SithThing* pThing)
{
    // Can't pull up on moving thing
    SithThing* pAttachedThing = pThing->attach.attachedToStructure.pThingAttached;
    if ( pThing->attach.flags == SITH_ATTACH_THINGFACE && pAttachedThing )
    {
        if ( pAttachedThing->moveType == SITH_MT_PHYSICS )
        {
            if ( !sithPlayerActions_IsZeroVector(&pAttachedThing->moveInfo.physics.velocity) )
            {
                return 0;
            }
        }
        else if ( pAttachedThing->moveType == SITH_MT_PATH && (pAttachedThing->moveInfo.pathMovement.mode & 3) != 0 )
        {
            return 0;
        }
    }

    // Check head room
    if ( sithPlayerActions_IsPullUpBlocked(pThing->pInSector, pThing, &pThing->pos, &rdroid_g_zVector3, 0.22f, 0.015f) )
    {
        return 0;
    }

    rdVector3 checkPos;
    checkPos.x = pThing->pos.x;
    checkPos.y = pThing->pos.y;
    checkPos.z = pThing->pos.z - -0.178f;
    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &checkPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    rdVector3 lookDir;
    rdVector_Copy3(&lookDir, &pThing->orient.lvec);
    if ( sithPlayerActions_IsPullUpBlocked(pSector, NULL, &checkPos, &lookDir, 0.07f, 0.0099999998f) )
    {
        return 0;
    }

    checkPos.z = checkPos.z - -0.068999998f;
    pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &checkPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    rdVector_Copy3(&lookDir, &pThing->orient.lvec);
    return !sithPlayerActions_IsPullUpBlocked(pSector, NULL, &checkPos, &lookDir, 0.07f, 0.02f);
}

float J3DAPI sithPlayerActions_GetFaceTopZ(const SithThing* pThing, const rdFace* pFace, const rdModel3Mesh* pMesh)
{
    int numVertices   = (int)pFace->numVertices;
    rdMatrix34 orient = pThing->orient;
    if ( numVertices < 1 )
    {
        return 0.0f;
    }

    rdVector3 vert;
    rdMatrix_TransformVector34(&vert, &pMesh->apVertices[pFace->aVertices[0]], &orient);
    float topZ = vert.z;
    for ( int i = 1; i < numVertices; ++i )
    {
        rdMatrix_TransformVector34(&vert, &pMesh->apVertices[pFace->aVertices[i]], &orient);
        if ( vert.z > topZ )
        {
            topZ = vert.z;
        }
    }

    return topZ;
}

void J3DAPI sithPlayerActions_StartInvisibility()
{
    SithThing* pPlayer = sithPlayer_g_pLocalPlayerThing;
    if ( sithPlayerActions_g_bPlayerInvisible == 1 )
    {
        return;
    }

    pPlayer->flags &= ~SITH_TF_SHADOW;

    rdVector4 startColor = { { 0.0f }, { 0.0f }, { 0.0f }, { 1.0f } };
    rdVector4 endColor   = { { 0.0f }, { 0.0f }, { 0.0f }, { 0.1f } };
    sithAnimate_StartThingFadeAnim(sithPlayer_g_pLocalPlayerThing, &startColor, &endColor, 1.0f, (SithAnimateFlags)0);
    sithPlayerActions_g_bPlayerInvisible = 1;

    if ( pPlayer )
    {
        pPlayer->thingInfo.actorInfo.flags |= SITH_AF_INVISIBLE;
    }
}

void J3DAPI sithPlayerActions_EndInvisibility()
{
    SithThing* pPlayer = sithPlayer_g_pLocalPlayerThing;
    if ( !sithPlayerActions_g_bPlayerInvisible )
    {
        return;
    }

    rdVector4 startColor = { { 0.0f }, { 0.0f }, { 0.0f }, { 0.1f } };
    rdVector4 endColor   = { { 0.0f }, { 0.0f }, { 0.0f }, { 1.0f } };
    sithAnimate_StartThingFadeAnim(sithPlayer_g_pLocalPlayerThing, &startColor, &endColor, 1.0f, (SithAnimateFlags)0);
    sithPlayerActions_g_bPlayerInvisible = 0;
    sithFX_DestroyFairyDustDeluxe(pPlayer);

    if ( pPlayer )
    {
        pPlayer->thingInfo.actorInfo.flags &= ~SITH_AF_INVISIBLE;
    }

    pPlayer->flags |= SITH_TF_SHADOW;
}

int J3DAPI sithPlayerActions_IsInvisible()
{
    return sithPlayerActions_g_bPlayerInvisible;
}

int J3DAPI sithPlayerActions_StartJewelFlying()
{
    SithThing* pPlayer = sithPlayer_g_pLocalPlayerThing;
    if ( !pPlayer )
    {
        return 0;
    }

    if ( sithInventory_GetCurrentWeapon(pPlayer) != SITHWEAPON_IMP3 || !sithPlayerActions_g_bJewelFlying )
    {
        return 0;
    }

    sithPhysics_ResetThingMovement(pPlayer);
    pPlayer->moveStatus = SITHPLAYERMOVE_JEWELFLYING;
    sithThing_DetachThing(pPlayer);
    pPlayer->moveInfo.physics.flags = (pPlayer->moveInfo.physics.flags & ~SITH_PF_USEGRAVITY) | SITH_PF_FLY;

    if ( sithPlayerActions_g_pPlasma )
    {
        // Move player above the plasma
        sithPlayerActions_g_pPlasma->flags &= ~SITH_TF_DISABLED;

        rdVector3 pos;
        pos.x = sithPlayerActions_g_pPlasma->pos.x;
        pos.z = pPlayer->pos.z - -0.050000001f;
        pos.y = sithPlayerActions_g_pPlasma->pos.y;
        pPlayer->collide.type = SITH_COLLIDE_NONE;
        pPlayer->thingInfo.actorInfo.bControlsDisabled = 1;
        sithAnimate_StartThingMoveAnimToPosAnim(pPlayer, &pos, 1.0f);
    }

    sithInventory_SetSwimmingInventory(pPlayer, 0);

    rdKeyframe* pKey = sithPuppet_GetKeyframe("in_imp3_float.key");
    if ( pKey )
    {
        sithPlayerActions_g_jewelFlyingPuppetTrackNum = sithPuppet_PlayKey(pPlayer->renderData.pPuppet, pKey, 3, 4, (rdKeyframeFlags)0, NULL);
        sithPuppet_ClearMode(pPlayer, SITHPUPPETSUBMODE_STAND);
    }

    return 1;
}

int J3DAPI sithPlayerActions_EnableJewelFlying(SithThing* pPlasma)
{
    sithPlayerActions_g_bJewelFlying = 1;
    sithPlayerActions_g_pPlasma      = pPlasma;
    if ( sithPlayer_g_impFireType == SITHWEAPON_IMP3 )
    {
        return sithPlayerActions_StartJewelFlying();
    }

    return sithPlayer_g_impFireType;
}

SithThing* J3DAPI sithPlayerActions_DisableJewelFlying()
{
    SithThing* pPlayer = sithPlayer_g_pLocalPlayerThing;
    if ( pPlayer )
    {
        switch ( pPlayer->moveStatus )
        {
            case SITHPLAYERMOVE_CRAWL_STILL:
            case SITHPLAYERMOVE_UNKNOWN_4:
            case SITHPLAYERMOVE_UNKNOWN_5:
            case SITHPLAYERMOVE_PUSHING:
            case SITHPLAYERMOVE_PULLING:
            case SITHPLAYERMOVE_STAND_TO_CRAWL:
            case SITHPLAYERMOVE_CRAWL_TO_STAND:
                break;

            default:
                pPlayer->collide.movesize = 0.039999999f;
                break;
        }
    }

    SithThing* pPlasma = sithPlayerActions_g_pPlasma;
    if ( sithPlayerActions_g_pPlasma )
    {
        sithPlayerActions_g_pPlasma->flags |= SITH_TF_DISABLED;
    }

    sithPlayerActions_g_pPlasma      = NULL;
    sithPlayerActions_g_bJewelFlying = 0;
    return pPlasma;
}

void J3DAPI sithPlayerActions_MoveToClimbSector(SithThing* pThing, SithSurface* pSurface)
{
    rdVector3 normal;
    normal.x = pSurface->face.normal.x;
    normal.y = pSurface->face.normal.y;
    normal.z = 0.0f;
    rdVector_Normalize3Acc(&normal);

    // Get horizontal bounds of the climb face
    const int* aVertices         = pSurface->face.aVertices;
    const rdVector3* aWorldVerts = sithWorld_g_pCurrentWorld->aVertices;
    float maxX = aWorldVerts[aVertices[0]].x;
    float minX = maxX;
    float maxY = aWorldVerts[aVertices[0]].y;
    float minY = maxY;
    for ( size_t i = 0; i < pSurface->face.numVertices; ++i )
    {
        const rdVector3* pVert = &aWorldVerts[aVertices[i]];
        if ( maxX <= pVert->x )
        {
            maxX = pVert->x;
        }

        if ( pVert->x <= minX )
        {
            minX = pVert->x;
        }

        if ( maxY <= pVert->y )
        {
            maxY = pVert->y;
        }

        if ( pVert->y <= minY )
        {
            minY = pVert->y;
        }
    }

    // Move to the center of climb face
    rdVector3 newPos;
    newPos.x = normal.x * 0.050000001f + (((maxX - minX) * 0.5f + minX) - pThing->pos.x) + pThing->pos.x;
    newPos.y = normal.y * 0.050000001f + (((maxY - minY) * 0.5f + minY) - pThing->pos.y) + pThing->pos.y;
    newPos.z = normal.z * 0.050000001f + pThing->pos.z;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &newPos, 0.0f);
    if ( !pSector )
    {
        SITHLOG_ERROR("can't move to valid sector centering on climb face!");
        return;
    }

    SithSector* pCurSector = pThing->pInSector;
    rdVector_Copy3(&pThing->pos, &newPos);
    if ( pSector != pCurSector )
    {
        sithThing_ExitSector(pThing);
        sithThing_EnterSector(pThing, pSector, 1, 1);
    }
}

void J3DAPI sithPlayerActions_Crawl(SithThing* pThing)
{
    rdVector3 downDir;
    rdVector_Neg3(&downDir, &rdroid_g_zVector3);

    // Search for floor below
    float floorDist = 0.0f;
    sithCollision_SearchForCollisions(pThing->pInSector, NULL, &pThing->pos, &downDir, 0.2f, 0.0f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 && pCollision->pSurfaceCollided && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0 )
        {
            floorDist = pCollision->distance;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();

    // Move down onto the floor
    floorDist = floorDist - 0.045000002f;
    if ( floorDist > 0.0f )
    {
        rdVector3 newPos;
        newPos.x = floorDist * downDir.x + pThing->pos.x;
        newPos.y = pThing->pos.y + floorDist * downDir.y;
        newPos.z = pThing->pos.z + floorDist * downDir.z;

        // Note, original doesn't check for null sector
        SithSector* pSector    = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &newPos, 0.0f);
        SithSector* pCurSector = pThing->pInSector;
        rdVector_Copy3(&pThing->pos, &newPos);
        if ( pSector != pCurSector )
        {
            sithThing_ExitSector(pThing);
            sithThing_EnterSector(pThing, pSector, 0, 0);
        }
    }
}
