#include "sithPhysics.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdMath.h>
#include <rdroid/Math/rdVector.h>
#include <sith/AI/sithAIAwareness.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Devices/sithControl.h>
#include <sith/World/sithTemplate.h>
#include <sith/Engine/sithAnimate.h>
#include <sith/World/sithActor.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithFX.h>
#include <sith/World/sithMaterial.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWorld.h>
#include <std/General/stdMath.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>

static int sithPhysics_bMineCarEngineRunFx = 1;
static int sithPhysics_bMineCarEngineRumbleFx = 1;
static int sithPhysics_bMineCarSparksFx = 1;
static int sithPhysics_bMineCarClatterFx = 1;
static int sithPhysics_bMineCarRailClackFx = 1;
static rdVector3 sithPhysics_vecMinecarSparkPosLeft = { { 0.050000001f }, { -0.039999999f }, { -0.090000004f } };
static rdVector3 sithPhysics_vecMinecarSparkPosRight = { { -0.050000001f }, { -0.039999999f }, { -0.090000004f } };
static int sithPhysics_bUpdateTrackJeepEngineFx = 1;
static float sithPhysics_truckMinSndPitch = 1.0f;
static float sithPhysics_truckMaxSndPitch = 2.0f;
static float sithPhysics_truckMinSndVolume = 0.60000002f;
static int sithPhysics_bUpdateTrackTruckEngineFx = 1;
static float sithPhysics_flt_538D04 = 0.60000002f;
static int sithPhysics_dword_538D2C = 1;
static int sithPhysics_dword_538D30 = 1;
static int sithPhysics_bJeepMoveFx = 1;
static int sithPhysics_dword_538D38 = 1;
static int sithPhysics_dword_58540C;
static float sithPhysics_flt_585410;

void sithPhysics_InstallHooks(void)
{
    J3D_HOOKFUNC(sithPhysics_FindFloor);
    J3D_HOOKFUNC(sithPhysics_FindWaterSurface);
    J3D_HOOKFUNC(sithPhysics_UpdateThing);
    J3D_HOOKFUNC(sithPhysics_ApplyForce);
    J3D_HOOKFUNC(sithPhysics_SetThingLook);
    J3D_HOOKFUNC(sithPhysics_ApplyDrag);
    J3D_HOOKFUNC(sithPhysics_ParseArg);
    J3D_HOOKFUNC(sithPhysics_ResetThingMovement);
    J3D_HOOKFUNC(sithPhysics_GetThingHeight);
    J3D_HOOKFUNC(sithPhysics_UpdateThingPhysics);
    J3D_HOOKFUNC(sithPhysics_UpdatePlayerPhysics);
    J3D_HOOKFUNC(sithPhysics_UpdateUnderwaterThingPhysics);
    J3D_HOOKFUNC(sithPhysics_UpdateClimbingThingPhysics);
    J3D_HOOKFUNC(sithPhysics_UpdateAttachedThingPhysics);
    J3D_HOOKFUNC(sithPhysics_CreateMineCarUserBlock);
    J3D_HOOKFUNC(sithPhysics_InitMineCarFxState);
    J3D_HOOKFUNC(sithPhysics_InitVehicleFxState);
    J3D_HOOKFUNC(sithPhysics_InitDefaultMineCarInfo);
    J3D_HOOKFUNC(sithPhysics_InitMineCarChassis);
    J3D_HOOKFUNC(sithPhysics_InitTrackTruckChassis);
    J3D_HOOKFUNC(sithPhysics_InitTrackJeepChassis);
    J3D_HOOKFUNC(sithPhysics_InitMineCarExhaust);
    J3D_HOOKFUNC(sithPhysics_InitJeepExhaust);
    J3D_HOOKFUNC(sithPhysics_InitTrackTruckExhaust);
    J3D_HOOKFUNC(sithPhysics_InitMineCarState);
    J3D_HOOKFUNC(sithPhysics_InitTrackTruckState);
    J3D_HOOKFUNC(sithPhysics_UpdateMineCarPhysics);
    J3D_HOOKFUNC(sithPhysics_UpdateTrackVehicleFx);
    J3D_HOOKFUNC(sithPhysics_UpdateMineCarFx);
    J3D_HOOKFUNC(sithPhysics_UpdateTrackJeepFx);
    J3D_HOOKFUNC(sithPhysics_UpdateTrackTruckFx);
    J3D_HOOKFUNC(sithPhysics_UpdateVehicleChassis);
    J3D_HOOKFUNC(sithPhysics_UpdateExhaustFx);
    J3D_HOOKFUNC(sithPhysics_UpdateRaftPhysics);
    J3D_HOOKFUNC(sithPhysics_GetWaterNormalAtPos);
    J3D_HOOKFUNC(sithPhysics_GetWaterThrust);
    J3D_HOOKFUNC(sithPhysics_ProcessMineCarTrackMove);
    J3D_HOOKFUNC(sithPhysics_ProcessTrackFace);
    J3D_HOOKFUNC(sithPhysics_CheckForPointOnTrack);
    J3D_HOOKFUNC(sithPhysics_MoveAlongTrackFace);
    J3D_HOOKFUNC(sithPhysics_UpdateJeepPhysics);
    J3D_HOOKFUNC(sithPhysics_UpdateJeepWheels);
    J3D_HOOKFUNC(sithPhysics_UpdateJeepWheel);
    J3D_HOOKFUNC(sithPhysics_UpdateJeepBody);
    J3D_HOOKFUNC(sithPhysics_CreateJeepUserBlock);
    J3D_HOOKFUNC(sithPhysics_InitJeepState);
    J3D_HOOKFUNC(sithPhysics_UpdateJeepFx);
}


void J3DAPI sithPhysics_FindFloor(SithThing* pThing, int bNoThingStateUpdate)
{
    if ( !pThing )
    {
        SITHLOG_ERROR("NULL thing pointer in FindFloor()");
        return;
    }

    if ( pThing->moveType != SITH_MT_PHYSICS )
    {
        SITHLOG_ERROR("Thing %s not physics type in FindFloor()", pThing->aName);
        return;
    }

    if ( !pThing->pInSector )
        return;

    if ( pThing->pInSector->flags & SITH_SECTOR_UNDERWATER )
    {
        if ( pThing->type == SITH_THING_PLAYER )
            sithPhysics_FindWaterSurface(pThing);
        return;
    }

    if ( pThing->pInSector->flags & SITH_SECTOR_AETHERIUM )
    {
        if ( pThing->attach.flags )
            sithThing_DetachThing(pThing);
        return;
    }

    SithPhysicsFlags physFlags = pThing->moveInfo.physics.flags;
    SithThingType thingType = pThing->type;

    unsigned int searchFlags = 0x2800;
    if ( thingType == SITH_THING_PLAYER && !(physFlags & (SITH_PF_MINECAR | SITH_PF_RAFT | SITH_PF_JEEP)) )
        searchFlags = 0x2a00;

    rdVector3 downVec;
    if ( physFlags & SITH_PF_WALLSTICK )
    {
        downVec.x = -pThing->orient.uvec.x;
        downVec.y = -pThing->orient.uvec.y;
        downVec.z = -pThing->orient.uvec.z;
        searchFlags |= 2;
    }
    else
    {
        downVec.x = 0.0f;
        downVec.y = 0.0f;
        downVec.z = -1.0f;
        searchFlags |= (physFlags & SITH_PF_RAFT) ? 1 : 0x10;
    }

    float floorDist;
    if ( !bNoThingStateUpdate && !pThing->attach.flags )
    {
        floorDist = sithPhysics_GetThingHeight(pThing);
        if ( floorDist == 0.0f )
            floorDist = pThing->collide.movesize + 0.005f;
    }
    else
    {
        floorDist = sithPhysics_GetThingHeight(pThing);
        floorDist *= (physFlags & (SITH_PF_FLOORSTICK | SITH_PF_WALLSTICK)) ? 2.5f : 1.1f;
    }

    float stepDist = 0.0f;
    if ( floorDist > 0.0f )
    {
        while ( true )
        {
            sithCollision_SearchForCollisions(pThing->pInSector, NULL, &pThing->pos, &downVec, floorDist, stepDist, searchFlags);
            SithCollision* pCollision = sithCollision_PopStack();
            while ( pCollision )
            {
                if ( pCollision->type & SITHCOLLISION_WORLD )
                {
                    if ( !(physFlags & SITH_PF_RAFT) )
                    {
                        sithCollision_DecreaseStackLevel();
                        sithThing_AttachThingToSurface(pThing, pCollision->pSurfaceCollided, bNoThingStateUpdate);
                        return;
                    }

                    if ( (pCollision->pSurfaceCollided->flags & SITH_SURFACE_WATER) &&
                         !(pCollision->pSurfaceCollided->pSector->flags & SITH_SECTOR_UNDERWATER) )
                    {
                        sithThing_AttachThingToSurface(pThing, pCollision->pSurfaceCollided, bNoThingStateUpdate);
                        sithCollision_DecreaseStackLevel();
                        return;
                    }
                }
                else if ( pCollision->type & SITHCOLLISION_THING )
                {
                    SithThing* pHitThing = pCollision->pThingCollided;
                    SITH_ASSERTREL(pHitThing);

                    if ( pHitThing != pThing )
                    {
                        if ( !pCollision->pFaceCollided || !pCollision->pMeshCollided )
                        {
                            SITHLOG_ERROR("Hit standon object but not enough collide info to attach.");
                            sithCollision_DecreaseStackLevel();
                            return;
                        }

                        rdVector3 faceNormalWorld;
                        rdMatrix_TransformVector34(&faceNormalWorld, &pCollision->pFaceCollided->normal, &pHitThing->orient);
                        if ( !(searchFlags & 0x10) || faceNormalWorld.z >= 0.6f )
                        {
                            sithThing_AttachThingToThingFace(pThing, pHitThing, pCollision->pFaceCollided, pCollision->pMeshCollided->apVertices, bNoThingStateUpdate);
                            sithCollision_DecreaseStackLevel();
                            return;
                        }
                    }
                }
                else if ( pCollision->type & SITHCOLLISION_ADJOINTOUCH )
                {
                    if ( physFlags & SITH_PF_RAFT )
                    {
                        SITH_ASSERTREL(pCollision->pSurfaceCollided);
                        SITH_ASSERTREL(pCollision->pSurfaceCollided->pAdjoin);
                        if ( pCollision->pSurfaceCollided->pAdjoin->pAdjoinSector->flags & SITH_SECTOR_UNDERWATER )
                        {
                            sithThing_AttachThingToSurface(pThing, pCollision->pSurfaceCollided, bNoThingStateUpdate);
                            sithCollision_DecreaseStackLevel();
                            return;
                        }
                    }
                }

                pCollision = sithCollision_PopStack();
            }
            sithCollision_DecreaseStackLevel();

            if ( stepDist != 0.0f || (thingType != SITH_THING_ACTOR && thingType != SITH_THING_PLAYER) || pThing->collide.movesize == 0.0f )
                break;

            if ( physFlags & SITH_PF_RAFT )
            {
                floorDist += floorDist;
                stepDist = 0.001f;
            }
            else if ( thingType == SITH_THING_PLAYER && pThing->moveStatus == SITHPLAYERMOVE_RUNNING && sithPuppet_g_bPlayerLeapForward != 0 )
            {
                stepDist = pThing->collide.movesize;
                floorDist -= stepDist;
                if ( floorDist <= 0.05f )
                    floorDist = 0.05f;
            }
            else
            {
                floorDist -= 0.005f;
                stepDist = 0.005f;
                if ( floorDist <= 0.05f )
                    floorDist = 0.05f;
            }
        }
    }

    if ( pThing->attach.flags )
    {
        sithThing_DetachThing(pThing);

        if ( !(physFlags & SITH_PF_JEEP) &&
             (thingType == SITH_THING_PLAYER || thingType == SITH_THING_ACTOR) &&
             pThing->thingInfo.actorInfo.bForceMovePlay == 0 )
        {
            if ( thingType == SITH_THING_PLAYER )
            {
                if ( !(physFlags & SITH_PF_RAFT) &&
                     (pThing->moveStatus == SITHPLAYERMOVE_CRAWL_STILL || pThing->moveInfo.physics.height < 0.09f) )
                {
                    sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_NORMAL);
                    pThing->collide.movesize = 0.04f;
                    pThing->moveInfo.physics.flags = (pThing->moveInfo.physics.flags & ~SITH_PF_ALIGNSURFACE) | SITH_PF_ALIGNUP;
                    pThing->moveInfo.physics.height = 0.09f;
                    sithPuppet_StopForceMove(pThing, 1);
                    pThing->moveStatus = SITHPLAYERMOVE_FALLING;
                    return;
                }

                if ( (pThing->moveStatus == SITHPLAYERMOVE_RUNNING && sithPuppet_g_bPlayerLeapForward == 1) ||
                     pThing->moveStatus == SITHPLAYERMOVE_LEAPFWD )
                {
                    pThing->moveStatus = SITHPLAYERMOVE_LEAPFWD;
                    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_LEAPLEFT, NULL);
                    sithPuppet_g_bPlayerLeapForward = 0;
                    sithPlayerActions_LeapForward(pThing);
                    return;
                }
            }

            pThing->moveStatus = SITHPLAYERMOVE_FALLING;
        }
    }
}

void J3DAPI sithPhysics_FindWaterSurface(SithThing* pThing)
{
    pThing->attach.someAttachDistance = 0.1f;

    // Fixed: Was searching downwards; the original searches upwards from the submerged thing for the water surface
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &rdroid_g_zVector3, 0.1f, 0.0f, 1);

    SithCollision* pCollision = sithCollision_PopStack();
    while ( pCollision )
    {
        if ( pCollision->type & SITHCOLLISION_ADJOINTOUCH )
        {
            SITH_ASSERTREL(pCollision->pSurfaceCollided);
            SITH_ASSERTREL(pCollision->pSurfaceCollided->pAdjoin);

            if ( !(pCollision->pSurfaceCollided->pAdjoin->pAdjoinSector->flags & SITH_SECTOR_UNDERWATER) )
            {
                if ( !(pThing->moveInfo.physics.flags & SITH_PF_ONWATERSURFACE) )
                {
                    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_EXITWATER);
                }

                pThing->moveInfo.physics.flags |= SITH_PF_ONWATERSURFACE;
                pThing->attach.someAttachDistance = pCollision->distance;
                sithCollision_DecreaseStackLevel();
                return;
            }
        }

        pCollision = sithCollision_PopStack();
    }
    sithCollision_DecreaseStackLevel();

    pThing->moveInfo.physics.flags &= ~SITH_PF_ONWATERSURFACE;
}

void J3DAPI sithPhysics_UpdateThing(SithThing* pThing, float secDeltaTime)
{
    SITH_ASSERTREL(sithThing_ValidateThingPointer(sithWorld_g_pCurrentWorld, pThing));
    SITH_ASSERTREL(pThing->moveType == SITH_MT_PHYSICS);

    if ( !pThing->pInSector )
        return;

    if ( pThing->type == SITH_THING_ACTOR && pThing->thingInfo.actorInfo.bForceMovePlay == 1 )
        return;

    if ( pThing->moveInfo.physics.flags & SITH_PF_NOUPDATE )
        return;

    // Fixed: Were decompiled as pathMovement.goalPYR and pathMovement.rotOffset, which alias these fields only in 32-bit layout
    memset(&pThing->moveInfo.physics.deltaVelocity, 0, sizeof(pThing->moveInfo.physics.deltaVelocity));
    memset(&pThing->moveInfo.physics.gravityForce, 0, sizeof(pThing->moveInfo.physics.gravityForce));

    if ( (pThing->type == SITH_THING_ACTOR || pThing->type == SITH_THING_PLAYER) &&
         (pThing->thingInfo.weaponInfo.flags & SITH_WF_DECAYEMITSOUNDAWARENESSEVENT) )
    {
        // Fixed: Was decompiled as pathMovement.moveTimeRemaining, moveVel and curOrient.rvec.x, which alias thrust only in 32-bit layout
        memset(&pThing->moveInfo.physics.thrust, 0, sizeof(pThing->moveInfo.physics.thrust));
    }

    SithAttachFlag attachFlags = pThing->attach.flags;
    if ( (attachFlags & SITH_ATTACH_THINGFACE) && pThing->moveStatus == SITHPLAYERMOVE_HANGING )
    {
        sithPhysics_UpdateClimbingThingPhysics(pThing, secDeltaTime);
        return;
    }

    SithPhysicsFlags physFlags = pThing->moveInfo.physics.flags;
    if ( physFlags & SITH_PF_JEEP )
    {
        sithPhysics_UpdateJeepPhysics(pThing, secDeltaTime);
        if ( SITH_ISFRAMECYCLE(pThing->idx, 4) )
        {
            // Let AI hear the jeep engine
            sithAIAwareness_CreateTransmittingEvent(pThing->pInSector, &pThing->pos, 2, 4.0f, pThing);
        }
        return;
    }

    if ( (physFlags & SITH_PF_MINECAR) && (attachFlags & SITH_ATTACH_SURFACE) )
    {
        sithPhysics_UpdateMineCarPhysics(pThing, secDeltaTime);
        if ( SITH_ISFRAMECYCLE(pThing->idx, 8) )
        {
            // Let AI hear the mine car
            sithAIAwareness_CreateTransmittingEvent(pThing->pInSector, &pThing->pos, 2, 2.0f, pThing);
        }
        return;
    }

    if ( (physFlags & SITH_PF_RAFT) && (attachFlags & SITH_ATTACH_SURFACE) )
    {
        sithPhysics_UpdateRaftPhysics(pThing, secDeltaTime);
        return;
    }

    if ( attachFlags & (SITH_ATTACH_THINGFACE | SITH_ATTACH_SURFACE) )
    {
        sithPhysics_UpdateAttachedThingPhysics(pThing, secDeltaTime);
        return;
    }

    if ( attachFlags & SITH_ATTACH_CLIMBSURFACE )
    {
        sithPhysics_UpdateClimbingThingPhysics(pThing, secDeltaTime);
        return;
    }

    SithSectorFlag sectorFlags = pThing->pInSector->flags;
    if ( !(sectorFlags & SITH_SECTOR_UNDERWATER) && !(sectorFlags & SITH_SECTOR_AETHERIUM) )
    {
        if ( pThing->type == SITH_THING_PLAYER )
            sithPhysics_UpdatePlayerPhysics(pThing, secDeltaTime);
        else
            sithPhysics_UpdateThingPhysics(pThing, secDeltaTime);
        return;
    }

    if ( pThing->type != SITH_THING_PLAYER && (sectorFlags & SITH_SECTOR_AETHERIUM) )
    {
        sithPhysics_UpdateThingPhysics(pThing, secDeltaTime);
        return;
    }

    sithPhysics_UpdateUnderwaterThingPhysics(pThing, secDeltaTime);
}

void J3DAPI sithPhysics_ApplyForce(SithThing* pThing, const rdVector3* force)
{
    if ( pThing->moveInfo.physics.mass > 0.0f && (force->x != 0.0f || force->y != 0.0f || force->z != 0.0f) )
    {
        float invMass = 1.0f / pThing->moveInfo.physics.mass;

        if ( force->z * invMass > 0.5f )
            sithThing_DetachThing(pThing);

        pThing->moveInfo.physics.velocity.x += force->x * invMass;
        pThing->moveInfo.physics.velocity.y += force->y * invMass;
        pThing->moveInfo.physics.velocity.z += force->z * invMass;
        pThing->moveInfo.physics.flags |= 0x8000; // unidentified status bit, likely "force applied this frame"
    }
}

void J3DAPI sithPhysics_SetThingLook(SithThing* pThing, const rdVector3* pNormal, float secDeltaTime)
{
    float dot = stdMath_ClipNearZero(1.0f - rdVector_Dot3(&pThing->orient.uvec, pNormal));
    if ( dot == 0.0f )
    {
        // Already aligned
        pThing->moveInfo.physics.flags |= SITH_PF_ALIGNED;
        return;
    }

    if ( secDeltaTime == 0.0f )
    {
        // Snap directly to the given normal, then rebuild an orthonormal basis around it.
        pThing->orient.uvec = *pNormal;
        rdVector_Cross3(&pThing->orient.rvec, &pThing->orient.lvec, &pThing->orient.uvec);
        rdVector_Normalize3Acc(&pThing->orient.rvec);
        rdVector_Cross3(&pThing->orient.lvec, &pThing->orient.uvec, &pThing->orient.rvec);
        pThing->moveInfo.physics.flags |= SITH_PF_ALIGNED;
        return;
    }

    // Ease the up vector towards the given normal over time instead of snapping to it.
    rdVector_ScaleAdd3Acc(&pThing->orient.uvec, pNormal, secDeltaTime * 10.0f);
    rdVector_Normalize3Acc(&pThing->orient.uvec);

    // lvec is rebuilt first here, from the new uvec and the old rvec (unlike the snap branch above).
    rdVector_Cross3(&pThing->orient.lvec, &pThing->orient.uvec, &pThing->orient.rvec);
    rdVector_Normalize3Acc(&pThing->orient.lvec);
    rdVector_Cross3(&pThing->orient.rvec, &pThing->orient.lvec, &pThing->orient.uvec);
}

void J3DAPI sithPhysics_ApplyDrag(rdVector3* pVelocity, float drag, float mag, float secDeltaTime)
{
    if ( mag != 0.0f && rdVector_Len3(pVelocity) < mag )
    {
        pVelocity->x = 0.0f;
        pVelocity->y = 0.0f;
        pVelocity->z = 0.0f;
        return;
    }

    if ( drag == 0.0f )
        return;

    float decay = secDeltaTime * drag;
    if (decay > 1.0f)
        decay = 1.0f;
    decay = -decay;

    pVelocity->x += pVelocity->x * decay;
    pVelocity->y += pVelocity->y * decay;
    pVelocity->z += pVelocity->z * decay;

    // Snap near-zero components to exactly zero.
    if ( (pVelocity->x < 0.0f ? -pVelocity->x : pVelocity->x) <= 1e-05f ) pVelocity->x = 0.0f;
    if ( (pVelocity->y < 0.0f ? -pVelocity->y : pVelocity->y) <= 1e-05f ) pVelocity->y = 0.0f;
    if ( (pVelocity->z < 0.0f ? -pVelocity->z : pVelocity->z) <= 1e-05f ) pVelocity->z = 0.0f;
}

int J3DAPI sithPhysics_ParseArg(const StdConffileArg* pArg, SithThing* pThing, int adjNum)
{
    SITH_ASSERTREL(pThing->moveType == SITH_MT_PHYSICS);

    switch ( adjNum )
    {
        case SITHTHING_ARG_SURFDRAG:
        {
            float surfDrag = strtof(pArg->argValue, NULL);
            if ( surfDrag >= 0.0f )
            {
                pThing->moveInfo.physics.surfDrag = surfDrag;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_AIRDRAG:
        {
            float airDrag = strtof(pArg->argValue, NULL);
            if ( airDrag >= 0.0f )
            {
                pThing->moveInfo.physics.airDrag = airDrag;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_STATICDRAG:
        {
            float staticDrag = strtof(pArg->argValue, NULL);
            if ( staticDrag >= 0.0f )
            {
                pThing->moveInfo.physics.staticDrag = staticDrag;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_MASS:
        {
            float mass = strtof(pArg->argValue, NULL);
            if ( mass >= 0.0f )
            {
                pThing->moveInfo.physics.mass = mass;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_HEIGHT:
        {
            float height = strtof(pArg->argValue, NULL);
            if ( height >= 0.0f )
            {
                pThing->moveInfo.physics.height = height;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_PHYSFLAGS:
        {
            // Original uses a symbolic flag-name lookup table here (FUN_004f6f60 against a static
            // token table); approximated with raw-hex parsing to match this project's established
            // style for other flag fields (e.g. sithThing_ParseArg's SITHTHING_ARG_THINGFLAGS).
            unsigned int physFlags;
            if ( sscanf_s(pArg->argValue, "%x", &physFlags) == 1 )
            {
                pThing->moveInfo.physics.flags = physFlags;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_MAXROTVEL:
        {
            float maxRotVel = strtof(pArg->argValue, NULL);
            if ( maxRotVel >= 0.0f && pThing->moveType == SITH_MT_PHYSICS )
            {
                pThing->moveInfo.physics.maxRotationVelocity = maxRotVel;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_MAXVEL:
        {
            float maxVel = strtof(pArg->argValue, NULL);
            if ( maxVel >= 0.0f && pThing->moveType == SITH_MT_PHYSICS )
            {
                pThing->moveInfo.physics.maxVelocity = maxVel;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_VEL:
        {
            // Fixed: Format was missing the parentheses used in level files, e.g.: vel=(0.0/0.0/1.0)
            if ( sscanf_s(pArg->argValue, "(%f/%f/%f)", &pThing->moveInfo.physics.velocity.x, &pThing->moveInfo.physics.velocity.y, &pThing->moveInfo.physics.velocity.z) == 3 )
                return 1;
            break;
        }
        case SITHTHING_ARG_ANGVEL:
        {
            // Fixed: Format was missing the parentheses used in level files, e.g.: angvel=(0.0/90.0/0.0)
            if ( sscanf_s(pArg->argValue, "(%f/%f/%f)", &pThing->moveInfo.physics.angularVelocity.x, &pThing->moveInfo.physics.angularVelocity.y, &pThing->moveInfo.physics.angularVelocity.z) == 3 )
                return 1;
            break;
        }
        case SITHTHING_ARG_ORIENTSPEED:
        {
            float orientSpeed = strtof(pArg->argValue, NULL);
            if ( orientSpeed >= 0.0f && pThing->moveType == SITH_MT_PHYSICS )
            {
                pThing->moveInfo.physics.orientSpeed = orientSpeed;
                return 1;
            }
            break;
        }
        case SITHTHING_ARG_BUOYANCY:
        {
            pThing->moveInfo.physics.buoyancy = strtof(pArg->argValue, NULL);
            return 1;
        }
        default:
            return 0;
    }

    SITHLOG_ERROR("Bad argument %s=%s line %d.\n", pArg->argName, pArg->argValue, stdConffile_GetLineNumber());
    return 0;
}

void J3DAPI sithPhysics_ResetThingMovement(SithThing* pThing)
{
    SITH_ASSERTREL(pThing && pThing->moveType == SITH_MT_PHYSICS);

    memset(&pThing->moveInfo.physics.velocity, 0, sizeof(pThing->moveInfo.physics.velocity));
    memset(&pThing->moveInfo.physics.angularVelocity, 0, sizeof(pThing->moveInfo.physics.angularVelocity));
    memset(&pThing->moveInfo.physics.rotThrust, 0, sizeof(pThing->moveInfo.physics.rotThrust));
    memset(&pThing->moveInfo.physics.thrust, 0, sizeof(pThing->moveInfo.physics.thrust));
    memset(&pThing->moveInfo.physics.deltaVelocity, 0, sizeof(pThing->moveInfo.physics.deltaVelocity));
    memset(&pThing->moveDir, 0, sizeof(pThing->moveDir));
}

float J3DAPI sithPhysics_GetThingHeight(const SithThing* pThing)
{
    SITH_ASSERTREL(pThing->moveType == SITH_MT_PHYSICS);

    float height = pThing->moveInfo.physics.height;
    if ( height == 0.0f )
    {
        if ( pThing->renderData.type == RD_THING_MODEL3 )
        {
            height = pThing->renderData.data.pModel3->insertOffset.z;
        }

        float minHeight = pThing->collide.movesize + 0.005f;
        if ( height <= minHeight )
        {
            height = minHeight;
        }
    }

    return height;
}

// Updates physics include gravity effect on thing
void J3DAPI sithPhysics_UpdateThingPhysics(SithThing* pThing, float secDeltaTime)
{
    SithPhysicsFlags physFlags = pThing->moveInfo.physics.flags;
    memset(&pThing->moveInfo.physics.gravityForce, 0, sizeof(pThing->moveInfo.physics.gravityForce));
    // Note, original also cleared gravityForce a second time here via mislabelled pathMovement.rotOffset

    if ( physFlags & SITH_PF_RAFT )
    {
        // Decay the raft's front/back paddle-joint wobble angle back towards zero.
        int frontJoint = sithThing_GetThingJointIndex(pThing, "inraftfr");
        int backJoint  = sithThing_GetThingJointIndex(pThing, "inraftbk");
        rdVector3* aAngles = pThing->renderData.apTweakedAngles;
        aAngles[frontJoint].pitch -= stdMath_NormalizeAngleAcute(aAngles[frontJoint].pitch) * 0.5f;
        aAngles[backJoint].pitch  -= stdMath_NormalizeAngleAcute(aAngles[backJoint].pitch) * 0.5f;

        rdVector3 worldUp = { 0.0f, 0.0f, 1.0f };
        sithPhysics_SetThingLook(pThing, &worldUp, 0.2f * secDeltaTime);
    }

    if ( physFlags & SITH_PF_USEANGULARTHRUST )
    {
        rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;

        if ( pAngularVel->x != 0.0f || pAngularVel->y != 0.0f || pAngularVel->z != 0.0f )
            sithPhysics_ApplyDrag(pAngularVel, pThing->moveInfo.physics.airDrag + 0.2f, 0.0f, secDeltaTime);

        float maxRotVel = pThing->moveInfo.physics.maxRotationVelocity;
        float newX = pThing->moveInfo.physics.rotThrust.x * secDeltaTime + pAngularVel->x;
        float newY = pThing->moveInfo.physics.rotThrust.y * secDeltaTime + pAngularVel->y;
        float newZ = pThing->moveInfo.physics.rotThrust.z * secDeltaTime + pAngularVel->z;

        if ( newX < -maxRotVel ) newX = -maxRotVel; else if ( newX > maxRotVel ) newX = maxRotVel;
        if ( newY < -maxRotVel ) newY = -maxRotVel; else if ( newY > maxRotVel ) newY = maxRotVel;
        if ( newZ < -maxRotVel ) newZ = -maxRotVel; else if ( newZ > maxRotVel ) newZ = maxRotVel;

        if ( (newX < 0.0f ? -newX : newX) <= 1e-05f ) newX = 0.0f;
        if ( (newY < 0.0f ? -newY : newY) <= 1e-05f ) newY = 0.0f;
        if ( (newZ < 0.0f ? -newZ : newZ) <= 1e-05f ) newZ = 0.0f;

        pAngularVel->x = newX;
        pAngularVel->y = newY;
        pAngularVel->z = newZ;
    }

    rdVector3 deltaRot;
    rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;
    if ( pAngularVel->x == 0.0f && pAngularVel->y == 0.0f && pAngularVel->z == 0.0f )
    {
        deltaRot.x = 0.0f;
        deltaRot.y = 0.0f;
        deltaRot.z = 0.0f;
    }
    else
    {
        deltaRot.x = pAngularVel->x * secDeltaTime;
        deltaRot.y = pAngularVel->y * secDeltaTime;
        deltaRot.z = pAngularVel->z * secDeltaTime;
    }

    if ( deltaRot.x != 0.0f || deltaRot.y != 0.0f || deltaRot.z != 0.0f )
    {
        rdMatrix34 deltaRotMat;
        rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
        sithCollision_RotateThing(pThing, &deltaRotMat);

        // Fixed: Was rotating angular velocity; the original turns the velocity of the flying thing with it
        if ( physFlags & SITH_PF_FLY )
            rdMatrix_TransformVector34Acc(&pThing->moveInfo.physics.velocity, &deltaRotMat);

        if ( SITH_ISFRAMECYCLE(pThing->idx, 8) )
            rdMatrix_Normalize34(&pThing->orient);
    }

    if ( pThing->moveInfo.physics.airDrag != 0.0f )
        sithPhysics_ApplyDrag(&pThing->moveInfo.physics.velocity, pThing->moveInfo.physics.airDrag, 0.0f, secDeltaTime);

    rdVector3 deltaVel = { 0.0f, 0.0f, 0.0f };
    if ( physFlags & SITH_PF_USETHRUST )
    {
        if ( !(physFlags & SITH_PF_FLY) && pThing->moveStatus != SITHPLAYERMOVE_FALLING )
        {
            // Thrust bleeds off quickly unless actively flying or falling.
            pThing->moveInfo.physics.thrust.x *= 0.3f;
            pThing->moveInfo.physics.thrust.y *= 0.3f;
            pThing->moveInfo.physics.thrust.z *= 0.3f;
        }

        deltaVel.x = pThing->moveInfo.physics.thrust.x * secDeltaTime;
        deltaVel.y = pThing->moveInfo.physics.thrust.y * secDeltaTime;
        deltaVel.z = pThing->moveInfo.physics.thrust.z * secDeltaTime;
        rdMatrix_TransformVector34Acc(&deltaVel, &pThing->orient);
    }

    if ( pThing->moveInfo.physics.mass != 0.0f && (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) && !(physFlags & SITH_PF_NOTHRUST) )
    {
        deltaVel.x += pThing->pInSector->thrust.x * secDeltaTime;
        deltaVel.y += pThing->pInSector->thrust.y * secDeltaTime;
        deltaVel.z += pThing->pInSector->thrust.z * secDeltaTime;
    }

    if ( pThing->moveInfo.physics.mass != 0.0f && (physFlags & SITH_PF_USEGRAVITY) && !(pThing->pInSector->flags & SITH_SECTOR_NOGRAVITY) )
    {
        float gravity = sithWorld_g_pCurrentWorld->gravity * secDeltaTime;
        if ( physFlags & SITH_PF_PARTIALGRAVITY )
            gravity *= 0.5f;

        deltaVel.z -= gravity;
        pThing->moveInfo.physics.gravityForce.z = -gravity;
    }

    deltaVel.x += pThing->moveInfo.physics.velocity.x;
    deltaVel.y += pThing->moveInfo.physics.velocity.y;
    deltaVel.z += pThing->moveInfo.physics.velocity.z;

    if ( (deltaVel.x < 0.0f ? -deltaVel.x : deltaVel.x) <= 1e-05f ) deltaVel.x = 0.0f;
    if ( (deltaVel.y < 0.0f ? -deltaVel.y : deltaVel.y) <= 1e-05f ) deltaVel.y = 0.0f;
    if ( (deltaVel.z < 0.0f ? -deltaVel.z : deltaVel.z) <= 1e-05f ) deltaVel.z = 0.0f;

    pThing->moveInfo.physics.velocity = deltaVel;

    if ( deltaVel.x == 0.0f && deltaVel.y == 0.0f && deltaVel.z == 0.0f )
        return;

    pThing->moveInfo.physics.deltaVelocity.x = deltaVel.x * secDeltaTime;
    pThing->moveInfo.physics.deltaVelocity.y = deltaVel.y * secDeltaTime;
    pThing->moveInfo.physics.deltaVelocity.z = deltaVel.z * secDeltaTime;
}

// update player physics in the air or water/aeterium
// local variable allocation has failed, the output may be wrong!
void J3DAPI sithPhysics_UpdatePlayerPhysics(SithThing* pThing, float msDeltaTime)
{
    SithPhysicsFlags physFlags = pThing->moveInfo.physics.flags;

    memset(&pThing->moveInfo.physics.gravityForce, 0, sizeof(pThing->moveInfo.physics.gravityForce)); // Fixed: Was decompiled as pathMovement.rotOffset

    if ( physFlags & SITH_PF_USEANGULARTHRUST )
    {
        rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;

        if ( pAngularVel->x != 0.0f || pAngularVel->y != 0.0f || pAngularVel->z != 0.0f )
            sithPhysics_ApplyDrag(pAngularVel, pThing->moveInfo.physics.airDrag + 0.2f, 0.0f, msDeltaTime);

        float maxRotVel = pThing->moveInfo.physics.maxRotationVelocity;
        float newX = pThing->moveInfo.physics.rotThrust.x * msDeltaTime + pAngularVel->x;
        float newY = pThing->moveInfo.physics.rotThrust.y * msDeltaTime + pAngularVel->y;
        float newZ = pThing->moveInfo.physics.rotThrust.z * msDeltaTime + pAngularVel->z;

        if ( newX < -maxRotVel ) newX = -maxRotVel; else if ( newX > maxRotVel ) newX = maxRotVel;
        if ( newY < -maxRotVel ) newY = -maxRotVel; else if ( newY > maxRotVel ) newY = maxRotVel;
        if ( newZ < -maxRotVel ) newZ = -maxRotVel; else if ( newZ > maxRotVel ) newZ = maxRotVel;

        if ( (newX < 0.0f ? -newX : newX) <= 1e-05f ) newX = 0.0f;
        if ( (newY < 0.0f ? -newY : newY) <= 1e-05f ) newY = 0.0f;
        if ( (newZ < 0.0f ? -newZ : newZ) <= 1e-05f ) newZ = 0.0f;

        pAngularVel->x = newX;
        pAngularVel->y = newY;
        pAngularVel->z = newZ;
    }

    rdVector3 deltaRot;
    rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;
    if ( pAngularVel->x == 0.0f && pAngularVel->y == 0.0f && pAngularVel->z == 0.0f )
    {
        deltaRot.x = 0.0f;
        deltaRot.y = 0.0f;
        deltaRot.z = 0.0f;
    }
    else
    {
        deltaRot.x = pAngularVel->x * msDeltaTime;
        deltaRot.y = pAngularVel->y * msDeltaTime;
        deltaRot.z = pAngularVel->z * msDeltaTime;
    }

    if ( deltaRot.x != 0.0f || deltaRot.y != 0.0f || deltaRot.z != 0.0f )
    {
        rdMatrix34 deltaRotMat;
        rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
        sithCollision_RotateThing(pThing, &deltaRotMat);

        if ( physFlags & SITH_PF_FLY )
            rdMatrix_TransformVector34Acc(&pThing->moveInfo.physics.velocity, &deltaRotMat);

        if ( SITH_ISFRAMECYCLE(pThing->idx, 8) )
            rdMatrix_Normalize34(&pThing->orient);
    }

    // Thrust bleeds off quickly unless actively flying or falling (same decay as sithPhysics_UpdateThingPhysics).
    if ( !(physFlags & SITH_PF_FLY) && pThing->moveStatus != SITHPLAYERMOVE_FALLING )
    {
        pThing->moveInfo.physics.thrust.x *= 0.3f;
        pThing->moveInfo.physics.thrust.y *= 0.3f;
        pThing->moveInfo.physics.thrust.z *= 0.3f;
    }

    // Integrate velocity on a fixed 20ms (50Hz) tick, carrying any fractional tick across frames.
    float totalMs = msDeltaTime + pThing->moveInfo.physics.physicsRolloverFrames;
    int numSteps = (int)roundf(totalMs * 50.0f);
    pThing->moveInfo.physics.physicsRolloverFrames = totalMs - (float)numSteps * 0.02f;

    int bUseThrust = physFlags & SITH_PF_USETHRUST;
    for ( int i = 0; i < numSteps; i++ )
    {
        if ( pThing->moveInfo.physics.airDrag != 0.0f )
            sithPhysics_ApplyDrag(&pThing->moveInfo.physics.velocity, pThing->moveInfo.physics.airDrag, 0.0f, 0.02f);

        rdVector3 deltaVel = { 0.0f, 0.0f, 0.0f };
        if ( bUseThrust )
        {
            deltaVel.x = pThing->moveInfo.physics.thrust.x * 0.02f;
            deltaVel.y = pThing->moveInfo.physics.thrust.y * 0.02f;
            deltaVel.z = pThing->moveInfo.physics.thrust.z * 0.02f;
            rdMatrix_TransformVector34Acc(&deltaVel, &pThing->orient);
        }

        if ( pThing->moveInfo.physics.mass != 0.0f && (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) && !(physFlags & SITH_PF_NOTHRUST) )
        {
            deltaVel.x += pThing->pInSector->thrust.x * 0.02f;
            deltaVel.y += pThing->pInSector->thrust.y * 0.02f;
            deltaVel.z += pThing->pInSector->thrust.z * 0.02f;
        }

        if ( pThing->moveInfo.physics.mass != 0.0f && (physFlags & SITH_PF_USEGRAVITY) && !(pThing->pInSector->flags & SITH_SECTOR_NOGRAVITY) )
        {
            float gravity = sithWorld_g_pCurrentWorld->gravity * 0.02f;
            if ( physFlags & SITH_PF_PARTIALGRAVITY )
                gravity *= 0.5f;

            deltaVel.z -= gravity;
            pThing->moveInfo.physics.gravityForce.z = -gravity;
        }

        pThing->moveInfo.physics.velocity.x += deltaVel.x;
        pThing->moveInfo.physics.velocity.y += deltaVel.y;
        pThing->moveInfo.physics.velocity.z += deltaVel.z;
        pThing->moveInfo.physics.deltaVelocity.x += pThing->moveInfo.physics.velocity.x * 0.02f;
        pThing->moveInfo.physics.deltaVelocity.y += pThing->moveInfo.physics.velocity.y * 0.02f;
        pThing->moveInfo.physics.deltaVelocity.z += pThing->moveInfo.physics.velocity.z * 0.02f;
    }
}

void J3DAPI sithPhysics_UpdateUnderwaterThingPhysics(SithThing* pThing, float secDeltaTime)
{
    SithPhysicsFlags physFlags = pThing->moveInfo.physics.flags;

    memset(&pThing->moveInfo.physics.gravityForce, 0, sizeof(pThing->moveInfo.physics.gravityForce)); // Fixed: Was decompiled as pathMovement.rotOffset

    if ( physFlags & SITH_PF_USEANGULARTHRUST )
    {
        rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;

        if ( pAngularVel->x != 0.0f || pAngularVel->y != 0.0f || pAngularVel->z != 0.0f )
            sithPhysics_ApplyDrag(pAngularVel, pThing->moveInfo.physics.airDrag + 0.2f, 0.0f, secDeltaTime);

        float maxRotVel = pThing->moveInfo.physics.maxRotationVelocity;
        float newX = pThing->moveInfo.physics.rotThrust.x * secDeltaTime + pAngularVel->x;
        float newY = pThing->moveInfo.physics.rotThrust.y * secDeltaTime + pAngularVel->y;
        float newZ = pThing->moveInfo.physics.rotThrust.z * secDeltaTime + pAngularVel->z;

        if ( newX < -maxRotVel ) newX = -maxRotVel; else if ( newX > maxRotVel ) newX = maxRotVel;
        if ( newY < -maxRotVel ) newY = -maxRotVel; else if ( newY > maxRotVel ) newY = maxRotVel;
        if ( newZ < -maxRotVel ) newZ = -maxRotVel; else if ( newZ > maxRotVel ) newZ = maxRotVel;

        if ( (newX < 0.0f ? -newX : newX) <= 1e-05f ) newX = 0.0f;
        if ( (newY < 0.0f ? -newY : newY) <= 1e-05f ) newY = 0.0f;
        if ( (newZ < 0.0f ? -newZ : newZ) <= 1e-05f ) newZ = 0.0f;

        pAngularVel->x = newX;
        pAngularVel->y = newY;
        pAngularVel->z = newZ;
    }

    rdVector3 deltaRot;
    rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;
    if ( pAngularVel->x == 0.0f && pAngularVel->y == 0.0f && pAngularVel->z == 0.0f )
    {
        deltaRot.x = 0.0f;
        deltaRot.y = 0.0f;
        deltaRot.z = 0.0f;
    }
    else
    {
        deltaRot.x = pAngularVel->x * secDeltaTime;
        deltaRot.y = pAngularVel->y * secDeltaTime;
        deltaRot.z = pAngularVel->z * secDeltaTime;
    }

    if ( deltaRot.x != 0.0f || deltaRot.y != 0.0f || deltaRot.z != 0.0f )
    {
        rdMatrix34 deltaRotMat;
        rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
        sithCollision_RotateThing(pThing, &deltaRotMat);

        if ( SITH_ISFRAMECYCLE(pThing->idx, 8) )
            rdMatrix_Normalize34(&pThing->orient);
    }

    // Air drag is 4x stronger underwater.
    if ( pThing->moveInfo.physics.airDrag != 0.0f )
        sithPhysics_ApplyDrag(&pThing->moveInfo.physics.velocity, pThing->moveInfo.physics.airDrag * 4.0f, 0.0f, secDeltaTime);

    rdVector3 deltaVel = { 0.0f, 0.0f, 0.0f };
    if ( physFlags & SITH_PF_USETHRUST )
    {
        // Thrust decays towards zero (at 0.6x per frame) while underwater.
        pThing->moveInfo.physics.thrust.x *= 0.6f;
        pThing->moveInfo.physics.thrust.y *= 0.6f;
        pThing->moveInfo.physics.thrust.z *= 0.6f;

        deltaVel.x = pThing->moveInfo.physics.thrust.x * secDeltaTime;
        deltaVel.y = pThing->moveInfo.physics.thrust.y * secDeltaTime;
        deltaVel.z = pThing->moveInfo.physics.thrust.z * secDeltaTime;
        rdMatrix_TransformVector34Acc(&deltaVel, &pThing->orient);
    }

    if ( pThing->moveInfo.physics.mass != 0.0f && (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) && !(physFlags & SITH_PF_NOTHRUST) )
    {
        deltaVel.x += pThing->pInSector->thrust.x * secDeltaTime;
        deltaVel.y += pThing->pInSector->thrust.y * secDeltaTime;
        deltaVel.z += pThing->pInSector->thrust.z * secDeltaTime;
    }

    // Gravity reduced by buoyancy while submerged.
    // Fixed: The condition was garbled (always true), so gravity also pulled a thing floating on the water surface,
    //        sinking e.g. the player after falling into water. The original applies no gravity on the water surface
    //        unless the thing is dying.
    if ( ((physFlags & SITH_PF_ONWATERSURFACE) == 0 || (pThing->flags & SITH_TF_DYING) != 0) && (physFlags & SITH_PF_USEGRAVITY) != 0 )
    {
        float gravity = sithWorld_g_pCurrentWorld->gravity * secDeltaTime * pThing->moveInfo.physics.buoyancy;
        deltaVel.z -= gravity;
        pThing->moveInfo.physics.gravityForce.z -= gravity;
    }

    deltaVel.x += pThing->moveInfo.physics.velocity.x;
    deltaVel.y += pThing->moveInfo.physics.velocity.y;
    deltaVel.z += pThing->moveInfo.physics.velocity.z;

    if ( (deltaVel.x < 0.0f ? -deltaVel.x : deltaVel.x) <= 1e-05f ) deltaVel.x = 0.0f;
    if ( (deltaVel.y < 0.0f ? -deltaVel.y : deltaVel.y) <= 1e-05f ) deltaVel.y = 0.0f;
    if ( (deltaVel.z < 0.0f ? -deltaVel.z : deltaVel.z) <= 1e-05f ) deltaVel.z = 0.0f;

    pThing->moveInfo.physics.velocity = deltaVel;

    if ( deltaVel.x != 0.0f || deltaVel.y != 0.0f || deltaVel.z != 0.0f )
    {
        pThing->moveInfo.physics.deltaVelocity.x = deltaVel.x * secDeltaTime;
        pThing->moveInfo.physics.deltaVelocity.y = deltaVel.y * secDeltaTime;
        pThing->moveInfo.physics.deltaVelocity.z = deltaVel.z * secDeltaTime;
    }

    // If on the water surface and still rising, ease towards the resting buoyancy depth instead of
    // overshooting above the surface.
    if ( (physFlags & SITH_PF_ONWATERSURFACE) && pThing->moveInfo.physics.thrust.z >= 0.0f )
    {
        float maxRise = secDeltaTime * 0.01f;
        float remainingRise = pThing->attach.someAttachDistance - 0.051f;

        if ( pThing->moveInfo.physics.deltaVelocity.z > 0.0f && pThing->moveInfo.physics.deltaVelocity.z < maxRise )
            pThing->moveInfo.physics.deltaVelocity.z = 0.0f;

        if ( remainingRise != 0.0f )
        {
            if ( remainingRise > maxRise )
                remainingRise = maxRise;

            rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.deltaVelocity, &rdroid_g_zVector3, remainingRise);
            sithPuppet_StopMode(pThing, (SithPuppetSubMode)0x44, 0.0f); // TODO: 0x44 is named SITHPUPPETSUBMODE_FIRE4, likely misnamed
        }
    }
}

void J3DAPI sithPhysics_UpdateClimbingThingPhysics(SithThing* pThing, float secDeltaTime)
{
    if ( pThing->moveInfo.physics.thrust.x == 0.0f && pThing->moveInfo.physics.thrust.y == 0.0f && pThing->moveInfo.physics.thrust.z == 0.0f )
        return;

    rdVector3 deltaVel;
    float scale = secDeltaTime * 0.1f;
    deltaVel.x = pThing->moveInfo.physics.thrust.x * scale;
    deltaVel.y = pThing->moveInfo.physics.thrust.y * scale;
    deltaVel.z = pThing->moveInfo.physics.thrust.z * scale;

    if ( (deltaVel.x < 0.0f ? -deltaVel.x : deltaVel.x) <= 1e-05f ) deltaVel.x = 0.0f;
    if ( (deltaVel.y < 0.0f ? -deltaVel.y : deltaVel.y) <= 1e-05f ) deltaVel.y = 0.0f;
    if ( (deltaVel.z < 0.0f ? -deltaVel.z : deltaVel.z) <= 1e-05f ) deltaVel.z = 0.0f;

    if ( deltaVel.x != 0.0f || deltaVel.y != 0.0f || deltaVel.z != 0.0f )
        rdMatrix_TransformVector34Acc(&deltaVel, &pThing->orient);

    if ( pThing->moveInfo.physics.mass != 0.0f && (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) && !(pThing->moveInfo.physics.flags & SITH_PF_NOTHRUST) )
    {
        deltaVel.x += pThing->pInSector->thrust.x * secDeltaTime;
        deltaVel.y += pThing->pInSector->thrust.y * secDeltaTime;
        deltaVel.z += pThing->pInSector->thrust.z * secDeltaTime;
    }

    pThing->moveInfo.physics.velocity = deltaVel;
    pThing->moveInfo.physics.deltaVelocity.x = deltaVel.x * secDeltaTime;
    pThing->moveInfo.physics.deltaVelocity.y = deltaVel.y * secDeltaTime;
    pThing->moveInfo.physics.deltaVelocity.z = deltaVel.z * secDeltaTime;
}

static bool sithPhysics_IsZeroVector(const rdVector3* pVec)
{
    return pVec->x == 0.0f && pVec->y == 0.0f && pVec->z == 0.0f;
}

// Clips each component of the vector to 0 when it is within 1e-5 of 0
static void sithPhysics_ClipVectorNearZero(rdVector3* pVec)
{
    pVec->x = stdMath_ClipNearZero(pVec->x);
    pVec->y = stdMath_ClipNearZero(pVec->y);
    pVec->z = stdMath_ClipNearZero(pVec->z);
}

static void sithPhysics_ClampVector(rdVector3* pVec, float minVal, float maxVal)
{
    pVec->x = STDMATH_CLAMP(pVec->x, minVal, maxVal);
    pVec->y = STDMATH_CLAMP(pVec->y, minVal, maxVal);
    pVec->z = STDMATH_CLAMP(pVec->z, minVal, maxVal);
}

// Updates physics of thing attached to surface or to the face of another thing (e.g.: walking on the floor).
// Handles surface alignment, slope sliding, turning, surface drag, thrust and keeping the thing at its height above the floor.
void J3DAPI sithPhysics_UpdateAttachedThingPhysics(SithThing* pThing, float secDeltaTime)
{
    rdVector3 deltaVel = { 0 };
    pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_200000;

    SITH_ASSERTREL(pThing->attach.flags);

    // Get attached surface normal and the distance of thing from attached surface
    // Added: Init to default values; originally uninitialized if none of the flags below is set
    //        which doesn't happen as the function is called only for things attached to surface or thing face.
    rdVector3 normal  = rdroid_g_zVector3;
    float distance    = 0.0f;
    float slopeFactor = 1.0f;

    SithAttachFlag attachFlags = pThing->attach.flags;
    if ( (attachFlags & SITH_ATTACH_SURFACE) != 0 || (attachFlags & (SITH_ATTACH_THINGFACE | SITH_ATTACH_CLIMBSURFACE)) == SITH_ATTACH_CLIMBSURFACE )
    {
        normal = pThing->attach.pFace->normal;

        rdVector3 dir;
        rdVector_Sub3(&dir, &pThing->pos, &pThing->attach.attachedFaceFirstVert);
        distance = rdVector_Dot3(&dir, &normal);
    }
    else if ( (attachFlags & SITH_ATTACH_THINGFACE) != 0 )
    {
        SithThing* pAttached = pThing->attach.attachedToStructure.pThingAttached;
        rdMatrix_TransformVector34(&normal, &pThing->attach.pFace->normal, &pAttached->orient);

        rdVector3 vert;
        rdMatrix_TransformVector34(&vert, &pThing->attach.attachedFaceFirstVert, &pAttached->orient);
        rdVector_Add3Acc(&vert, &pAttached->pos);

        rdVector3 dir;
        rdVector_Sub3(&dir, &pThing->pos, &vert);
        distance = rdVector_Dot3(&dir, &normal);
    }

    float floorSlopeDot = rdVector_Dot3(&normal, &rdroid_g_zVector3);
    if ( (pThing->moveInfo.physics.flags & SITH_PF_ALIGNUP) != 0 && floorSlopeDot < 1.0f )
    {
        SITH_ASSERTREL(floorSlopeDot > 0.0f);
        distance = distance / floorSlopeDot;
    }

    // Align thing to the surface
    SithPhysicsFlags physFlags = pThing->moveInfo.physics.flags;
    if ( (physFlags & SITH_PF_ALIGNED) == 0 )
    {
        if ( (physFlags & SITH_PF_ALIGNSURFACE) != 0 )
        {
            sithPhysics_SetThingLook(pThing, &normal, pThing->moveInfo.physics.orientSpeed * secDeltaTime);
        }
        else if ( (physFlags & SITH_PF_ALIGNUP) != 0 )
        {
            sithPhysics_SetThingLook(pThing, &rdroid_g_zVector3, pThing->moveInfo.physics.orientSpeed * secDeltaTime);
        }
        else
        {
            pThing->moveInfo.physics.flags = physFlags | SITH_PF_ALIGNED;
        }
    }

    // Slide down the slope that is too steep
    if ( pThing->moveStatus != SITHPLAYERMOVE_LEAPFWD
        && floorSlopeDot < 0.8f && floorSlopeDot > 0.69f
        && (pThing->type == SITH_THING_PLAYER || (pThing->thingInfo.actorInfo.flags & SITH_AF_NOSLOPEMOVE) != 0) )
    {
        rdVector3 lookDir = { 0 };
        lookDir.x = pThing->orient.lvec.x;
        lookDir.y = pThing->orient.lvec.y;
        rdVector_Normalize3Acc(&lookDir);

        rdVector3 slideDir = { 0 };
        slideDir.x = normal.x;
        slideDir.y = normal.y;
        rdVector_Normalize3Acc(&slideDir);

        if ( rdVector_Dot3(&slideDir, &lookDir) < 0.0f )
        {
            rdVector_Neg3Acc(&slideDir);
            pThing->moveStatus = SITHPLAYERMOVE_SLIDEDOWNBACK;
        }
        else
        {
            pThing->moveStatus = SITHPLAYERMOVE_SLIDEDOWNFORWARD;
        }

        // Face the thing along the slope
        pThing->orient.lvec = slideDir;
        rdVector_Cross3(&pThing->orient.rvec, &pThing->orient.lvec, &rdroid_g_zVector3);
        rdVector_Normalize3Acc(&pThing->orient.rvec);
        pThing->orient.uvec = rdroid_g_zVector3;

        rdVector3 slideVel;
        slideVel.x = normal.x;
        slideVel.y = normal.y;
        slideVel.z = normal.z * -1.0f;
        rdVector_Normalize3Acc(&slideVel);
        pThing->moveInfo.physics.velocity = slideVel;

        float dragFactor = STDMATH_CLAMP(slopeFactor, 0.25f, 1.0f);
        sithPhysics_ApplyDrag(&pThing->moveInfo.physics.velocity, pThing->moveInfo.physics.surfDrag * dragFactor, 0.0f, secDeltaTime);

        rdVector_Scale3Acc(&pThing->moveInfo.physics.velocity, 0.5f);
        sithPhysics_ClipVectorNearZero(&pThing->moveInfo.physics.velocity);
        if ( sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) )
        {
            return;
        }

        rdVector_Scale3(&pThing->moveInfo.physics.deltaVelocity, &pThing->moveInfo.physics.velocity, secDeltaTime);
        return;
    }

    // Stop sliding when slope isn't steep anymore
    if ( (pThing->moveStatus == SITHPLAYERMOVE_SLIDEDOWNFORWARD || pThing->moveStatus == SITHPLAYERMOVE_SLIDEDOWNBACK)
        && (sithPhysics_ResetThingMovement(pThing), pThing == sithPlayer_g_pLocalPlayerThing || (pThing->thingInfo.actorInfo.flags & SITH_AF_NOSLOPEMOVE) != 0)
        && (pThing->moveInfo.physics.flags & (SITH_PF_MINECAR | SITH_PF_RAFT | SITH_PF_JEEP | SITH_PF_UNKNOWN_8000000)) == 0 )
    {
        SithSector* pSector = pThing->pInSector;
        if ( (!pSector || (pSector->flags & SITH_SECTOR_UNDERWATER) == 0) && (pSector->flags & SITH_SECTOR_AETHERIUM) == 0 )
        {
            pThing->moveStatus = SITHPLAYERMOVE_STILL;
        }
        else
        {
            pThing->moveStatus = SITHPLAYERMOVE_SWIMIDLE;
            if ( pThing->pPuppetState->moveMode != SITHPUPPET_MOVEMODE_SWIM )
            {
                sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_SWIM);
            }
        }
    }

    // Update turning
    rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;
    if ( (pThing->moveInfo.physics.flags & SITH_PF_USEANGULARTHRUST) != 0 )
    {
        if ( !sithPhysics_IsZeroVector(pAngularVel) )
        {
            sithPhysics_ApplyDrag(pAngularVel, pThing->moveInfo.physics.surfDrag + 0.2f, 0.0f, secDeltaTime);
        }

        // Note, only yaw is affected by rotation thrust on surface
        pAngularVel->y = pThing->moveInfo.physics.rotThrust.y * secDeltaTime + pAngularVel->y;

        float maxRotVel = pThing->moveInfo.physics.maxRotationVelocity;
        sithPhysics_ClampVector(pAngularVel, -maxRotVel, maxRotVel);
        sithPhysics_ClipVectorNearZero(pAngularVel);
    }

    if ( pAngularVel->y != 0.0f )
    {
        rdVector3 deltaRot;
        rdVector_Scale3(&deltaRot, pAngularVel, secDeltaTime);

        rdMatrix34 deltaRotMat;
        rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
        sithCollision_RotateThing(pThing, &deltaRotMat);

        if ( slopeFactor >= 1.0f )
        {
            rdMatrix_TransformVector34Acc(&pThing->moveInfo.physics.velocity, &deltaRotMat);
        }
        else
        {
            // Turn only part of velocity
            float keepFactor = 1.0f - slopeFactor;
            rdVector3 turnedVel;
            rdMatrix_TransformVector34(&turnedVel, &pThing->moveInfo.physics.velocity, &deltaRotMat);
            pThing->moveInfo.physics.velocity.x = turnedVel.x * slopeFactor + pThing->moveInfo.physics.velocity.x * keepFactor;
            pThing->moveInfo.physics.velocity.y = turnedVel.y * slopeFactor + pThing->moveInfo.physics.velocity.y * keepFactor;
            pThing->moveInfo.physics.velocity.z = turnedVel.z * slopeFactor + pThing->moveInfo.physics.velocity.z * keepFactor;
        }

        rdMatrix_Normalize34(&pThing->orient);
    }

    slopeFactor = STDMATH_CLAMP(slopeFactor, 0.25f, 1.0f);

    // Apply surface drag
    if ( !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) && pThing->moveInfo.physics.surfDrag != 0.0f )
    {
        if ( (pThing->moveInfo.physics.flags & SITH_PF_UNKNOWN_8000) == 0 )
        {
            float staticDrag = 0.0f;
            if ( sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust)
                && (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) == 0
                && slopeFactor > 0.8f )
            {
                staticDrag = pThing->moveInfo.physics.staticDrag * slopeFactor;
            }

            sithPhysics_ApplyDrag(&pThing->moveInfo.physics.velocity, pThing->moveInfo.physics.surfDrag * slopeFactor, staticDrag, secDeltaTime);
        }
        else
        {
            pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_8000;
        }
    }

    // Apply thrust
    if ( (pThing->moveInfo.physics.flags & SITH_PF_USETHRUST) != 0 && !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust) )
    {
        float thrustFactor = slopeFactor;
        if ( (pThing->moveInfo.physics.flags & SITH_PF_CROUCHING) != 0 )
        {
            thrustFactor = 0.8f;
        }

        rdVector_Scale3(&deltaVel, &pThing->moveInfo.physics.thrust, thrustFactor * secDeltaTime);
        sithPhysics_ClipVectorNearZero(&deltaVel);
        if ( !sithPhysics_IsZeroVector(&deltaVel) )
        {
            rdMatrix_TransformVector34Acc(&deltaVel, &pThing->orient);
        }
    }

    // Apply sector thrust
    if ( pThing->moveInfo.physics.mass != 0.0f
        && (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) != 0
        && (pThing->moveInfo.physics.flags & SITH_PF_NOTHRUST) == 0 )
    {
        if ( sithWorld_g_pCurrentWorld->gravity * pThing->moveInfo.physics.mass < pThing->pInSector->thrust.z )
        {
            // Sector thrust is stronger than gravity, lift the thing off the surface and update it as unattached thing
            sithThing_DetachThing(pThing);
            sithPhysics_UpdateThingPhysics(pThing, secDeltaTime); // Note, the original inlines the same code here
            return;
        }

        rdVector_ScaleAdd3Acc(&deltaVel, &pThing->pInSector->thrust, secDeltaTime);
    }

    rdVector_Add3Acc(&pThing->moveInfo.physics.velocity, &deltaVel);

    // Apply slope gravity to player
    if ( pThing->type == SITH_THING_PLAYER
        && (pThing->moveInfo.physics.flags & SITH_PF_USEGRAVITY) != 0
        && floorSlopeDot < 1.0f
        && (slopeFactor < 0.8f || !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity)) )
    {
        float gravityFactor = STDMATH_CLAMP(1.0f - slopeFactor, 0.2f, 0.8f);
        pThing->moveInfo.physics.velocity.z = pThing->moveInfo.physics.velocity.z - sithWorld_g_pCurrentWorld->gravity * secDeltaTime * gravityFactor;
    }

    // Project velocity on the surface plane when in water or aetherium
    if ( !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        float normalVel = stdMath_ClipNearZero(rdVector_Dot3(&normal, &pThing->moveInfo.physics.velocity));
        SithSector* pSector = pThing->pInSector;
        if ( ((pSector && (pSector->flags & SITH_SECTOR_UNDERWATER) != 0) || (pSector->flags & SITH_SECTOR_AETHERIUM) != 0) && normalVel != 0.0f )
        {
            rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.velocity, &normal, -normalVel);
        }
    }

    sithPhysics_ClipVectorNearZero(&pThing->moveInfo.physics.velocity);
    if ( !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        rdVector_Scale3(&pThing->moveInfo.physics.deltaVelocity, &pThing->moveInfo.physics.velocity, secDeltaTime);
    }

    // Keep thing at its height above the surface
    float heightDelta = stdMath_ClipNearZero(distance - sithPhysics_GetThingHeight(pThing));
    if ( heightDelta == 0.0f )
    {
        return;
    }

    float maxDelta = secDeltaTime * 1.5f;
    if ( pThing->type == SITH_THING_PLAYER && fabsf(heightDelta) >= 2.0f )
    {
        maxDelta = secDeltaTime * 0.5f;
    }

    heightDelta = STDMATH_CLAMP(heightDelta, -maxDelta, maxDelta);
    if ( (pThing->moveInfo.physics.flags & SITH_PF_ALIGNUP) != 0 )
    {
        rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.deltaVelocity, &rdroid_g_zVector3, -heightDelta);
    }
    else
    {
        rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.deltaVelocity, &normal, -heightDelta);
    }
}

int J3DAPI sithPhysics_CreateMineCarUserBlock(SithThing* pMineCar)
{
    if ( pMineCar->userblock.pMinecar )
        return 1;

    SithMineCarUserBlock* pBlock = (SithMineCarUserBlock*)STDMALLOC(sizeof(SithMineCarUserBlock));
    pMineCar->userblock.pMinecar = pBlock;
    if ( !pBlock )
    {
        SITHLOG_ERROR("CreateMineCarUserBlock failed to allocate.\n");
        return 0;
    }

    memset(pBlock, 0, sizeof(SithMineCarUserBlock));

    if ( strcmpi(pMineCar->aName, "mineplayer") == 0 )
    {
        pBlock->type = SITHMINECAR_PLAYER;
        sithPhysics_InitMineCarExhaust(pMineCar, &pBlock->exhaustInfo);
        sithPhysics_InitMineCarChassis(pMineCar, &pBlock->chassisInfo);
        sithPhysics_InitMineCarFxState(pMineCar, &pBlock->fxstate);
        sithPhysics_InitMineCarState(pMineCar, &pBlock->state);
        pBlock->state.bEngineAnim = 1;
        return 1;
    }

    if ( strcmpi(pMineCar->aName, "killtruk") == 0 )
    {
        pBlock->type = SITHMINECAR_KILLTRUCK_AI;
        sithPhysics_InitTrackTruckExhaust(pMineCar, &pBlock->exhaustInfo);
        sithPhysics_InitTrackTruckChassis(pMineCar, &pBlock->chassisInfo);
        sithPhysics_InitTrackTruckState(pMineCar, &pBlock->state);
    }
    else if ( strcmpi(pMineCar->aName, "commietruck") == 0 )
    {
        pBlock->type = SITHMINECAR_TRUCK_AI;
        sithPhysics_InitTrackTruckExhaust(pMineCar, &pBlock->exhaustInfo);
        sithPhysics_InitTrackTruckChassis(pMineCar, &pBlock->chassisInfo);
        sithPhysics_InitTrackTruckState(pMineCar, &pBlock->state);
        sithPhysics_InitVehicleFxState(pMineCar, &pBlock->fxstate.engine);
        return 1;
    }
    else if ( strcmpi(pMineCar->aName, "jeep_cs") != 0 && strcmpi(pMineCar->aName, "jeep_cs_pyr") != 0 )
    {
        if ( strcmpi(pMineCar->aName, "mine_2commcar") == 0 )
        {
            pBlock->type = SITHMINECAR_DEFAULT;
            sithPhysics_InitMineCarExhaust(pMineCar, &pBlock->exhaustInfo);
            sithPhysics_InitMineCarChassis(pMineCar, &pBlock->chassisInfo);
            sithPhysics_InitMineCarFxState(pMineCar, &pBlock->fxstate);
        }
        else
        {
            pBlock->type = (strcmpi(pMineCar->aName, "boulder_mc") == 0) ? SITHMINECAR_BOULDER : SITHMINECAR_MINECAR_AI;
            // Note, the original "clears" both exhaust and chassis info with the same function
            sithPhysics_InitDefaultMineCarInfo(pMineCar, &pBlock->exhaustInfo);
            sithPhysics_InitDefaultMineCarInfo(pMineCar, &pBlock->chassisInfo);
        }
        sithPhysics_InitMineCarState(pMineCar, &pBlock->state);
        return 1;
    }
    else
    {
        pBlock->type = SITHMINECAR_JEEP_AI;
        sithPhysics_InitJeepExhaust(pMineCar, &pBlock->exhaustInfo);
        sithPhysics_InitTrackJeepChassis(pMineCar, &pBlock->chassisInfo);
        sithPhysics_InitMineCarState(pMineCar, &pBlock->state);
    }

    sithPhysics_InitVehicleFxState(pMineCar, &pBlock->fxstate.engine);
    return 1;
}

void J3DAPI sithPhysics_InitMineCarFxState(SithThing* pThing, SithMineCarFxState* pFxState)
{
    pFxState->engine.secUpdateInterval = 0.2f;
    pFxState->engine.secUpdateTimer = 0.0f;
    pFxState->railClackDistance = 2.0f;
    pFxState->railClackState = 2.0f;
}

void J3DAPI sithPhysics_InitVehicleFxState(SithThing* pThing, SithVehicleEngineFxState* pFxState)
{
    pFxState->secUpdateInterval = 0.05f;
    pFxState->secUpdateTimer = 0.0f;
}

// Clears the info block (exhaust or chassis) of plain AI minecars and boulders, i.e.: no exhaust and no wheels.
void J3DAPI sithPhysics_InitDefaultMineCarInfo(SithThing* pThing, void* pInfo)
{
    J3D_UNUSED(pThing);
    *(int*)pInfo = 0; // bEnabled / numNodes
}


void J3DAPI sithPhysics_InitMineCarChassis(SithThing* pThing, SithVehicleChassisInfo* pChassisInfo)
{
    pChassisInfo->numNodes = 2;
    strncpy(pChassisInfo->aWheelMeshNames[0], "bkwheel", 0x3f);
    strncpy(pChassisInfo->aWheelMeshNames[1], "frwheel", 0x3f);
    pChassisInfo->aWheelNodeNums[0] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[0]);
    pChassisInfo->aWheelNodeNums[1] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[1]);
    pChassisInfo->wheelRadius = 0.015f;
}

void J3DAPI sithPhysics_InitTrackTruckChassis(SithThing* pThing, SithVehicleChassisInfo* pChassisInfo)
{
    pChassisInfo->numNodes = 2;
    strncpy(pChassisInfo->aWheelMeshNames[0], "bkwheel", 0x3f);
    strncpy(pChassisInfo->aWheelMeshNames[1], "frwheel", 0x3f);
    pChassisInfo->aWheelNodeNums[0] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[0]);
    pChassisInfo->aWheelNodeNums[1] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[1]);
    pChassisInfo->wheelRadius = 0.0375f;
}

void J3DAPI sithPhysics_InitTrackJeepChassis(SithThing* pThing, SithVehicleChassisInfo* pChassisInfo)
{
    pChassisInfo->numNodes = 4;
    strncpy(pChassisInfo->aWheelMeshNames[0], "brwheel", 0x3f);
    strncpy(pChassisInfo->aWheelMeshNames[1], "flwheel", 0x3f);
    strncpy(pChassisInfo->aWheelMeshNames[2], "blwheel", 0x3f);
    strncpy(pChassisInfo->aWheelMeshNames[3], "frwheel", 0x3f);
    pChassisInfo->aWheelNodeNums[0] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[0]);
    pChassisInfo->aWheelNodeNums[1] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[1]);
    pChassisInfo->aWheelNodeNums[2] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[2]);
    pChassisInfo->aWheelNodeNums[3] = sithThing_GetThingJointIndex(pThing, pChassisInfo->aWheelMeshNames[3]);
    pChassisInfo->wheelRadius = 0.0375f;
}

// Fixed: Values are now set to the same fields as in the original (several were assigned to wrong fields),
//        and the bodies of this function and sithPhysics_InitDefaultMineCarInfo were swapped.
void J3DAPI sithPhysics_InitMineCarExhaust(SithThing* pThing, SithVehicleExhaustInfo* pExhaustInfo)
{
    J3D_UNUSED(pThing);
    pExhaustInfo->bEnabled       = 1;
    pExhaustInfo->moveLength     = 0.008f;
    pExhaustInfo->offsetY        = -0.09f;
    pExhaustInfo->offsetZ        = -0.034f;
    pExhaustInfo->offsetX        = -0.02f;
    pExhaustInfo->velocity       = 0.15f;
    pExhaustInfo->velScale       = 0.2f;
    pExhaustInfo->smokeLifeScale = 0.2f;
    pExhaustInfo->startMinSize   = 0.01f;
    pExhaustInfo->startMaxSize   = 0.1f;
    pExhaustInfo->startSizeScale = 0.01f;
    pExhaustInfo->endMinSize     = 0.15f;
    pExhaustInfo->endMaxSize     = 0.0f;
    pExhaustInfo->endSizeScale   = 0.01f;
    pExhaustInfo->dirX           = -0.1f;
    pExhaustInfo->scatterX       = 0.1f;
    pExhaustInfo->dirZ           = 0.0f;
    pExhaustInfo->scatterZ       = 0.1f;
    strncpy(pExhaustInfo->aExhaustTemplate, "+exhaust", STD_ARRAYLEN(pExhaustInfo->aExhaustTemplate) - 1);
    pExhaustInfo->aExhaustTemplate[STD_ARRAYLEN(pExhaustInfo->aExhaustTemplate) - 1] = '\0';
}


// Fixed: The original disables jeep exhaust (bEnabled = 0)
void J3DAPI sithPhysics_InitJeepExhaust(SithThing* pThing, SithVehicleExhaustInfo* pExhaustInfo)
{
    J3D_UNUSED(pThing);
    pExhaustInfo->bEnabled       = 0;
    pExhaustInfo->moveLength     = 0.008f;
    pExhaustInfo->offsetY        = -0.16f;
    pExhaustInfo->offsetZ        = -0.034f;
    pExhaustInfo->offsetX        = 0.02f;
    pExhaustInfo->velocity       = 0.05f;
    pExhaustInfo->velScale       = 0.02f;
    pExhaustInfo->smokeLifeScale = 0.2f;
    pExhaustInfo->startMinSize   = 0.01f;
    pExhaustInfo->startMaxSize   = 0.1f;
    pExhaustInfo->startSizeScale = 0.01f;
    pExhaustInfo->endMinSize     = 0.15f;
    pExhaustInfo->endMaxSize     = 0.0f;
    pExhaustInfo->endSizeScale   = 0.01f;
    pExhaustInfo->dirX           = 0.0f;
    pExhaustInfo->scatterX       = 0.1f;
    pExhaustInfo->dirZ           = 0.0f;
    pExhaustInfo->scatterZ       = 0.1f;
    strncpy(pExhaustInfo->aExhaustTemplate, "+exhaust", STD_ARRAYLEN(pExhaustInfo->aExhaustTemplate) - 1);
    pExhaustInfo->aExhaustTemplate[STD_ARRAYLEN(pExhaustInfo->aExhaustTemplate) - 1] = '\0';
}

void J3DAPI sithPhysics_InitTrackTruckExhaust(SithThing* pThing, SithVehicleExhaustInfo* pExhaustInfo)
{
    J3D_UNUSED(pThing);
    pExhaustInfo->bEnabled       = 1;
    pExhaustInfo->moveLength     = 0.008f;
    pExhaustInfo->offsetY        = -0.3f;
    pExhaustInfo->offsetZ        = -0.1f;
    pExhaustInfo->offsetX        = 0.06f;
    pExhaustInfo->velocity       = 0.1f;
    pExhaustInfo->velScale       = 0.2f;
    pExhaustInfo->smokeLifeScale = 0.2f;
    pExhaustInfo->startMinSize   = 0.01f;
    pExhaustInfo->startMaxSize   = 0.1f;
    pExhaustInfo->startSizeScale = 0.01f;
    pExhaustInfo->endMinSize     = 0.15f;
    pExhaustInfo->endMaxSize     = 0.0f;
    pExhaustInfo->endSizeScale   = 0.01f;
    pExhaustInfo->dirX           = 0.0f;
    pExhaustInfo->scatterX       = 0.1f;
    pExhaustInfo->dirZ           = 0.0f;
    pExhaustInfo->scatterZ       = 0.1f;
    strncpy(pExhaustInfo->aExhaustTemplate, "+exhaust", STD_ARRAYLEN(pExhaustInfo->aExhaustTemplate) - 1);
    pExhaustInfo->aExhaustTemplate[STD_ARRAYLEN(pExhaustInfo->aExhaustTemplate) - 1] = '\0';
}

void J3DAPI sithPhysics_InitMineCarState(SithThing* pThing, SithMineCarState* pState)
{
    pState->turnState = SITHMINECAR_TURN_NONE;
    pState->bOnAdjoinTrack = 0;
    pState->surfDrag = pThing->moveInfo.physics.surfDrag + 1.0f;
    pState->pEngineAnim = NULL;
    pState->bEngineAnim = 0;
    pState->bUpdateLeftSparks = 0;
    pState->bUpdateRightSparks = 0;
    pState->secTimeLeftSparks = 0.0f;
    pState->secTimeRightSparks = 0.0f;
    pState->longFaceBlendLength = 0.2f;
    pState->maxBlendFaceLength = 0.3f;
    pState->bWheelsLocked = 0;
}

void J3DAPI sithPhysics_InitTrackTruckState(SithThing* pThing, SithMineCarState* pState)
{
    pState->turnState = SITHMINECAR_TURN_NONE;
    pState->bOnAdjoinTrack = 0;
    pState->surfDrag = pThing->moveInfo.physics.surfDrag + 1.0f;
    pState->pEngineAnim = NULL;
    pState->bEngineAnim = 0;
    pState->bUpdateLeftSparks = 0;
    pState->bUpdateRightSparks = 0;
    pState->secTimeLeftSparks = 0.0f;
    pState->secTimeRightSparks = 0.0f;
    pState->longFaceBlendLength = 0.6f;
    pState->maxBlendFaceLength = 0.8f;
    pState->bWheelsLocked = 0;
}

// Returns random number in range [0, 1]
static float sithPhysics_Random(void)
{
    return (float)rand() * (1.0f / 32767.0f);
}

// Interpolates value between [from, to] by x in [0, range] and clamps it to [from, to]
static float sithPhysics_LerpClamped(float from, float to, float x, float range)
{
    float value = ((to - from) / range) * x + from;
    if ( value < from )
    {
        return from;
    }
    return to < value ? to : value;
}

// Mine car sound class modes
#define SITHMINECAR_SOUND_RAILCLACK (SithSoundClassMode)6
#define SITHMINECAR_SOUND_SPARKS    (SithSoundClassMode)7
#define SITHMINECAR_SOUND_ENGINE    (SithSoundClassMode)10
#define SITHMINECAR_SOUND_RUMBLE    (SithSoundClassMode)11
#define SITHMINECAR_SOUND_CLATTER   (SithSoundClassMode)12

// Moves the mine car (or other track vehicle) along the track
void J3DAPI sithPhysics_UpdateMineCarPhysics(SithThing* pThing, float secDeltaTime)
{
    if ( secDeltaTime > 0.1f )
    {
        secDeltaTime = 0.1f;
    }

    if ( !sithPhysics_CreateMineCarUserBlock(pThing) )
    {
        return;
    }

    SithMineCarUserBlock* pBlock = pThing->userblock.pMinecar;
    SithMineCarState* pState     = &pBlock->state;
    rdVector3* pVelocity         = &pThing->moveInfo.physics.velocity;

    pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_200000;

    SITH_ASSERTREL(pThing->attach.flags);

    // Note, the original also calculated the attached surface normal here, but didn't use it
    // Fixed: Init to 0.25 when not attached to surface; originally the value was uninitialized (pThing pointer reinterpreted as float, which then got clamped to 0.25 for any common pointer value)
    float attachFactor = 0.25f;
    if ( (pThing->attach.flags & (SITH_ATTACH_SURFACE | SITH_ATTACH_THINGFACE | SITH_ATTACH_CLIMBSURFACE)) != 0 )
    {
        attachFactor = 1.0f;
    }

    // Apply drag
    if ( !sithPhysics_IsZeroVector(pVelocity) && pThing->moveInfo.physics.surfDrag != 0.0f )
    {
        if ( (pThing->moveInfo.physics.flags & SITH_PF_UNKNOWN_8000) == 0 )
        {
            float drag, staticDrag;
            if ( !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust)
                || (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) != 0
                || attachFactor <= 0.8f )
            {
                // Going uphill increases the drag, going downhill decreases it.
                // The drag is gradually changed towards the target slope drag.
                float slope = pThing->orient.lvec.z;
                if ( pThing->moveInfo.physics.thrust.y < 0.0f )
                {
                    slope = -slope; // moving backwards
                }

                float targetDrag = slope * 6.0f - -1.0f;
                if ( targetDrag < 0.0f )
                {
                    targetDrag = 0.0f;
                }
                targetDrag += pThing->moveInfo.physics.surfDrag;

                float deltaDrag = targetDrag - pState->surfDrag;
                float dragRate;
                if ( fabsf(slope) >= 0.1f )
                {
                    dragRate = deltaDrag >= 0.0f ? 4.0f : 4.0f;
                }
                else
                {
                    dragRate = deltaDrag >= 0.0f ? 0.2f : 1.0f;
                }

                drag = targetDrag;
                if ( fabsf(deltaDrag) > dragRate * secDeltaTime )
                {
                    drag = dragRate * secDeltaTime * (float)(deltaDrag >= 0.0f ? 1 : -1) + pState->surfDrag;
                }

                staticDrag       = 0.0f;
                pState->surfDrag = drag;
            }
            else if ( (pThing->flags & SITH_TF_DYING) == 0 )
            {
                staticDrag = pThing->moveInfo.physics.staticDrag * attachFactor;
                drag       = pThing->moveInfo.physics.surfDrag;
            }
            else
            {
                staticDrag = attachFactor * 0.3f;
                drag       = 3.0f;
            }

            sithPhysics_ApplyDrag(pVelocity, attachFactor * drag, staticDrag, secDeltaTime);
        }
        else
        {
            pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_8000;
        }
    }

    // Accelerate or brake
    float speed       = rdVector_Len3(pVelocity);
    float accel       = attachFactor * secDeltaTime * fabsf(pThing->moveInfo.physics.thrust.y);
    float deltaSpeed  = accel;
    if ( !sithPhysics_IsZeroVector(pVelocity) )
    {
        bool bThrustBack  = pThing->moveInfo.physics.thrust.y < 0.0f;
        bool bMovingFwd   = rdVector_Dot3(&pThing->orient.lvec, pVelocity) > 0.0f;
        if ( bMovingFwd == bThrustBack )
        {
            // Thrust is in the opposite direction of movement
            deltaSpeed = -accel;
            if ( speed < fabsf(deltaSpeed) )
            {
                // Stopped
                memset(pVelocity, 0, sizeof(*pVelocity));
                deltaSpeed = 0.0f;
            }
        }
    }

    if ( fabsf(deltaSpeed) <= 0.00001f )
    {
        deltaSpeed = 0.0f;
    }

    // Note, when the car is stopped by braking, the speed is still the speed before braking
    float newSpeed = speed + deltaSpeed;
    if ( newSpeed > 1.25f )
    {
        newSpeed = 1.25f;
    }

    sithPhysics_ProcessMineCarTrackMove(pThing, secDeltaTime, newSpeed, &pThing->moveInfo.physics.deltaVelocity);

    // Turn the car along the track
    rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;
    if ( pAngularVel->y != 0.0f || pAngularVel->x != 0.0f )
    {
        rdVector3 deltaRot;
        rdVector_Scale3(&deltaRot, pAngularVel, secDeltaTime);

        rdMatrix34 deltaRotMat;
        rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
        sithCollision_RotateThing(pThing, &deltaRotMat);
        rdMatrix_Normalize34(&pThing->orient);
    }

    if ( !pState->bWheelsLocked )
    {
        sithPhysics_UpdateVehicleChassis(pThing, &pBlock->chassisInfo, secDeltaTime);
    }

    sithPhysics_UpdateTrackVehicleFx(pThing, pBlock, secDeltaTime);
    sithPhysics_UpdateExhaustFx(pThing, &pBlock->exhaustInfo, secDeltaTime);
}

void J3DAPI sithPhysics_UpdateTrackVehicleFx(SithThing* pThing, SithMineCarUserBlock* pMineCarUserBlock, float secDeltaTime)
{
    switch ( pMineCarUserBlock->type )
    {
        case SITHMINECAR_PLAYER:
        case SITHMINECAR_DEFAULT:
            sithPhysics_UpdateMineCarFx(pThing, &pMineCarUserBlock->fxstate, secDeltaTime);
            break;

        case SITHMINECAR_KILLTRUCK_AI:
        case SITHMINECAR_TRUCK_AI:
            sithPhysics_UpdateTrackTruckFx(pThing, &pMineCarUserBlock->fxstate.engine, secDeltaTime);
            break;

        case SITHMINECAR_JEEP_AI:
            sithPhysics_UpdateTrackJeepFx(pThing, &pMineCarUserBlock->fxstate.engine, secDeltaTime);
            break;

        default:
            break;
    }
}

// Updates mine car engine, rumble, sparks, clatter and rail clack sounds, engine material animation and wheel sparks
void J3DAPI sithPhysics_UpdateMineCarFx(SithThing* pThing, SithMineCarFxState* pFxState, float secDeltaTime)
{
    SithMineCarState* pState = &pThing->userblock.pMinecar->state;
    float speed = rdVector_Len3(&pThing->moveInfo.physics.velocity);

    int bInShallowWater = 0;
    if ( (pThing->attach.flags & SITH_ATTACH_SURFACE) != 0 && (pThing->attach.attachedToStructure.pSurfaceAttached->flags & SITH_SURFACE_SHALLOWWATER) != 0 )
    {
        bInShallowWater = 1;
    }

    pFxState->engine.secUpdateTimer = pFxState->engine.secUpdateTimer - secDeltaTime;
    int bUpdate = pFxState->engine.secUpdateTimer < 0.0f;
    if ( bUpdate )
    {
        pFxState->engine.secUpdateTimer = pFxState->engine.secUpdateInterval;
    }

    // Engine sound & animation
    if ( !sithPhysics_bMineCarEngineRunFx )
    {
        sithSoundClass_StopMode(pThing, SITHMINECAR_SOUND_ENGINE);
        // Note, the original checks thing type bits 0xA here, possibly bEngineAnim was intended
        if ( pState->pEngineAnim && (pThing->type & 0xA) != 0 )
        {
            sithAnimate_Stop(pState->pEngineAnim);
            pState->pEngineAnim = NULL;
        }
    }
    else if ( pThing->moveStatus == SITHPLAYERMOVE_MINECAR_BOARDING )
    {
        sithSoundClass_StopMode(pThing, SITHMINECAR_SOUND_ENGINE);
        pState->pEngineAnim = NULL;
    }
    else if ( pThing->moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_LEFT || pThing->moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_RIGHT )
    {
        sithSoundClass_StopMode(pThing, SITHMINECAR_SOUND_ENGINE);
        if ( pState->pEngineAnim && pState->bEngineAnim )
        {
            sithAnimate_Stop(pState->pEngineAnim);
            pState->pEngineAnim = NULL;
        }
    }
    else
    {
        sithSoundClass_PlayModeFirstEx(pThing, SITHMINECAR_SOUND_ENGINE, 0.5f);
        if ( !pState->pEngineAnim && pState->bEngineAnim )
        {
            rdMaterial* pMat = sithMaterial_Load("minecar_a_ngenfrnt.mat");
            if ( pMat )
            {
                pState->pEngineAnim = sithAnimate_StartMaterialAnim(pMat, 18.0f, SITHANIMATE_LOOP);
            }
        }

        if ( bUpdate )
        {
            sithSoundClass_SetModeVolume(pThing, SITHMINECAR_SOUND_ENGINE, 0.5f * speed + 0.5f);

            float pitch = sithPhysics_LerpClamped(1.0f, 1.15f, speed, pThing->moveInfo.physics.maxVelocity);
            sithSoundClass_SetModePitch(pThing, SITHMINECAR_SOUND_ENGINE, pitch);

            if ( pState->pEngineAnim && pState->bEngineAnim )
            {
                // Speed up the engine animation with the engine pitch
                pState->pEngineAnim->msecPerFrame = (uint32_t)(1000.0f / (((pitch - 1.0f) * 10.0f - -1.0f) * 18.0f));
            }
        }
    }

    // Engine rumble sound
    if ( !sithPhysics_bMineCarEngineRumbleFx || bInShallowWater )
    {
        sithSoundClass_StopMode(pThing, SITHMINECAR_SOUND_RUMBLE);
    }
    else if ( speed == 0.0f )
    {
        if ( !sithSoundClass_IsModeFadingVol(pThing, SITHMINECAR_SOUND_RUMBLE) )
        {
            sithSoundClass_FadeModeVolume(pThing, SITHMINECAR_SOUND_RUMBLE, 0.0f, 0.6f);
        }
    }
    else
    {
        sithSoundClass_PlayModeFirstEx(pThing, SITHMINECAR_SOUND_RUMBLE, 0.05f);
        if ( bUpdate )
        {
            sithSoundClass_SetModeVolume(pThing, SITHMINECAR_SOUND_RUMBLE, 0.95f * speed + 0.05f);
        }
    }

    // Wheel sparks when taking a sharp turn
    if ( sithPhysics_bMineCarSparksFx && !bInShallowWater )
    {
        float sparksChance = 0.5f * speed;
        if ( pState->turnState == SITHMINECAR_TURN_LEFT_START || pState->turnState == SITHMINECAR_TURN_RIGHT_START )
        {
            if ( fabsf(pThing->moveInfo.physics.angularVelocity.y) > 20.0f && speed > 0.3f && sithPhysics_Random() < sparksChance )
            {
                float volume = (speed - 0.3f) * 2.0f + 0.05f;
                sithSoundClass_PlayModeRandom(pThing, SITHMINECAR_SOUND_SPARKS);
                sithSoundClass_SetModeVolume(pThing, SITHMINECAR_SOUND_SPARKS, volume);

                if ( pState->turnState == SITHMINECAR_TURN_LEFT_START )
                {
                    pState->bUpdateLeftSparks = 1;
                    pState->secTimeLeftSparks = 0.0f;
                }
                else if ( pState->turnState == SITHMINECAR_TURN_RIGHT_START )
                {
                    pState->bUpdateRightSparks = 1;
                    pState->secTimeRightSparks = 0.0f;
                }
            }
        }
        else if ( pState->turnState == SITHMINECAR_TURN_END )
        {
            sithSoundClass_FadeModeVolume(pThing, SITHMINECAR_SOUND_SPARKS, 0.0f, 0.5f);
            pState->bUpdateLeftSparks  = 0;
            pState->bUpdateRightSparks = 0;
        }

        if ( pState->bWheelsLocked )
        {
            if ( !pState->bUpdateLeftSparks )
            {
                pState->bUpdateLeftSparks = 1;
                pState->secTimeLeftSparks = 0.0f;
            }

            if ( pState->bWheelsLocked && !pState->bUpdateRightSparks )
            {
                pState->bUpdateRightSparks = 1;
                pState->secTimeRightSparks = 0.0f;
            }
        }

        if ( pState->bUpdateLeftSparks )
        {
            pState->secTimeLeftSparks = pState->secTimeLeftSparks - secDeltaTime;
            if ( pState->secTimeLeftSparks <= 0.0f )
            {
                rdVector3 sparkPos = sithPhysics_vecMinecarSparkPosLeft;
                sithFX_CreateMineCarSparks(pThing, &sparkPos);
                pState->secTimeLeftSparks = 0.05f;
            }
        }

        if ( pState->bUpdateRightSparks )
        {
            pState->secTimeRightSparks = pState->secTimeRightSparks - secDeltaTime;
            if ( pState->secTimeRightSparks <= 0.0f )
            {
                rdVector3 sparkPos = sithPhysics_vecMinecarSparkPosRight;
                sithFX_CreateMineCarSparks(pThing, &sparkPos);
                pState->secTimeRightSparks = 0.05f;
            }
        }
    }

    // Wheel clatter sound when moving on track with adjoin
    if ( sithPhysics_bMineCarClatterFx && !bInShallowWater )
    {
        if ( pState->bOnAdjoinTrack && speed != 0.0f )
        {
            sithSoundClass_PlayModeFirstEx(pThing, SITHMINECAR_SOUND_CLATTER, 1.0f * speed + 0.1f);
        }
        else if ( !sithSoundClass_IsModeFadingVol(pThing, SITHMINECAR_SOUND_CLATTER) )
        {
            sithSoundClass_FadeModeVolume(pThing, SITHMINECAR_SOUND_CLATTER, 0.0f, 0.5f);
        }
    }
    else
    {
        sithSoundClass_FadeModeVolume(pThing, SITHMINECAR_SOUND_CLATTER, 0.0f, 0.2f);
    }

    // Rail clack sound, the clack state is counted towards 0 by the distance traveled
    // alternating the sign, and each time it crosses 0 the clack sound is played.
    if ( sithPhysics_bMineCarRailClackFx && !bInShallowWater )
    {
        if ( speed == 0.0f )
        {
            pFxState->railClackState = pFxState->railClackDistance;
        }
        else
        {
            float clackStep = (((3.0f - 0.1f) / pThing->moveInfo.physics.maxVelocity) * speed + 0.1f) * secDeltaTime;
            float prevState = pFxState->railClackState;
            if ( prevState <= 0.0f )
            {
                pFxState->railClackState = clackStep + pFxState->railClackState;
            }
            else
            {
                pFxState->railClackState = pFxState->railClackState - clackStep;
            }

            int newSign  = pFxState->railClackState >= 0.0f ? 1 : -1;
            int prevSign = prevState >= 0.0f ? 1 : -1;
            if ( newSign != prevSign || pFxState->railClackState == 0.0f )
            {
                sithSoundClass_PlayMode(pThing, SITHMINECAR_SOUND_RAILCLACK, pFxState->railClackState < 0.0f ? 0.0f : 1.0f);

                float volume = 1.0f * speed + 0.1f;
                float pitch  = 0.05f * speed - -1.0f;
                float pitchScatter = sithPhysics_Random() * 0.01f - 0.005f;
                sithSoundClass_SetModeVolume(pThing, SITHMINECAR_SOUND_RAILCLACK, volume);
                sithSoundClass_SetModePitch(pThing, SITHMINECAR_SOUND_RAILCLACK, pitchScatter + pitch);

                pFxState->railClackState = (float)-prevSign * (sithPhysics_Random() * 0.2f + pFxState->railClackDistance);
            }
        }
    }
}

// Updates the engine sound of jeep on track
void J3DAPI sithPhysics_UpdateTrackJeepFx(SithThing* pThing, SithVehicleEngineFxState* pFxState, float secDeltaTime)
{
    if ( (pThing->flags & SITH_TF_DYING) != 0 )
    {
        return;
    }

    pFxState->secUpdateTimer = pFxState->secUpdateTimer - secDeltaTime;
    int bUpdate = pFxState->secUpdateTimer < 0.0f;
    if ( bUpdate )
    {
        pFxState->secUpdateTimer = pFxState->secUpdateInterval;
    }

    if ( !sithPhysics_bUpdateTrackJeepEngineFx )
    {
        sithSoundClass_StopMode(pThing, SITHMINECAR_SOUND_ENGINE);
        return;
    }

    sithSoundClass_PlayModeFirstEx(pThing, SITHMINECAR_SOUND_ENGINE, sithPhysics_truckMinSndVolume);
    if ( bUpdate )
    {
        float speed = rdVector_Len3(&pThing->moveInfo.physics.velocity);
        sithSoundClass_SetModeVolume(pThing, SITHMINECAR_SOUND_ENGINE, sithPhysics_LerpClamped(sithPhysics_truckMinSndVolume, 1.0f, speed, 1.0f));
        sithSoundClass_SetModePitch(pThing, SITHMINECAR_SOUND_ENGINE, sithPhysics_LerpClamped(sithPhysics_truckMinSndPitch, sithPhysics_truckMaxSndPitch, speed, 1.0f));
    }
}

// Updates the engine sound of truck on track
void J3DAPI sithPhysics_UpdateTrackTruckFx(SithThing* pThing, SithVehicleEngineFxState* pFxState, float secDeltaTime)
{
    if ( (pThing->flags & SITH_TF_DYING) != 0 )
    {
        return;
    }

    pFxState->secUpdateTimer = pFxState->secUpdateTimer - secDeltaTime;
    int bUpdate = pFxState->secUpdateTimer < 0.0f;
    if ( bUpdate )
    {
        pFxState->secUpdateTimer = pFxState->secUpdateInterval;
    }

    if ( !sithPhysics_bUpdateTrackTruckEngineFx )
    {
        sithSoundClass_StopMode(pThing, SITHMINECAR_SOUND_ENGINE);
        return;
    }

    sithSoundClass_PlayModeFirstEx(pThing, SITHMINECAR_SOUND_ENGINE, 0.5f);
    if ( bUpdate )
    {
        float speed = rdVector_Len3(&pThing->moveInfo.physics.velocity);
        sithSoundClass_SetModeVolume(pThing, SITHMINECAR_SOUND_ENGINE, sithPhysics_LerpClamped(0.5f, 1.0f, speed, 1.0f));
        sithSoundClass_SetModePitch(pThing, SITHMINECAR_SOUND_ENGINE, ((1.0f - 0.52f) / 1.0f) * speed + 0.52f); // Note, not clamped
    }
}

// Spins vehicle wheels by the distance traveled
void J3DAPI sithPhysics_UpdateVehicleChassis(SithThing* pThing, SithVehicleChassisInfo* pChasisInfo, float secDeltaTime)
{
    if ( !pChasisInfo->numNodes || sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        return;
    }

    float halfCircumference = pChasisInfo->wheelRadius * 3.14159f;
    float angle = stdMath_NormalizeAngle(rdVector_Len3(&pThing->moveInfo.physics.velocity) / (halfCircumference + halfCircumference) * 360.0f * secDeltaTime);
    if ( rdVector_Dot3(&pThing->orient.lvec, &pThing->moveInfo.physics.velocity) < 0.0f )
    {
        angle = angle * -1.0f; // moving backwards
    }

    for ( size_t i = 0; i < pChasisInfo->numNodes; ++i )
    {
        int nodeNum = pChasisInfo->aWheelNodeNums[i];
        if ( nodeNum != -1 )
        {
            pThing->renderData.apTweakedAngles[nodeNum].pitch -= angle;
        }
    }
}

// Emits exhaust smoke particles along the path the vehicle moved this frame
void J3DAPI sithPhysics_UpdateExhaustFx(SithThing* pThing, SithVehicleExhaustInfo* pExhaustInfo, float secDeltaTime)
{
    if ( !pExhaustInfo->bEnabled
        || pThing->moveStatus == SITHPLAYERMOVE_MINECAR_BOARDING
        || pThing->moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_RIGHT
        || pThing->moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_LEFT )
    {
        return;
    }

    const rdMatrix34* pOrient = &pThing->orient;
    const rdVector3* pMoveDelta = &pThing->moveInfo.physics.deltaVelocity; // position delta of this frame

    // Exhaust position at the end of this frame
    rdVector3 exhaustPos;
    exhaustPos.x = pOrient->rvec.x * pExhaustInfo->offsetX + pOrient->uvec.x * pExhaustInfo->offsetZ + pOrient->lvec.x * pExhaustInfo->offsetY + pThing->pos.x + pMoveDelta->x;
    exhaustPos.y = pOrient->rvec.y * pExhaustInfo->offsetX + pOrient->uvec.y * pExhaustInfo->offsetZ + pOrient->lvec.y * pExhaustInfo->offsetY + pThing->pos.y + pMoveDelta->y;
    exhaustPos.z = pOrient->rvec.z * pExhaustInfo->offsetX + pOrient->uvec.z * pExhaustInfo->offsetZ + pOrient->lvec.z * pExhaustInfo->offsetY + pThing->pos.z + pMoveDelta->z;

    rdVector3 moveDir;
    rdVector_Normalize3(&moveDir, pMoveDelta);
    if ( rdVector_Dot3(&pThing->moveInfo.physics.velocity, &pOrient->lvec) < 0.0f )
    {
        rdVector_Scale3Acc(&moveDir, -0.5f); // reversing
    }

    float moveDist = rdVector_Len3(pMoveDelta);
    float speed    = rdVector_Len3(&pThing->moveInfo.physics.velocity);

    uint32_t numParticles = (uint32_t)(moveDist / pExhaustInfo->moveLength);
    if ( numParticles == 0 )
    {
        numParticles = 1;
    }
    if ( numParticles > 10 )
    {
        numParticles = 10;
    }

    float secParticleDelta = secDeltaTime / (float)numParticles;
    SithThing* pTemplate = sithTemplate_GetTemplate(pExhaustInfo->aExhaustTemplate);
    for ( uint32_t i = 0; i < numParticles; ++i )
    {
        if ( !pTemplate )
        {
            continue;
        }

        float secAge = (float)i * secParticleDelta;
        float maxLife = pThing->moveInfo.physics.maxVelocity * pExhaustInfo->velScale + pExhaustInfo->velocity;

        // Smoke velocity
        float velX = ((sithPhysics_Random() - 0.5f) * pExhaustInfo->scatterX + pExhaustInfo->dirX) * speed;
        rdVector3 smokeVel;
        rdVector_Scale3(&smokeVel, &pOrient->rvec, velX);

        float velZ = ((pExhaustInfo->scatterZ - pExhaustInfo->dirZ) * sithPhysics_Random() + pExhaustInfo->dirZ) * speed;
        rdVector_ScaleAdd3Acc(&smokeVel, &pOrient->uvec, velZ);

        // Spawn particles back along the moved path, moved forward by their age
        rdVector_ScaleAdd3Acc(&exhaustPos, &moveDir, -pExhaustInfo->moveLength);

        rdVector3 spawnPos;
        spawnPos.x = pOrient->uvec.x * (velZ * secAge) + pOrient->rvec.x * (velX * secAge) + exhaustPos.x;
        spawnPos.y = pOrient->uvec.y * (velZ * secAge) + pOrient->rvec.y * (velX * secAge) + exhaustPos.y;
        spawnPos.z = pOrient->uvec.z * (velZ * secAge) + pOrient->rvec.z * (velX * secAge) + exhaustPos.z;

        SithThing* pSmoke = sithThing_CreateThingAtPos(pTemplate, &spawnPos, pOrient, pThing->pInSector, NULL);
        if ( !pSmoke )
        {
            continue;
        }

        float secLife = (pExhaustInfo->velScale * speed + pExhaustInfo->velocity) - secAge;
        float lifeScatter = (sithPhysics_Random() - 0.5f) * (pExhaustInfo->smokeLifeScale * secLife);
        pSmoke->msecLifeLeft = (uint32_t)((lifeScatter + secLife) * 1000.0f);

        float startSize = ((pExhaustInfo->startMaxSize - pExhaustInfo->startMinSize) / maxLife) * secAge + pExhaustInfo->startMinSize;
        float endSize   = ((pExhaustInfo->endMaxSize - pExhaustInfo->endMinSize) / maxLife) * secAge + pExhaustInfo->endMinSize;

        rdVector3 startScale, endScale;
        float scatter = sithPhysics_Random() - 0.5f;
        startScale.x = scatter * pExhaustInfo->startSizeScale + startSize;
        startScale.y = (sithPhysics_Random() - 0.5f) * pExhaustInfo->startSizeScale + startSize;
        endScale.x   = (sithPhysics_Random() - 0.5f) * pExhaustInfo->startSizeScale + pExhaustInfo->startMaxSize;
        endScale.y   = (sithPhysics_Random() - 0.5f) * pExhaustInfo->startSizeScale + pExhaustInfo->startMaxSize;
        startScale.z = (sithPhysics_Random() - 0.5f) * pExhaustInfo->endSizeScale + endSize;
        endScale.z   = pExhaustInfo->endMaxSize;

        sithAnimate_StartSpriteSizeAnim(pSmoke, &startScale, &endScale, lifeScatter + secLife);
        pSmoke->moveInfo.physics.velocity = smokeVel;
    }
}

// update physics
// Removes the component of vec along unit vector n and normalizes the result
static void sithPhysics_ProjectOnPlaneNormalize(rdVector3* pOut, const rdVector3* pVec, const rdVector3* n)
{
    float dot = -rdVector_Dot3(pVec, n);
    pOut->x = n->x * dot + pVec->x;
    pOut->y = n->y * dot + pVec->y;
    pOut->z = n->z * dot + pVec->z;
    rdVector_Normalize3Acc(pOut);
}

// Probes the raft corner offset in raft's local space for water and returns the probe result.
static int sithPhysics_ProbeRaftPoint(SithThing* pThing, float offsetX, float offsetY, rdVector3* pOutPoint, rdVector3* pOutNormal, float secDeltaTime)
{
    pOutPoint->x = offsetX;
    pOutPoint->y = offsetY;
    pOutPoint->z = 0.0f;
    rdMatrix_TransformPoint34Acc(pOutPoint, &pThing->orient);
    rdVector_Add3Acc(pOutPoint, &pThing->pos);

    float distance;
    int result = sithPhysics_GetWaterNormalAtPos(pThing, &distance, pOutNormal, pOutPoint, secDeltaTime);
    pOutPoint->z -= distance;
    return result;
}

// Returns normalized cross(b - a, c - b), the normal of the raft surface triangle a, b, c
static void sithPhysics_GetRaftTriangleNormal(rdVector3* pOut, const rdVector3* a, const rdVector3* b, const rdVector3* c)
{
    rdVector3 edge1, edge2;
    rdVector_Sub3(&edge1, b, a);
    rdVector_Sub3(&edge2, c, b);
    rdVector_Cross3(pOut, &edge1, &edge2);
    rdVector_Normalize3Acc(pOut);
}

// Updates physics of raft floating on water.
// The water surface is probed under each raft corner and center to tilt the raft with the water,
// bank collisions are handled by the corners that hit the ground and the water current is applied via sector thrust.
void J3DAPI sithPhysics_UpdateRaftPhysics(SithThing* pThing, float secDeltaTime)
{
    rdVector3 deltaVel = { 0 };
    pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_200000;

    SITH_ASSERTREL(pThing->attach.flags);

    // Get attached surface normal and distance of raft from it
    // Added: Init to default values; originally uninitialized if none of the flags below is set.
    rdVector3 normal  = rdroid_g_zVector3;
    float distance    = 0.0f;
    float slopeFactor = 1.0f;

    SithAttachFlag attachFlags = pThing->attach.flags;
    if ( (attachFlags & SITH_ATTACH_SURFACE) != 0 || (attachFlags & (SITH_ATTACH_THINGFACE | SITH_ATTACH_CLIMBSURFACE)) == SITH_ATTACH_CLIMBSURFACE )
    {
        normal = pThing->attach.pFace->normal;

        rdVector3 dir;
        rdVector_Sub3(&dir, &pThing->pos, &pThing->attach.attachedFaceFirstVert);
        distance = rdVector_Dot3(&dir, &normal);
    }
    else if ( (attachFlags & SITH_ATTACH_THINGFACE) != 0 )
    {
        SithThing* pAttached = pThing->attach.attachedToStructure.pThingAttached;
        rdMatrix_TransformVector34(&normal, &pThing->attach.pFace->normal, &pAttached->orient);

        rdVector3 vert;
        rdMatrix_TransformVector34(&vert, &pThing->attach.attachedFaceFirstVert, &pAttached->orient);
        rdVector_Add3Acc(&vert, &pAttached->pos);

        rdVector3 dir;
        rdVector_Sub3(&dir, &pThing->pos, &vert);
        distance = rdVector_Dot3(&dir, &normal);
    }

    float floorSlopeDot = rdVector_Dot3(&normal, &rdroid_g_zVector3);

    // Probe water under raft corners and center
    rdVector3 aCorners[4], aCornerNormals[4];
    int aCornerResults[4];
    aCornerResults[0] = sithPhysics_ProbeRaftPoint(pThing, 0.06f, 0.09f, &aCorners[0], &aCornerNormals[0], secDeltaTime);   // front right
    aCornerResults[1] = sithPhysics_ProbeRaftPoint(pThing, -0.06f, 0.09f, &aCorners[1], &aCornerNormals[1], secDeltaTime);  // front left
    aCornerResults[2] = sithPhysics_ProbeRaftPoint(pThing, -0.06f, -0.09f, &aCorners[2], &aCornerNormals[2], secDeltaTime); // back left
    aCornerResults[3] = sithPhysics_ProbeRaftPoint(pThing, 0.06f, -0.09f, &aCorners[3], &aCornerNormals[3], secDeltaTime);  // back right

    rdVector3 center = pThing->pos, centerNormal;
    float centerDistance;
    int centerResult = sithPhysics_GetWaterNormalAtPos(pThing, &centerDistance, &centerNormal, &center, secDeltaTime);
    center.z -= centerDistance;
    pThing->userval = (float)(unsigned int)centerResult; // Note, raft cogs can read probe result via thing user value

    // Raft up vector is the average normal of the 4 triangles fanning from the center
    rdVector3 aTriNormals[4];
    rdVector3 raftUp = { 0 };
    for ( size_t i = 0; i < 4; ++i )
    {
        sithPhysics_GetRaftTriangleNormal(&aTriNormals[i], &aCorners[(i + 3) % 4], &aCorners[i], &center);
        rdVector_Add3Acc(&raftUp, &aTriNormals[i]);
    }
    rdVector_Normalize3Acc(&raftUp);

    // Tilt front and back of the raft by the pitch of the front and back triangles
    int frontJoint = sithThing_GetThingJointIndex(pThing, "inraftfr");
    int backJoint  = sithThing_GetThingJointIndex(pThing, "inraftbk");

    rdVector3 upInPitchPlane;
    sithPhysics_ProjectOnPlaneNormalize(&upInPitchPlane, &raftUp, &pThing->orient.rvec);

    rdVector3 pitchAxis;
    rdVector_Cross3(&pitchAxis, &upInPitchPlane, &pThing->orient.rvec);

    rdVector3 triNormal;
    sithPhysics_ProjectOnPlaneNormalize(&triNormal, &aTriNormals[1], &pThing->orient.rvec);
    float angle = stdMath_ArcSin1(rdVector_Dot3(&pitchAxis, &triNormal));
    if ( frontJoint != -1 )
    {
        pThing->renderData.apTweakedAngles[frontJoint].pitch = angle * 1.0f;
    }

    sithPhysics_ProjectOnPlaneNormalize(&triNormal, &aTriNormals[3], &pThing->orient.rvec);
    angle = stdMath_ArcSin1(rdVector_Dot3(&pitchAxis, &triNormal));
    if ( backJoint != -1 )
    {
        pThing->renderData.apTweakedAngles[backJoint].pitch = angle * 1.0f;
    }

    sithPhysics_SetThingLook(pThing, &raftUp, pThing->moveInfo.physics.orientSpeed * secDeltaTime);

    // Update turning
    rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;
    if ( (pThing->moveInfo.physics.flags & SITH_PF_USEANGULARTHRUST) != 0 )
    {
        if ( !sithPhysics_IsZeroVector(pAngularVel) )
        {
            sithPhysics_ApplyDrag(pAngularVel, pThing->moveInfo.physics.surfDrag + 0.4f, 0.0f, secDeltaTime);
        }

        if ( centerResult > 2 ) // in water
        {
            float maxRotVel = pThing->moveInfo.physics.maxRotationVelocity;
            sithPhysics_ClampVector(pAngularVel, -maxRotVel, maxRotVel);
        }

        pAngularVel->y = pThing->moveInfo.physics.rotThrust.y * secDeltaTime + pAngularVel->y;
        sithPhysics_ClipVectorNearZero(pAngularVel);
    }

    if ( pAngularVel->y != 0.0f )
    {
        rdVector3 deltaRot;
        rdVector_Scale3(&deltaRot, pAngularVel, secDeltaTime);

        rdMatrix34 deltaRotMat;
        rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
        sithCollision_RotateThing(pThing, &deltaRotMat);
        rdMatrix_Normalize34(&pThing->orient);
    }

    slopeFactor = STDMATH_CLAMP(slopeFactor, 0.25f, 1.0f);

    // Apply surface drag
    if ( !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) && pThing->moveInfo.physics.surfDrag != 0.0f )
    {
        if ( (pThing->moveInfo.physics.flags & SITH_PF_UNKNOWN_8000) == 0 )
        {
            float staticDrag = 0.0f;
            if ( sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust)
                && (pThing->pInSector->flags & SITH_SECTOR_USETHRUST) == 0
                && slopeFactor > 0.8f )
            {
                staticDrag = pThing->moveInfo.physics.staticDrag * slopeFactor;
            }

            sithPhysics_ApplyDrag(&pThing->moveInfo.physics.velocity, pThing->moveInfo.physics.surfDrag * slopeFactor, staticDrag, secDeltaTime);
        }
        else
        {
            pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_8000;
        }
    }

    // Apply paddling thrust while in water
    if ( (pThing->moveInfo.physics.flags & SITH_PF_USETHRUST) != 0 && !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust) && centerResult > 2 )
    {
        rdVector_Scale3(&deltaVel, &pThing->moveInfo.physics.thrust, slopeFactor * secDeltaTime);
        sithPhysics_ClipVectorNearZero(&deltaVel);
        if ( !sithPhysics_IsZeroVector(&deltaVel) )
        {
            rdMatrix_TransformVector34Acc(&deltaVel, &pThing->orient);
        }
    }

    // Push the raft down the slope of the water
    if ( floorSlopeDot < 1.0f )
    {
        rdVector3 side, downSlope;
        rdVector_Cross3(&side, &normal, &rdroid_g_zVector3);
        rdVector_Cross3(&downSlope, &normal, &side);
        rdVector_Normalize3Acc(&downSlope);
        rdVector_ScaleAdd3Acc(&deltaVel, &downSlope, (1.0f - floorSlopeDot) * 0.3f * secDeltaTime);
    }

    // Apply water current
    sithPhysics_GetWaterThrust(pThing, &normal, &deltaVel, secDeltaTime);
    rdVector_Add3Acc(&pThing->moveInfo.physics.velocity, &deltaVel);

    // Bounce off the banks hit by raft corners
    for ( size_t i = 0; i < 4; ++i )
    {
        if ( aCornerResults[i] == 0 )
        {
            rdVector3 bankNormal;
            sithPhysics_ProjectOnPlaneNormalize(&bankNormal, &aCornerNormals[i], &pThing->orient.uvec);

            float hitVel = rdVector_Dot3(&pThing->moveInfo.physics.velocity, &bankNormal);
            if ( hitVel < 0.0f )
            {
                rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.velocity, &bankNormal, -(hitVel * 1.1f));
            }
        }
    }

    // Redirect velocity along the water surface, keeping the speed
    float t = -rdVector_Dot3(&pThing->moveInfo.physics.velocity, &normal) / rdVector_Dot3(&normal, &rdroid_g_zVector3);
    rdVector3 moveDir;
    rdVector_ScaleAdd3(&moveDir, &rdroid_g_zVector3, t, &pThing->moveInfo.physics.velocity);
    rdVector_Normalize3Acc(&moveDir);
    rdVector_Scale3(&pThing->moveInfo.physics.velocity, &moveDir, rdVector_Len3(&pThing->moveInfo.physics.velocity));

    if ( !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        float normalVel = stdMath_ClipNearZero(rdVector_Dot3(&pThing->moveInfo.physics.velocity, &normal));
        if ( normalVel < 0.0f )
        {
            rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.velocity, &normal, -normalVel);
        }
    }

    sithPhysics_ClipVectorNearZero(&pThing->moveInfo.physics.velocity);
    if ( !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        rdVector_Scale3(&pThing->moveInfo.physics.deltaVelocity, &pThing->moveInfo.physics.velocity, secDeltaTime);
    }

    // Keep raft at its height above the water
    float heightDelta = stdMath_ClipNearZero(distance - sithPhysics_GetThingHeight(pThing));
    if ( heightDelta != 0.0f )
    {
        float maxDelta = 0.05f * secDeltaTime;
        heightDelta = STDMATH_CLAMP(heightDelta, -maxDelta, maxDelta);
        rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.deltaVelocity, &normal, -heightDelta);
    }
}

// Probes water surface downwards from pPos.
// Returns: 0 - ground (non-water) surface was hit, 1 - nothing was hit, 3 - water hit via adjoin to underwater sector,
//          4 - water surface was hit. pOutNormal and pOutDistance are set to the normal and distance to the surface hit.
int J3DAPI sithPhysics_GetWaterNormalAtPos(const SithThing* pThing, float* pOutDistance, rdVector3* pOutNormal, const rdVector3* pPos, float secDeltaTime)
{
    float searchDist = sithPhysics_GetThingHeight(pThing) + 1.5f * secDeltaTime + 0.019f;

    rdVector3 startPos = *pPos;
    startPos.z += 0.019f;

    rdVector3 dir;
    rdVector_Neg3(&dir, &rdroid_g_zVector3);

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.001f);
    if ( !pSector )
    {
        return 0;
    }

    int result = 1;
    sithCollision_SearchForCollisions(pSector, NULL, &startPos, &dir, searchDist, 0.0f, 1);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0 )
        {
            if ( (pCollision->pSurfaceCollided->pAdjoin->pAdjoinSector->flags & SITH_SECTOR_UNDERWATER) != 0 )
            {
                *pOutNormal   = pCollision->pSurfaceCollided->face.normal;
                *pOutDistance = pCollision->distance - 0.019f;
                sithCollision_DecreaseStackLevel();
                return 3;
            }
        }
        else if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            SithSurface* pSurface = pCollision->pSurfaceCollided;
            if ( (pSurface->flags & SITH_SURFACE_WATER) != 0 && (pSurface->pSector->flags & SITH_SECTOR_UNDERWATER) == 0 )
            {
                if ( pSurface->face.normal.z >= 0.45f )
                {
                    *pOutNormal   = pSurface->face.normal;
                    *pOutDistance = pCollision->distance - 0.019f;
                    sithCollision_DecreaseStackLevel();
                    return 4;
                }
            }
            else if ( result != 0 )
            {
                // First ground surface hit
                *pOutNormal   = pSurface->face.normal;
                *pOutDistance = pSurface->face.normal.z < 0.45f ? sithPhysics_GetThingHeight(pThing) : pCollision->distance - 0.019f;
                result = 0;
            }
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( result == 1 )
    {
        *pOutDistance = searchDist - 0.019f;
        *pOutNormal   = rdroid_g_zVector3;
    }

    return result;
}

// Adds the water current (thrust of water sector under the thing) projected along the plane with normal pNormal to pOutThrust.
void J3DAPI sithPhysics_GetWaterThrust(SithThing* pThing, rdVector3* pNormal, rdVector3* pOutThrust, float secDeltaTime)
{
    if ( (pThing->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 )
    {
        return;
    }

    rdVector3 dir;
    rdVector_Neg3(&dir, &rdroid_g_zVector3);

    SithSector* pWaterSector = NULL;
    sithCollision_SearchForCollisions(pThing->pInSector, NULL, &pThing->pos, &dir, sithPhysics_GetThingHeight(pThing) + 0.05f, 0.001f, 1);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0 )
        {
            pWaterSector = pCollision->pSurfaceCollided->pAdjoin->pAdjoinSector;
            if ( (pWaterSector->flags & (SITH_SECTOR_UNDERWATER | SITH_SECTOR_USETHRUST)) != 0 )
            {
                break;
            }
        }
        else if ( (pCollision->type & SITHCOLLISION_WORLD) != 0
            && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_WATER) != 0
            && (pCollision->pSurfaceCollided->pSector->flags & SITH_SECTOR_UNDERWATER) == 0 )
        {
            pWaterSector = pCollision->pSurfaceCollided->pSector;
            break;
        }
    }
    sithCollision_DecreaseStackLevel();

    if ( pWaterSector && (pWaterSector->flags & SITH_SECTOR_USETHRUST) != 0 )
    {
        // Project current along the water plane vertically
        const rdVector3* pThrust = &pWaterSector->thrust;
        float t = -rdVector_Dot3(pThrust, pNormal) / rdVector_Dot3(pNormal, &rdroid_g_zVector3);

        rdVector3 thrustDir;
        rdVector_ScaleAdd3(&thrustDir, &rdroid_g_zVector3, t, pThrust);
        rdVector_Normalize3Acc(&thrustDir);

        rdVector_ScaleAdd3Acc(pOutThrust, &thrustDir, rdVector_Len3(pThrust) * secDeltaTime);
    }
}

// Moves the track vehicle along the track faces for the distance speed * secDeltaTime, sets its velocity along the track
// and turns the vehicle towards the direction of track, blending the direction with the adjacent track face.
// Note, physics rotThrust and buoyancy are used to store the direction of previous track face.
void J3DAPI sithPhysics_ProcessMineCarTrackMove(SithThing* pThing, float secDeltaTime, float speed, rdVector3* pOutPosDelta)
{
    SithMineCarState* pState = &pThing->userblock.pMinecar->state;
    rdFace* pFace            = pThing->attach.pFace;
    if ( !pFace )
    {
        return;
    }

    // Track point under the vehicle
    rdVector3 startPos = pThing->pos;
    startPos.z = startPos.z - sithPhysics_GetThingHeight(pThing);

    rdVector3 curPos = startPos;
    rdVector3 dir, right;
    int bReverse = 0;
    if ( sithPhysics_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        rdVector_Normalize3(&dir, &pThing->orient.lvec);
        rdVector_Normalize3(&right, &pThing->orient.rvec);
        if ( pThing->moveInfo.physics.thrust.y < 0.0f )
        {
            rdVector_Neg3Acc(&dir);
            rdVector_Neg3Acc(&right);
            bReverse = 1;
        }
    }
    else
    {
        rdVector_Normalize3(&dir, &pThing->moveInfo.physics.velocity);
        rdVector_Cross3(&right, &dir, &pThing->orient.uvec);
        if ( rdVector_Dot3(&pThing->moveInfo.physics.velocity, &pThing->orient.lvec) < 0.0f )
        {
            bReverse = 1;
        }
    }

    // Move along the track faces
    SithTrackFaceInfo faceInfo;
    rdVector3 newPos;
    float secRemaining = secDeltaTime;
    for ( ;; )
    {
        if ( !sithPhysics_ProcessTrackFace(pThing, pFace, &dir, &right, &curPos, &faceInfo) )
        {
            SITHLOG_ERROR("Couldn't process track face during move.\n");
            goto error;
        }

        sithPhysics_MoveAlongTrackFace(pThing, &faceInfo, &secRemaining, &curPos, &newPos, speed);
        if ( secRemaining == 0.0f )
        {
            break;
        }

        // Reached the front of face, continue on the next face
        rdVector_Scale3(&pThing->moveInfo.physics.rotThrust, &faceInfo.dir, -1.0f);
        pThing->moveInfo.physics.buoyancy = bReverse ? -1.0f : 1.0f;

        curPos = newPos;
        rdVector_ScaleAdd3Acc(&newPos, &faceInfo.dir, 0.01f);
        if ( !sithPhysics_CheckForPointOnTrack(pThing, &newPos, &pFace, 1, pFace) )
        {
            SITHLOG_ERROR("Couldn't find track face ahead of current face during move.\n");
            goto error;
        }

        dir   = faceInfo.dir;
        right = faceInfo.right;
    }

    rdVector3 moveDir;
    rdVector_Sub3(&moveDir, &newPos, &curPos);
    sithPhysics_ClipVectorNearZero(&moveDir);
    rdVector_Normalize3Acc(&moveDir);

    rdVector_Sub3(pOutPosDelta, &newPos, &startPos);
    sithPhysics_ClipVectorNearZero(pOutPosDelta);

    rdVector_Scale3(&pThing->moveInfo.physics.velocity, &moveDir, speed);

    // Get the direction of track at new position by blending the direction of current face with
    // the direction of previous face when in back half of the face or with the next face when in front half of the face.
    rdVector3 tmp;
    rdVector_Sub3(&tmp, &newPos, &faceInfo.front);
    float distToFront = -rdVector_Dot3(&tmp, &faceInfo.dir);
    if ( distToFront < 0.0f )
    {
        distToFront = 0.0f;
    }

    rdVector_Sub3(&tmp, &newPos, &faceInfo.back);
    float distFromBack = rdVector_Dot3(&tmp, &faceInfo.dir);
    if ( distFromBack < 0.0f )
    {
        distFromBack = 0.0f;
    }

    rdVector_Sub3(&tmp, &faceInfo.front, &faceInfo.back);
    float faceLength = rdVector_Dot3(&tmp, &faceInfo.dir);

    SithTrackFaceInfo adjFaceInfo;
    rdVector3 probePos;
    rdFace* pAdjFace;
    rdVector3 trackDir;
    bool bBlend = true;
    if ( distFromBack < distToFront )
    {
        // Back half of face, blend with previous face
        bool bHavePrevDir = bReverse ? pThing->moveInfo.physics.buoyancy < 0.0f : pThing->moveInfo.physics.buoyancy > 0.0f;
        if ( bHavePrevDir )
        {
            adjFaceInfo.dir = pThing->moveInfo.physics.rotThrust;
        }
        else
        {
            rdVector3 backDir, backRight;
            rdVector_Scale3(&backDir, &faceInfo.dir, -1.0f);
            rdVector_Scale3(&backRight, &faceInfo.right, -1.0f);

            rdVector_ScaleAdd3(&probePos, &backDir, 0.025f, &faceInfo.back);
            if ( !sithPhysics_CheckForPointOnTrack(pThing, &probePos, &pAdjFace, 0, pFace) )
            {
                SITHLOG_ERROR("Couldn't find track behind during orientation.\n");
                goto error;
            }

            if ( !sithPhysics_ProcessTrackFace(pThing, pAdjFace, &backDir, &backRight, &probePos, &adjFaceInfo) )
            {
                SITHLOG_ERROR("Couldn't process track face behind during orientation.\n");
                goto error;
            }
        }

        if ( faceLength > pState->maxBlendFaceLength )
        {
            if ( distFromBack > pState->longFaceBlendLength * 0.5f )
            {
                bBlend = false;
            }
            else
            {
                faceLength = pState->longFaceBlendLength;
            }
        }

        if ( bBlend )
        {
            float t = distFromBack / faceLength - -0.5f;
            t = t >= 0.0f ? (t > 1.0f ? 1.0f : t) : 0.0f;

            // Note, the dir of previous face is pointing backwards
            float s = -(1.0f - t);
            trackDir.x = t * faceInfo.dir.x + adjFaceInfo.dir.x * s;
            trackDir.y = t * faceInfo.dir.y + adjFaceInfo.dir.y * s;
            trackDir.z = t * faceInfo.dir.z + adjFaceInfo.dir.z * s;
            rdVector_Normalize3Acc(&trackDir);
        }
        else
        {
            trackDir = faceInfo.dir;
        }
    }
    else
    {
        // Front half of face, blend with next face
        rdVector_ScaleAdd3(&probePos, &faceInfo.dir, 0.025f, &faceInfo.front);
        if ( !sithPhysics_CheckForPointOnTrack(pThing, &probePos, &pAdjFace, 0, pFace) )
        {
            SITHLOG_ERROR("Couldn't find track face ahead during orientation.\n");
            goto error;
        }

        if ( !sithPhysics_ProcessTrackFace(pThing, pAdjFace, &faceInfo.dir, &faceInfo.right, &probePos, &adjFaceInfo) )
        {
            SITHLOG_ERROR("Couldn't process track face ahead during orientation.\n");
            goto error;
        }

        if ( faceLength > pState->maxBlendFaceLength )
        {
            if ( distToFront > pState->longFaceBlendLength * 0.5f )
            {
                bBlend = false;
            }
            else
            {
                faceLength = pState->longFaceBlendLength;
            }
        }

        if ( bBlend )
        {
            float t = distToFront / faceLength - -0.5f;
            t = t >= 0.0f ? (t > 1.0f ? 1.0f : t) : 0.0f;

            float s = 1.0f - t;
            trackDir.x = t * faceInfo.dir.x + adjFaceInfo.dir.x * s;
            trackDir.y = t * faceInfo.dir.y + adjFaceInfo.dir.y * s;
            trackDir.z = t * faceInfo.dir.z + adjFaceInfo.dir.z * s;
            rdVector_Normalize3Acc(&trackDir);
        }
        else
        {
            trackDir = faceInfo.dir;
        }
    }

    if ( bReverse )
    {
        rdVector_Neg3Acc(&trackDir);
    }

    // Get yaw and pitch to turn the vehicle towards the track direction
    rdVector3 lookXY  = { pThing->orient.lvec.x, pThing->orient.lvec.y, 0.0f };
    rdVector3 trackXY = { trackDir.x, trackDir.y, 0.0f };
    rdVector_Normalize3Acc(&trackXY);
    rdVector_Normalize3Acc(&lookXY);
    float yaw = rdMath_DeltaAngleNormalized(&lookXY, &trackXY, &rdroid_g_zVector3);

    // Note, the original calculates the horizontal lengths after normalizing the vectors, so the lengths are always 1 (or 0)
    rdVector3 lookPitch = { rdVector_Len3(&lookXY), 0.0f, pThing->orient.lvec.z };
    float trackLenXY    = rdVector_Len3(&trackXY);
    rdVector_Normalize3Acc(&lookPitch);

    rdVector3 trackPitch = { trackLenXY, 0.0f, trackDir.z };
    rdVector_Normalize3Acc(&trackPitch);

    const rdVector3 yAxis = { 0.0f, 1.0f, 0.0f };
    float pitch = rdMath_DeltaAngleNormalized(&trackPitch, &lookPitch, &yAxis);

    float yawVel = yaw / secDeltaTime;
    pThing->moveInfo.physics.angularVelocity.y = yawVel;
    pThing->moveInfo.physics.angularVelocity.x = pitch / secDeltaTime;

    switch ( pState->turnState )
    {
        case SITHMINECAR_TURN_NONE:
            if ( yawVel > 20.0f )
            {
                pState->turnState = SITHMINECAR_TURN_LEFT_START;
            }
            else if ( yawVel < -20.0f )
            {
                pState->turnState = SITHMINECAR_TURN_RIGHT_START;
            }
            break;

        case SITHMINECAR_TURN_LEFT_START:
            pState->turnState = SITHMINECAR_TURN_LEFT;
            break;

        case SITHMINECAR_TURN_LEFT:
            if ( yawVel < 20.0f )
            {
                pState->turnState = SITHMINECAR_TURN_END;
            }
            break;

        case SITHMINECAR_TURN_RIGHT_START:
            pState->turnState = SITHMINECAR_TURN_RIGHT;
            break;

        case SITHMINECAR_TURN_RIGHT:
            if ( yawVel > -20.0f )
            {
                pState->turnState = SITHMINECAR_TURN_END;
            }
            break;

        default:
            pState->turnState = SITHMINECAR_TURN_NONE;
            break;
    }
    return;

error:
    memset(&pThing->moveInfo.physics.angularVelocity, 0, sizeof(pThing->moveInfo.physics.angularVelocity));
    memset(&pThing->moveInfo.physics.velocity, 0, sizeof(pThing->moveInfo.physics.velocity));
    memset(&pThing->moveInfo.physics.thrust, 0, sizeof(pThing->moveInfo.physics.thrust));
    memset(pOutPosDelta, 0, sizeof(*pOutPosDelta));
}

// Sets the vertex flags of the vertex in aIdxs with smaller distance to newFlags
static void sithPhysics_FixTrackCorner(SithTrackFaceInfo* pInfo, const size_t* aIdxs, const float* aDots, int newFlags)
{
    if ( fabsf(aDots[aIdxs[1]]) <= fabsf(aDots[aIdxs[0]]) )
    {
        pInfo->aVertFlags[aIdxs[1]] = newFlags;
    }
    else
    {
        pInfo->aVertFlags[aIdxs[0]] = newFlags;
    }
}

static float sithPhysics_Sign(float value)
{
    return (float)(value >= 0.0f ? 1 : -1);
}

// Gets the geometry of track face relative to moving direction pDir and position pPos.
// Track face is quad or triangle (e.g. at the track switch), the face corners are classified as front/back left/right.
int J3DAPI sithPhysics_ProcessTrackFace(SithThing* pThing, const rdFace* pFace, const rdVector3* pDir, const rdVector3* pRight, const rdVector3* pPos, SithTrackFaceInfo* pInfo)
{
    rdVector3 dir   = *pDir;
    rdVector3 right = *pRight;

    for ( size_t i = 0; i < 4; ++i )
    {
        pInfo->aFwdDots[i]   = 0.0f;
        pInfo->aSideDots[i]  = 0.0f;
        pInfo->aVertFlags[i] = 0;
    }

    pInfo->nVertices = 0;

    memset(&pInfo->vertCenter, 0, sizeof(pInfo->vertCenter));

    // Collect the corner vertices, vertices which lie on straight edge are skipped
    const rdVector3* aWorldVerts = sithWorld_g_pCurrentWorld->aVertices;
    for ( size_t i = 0; i < pFace->numVertices; ++i )
    {
        size_t prev = i == 0 ? pFace->numVertices - 1 : i - 1;
        size_t next = (i + 1) % pFace->numVertices;

        const rdVector3* pVert = &aWorldVerts[pFace->aVertices[i]];

        rdVector3 nextEdge, prevEdge;
        rdVector_Sub3(&nextEdge, &aWorldVerts[pFace->aVertices[next]], pVert);
        rdVector_Sub3(&prevEdge, pVert, &aWorldVerts[pFace->aVertices[prev]]);
        rdVector_Normalize3Acc(&nextEdge);
        rdVector_Normalize3Acc(&prevEdge);

        float angle = stdMath_NormalizeAngle(rdMath_DeltaAngleNormalized(&nextEdge, &prevEdge, &pFace->normal));
        if ( angle >= 5.0f && angle <= 355.0f )
        {
            if ( pInfo->nVertices > 3 )
            {
                SITHLOG_ERROR("Too many vertices on track face.\n");
                return 0;
            }

            pInfo->aVertices[pInfo->nVertices] = *pVert;
            rdVector_Add3Acc(&pInfo->vertCenter, &pInfo->aVertices[pInfo->nVertices]);
            pInfo->nVertices++;
        }
    }

    SITH_ASSERTREL((pInfo->nVertices == 3) || (pInfo->nVertices == 4));

    rdVector_Scale3Acc(&pInfo->vertCenter, 1.0f / (float)pInfo->nVertices);

    // For quad get the track direction from the edges
    if ( pInfo->nVertices == 4 )
    {
        for ( size_t i = 0; i < pInfo->nVertices; ++i )
        {
            rdVector_Sub3(&pInfo->aEdges[i], &pInfo->aVertices[(i + 1) % pInfo->nVertices], &pInfo->aVertices[i]);
            rdVector_Normalize3Acc(&pInfo->aEdges[i]);
            pInfo->aEdgeFwdDots[i]  = rdVector_Dot3(&pInfo->aEdges[i], pDir);
            pInfo->aEdgeSideDots[i] = rdVector_Dot3(&pInfo->aEdges[i], pRight);
        }

        size_t fwdEdge0, fwdEdge1, sideEdge0, sideEdge1;
        if ( fabsf(pInfo->aEdgeFwdDots[0]) <= fabsf(pInfo->aEdgeFwdDots[1]) )
        {
            fwdEdge0  = 1;
            fwdEdge1  = 3;
            sideEdge0 = 0;
            sideEdge1 = 2;
        }
        else
        {
            fwdEdge0  = 0;
            fwdEdge1  = 2;
            sideEdge0 = 1;
            sideEdge1 = 3;
        }

        // Sum the edges along the track pointed in the same direction
        float s0 = sithPhysics_Sign(pInfo->aEdgeFwdDots[fwdEdge0]);
        float s1 = sithPhysics_Sign(pInfo->aEdgeFwdDots[fwdEdge1]);
        rdVector3 fwd;
        fwd.x = s1 * pInfo->aEdges[fwdEdge1].x + s0 * pInfo->aEdges[fwdEdge0].x;
        fwd.y = s1 * pInfo->aEdges[fwdEdge1].y + s0 * pInfo->aEdges[fwdEdge0].y;
        fwd.z = s1 * pInfo->aEdges[fwdEdge1].z + s0 * pInfo->aEdges[fwdEdge0].z;

        s0 = sithPhysics_Sign(pInfo->aEdgeSideDots[sideEdge0]);
        s1 = sithPhysics_Sign(pInfo->aEdgeSideDots[sideEdge1]);
        rdVector3 side;
        side.x = s1 * pInfo->aEdges[sideEdge1].x + s0 * pInfo->aEdges[sideEdge0].x;
        side.y = s1 * pInfo->aEdges[sideEdge1].y + s0 * pInfo->aEdges[sideEdge0].y;
        side.z = s1 * pInfo->aEdges[sideEdge1].z + s0 * pInfo->aEdges[sideEdge0].z;

        rdVector_Normalize3Acc(&fwd);
        rdVector_Normalize3Acc(&side);

        rdVector_Cross3(&dir, &rdroid_g_zVector3, &side);
        rdVector_Cross3(&right, &fwd, &rdroid_g_zVector3);
    }

    // Classify vertices
    for ( size_t i = 0; i < pInfo->nVertices; ++i )
    {
        rdVector3 vec;
        rdVector_Sub3(&vec, &pInfo->aVertices[i], &pInfo->vertCenter);
        pInfo->aFwdDots[i]  = rdVector_Dot3(&vec, &dir);
        pInfo->aSideDots[i] = rdVector_Dot3(&vec, &right);
    }

    for ( size_t i = 0; i < pInfo->nVertices; ++i )
    {
        pInfo->aVertFlags[i] |= pInfo->aSideDots[i] > 0.0f ? SITHTRACK_VERTEX_RIGHT : SITHTRACK_VERTEX_LEFT;
        pInfo->aVertFlags[i] |= pInfo->aFwdDots[i] > 0.0f ? SITHTRACK_VERTEX_FRONT : SITHTRACK_VERTEX_BACK;
    }

    size_t aFrontRights[2] = { 0 }, aFrontLefts[2] = { 0 }, aBackRights[2] = { 0 }, aBackLefts[2] = { 0 };
    size_t nFrontRights = 0, nFrontLefts = 0, nBackRights = 0, nBackLefts = 0;
    for ( size_t i = 0; i < pInfo->nVertices; ++i )
    {
        int flags = pInfo->aVertFlags[i];
        if ( (flags & SITHTRACK_VERTEX_FRONT) != 0 && (flags & SITHTRACK_VERTEX_RIGHT) != 0 )
        {
            SITH_ASSERTREL(nFrontRights < 2);
            if ( nFrontRights < 2 ) // Added: Bounds check
            {
                aFrontRights[nFrontRights] = i;
            }
            nFrontRights++;
        }
        else if ( (flags & SITHTRACK_VERTEX_FRONT) != 0 && (flags & SITHTRACK_VERTEX_LEFT) != 0 )
        {
            SITH_ASSERTREL(nFrontLefts < 2);
            if ( nFrontLefts < 2 ) // Added: Bounds check
            {
                aFrontLefts[nFrontLefts] = i;
            }
            nFrontLefts++;
        }
        else if ( (flags & SITHTRACK_VERTEX_BACK) != 0 && (flags & SITHTRACK_VERTEX_RIGHT) != 0 )
        {
            SITH_ASSERTREL(nBackRights < 2);
            if ( nBackRights < 2 ) // Added: Bounds check
            {
                aBackRights[nBackRights] = i;
            }
            nBackRights++;
        }
        else if ( (flags & SITHTRACK_VERTEX_BACK) != 0 && (flags & SITHTRACK_VERTEX_LEFT) != 0 )
        {
            SITH_ASSERTREL(nBackLefts < 2);
            if ( nBackLefts < 2 ) // Added: Bounds check
            {
                aBackLefts[nBackLefts] = i;
            }
            nBackLefts++;
        }
    }

    // When there are 2 vertices in the same corner, move one of them to the missing corner
    if ( nFrontRights == 2 )
    {
        if ( nFrontLefts == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aFrontRights, pInfo->aSideDots, SITHTRACK_VERTEX_FRONT | SITHTRACK_VERTEX_LEFT);
        }
        else if ( nBackRights == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aFrontRights, pInfo->aFwdDots, SITHTRACK_VERTEX_BACK | SITHTRACK_VERTEX_RIGHT);
        }
        else
        {
            SITH_ASSERTREL(0);
        }
    }
    else if ( nFrontLefts == 2 )
    {
        if ( nFrontRights == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aFrontLefts, pInfo->aSideDots, SITHTRACK_VERTEX_FRONT | SITHTRACK_VERTEX_RIGHT);
        }
        else if ( nBackLefts == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aFrontLefts, pInfo->aFwdDots, SITHTRACK_VERTEX_BACK | SITHTRACK_VERTEX_LEFT);
        }
        else
        {
            SITH_ASSERTREL(0);
        }
    }

    if ( nBackRights == 2 )
    {
        if ( nBackLefts == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aBackRights, pInfo->aSideDots, SITHTRACK_VERTEX_BACK | SITHTRACK_VERTEX_LEFT);
        }
        else if ( nFrontRights == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aBackRights, pInfo->aFwdDots, SITHTRACK_VERTEX_FRONT | SITHTRACK_VERTEX_RIGHT);
        }
        else
        {
            SITH_ASSERTREL(0);
        }
    }
    else if ( nBackLefts == 2 )
    {
        if ( nBackRights == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aBackLefts, pInfo->aSideDots, SITHTRACK_VERTEX_BACK | SITHTRACK_VERTEX_RIGHT);
        }
        else if ( nFrontLefts == 0 )
        {
            sithPhysics_FixTrackCorner(pInfo, aBackLefts, pInfo->aFwdDots, SITHTRACK_VERTEX_FRONT | SITHTRACK_VERTEX_LEFT);
        }
        else
        {
            SITH_ASSERTREL(0);
        }
    }

    for ( size_t i = 0; i < pInfo->nVertices; ++i )
    {
        int flags = pInfo->aVertFlags[i];
        if ( (flags & SITHTRACK_VERTEX_FRONT) != 0 && (flags & SITHTRACK_VERTEX_RIGHT) != 0 )
        {
            pInfo->frontRight = pInfo->aVertices[i];
        }
        else if ( (flags & SITHTRACK_VERTEX_FRONT) != 0 && (flags & SITHTRACK_VERTEX_LEFT) != 0 )
        {
            pInfo->frontLeft = pInfo->aVertices[i];
        }
        else if ( (flags & SITHTRACK_VERTEX_BACK) != 0 && (flags & SITHTRACK_VERTEX_RIGHT) != 0 )
        {
            pInfo->backRight = pInfo->aVertices[i];
        }
        else if ( (flags & SITHTRACK_VERTEX_BACK) != 0 && (flags & SITHTRACK_VERTEX_LEFT) != 0 )
        {
            pInfo->backLeft = pInfo->aVertices[i];
        }
    }

    // Triangle has one corner missing, use the vertex of the adjacent corner
    if ( pInfo->nVertices == 3 )
    {
        if ( nBackLefts == 0 )
        {
            pInfo->backLeft = pInfo->frontLeft;
        }
        else if ( nFrontLefts == 0 )
        {
            pInfo->frontLeft = pInfo->backLeft;
        }
        else if ( nBackRights == 0 )
        {
            pInfo->backRight = pInfo->frontRight;
        }
        else if ( nFrontRights == 0 )
        {
            pInfo->frontRight = pInfo->backRight;
        }
    }

    rdVector_Add3(&pInfo->back, &pInfo->backLeft, &pInfo->backRight);
    rdVector_Scale3Acc(&pInfo->back, 0.5f);

    rdVector_Add3(&pInfo->front, &pInfo->frontLeft, &pInfo->frontRight);
    rdVector_Scale3Acc(&pInfo->front, 0.5f);

    if ( pInfo->nVertices == 3 )
    {
        // If there's no track in front of the triangle, the triangle ends with a corner (e.g. track switch);
        // move the front to the center of the side with both vertices.
        rdVector3 probeDir;
        rdVector_Sub3(&probeDir, &pInfo->front, &pInfo->back);
        rdVector_Normalize3Acc(&probeDir);

        rdVector3 probePos;
        rdVector_ScaleAdd3(&probePos, &probeDir, 0.025f, &pInfo->front);

        rdFace* pFrontFace;
        if ( !sithPhysics_CheckForPointOnTrack(pThing, &probePos, &pFrontFace, 0, pFace) )
        {
            rdVector3 leftEdge;
            rdVector_Sub3(&leftEdge, &pInfo->backLeft, &pInfo->frontLeft);
            if ( sithPhysics_IsZeroVector(&leftEdge) )
            {
                rdVector_Add3(&pInfo->front, &pInfo->backRight, &pInfo->frontRight);
            }
            else
            {
                rdVector_Add3(&pInfo->front, &pInfo->backLeft, &pInfo->frontLeft);
            }

            rdVector_Scale3Acc(&pInfo->front, 0.5f);
        }
    }

    rdVector_Add3(&pInfo->center, &pInfo->front, &pInfo->back);
    rdVector_Scale3Acc(&pInfo->center, 0.5f);

    rdVector_Sub3(&pInfo->dir, &pInfo->front, &pInfo->back);
    rdVector_Normalize3Acc(&pInfo->dir);

    rdVector_Cross3(&pInfo->right, &pInfo->dir, &rdroid_g_zVector3);
    rdVector_Normalize3Acc(&pInfo->right);

    rdVector3 vec;
    rdVector_Sub3(&vec, pPos, &pInfo->front);
    pInfo->distToFront = -rdVector_Dot3(&vec, &pInfo->dir);

    rdVector_Sub3(&vec, pPos, &pInfo->back);
    pInfo->distFromBack = rdVector_Dot3(&vec, &pInfo->dir);
    return 1;
}

// Searches for track surface under the point. If found and bUpdateState is set, mine car state bOnAdjoinTrack is updated.
int J3DAPI sithPhysics_CheckForPointOnTrack(SithThing* pThing, const rdVector3* pPoint, rdFace** ppOutFoundFace, int bUpdateState, const rdFace* pPrevFace)
{
    rdVector3 startPos;
    rdVector_ScaleAdd3(&startPos, &rdroid_g_zVector3, 0.2f, pPoint);

    rdVector3 dir = { 0.0f, 0.0f, -1.0f };

    SithMineCarState* pState = &pThing->userblock.pMinecar->state;
    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    SithSurface* pTrackSurface = NULL;
    sithCollision_SearchForCollisions(pSector, NULL, &startPos, &dir, 0.5f, 0.001f, 1);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & (SITHCOLLISION_WORLD | SITHCOLLISION_ADJOINCROSS)) != 0
            && pCollision->pSurfaceCollided
            && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_TRACK) != 0 )
        {
            pTrackSurface = pCollision->pSurfaceCollided;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();

    if ( pTrackSurface && ppOutFoundFace )
    {
        if ( pPrevFace && pPrevFace == &pTrackSurface->face )
        {
            SITHLOG_ERROR("Holy Smokes! CheckForPointOnTrack found prev face.\n");
        }

        *ppOutFoundFace = &pTrackSurface->face;
        if ( bUpdateState )
        {
            pState->bOnAdjoinTrack = pTrackSurface->pAdjoin != NULL;
        }
    }

    return pTrackSurface != NULL;
}

// Moves point pPos along the track face for distance speed * secDeltaTime. If the front of face is reached,
// pOutPos is set to the face front and the remaining time is returned in pSecDeltaTime, otherwise pSecDeltaTime is set to 0.
void J3DAPI sithPhysics_MoveAlongTrackFace(SithThing* pThing, const SithTrackFaceInfo* pInfo, float* pSecDeltaTime, const rdVector3* pPos, rdVector3* pOutPos, float speed)
{
    J3D_UNUSED(pThing);
    J3D_UNUSED(pPos);

    float moveDist  = speed * *pSecDeltaTime;
    float overshoot = moveDist - pInfo->distToFront;
    if ( overshoot <= 0.0f )
    {
        rdVector_ScaleAdd3(pOutPos, &pInfo->dir, overshoot, &pInfo->front);
        *pSecDeltaTime = 0.0f;
    }
    else
    {
        *pOutPos = pInfo->front;
        *pSecDeltaTime = *pSecDeltaTime - (pInfo->distToFront / moveDist) * *pSecDeltaTime;
    }
}

// Jeep move statuses
#define SITHJEEP_MOVE_DRIVING  SITHPLAYERMOVE_JEEP_STILL // wheels on ground
#define SITHJEEP_MOVE_AIRBORNE SITHPLAYERMOVE_UNKNOWN_82
#define SITHJEEP_MOVE_ROLLOVER SITHPLAYERMOVE_UNKNOWN_83
#define SITHJEEP_MOVE_CRASHED  SITHPLAYERMOVE_UNKNOWN_84
#define SITHJEEP_MOVE_DEAD     SITHPLAYERMOVE_JEEP_IMPACT

// Jeep sound class modes
#define SITHJEEP_SOUND_GRAVEL (SithSoundClassMode)7
#define SITHJEEP_SOUND_ENGINE (SithSoundClassMode)10
#define SITHJEEP_SOUND_SKID   (SithSoundClassMode)11

static bool sithPhysics_IsJeepWrecked(const SithThing* pThing)
{
    return pThing->moveStatus == SITHJEEP_MOVE_ROLLOVER || pThing->moveStatus == SITHJEEP_MOVE_CRASHED || pThing->moveStatus == SITHJEEP_MOVE_DEAD;
}

void J3DAPI sithPhysics_UpdateJeepPhysics(SithThing* pThing, float secDeltaTime)
{
    if ( pThing->moveStatus == SITHPLAYERMOVE_JEEP_BOARDING || pThing->moveStatus == SITHPLAYERMOVE_JEEP_UNBOARDING || !sithPhysics_CreateJeepUserBlock(pThing) )
    {
        return;
    }

    SithJeepUserBlock* pJeep = pThing->userblock.pJeep;
    SithJeepState* pState    = &pJeep->state;
    rdVector3* pVelocity     = &pThing->moveInfo.physics.velocity;
    rdVector3 deltaVel       = { 0 };

    pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_200000;
    pState->bSkidding = 0;

    // Get how much the jeep is moving sideways
    float forwardFactor = 1.0f;
    if ( !sithPhysics_IsZeroVector(pVelocity) && pThing->moveStatus != SITHJEEP_MOVE_AIRBORNE && pThing->moveStatus != SITHJEEP_MOVE_ROLLOVER )
    {
        rdVector3 moveDir;
        rdVector_Normalize3(&moveDir, pVelocity);
        sithPhysics_ProjectOnPlaneNormalize(&moveDir, &moveDir, &pThing->orient.uvec);

        forwardFactor = fabsf(rdVector_Dot3(&pThing->orient.lvec, &moveDir));
        forwardFactor = STDMATH_CLAMP(forwardFactor, 0.5f, 1.0f);
        if ( forwardFactor < 0.997f && rdVector_Len3(pVelocity) > 1.1f )
        {
            pState->bSkidding = 1;
        }
    }

    rdVector3 groundNormal;
    if ( !sithPhysics_IsJeepWrecked(pThing) )
    {
        sithPhysics_UpdateJeepWheels(pThing, pState, secDeltaTime);
        sithPhysics_UpdateJeepBody(pThing, pState, &groundNormal, secDeltaTime);
        if ( !pState->bAirborne )
        {
            pThing->moveStatus = SITHJEEP_MOVE_DRIVING;
            if ( pVelocity->z < -1.65f && (pThing->thingInfo.actorInfo.flags & SITH_AF_INVULNERABLE) == 0 )
            {
                // Hit the ground too hard
                pThing->moveStatus = SITHJEEP_MOVE_DEAD;
                sithActor_KillActor(pThing, pThing, SITH_DAMAGE_IMPACT);
            }
        }
        else
        {
            pThing->moveStatus = SITHJEEP_MOVE_AIRBORNE;
        }
    }
    else
    {
        // Added: Init; originally uninitialized but not used in this case
        groundNormal = rdroid_g_zeroVector3;
    }

    if ( sithPhysics_IsZeroVector(&groundNormal) )
    {
        groundNormal = rdroid_g_zVector3;
    }

    if ( pThing->moveStatus == SITHJEEP_MOVE_DRIVING )
    {
        sithPhysics_SetThingLook(pThing, &groundNormal, pThing->moveInfo.physics.orientSpeed * secDeltaTime);
    }
    else if ( pThing->moveStatus == SITHJEEP_MOVE_ROLLOVER )
    {
        // Flip over onto the roof and get wrecked
        rdVector_ScaleAdd3Acc(pVelocity, &pThing->orient.uvec, 2.0f * secDeltaTime);

        rdVector3 down;
        rdVector_Neg3(&down, &rdroid_g_zVector3);
        sithPhysics_SetThingLook(pThing, &down, 0.3f * secDeltaTime);

        if ( fabsf(1.0f - rdVector_Dot3(&pThing->orient.uvec, &down)) < 0.005f )
        {
            pThing->moveStatus = SITHJEEP_MOVE_DEAD;
            sithActor_KillActor(pThing, pThing, SITH_DAMAGE_IMPACT);
        }
    }

    // Check if jeep has rolled over
    if ( !sithPhysics_IsJeepWrecked(pThing) )
    {
        float sideTilt = fabsf(rdVector_Dot3(&pThing->orient.rvec, &rdroid_g_zVector3));
        if ( (sideTilt > 0.86f || pThing->orient.uvec.z <= 0.05f) && (pThing->thingInfo.actorInfo.flags & SITH_AF_INVULNERABLE) == 0 )
        {
            pThing->moveStatus = SITHJEEP_MOVE_ROLLOVER;
            pThing->collide.movesize = 0.02f;
            memset(&pThing->moveInfo.physics.rotThrust, 0, sizeof(pThing->moveInfo.physics.rotThrust));
            memset(&pThing->moveInfo.physics.thrust, 0, sizeof(pThing->moveInfo.physics.thrust));
        }
    }

    // Update steering
    rdVector3* pAngularVel = &pThing->moveInfo.physics.angularVelocity;
    if ( (pThing->moveInfo.physics.flags & SITH_PF_USEANGULARTHRUST) != 0 && pThing->moveStatus != SITHJEEP_MOVE_AIRBORNE )
    {
        if ( !sithPhysics_IsZeroVector(pAngularVel) )
        {
            sithPhysics_ApplyDrag(pAngularVel, pThing->moveInfo.physics.surfDrag, 0.0f, secDeltaTime);
        }

        pAngularVel->y = pThing->moveInfo.physics.rotThrust.y * secDeltaTime + pAngularVel->y;

        float maxRotVel = pThing->moveInfo.physics.maxRotationVelocity;
        sithPhysics_ClampVector(pAngularVel, -maxRotVel, maxRotVel);
        sithPhysics_ClipVectorNearZero(pAngularVel);
    }

    if ( pAngularVel->y != 0.0f )
    {
        rdVector3 deltaRot;
        rdVector_Scale3(&deltaRot, pAngularVel, secDeltaTime);

        rdMatrix34 deltaRotMat;
        rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
        sithCollision_RotateThing(pThing, &deltaRotMat);

        if ( !pState->bSkidding && pThing->moveStatus != SITHJEEP_MOVE_AIRBORNE && pThing->moveStatus != SITHJEEP_MOVE_ROLLOVER )
        {
            // Turn the velocity with the jeep, the faster jeep moves the less velocity can be turned
            float speed = rdVector_Len3(pVelocity);
            float maxTurnRate;
            if ( speed <= 0.85f )
            {
                maxTurnRate = 140.0f + 140.0f;
            }
            else
            {
                float range, rateFrom, rateTo, speedFrom;
                float maxVelocity = pThing->moveInfo.physics.maxVelocity;
                if ( speed <= maxVelocity )
                {
                    range     = maxVelocity - 0.85f;
                    rateTo    = 50.0f;
                    speedFrom = 0.0f;
                    rateFrom  = 140.0f;
                }
                else
                {
                    range     = 2.0f - maxVelocity;
                    rateTo    = 1.0f;
                    speedFrom = maxVelocity;
                    rateFrom  = 50.0f;
                }

                float x = STDMATH_CLAMP(speed - speedFrom, 0.0f, range);
                maxTurnRate = ((rateTo - rateFrom) / range) * x + rateFrom;
            }

            float maxTurn = maxTurnRate * secDeltaTime;
            sithPhysics_ClampVector(&deltaRot, -maxTurn, maxTurn);

            rdMatrix_BuildRotate34(&deltaRotMat, &deltaRot);
            rdMatrix_TransformVector34Acc(pVelocity, &deltaRotMat);
        }

        rdMatrix_Normalize34(&pThing->orient);
    }

    // Apply drag
    if ( !sithPhysics_IsZeroVector(pVelocity) && pThing->moveInfo.physics.surfDrag != 0.0f )
    {
        if ( (pThing->moveInfo.physics.flags & SITH_PF_UNKNOWN_8000) == 0 )
        {
            float drag, staticDrag;
            if ( pThing->moveStatus == SITHJEEP_MOVE_AIRBORNE )
            {
                staticDrag = 0.0f;
                drag       = 0.6f;
            }
            else
            {
                staticDrag = sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust) ? pThing->moveInfo.physics.staticDrag : 0.0f;
                drag       = pThing->moveInfo.physics.surfDrag;
            }

            sithPhysics_ApplyDrag(pVelocity, drag, staticDrag, secDeltaTime);
        }
        else
        {
            pThing->moveInfo.physics.flags &= ~SITH_PF_UNKNOWN_8000;
        }
    }

    // Apply thrust, the forward thrust is applied along the ground
    if ( (pThing->moveInfo.physics.flags & SITH_PF_USETHRUST) != 0
        && !sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust)
        && pThing->moveStatus != SITHJEEP_MOVE_AIRBORNE && !sithPhysics_IsJeepWrecked(pThing) )
    {
        float thrustFactor = secDeltaTime * forwardFactor;

        rdVector3 thrust;
        thrust.x = pThing->moveInfo.physics.thrust.x;
        thrust.y = 0.0f;
        thrust.z = pThing->moveInfo.physics.thrust.z;

        rdVector3 groundThrust;
        rdVector_Scale3(&groundThrust, &pState->normal, pThing->moveInfo.physics.thrust.y);

        if ( thrust.x != 0.0f || thrust.z != 0.0f )
        {
            rdMatrix_TransformVector34Acc(&thrust, &pThing->orient);
        }

        deltaVel.x = (thrust.x + groundThrust.x) * thrustFactor;
        deltaVel.y = (thrust.y + groundThrust.y) * thrustFactor;
        deltaVel.z = (thrust.z + groundThrust.z) * thrustFactor;
        sithPhysics_ClipVectorNearZero(&deltaVel);
    }

    // Slope handling
    float groundUpDot;
    if ( pThing->moveStatus == SITHJEEP_MOVE_DRIVING && (groundUpDot = rdVector_Dot3(&groundNormal, &rdroid_g_zVector3)) < 0.927f )
    {
        rdVector3 side, downSlope;
        rdVector_Cross3(&side, &groundNormal, &rdroid_g_zVector3);
        rdVector_Cross3(&downSlope, &groundNormal, &side);
        rdVector_Normalize3Acc(&downSlope);

        if ( groundUpDot < 0.6946f )
        {
            // Too steep, slide down
            float thrustScale = ((sithPhysics_flt_585410 - 0.7f) / (0.6946f - 0.5f)) * (0.6946f - groundUpDot) + 0.7f;
            thrustScale = STDMATH_CLAMP(thrustScale, 0.6f, 1.0f);

            float slideScale = ((1.0f - 0.6f) / (0.6946f - 0.5f)) * (0.6946f - groundUpDot) + 0.6f;
            slideScale = STDMATH_CLAMP(slideScale, 0.6f, 1.0f) * secDeltaTime;

            deltaVel.x = slideScale * downSlope.x + thrustScale * deltaVel.x;
            deltaVel.y = slideScale * downSlope.y + thrustScale * deltaVel.y;
            deltaVel.z = slideScale * downSlope.z + thrustScale * deltaVel.z;
        }

        float forwardVel  = stdMath_ClipNearZero(rdVector_Dot3(&pThing->orient.lvec, pVelocity));
        float slopeAlign  = stdMath_ClipNearZero(rdVector_Dot3(&pThing->orient.lvec, &downSlope));
        float slopeVel    = stdMath_ClipNearZero(forwardVel * slopeAlign);

        float slopeFactor = 0.0f;
        if ( pThing->moveInfo.physics.thrust.y != 0.0f && slopeVel != 0.0f )
        {
            slopeFactor = 2.0f;
            if ( slopeVel <= 0.0f )
            {
                slopeFactor = secDeltaTime;
                if ( slopeVel < 0.0f )
                {
                    slopeFactor = -(slopeVel / secDeltaTime);
                    if ( slopeFactor > 2.5f )
                    {
                        slopeFactor = 2.5f;
                    }
                }
            }
        }

        rdVector_ScaleAdd3Acc(&deltaVel, &pThing->orient.lvec, (1.25f - groundUpDot) * slopeFactor * secDeltaTime * slopeAlign);
    }

    rdVector_Add3Acc(pVelocity, &deltaVel);
    sithPhysics_ClipVectorNearZero(pVelocity);

    // Apply gravity
    if ( pThing->moveStatus == SITHJEEP_MOVE_AIRBORNE )
    {
        float gravity = 2.2f;
        if ( pVelocity->z >= 0.0f && pThing->moveInfo.physics.maxVelocity * 0.95f < rdVector_Len3(pVelocity) )
        {
            gravity = 1.7f;
        }

        pVelocity->z = pVelocity->z - gravity * secDeltaTime;
    }
    else if ( sithPhysics_IsJeepWrecked(pThing) )
    {
        pVelocity->z = pVelocity->z - 1.0f * secDeltaTime;
    }
    else if ( pState->numGroundedWheels > 2 && !sithPhysics_IsZeroVector(pVelocity) )
    {
        // Don't drive into the ground
        float normalVel = stdMath_ClipNearZero(rdVector_Dot3(pVelocity, &groundNormal));
        if ( normalVel < 0.0f )
        {
            rdVector_ScaleAdd3Acc(pVelocity, &groundNormal, -normalVel);
        }
    }

    sithPhysics_ClipVectorNearZero(pVelocity);
    if ( rdVector_Len3(pVelocity) < 0.005f && sithPhysics_IsZeroVector(&pThing->moveInfo.physics.thrust) )
    {
        memset(pVelocity, 0, sizeof(*pVelocity));
    }

    if ( !sithPhysics_IsZeroVector(pVelocity) )
    {
        rdVector_Scale3(&pThing->moveInfo.physics.deltaVelocity, pVelocity, secDeltaTime);
    }

    // Move jeep body to its position above the wheels
    if ( !sithPhysics_IsJeepWrecked(pThing) )
    {
        rdVector3 bodyDelta;
        rdVector_Sub3(&bodyDelta, &pState->targetPos, &pThing->pos);
        sithPhysics_ClipVectorNearZero(&bodyDelta);

        if ( rdVector_Len3(&bodyDelta) > 0.001f )
        {
            rdVector_Add3Acc(&pThing->moveInfo.physics.deltaVelocity, &bodyDelta);

            // When going over a bump, move the jeep up
            if ( pState->numGroundedWheels > 1 && bodyDelta.z < 0.0f && pVelocity->z > 0.0f
                && rdVector_Len3(pVelocity) < pThing->moveInfo.physics.maxVelocity * 0.95f )
            {
                pVelocity->z = (bodyDelta.z / secDeltaTime) * 0.75f + pVelocity->z;
            }
        }
    }

    sithPhysics_UpdateJeepFx(pThing, &pJeep->fxstate, secDeltaTime);
    sithPhysics_UpdateExhaustFx(pThing, &pJeep->exhaustInfo, secDeltaTime);
}

// Drops the wheels of axle or side that are both in the air towards the ground, i.e. suspension extends
static void sithPhysics_ExtendJeepWheel(const SithThing* pThing, SithJeepWheel* pWheel, float maxDrop)
{
    float drop = pWheel->groundDistance <= maxDrop ? pWheel->groundDistance : maxDrop;
    float upZ  = fabsf(pThing->orient.uvec.z);
    rdVector_ScaleAdd3Acc(&pWheel->contactPos, &pThing->orient.uvec, -(drop * upZ));
    pWheel->groundDistance = pWheel->groundDistance - drop * upZ;
}

// Pushes the wheel up the wall it's blocked by and bounces jeep off the wall. Returns impact strength (negative) or minImpact if there's no impact.
static float sithPhysics_BlockJeepWheel(SithThing* pThing, SithJeepWheel* pWheel, float maxPush, float minImpact)
{
    rdVector3 side, upWall;
    rdVector_Cross3(&side, &rdroid_g_zVector3, &pWheel->normal);
    rdVector_Cross3(&upWall, &side, &pWheel->normal);

    float push = -upWall.z * maxPush;
    if ( pWheel->suspensionDistance <= push )
    {
        push = pWheel->suspensionDistance;
    }
    rdVector_ScaleAdd3Acc(&pWheel->contactPos, &upWall, push);

    rdVector3 wallNormal;
    sithPhysics_ProjectOnPlaneNormalize(&wallNormal, &pWheel->normal, &pThing->orient.uvec);

    float hitVel = rdVector_Dot3(&pThing->moveInfo.physics.velocity, &wallNormal);
    if ( hitVel < 0.0f )
    {
        float impact = (0.7f - pWheel->normal.z) * hitVel * 1.0f;
        rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.velocity, &wallNormal, -impact);
        if ( impact < minImpact )
        {
            return impact;
        }
    }

    return minImpact;
}

static void sithPhysics_AddJeepWheelNormal(const SithThing* pThing, SithJeepState* pState, const SithJeepWheel* pWheel)
{
    if ( pWheel->state != SITHJEEP_WHEEL_GROUNDED && pWheel->state != SITHJEEP_WHEEL_COMPRESSED )
    {
        return;
    }

    rdVector3 side, forward;
    rdVector_Cross3(&side, &pThing->orient.lvec, &pWheel->normal);
    rdVector_Cross3(&forward, &pWheel->normal, &side);
    rdVector_Add3Acc(&pState->normal, &forward);
    pState->numGroundedWheels++;
}

// Probes the ground under 4 jeep wheels and updates the wheel contact positions
int J3DAPI sithPhysics_UpdateJeepWheels(SithThing* pThing, SithJeepState* pState, float secDeltaTime)
{
    pState->numGroundedWheels = 0;
    memset(&pState->normal, 0, sizeof(pState->normal));

    float halfWidth  = pState->size.x * 0.5f;
    float halfLength = pState->size.y * 0.5f;
    float wheelZ     = -(pState->size.z * 0.5f);

    static const struct { SithJeepWheelIndex wheel; float signX; float signY; } aProbes[4] = {
        { SITHJEEP_WHEEL_FRONTLEFT, -1.0f, 1.0f },
        { SITHJEEP_WHEEL_FRONTRIGHT, 1.0f, 1.0f },
        { SITHJEEP_WHEEL_BACKLEFT, -1.0f, -1.0f },
        { SITHJEEP_WHEEL_BACKRIGHT, 1.0f, -1.0f },
    };

    for ( size_t i = 0; i < STD_ARRAYLEN(aProbes); ++i )
    {
        rdVector3 probePos;
        probePos.x = aProbes[i].signX * halfWidth;
        probePos.y = aProbes[i].signY * halfLength;
        probePos.z = wheelZ;
        rdMatrix_TransformPoint34Acc(&probePos, &pThing->orient);
        rdVector_Add3Acc(&probePos, &pThing->pos);
        sithPhysics_UpdateJeepWheel(pThing, &pState->aWheels[aProbes[i].wheel], &probePos);
    }

    SithJeepWheel* pFL = &pState->aWheels[SITHJEEP_WHEEL_FRONTLEFT];
    SithJeepWheel* pBL = &pState->aWheels[SITHJEEP_WHEEL_BACKLEFT];
    SithJeepWheel* pBR = &pState->aWheels[SITHJEEP_WHEEL_BACKRIGHT];
    SithJeepWheel* pFR = &pState->aWheels[SITHJEEP_WHEEL_FRONTRIGHT];

    size_t numInAir = 0, numBlocked = 0;
    for ( size_t i = 0; i < STD_ARRAYLEN(pState->aWheels); ++i )
    {
        if ( pState->aWheels[i].state == SITHJEEP_WHEEL_INAIR )
        {
            ++numInAir;
        }
        else if ( pState->aWheels[i].state == SITHJEEP_WHEEL_BLOCKED )
        {
            ++numBlocked;
        }
    }

    int bWasAirborne = pState->bAirborne;
    pState->bAirborne = numInAir == 4 || (numInAir == 3 && numBlocked == 1) || (numInAir == 2 && numBlocked == 2);
    if ( !bWasAirborne )
    {
        if ( pState->bAirborne )
        {
            pState->secAirTime = 0.0f;
        }
    }
    else if ( pState->bAirborne )
    {
        pState->secAirTime = secDeltaTime + pState->secAirTime;
    }

    float maxDrop = 0.2f * secDeltaTime;
    float maxPush = 1.1f * secDeltaTime;
    pThing->moveInfo.physics.thrust.z = 0.0f;

    if ( !pState->bAirborne )
    {
        // Extend suspension of wheels hanging in the air
        if ( pFR->state == SITHJEEP_WHEEL_INAIR && pFL->state == SITHJEEP_WHEEL_INAIR )
        {
            sithPhysics_ExtendJeepWheel(pThing, pFR, maxDrop);
            sithPhysics_ExtendJeepWheel(pThing, pFL, maxDrop);
        }
        if ( pBR->state == SITHJEEP_WHEEL_INAIR && pBL->state == SITHJEEP_WHEEL_INAIR )
        {
            sithPhysics_ExtendJeepWheel(pThing, pBR, maxDrop);
            sithPhysics_ExtendJeepWheel(pThing, pBL, maxDrop);
        }
        if ( pFL->state == SITHJEEP_WHEEL_INAIR && pBL->state == SITHJEEP_WHEEL_INAIR )
        {
            sithPhysics_ExtendJeepWheel(pThing, pFL, maxDrop);
            sithPhysics_ExtendJeepWheel(pThing, pBL, maxDrop);
        }
        if ( pFR->state == SITHJEEP_WHEEL_INAIR && pBR->state == SITHJEEP_WHEEL_INAIR )
        {
            sithPhysics_ExtendJeepWheel(pThing, pFR, maxDrop);
            sithPhysics_ExtendJeepWheel(pThing, pBR, maxDrop);
        }

        // Handle wheels hitting walls
        float minImpact = 1.0f;
        SithJeepWheel* apWheels[4] = { pFR, pFL, pBL, pBR };
        for ( size_t i = 0; i < STD_ARRAYLEN(apWheels); ++i )
        {
            if ( apWheels[i]->state == SITHJEEP_WHEEL_BLOCKED )
            {
                minImpact = sithPhysics_BlockJeepWheel(pThing, apWheels[i], maxPush, minImpact);
            }
        }

        if ( minImpact < -0.2f )
        {
            // Crashed into wall
            sithSoundClass_PlayModeRandom(pThing, SITHSOUNDCLASS_LANDHARD);
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_LAND, NULL);
        }
    }

    // Jeep body center is above the average of the wheel contact positions
    pState->center.x = (((pBR->contactPos.x + pFL->contactPos.x) + pBL->contactPos.x) + pFR->contactPos.x) * 0.25f;
    pState->center.y = (((pBR->contactPos.y + pFL->contactPos.y) + pBL->contactPos.y) + pFR->contactPos.y) * 0.25f;
    pState->center.z = (((pBR->contactPos.z + pFL->contactPos.z) + pBL->contactPos.z) + pFR->contactPos.z) * 0.25f;

    sithPhysics_AddJeepWheelNormal(pThing, pState, pFR);
    sithPhysics_AddJeepWheelNormal(pThing, pState, pFL);
    sithPhysics_AddJeepWheelNormal(pThing, pState, pBL);
    sithPhysics_AddJeepWheelNormal(pThing, pState, pBR);
    rdVector_Normalize3Acc(&pState->normal);
    return 1;
}

// Probes the ground under wheel at pProbePos
void J3DAPI sithPhysics_UpdateJeepWheel(SithThing* pThing, SithJeepWheel* pWheel, const rdVector3* pProbePos)
{
    pWheel->state           = SITHJEEP_WHEEL_NOSECTOR;
    pWheel->probePos        = *pProbePos;
    pWheel->contactPos      = *pProbePos;
    pWheel->pGroundMaterial = NULL;

    rdVector3 startPos;
    rdVector_ScaleAdd3(&startPos, &pThing->orient.uvec, 0.2f, pProbePos);

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.001f);
    if ( !pSector )
    {
        return;
    }

    rdVector3 dir;
    rdVector_Neg3(&dir, &pThing->orient.uvec);
    sithCollision_SearchForCollisions(pSector, NULL, &startPos, &dir, 0.2f + 0.2f, pWheel->radius, 0);

    // Find the first two surfaces hit
    SithCollision* pFirstHit  = NULL;
    SithCollision* pSecondHit = NULL;
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 && (pCollision->type & SITHCOLLISION_FACE) != 0 )
        {
            if ( pFirstHit )
            {
                pSecondHit = pCollision;
                break;
            }
            pFirstHit = pCollision;
        }
    }

    if ( pFirstHit )
    {
        pWheel->pGroundMaterial = pFirstHit->pSurfaceCollided->face.pMaterial;
    }

    if ( !pFirstHit || pFirstHit->pSurfaceCollided->face.normal.z < 0.0f )
    {
        pWheel->state          = SITHJEEP_WHEEL_INAIR;
        pWheel->groundDistance = FLT_MAX;
        sithCollision_DecreaseStackLevel();
        return;
    }

    float distance = -((0.2f - pFirstHit->distance) - pWheel->radius);
    pWheel->groundDistance = distance;
    if ( distance > 0.0025f )
    {
        pWheel->state = SITHJEEP_WHEEL_INAIR;
        sithCollision_DecreaseStackLevel();
        return;
    }

    if ( pFirstHit->pSurfaceCollided->face.normal.z >= sithPhysics_flt_538D04 )
    {
        pWheel->state = fabsf(distance) < 0.0025f ? SITHJEEP_WHEEL_GROUNDED : SITHJEEP_WHEEL_COMPRESSED;
        rdVector_ScaleAdd3Acc(&pWheel->contactPos, &pThing->orient.uvec, -distance);
        pWheel->normal = pFirstHit->pSurfaceCollided->face.normal;
        sithCollision_DecreaseStackLevel();
        return;
    }

    // Surface is too steep
    pWheel->state  = SITHJEEP_WHEEL_BLOCKED;
    pWheel->normal = pFirstHit->pSurfaceCollided->face.normal;
    if ( distance < 0.0f )
    {
        rdVector_ScaleAdd3Acc(&pWheel->contactPos, &pThing->orient.uvec, -(1.0f * distance));
    }

    if ( pSecondHit )
    {
        pWheel->suspensionDistance = -((0.2f - pSecondHit->distance) - pWheel->radius);
    }
    else if ( (pThing->attach.flags & SITH_ATTACH_SURFACE) != 0 )
    {
        rdVector3 dir2;
        rdVector_Sub3(&dir2, &pWheel->contactPos, &pThing->attach.attachedFaceFirstVert);
        pWheel->suspensionDistance = 1.2f * rdVector_Dot3(&dir2, &pThing->attach.pFace->normal);
    }
    else
    {
        pWheel->suspensionDistance = FLT_MAX;
    }

    sithCollision_DecreaseStackLevel();
}

// Adds normalized cross(b - a, c - b) of jeep wheel contact positions to pOutNormal
static void sithPhysics_AddJeepWheelsNormal(rdVector3* pOutNormal, const SithJeepWheel* a, const SithJeepWheel* b, const SithJeepWheel* c)
{
    rdVector3 normal;
    sithPhysics_GetRaftTriangleNormal(&normal, &a->contactPos, &b->contactPos, &c->contactPos);
    rdVector_Add3Acc(pOutNormal, &normal);
}

// Turns the jeep away from the wall hit by wheel on one side
static void sithPhysics_TurnJeepFromWall(SithThing* pThing, const SithJeepWheel* pFront, const SithJeepWheel* pBack, float turnFactor)
{
    const SithJeepWheel* pBlocked;
    if ( pFront->state == SITHJEEP_WHEEL_BLOCKED )
    {
        if ( pBack->state == SITHJEEP_WHEEL_BLOCKED )
        {
            return;
        }
        pBlocked = pFront;
    }
    else if ( pBack->state == SITHJEEP_WHEEL_BLOCKED )
    {
        pBlocked = pBack;
    }
    else
    {
        return;
    }

    rdVector3 axis;
    rdVector_Sub3(&axis, &pFront->contactPos, &pBack->contactPos);
    float speed = rdVector_Len3(&pThing->moveInfo.physics.velocity);
    float turn  = (2.0f - pBlocked->normal.z) * rdVector_Dot3(&axis, &pThing->orient.rvec);
    pThing->moveInfo.physics.angularVelocity.y = pThing->moveInfo.physics.angularVelocity.y - speed * turn * turnFactor;
}

// Calculates ground normal from the wheel contact positions and new body position of the jeep
int J3DAPI sithPhysics_UpdateJeepBody(SithThing* pThing, SithJeepState* pState, rdVector3* pOutNormal, float secDeltaTime)
{
    memset(pOutNormal, 0, sizeof(*pOutNormal));

    const SithJeepWheel* pFL = &pState->aWheels[SITHJEEP_WHEEL_FRONTLEFT];
    const SithJeepWheel* pBL = &pState->aWheels[SITHJEEP_WHEEL_BACKLEFT];
    const SithJeepWheel* pBR = &pState->aWheels[SITHJEEP_WHEEL_BACKRIGHT];
    const SithJeepWheel* pFR = &pState->aWheels[SITHJEEP_WHEEL_FRONTRIGHT];

    // Average normals of the triangles formed by wheels that are in the world
    // Note, the original visits the 4 triangles via goto chain which results in the following order and conditions
    int fr = pFR->state != SITHJEEP_WHEEL_NOSECTOR;
    int fl = pFL->state != SITHJEEP_WHEEL_NOSECTOR;
    int bl = pBL->state != SITHJEEP_WHEEL_NOSECTOR;
    int br = pBR->state != SITHJEEP_WHEEL_NOSECTOR;
    if ( fr && fl && bl )
    {
        sithPhysics_AddJeepWheelsNormal(pOutNormal, pFR, pFL, pBL);
    }
    if ( fl && bl && br )
    {
        sithPhysics_AddJeepWheelsNormal(pOutNormal, pFL, pBL, pBR);
    }
    if ( bl && br && fr )
    {
        sithPhysics_AddJeepWheelsNormal(pOutNormal, pBL, pBR, pFR);
    }
    if ( br && fr && fl )
    {
        sithPhysics_AddJeepWheelsNormal(pOutNormal, pBR, pFR, pFL);
    }
    rdVector_Normalize3Acc(pOutNormal);

    rdVector_ScaleAdd3(&pState->targetPos, pOutNormal, pState->size.z * 0.5f, &pState->center);

    // Turn away from the wall
    float turnFactor = 40000.0f * secDeltaTime;
    sithPhysics_TurnJeepFromWall(pThing, pFR, pBR, turnFactor);
    sithPhysics_TurnJeepFromWall(pThing, pFL, pBL, turnFactor);

    if ( pState->bAirborne && pThing->attach.flags != 0 )
    {
        // Push the jeep slightly forward over the edge
        rdVector3 dirXY;
        dirXY.x = pOutNormal->x;
        dirXY.y = pOutNormal->y;
        dirXY.z = 0.0f;
        rdVector_Normalize3Acc(&dirXY);

        rdVector3 side, push;
        rdVector_Cross3(&side, &dirXY, pOutNormal);
        rdVector_Cross3(&push, pOutNormal, &side);
        rdVector_Normalize3Acc(&push);
        rdVector_ScaleAdd3Acc(&pThing->moveInfo.physics.velocity, &push, 0.1f * secDeltaTime);
        return 1;
    }

    return 0;
}

int J3DAPI sithPhysics_CreateJeepUserBlock(SithThing* pThing)
{
    if ( pThing->userblock.pJeep )
    {
        return 1;
    }

    SithJeepUserBlock* pJeep = (SithJeepUserBlock*)STDMALLOC(sizeof(SithJeepUserBlock));
    pThing->userblock.pJeep = pJeep;
    if ( !pJeep )
    {
        SITHLOG_ERROR("CreateJeepUserBlock failed to allocate.\n");
        return 0;
    }

    memset(pJeep, 0, sizeof(SithJeepUserBlock));

    if ( strcmpi(pThing->aName, "jeepplayer") == 0 || strcmpi(pThing->aName, "jeepplayerpyr") == 0 )
    {
        pJeep->type = 1;
        sithPhysics_InitJeepState(pThing, &pJeep->state);
        sithPhysics_InitVehicleFxState(pThing, &pJeep->fxstate);
        sithPhysics_InitJeepExhaust(pThing, &pJeep->exhaustInfo);
        return 1;
    }

    pJeep->type = 0;
    sithPhysics_InitJeepState(pThing, &pJeep->state);
    sithPhysics_InitVehicleFxState(pThing, &pJeep->fxstate);
    return 1;
}

void J3DAPI sithPhysics_InitJeepState(SithThing* pThing, SithJeepState* pState)
{
    J3D_UNUSED(pThing);
    pState->size.x = 0.14f;
    pState->size.y = 0.22f;
    pState->size.z = 0.18f;
    for ( size_t i = 0; i < STD_ARRAYLEN(pState->aWheels); ++i )
    {
        pState->aWheels[i].radius = 0.01f;
    }
}

// Updates jeep engine, skid and gravel sounds
void J3DAPI sithPhysics_UpdateJeepFx(SithThing* pThing, SithVehicleEngineFxState* pFxState, float secDeltaTime)
{
    if ( (pThing->flags & SITH_TF_DYING) != 0 )
    {
        return;
    }

    SithJeepUserBlock* pJeep = pThing->userblock.pJeep;

    rdVector3 moveDir;
    float speed     = rdVector_Normalize3(&moveDir, &pThing->moveInfo.physics.velocity);
    float thrustLen = rdVector_Len3(&pThing->moveInfo.physics.thrust);
    int bAirborne   = pJeep->state.bAirborne != 0;

    pFxState->secUpdateTimer = pFxState->secUpdateTimer - secDeltaTime;
    bool bUpdate = pFxState->secUpdateTimer < 0.0f;
    if ( bUpdate )
    {
        pFxState->secUpdateTimer = pFxState->secUpdateInterval;
    }

    // Engine sound
    if ( !sithPhysics_dword_538D2C || pThing->moveStatus == SITHPLAYERMOVE_JEEP_BOARDING || pThing->moveStatus == SITHPLAYERMOVE_JEEP_UNBOARDING )
    {
        sithSoundClass_StopMode(pThing, SITHJEEP_SOUND_ENGINE);
    }
    else
    {
        sithSoundClass_PlayModeFirstEx(pThing, SITHJEEP_SOUND_ENGINE, 0.1f);
        if ( bUpdate )
        {
            float gearSpeed, gearRange, pitchFrom, pitchTo, volumeFrom, volumeTo;
            int gear;
            if ( speed < 1.0f || !sithControl_GetKey(SITHCONTROL_ACT1, NULL) )
            {
                gear       = 1;
                gearSpeed  = speed;
                gearRange  = 1.0f;
                pitchFrom  = 1.0f;
                pitchTo    = 1.8f;
                volumeFrom = 0.6f;
                volumeTo   = 0.9f;
            }
            else
            {
                gear       = 2;
                gearSpeed  = speed - 1.0f;
                gearRange  = 2.0f - 1.0f;
                pitchFrom  = 1.3f;
                pitchTo    = 2.0f;
                volumeFrom = 0.7f;
                volumeTo   = 1.0f;
            }

            if ( sithPhysics_dword_58540C != gear )
            {
                sithPhysics_dword_58540C = gear; // current gear
            }

            sithSoundClass_SetModeVolume(pThing, SITHJEEP_SOUND_ENGINE, sithPhysics_LerpClamped(volumeFrom, volumeTo, gearSpeed, gearRange));

            float pitch = sithPhysics_LerpClamped(pitchFrom, pitchTo, gearSpeed, gearRange);
            if ( pJeep->state.bAirborne )
            {
                // Engine revs up in the air
                pitch = STDMATH_CLAMP(pJeep->state.secAirTime * 3.5f + pitch, 1.0f, 2.6f);
            }

            sithSoundClass_SetModePitch(pThing, SITHJEEP_SOUND_ENGINE, pitch);
        }
    }

    // Skid sound
    if ( (!sithPhysics_dword_538D30 && !sithPhysics_bJeepMoveFx) || bAirborne || speed <= 0.5f )
    {
        if ( !sithSoundClass_IsModeFadingVol(pThing, SITHJEEP_SOUND_SKID) )
        {
            sithSoundClass_FadeModeVolume(pThing, SITHJEEP_SOUND_SKID, 0.0f, 0.6f);
        }
    }
    else
    {
        sithSoundClass_PlayModeFirstEx(pThing, SITHJEEP_SOUND_SKID, 0.15f);
        if ( bUpdate )
        {
            float volume;
            float pitch = 1.0f;
            float forwardDot = rdVector_Dot3(&pThing->orient.lvec, &moveDir);
            if ( sithPhysics_bJeepMoveFx && thrustLen == 0.0f )
            {
                volume = 1.0f;
                pitch  = 1.3f;
            }
            else if ( sithPhysics_bJeepMoveFx && forwardDot <= 0.93f && forwardDot >= -0.9f )
            {
                // Sliding sideways
                volume = (1.0f - forwardDot) * 2.0f + 0.28f * speed + 0.15f;
            }
            else
            {
                volume = sithPhysics_dword_538D30 ? 0.28f * speed + 0.15f : 0.15f;
            }

            sithSoundClass_SetModeVolume(pThing, SITHJEEP_SOUND_SKID, volume);
            sithSoundClass_SetModePitch(pThing, SITHJEEP_SOUND_SKID, pitch);
        }
    }

    // Gravel sound when driving over bumps
    if ( sithPhysics_dword_538D38 && !bAirborne )
    {
        float chance = 0.5f * speed;
        bool bBump = false;
        for ( size_t i = 0; i < STD_ARRAYLEN(pJeep->state.aWheels); ++i )
        {
            if ( pJeep->state.aWheels[i].groundDistance < -0.01f )
            {
                bBump = true;
                break;
            }
        }

        if ( bBump && speed > 0.5f && sithPhysics_Random() < chance )
        {
            float volume = (speed - 0.5f) * 0.65f + 0.05f;
            sithSoundClass_PlayModeRandom(pThing, SITHJEEP_SOUND_GRAVEL);
            sithSoundClass_SetModeVolume(pThing, SITHJEEP_SOUND_GRAVEL, volume);
        }
    }
}
