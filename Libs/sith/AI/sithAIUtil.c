#include "sithAIUtil.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Engine/rdLight.h>
#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdVector.h>
#include <sith/AI/sithAI.h>
#include <sith/AI/sithAIMove.h>
#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithComm.h>
#include <sith/Dss/sithDSS.h>
#include <sith/Dss/sithMulti.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithFX.h>
#include <sith/Gameplay/sithInventory.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithActor.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>
#include <std/General/stdMath.h>
#include <std/General/stdUtil.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

static float sithAIUtil_thingsInViewFovY;
static size_t sithAIUtil_numVisitedSectors;
static SithAIWaypointLayerFlag sithAIUtil_activeWpntLayer;
static size_t sithAIUtil_numThingsInView;
static rdVector3 sithAIUtil_vec_585490;
static SithAIControlBlock* sithAIUtil_pMkPointCurLocal;
static rdVector3 sithAIUtil_mkPointCurPYR;
static float sithAIUtil_thingsInViewFovX;
static SithAIWaypointOwner sithAIUtil_aWpntOwners[10];
static SithThing** sithAIUtil_aThingsInView;
static SithAIWaypointDistance sithAIUtil_aWpntDistances[60];
static int sithAIUtil_sizeThingsInView;
static rdVector3 sithAIUtil_vec_585718;
static float sithAIUtil_maxDistanceToThingsInView;
static int sithAIUtil_thingInViewTypeMask;
static SithAIWaypoint sithAIUtil_aAIWpnts[60];
static float sithAIUtil_secLastUpdate;
static int sithAIUtil_mkPoinCurFlags;

void sithAIUtil_InstallHooks(void)
{
    J3D_HOOKFUNC(sithAIUtil_sub_49B2E0);
    J3D_HOOKFUNC(sithAIUtil_sub_49B640);
    J3D_HOOKFUNC(sithAIUtil_GetDistanceToTarget);
    J3D_HOOKFUNC(sithAIUtil_GetDistanceToTargetPos);
    J3D_HOOKFUNC(sithAIUtil_AIFire);
    J3D_HOOKFUNC(sithAIUtil_AIPauseMove);
    J3D_HOOKFUNC(sithAIUtil_AIResetWaypoints);
    J3D_HOOKFUNC(sithAIUtil_SyncWpnts);
    J3D_HOOKFUNC(sithAIUtil_ProcessSyncWpnts);
    J3D_HOOKFUNC(sithAIUtil_AISetWpnt);
    J3D_HOOKFUNC(sithAIUtil_AIGetWpntThing);
    J3D_HOOKFUNC(sithAIUtil_AISetWpntRank);
    J3D_HOOKFUNC(sithAIUtil_AISetWpntFlags);
    J3D_HOOKFUNC(sithAIUtil_AIClearWpntFlags);
    J3D_HOOKFUNC(sithAIUtil_AISetActiveWpntLayer);
    J3D_HOOKFUNC(sithAIUtil_AIConnectWpnts);
    J3D_HOOKFUNC(sithAIUtil_AIMoveToNextWpnt);
    J3D_HOOKFUNC(sithAIUtil_AITraverseWpnts);
    J3D_HOOKFUNC(sithAIUtil_AIAdvanceToNextWpnt);
    J3D_HOOKFUNC(sithAIUtil_sub_49CC60);
    J3D_HOOKFUNC(sithAIUtil_sub_49D170);
    J3D_HOOKFUNC(sithAIUtil_AIFindNearestWpnt);
    J3D_HOOKFUNC(sithAIUtil_sub_49D640);
    J3D_HOOKFUNC(sithAIUtil_sub_49D750);
    J3D_HOOKFUNC(sithAIUtil_CreateAIWaypoint);
    J3D_HOOKFUNC(sithAIUtil_ClearAIWaypoint);
    J3D_HOOKFUNC(sithAIUtil_GetAIWaypointNum);
    J3D_HOOKFUNC(sithAIUtil_CompareAIWpntDistances);
    J3D_HOOKFUNC(sithAIUtil_RenderAIWaypoints);
    J3D_HOOKFUNC(sithAIUtil_CheckPathToPoint);
    J3D_HOOKFUNC(sithAIUtil_sub_49DC90);
    J3D_HOOKFUNC(sithAIUtil_sub_49DD20);
    J3D_HOOKFUNC(sithAIUtil_MakeRandPoint);
    J3D_HOOKFUNC(sithAIUtil_RetryMakeRandPoint);
    J3D_HOOKFUNC(sithAIUtil_CheckPathToPos);
    J3D_HOOKFUNC(sithAIUtil_sub_49EE50);
    J3D_HOOKFUNC(sithAIUtil_sub_49F010);
    J3D_HOOKFUNC(sithAIUtil_MakePathPos);
    J3D_HOOKFUNC(sithAIUtil_sub_49F1F0);
    J3D_HOOKFUNC(sithAIUtil_AIPlaySoundMode);
    J3D_HOOKFUNC(sithAIUtil_AISetMode);
    J3D_HOOKFUNC(sithAIUtil_AIClearMode);
    J3D_HOOKFUNC(sithAIUtil_SetWeaponFireFlags);
    J3D_HOOKFUNC(sithAIUtil_AIGetMovePos);
    J3D_HOOKFUNC(sithAIUtil_GetXYHeadingVector);
    J3D_HOOKFUNC(sithAIUtil_GetXYZHeadingVector);
    J3D_HOOKFUNC(sithAIUtil_CanAttackTarget);
    J3D_HOOKFUNC(sithAIUtil_IsThingMoving);
    J3D_HOOKFUNC(sithAIUtil_ApplyForce);
    J3D_HOOKFUNC(sithAIUtil_RandomRotate);
    J3D_HOOKFUNC(sithAIUtil_GetThingsInView);
    J3D_HOOKFUNC(sithAIUtil_GetNextThingsInView);
    J3D_HOOKFUNC(sithAIUtil_CanSeeTarget);
}

int sithAIUtil_g_bRenderAIWpnts;

static bool sithAIUtil_IsZeroVector(const rdVector3* pVec)
{
    return pVec->x == 0.0f && pVec->y == 0.0f && pVec->z == 0.0f;
}

static void sithAIUtil_ClipVectorNearZero(rdVector3* pVec)
{
    pVec->x = stdMath_ClipNearZero(pVec->x);
    pVec->y = stdMath_ClipNearZero(pVec->y);
    pVec->z = stdMath_ClipNearZero(pVec->z);
}

// Waypoint owner waypointNum packing: low byte - previous waypoint, second byte - current waypoint
#define SITHAIUTIL_WPNT_PREV(n)   ((n) & 0xFF)
#define SITHAIUTIL_WPNT_CUR(n)    (((n) >> 8) & 0xFF)
#define SITHAIUTIL_WPNT_RANK_MASK 0xFF

static float sithAIUtil_Random(void)
{
    return (float)rand() * (1.0f / 32767.0f);
}

static bool sithAIUtil_IsWpntActive(size_t wpntIdx)
{
    SithAIWaypointLayerFlag flags = sithAIUtil_aAIWpnts[wpntIdx].flags;
    return (flags & SITH_AIWPNT_DISABLED) == 0 && (flags & sithAIUtil_activeWpntLayer) != 0;
}

static SithThing* sithAIUtil_GetWpntGhost(size_t wpntIdx)
{
    return &sithWorld_g_pCurrentWorld->aThings[sithAIUtil_aAIWpnts[wpntIdx].thingNum];
}

static bool sithAIUtil_HasEyeOffset(const SithThing* pThing)
{
    const rdVector3* pEye = &pThing->thingInfo.actorInfo.eyeOffset;
    return pEye->x != 0.0f || pEye->y != 0.0f || pEye->z != 0.0f;
}

// Returns non-zero when viewer can't perceive invisible target or viewer is blind
static bool sithAIUtil_IsTargetHidden(const SithThing* pViewer, const SithThing* pTarget)
{
    SithActorFlag flags = pViewer->thingInfo.actorInfo.flags;
    if ( (flags & SITH_AF_SEEINVISIBLE) != 0 )
    {
        return false;
    }

    if ( (pTarget->type == SITH_THING_ACTOR || pTarget->type == SITH_THING_PLAYER) && (pTarget->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
    {
        return true;
    }

    return (flags & SITH_AF_BLIND) != 0;
}

// Updates the sight state of AI target (or target position when there's no target thing) from the weapon fire position
void J3DAPI sithAIUtil_sub_49B2E0(SithAIControlBlock* pLocal, SithAIUtilFireFlags flags)
{
    int prevSightState = pLocal->targetSightState;
    SithThing* pOwner  = pLocal->pOwner;

    if ( (flags & (SITHAIUTIL_FIRE_ALT | SITHAIUTIL_FIRE_PRIMARY | SITHAIUTIL_FIRE_LOB)) == 0 && (size_t)pLocal->unknown124 == sithMain_g_frameNumber )
    {
        return; // Already updated this frame
    }

    pLocal->unknown124 = sithMain_g_frameNumber;

    float weaponSize = 0.0f;
    if ( sithWeapon_HasWeaponSelected(pOwner) )
    {
        weaponSize = sithWeapon_GetWeaponCollideSize(pOwner->thingInfo.actorInfo.curWeaponID);
    }
    else if ( pOwner->thingInfo.actorInfo.pWeaponTemplate )
    {
        weaponSize = pOwner->thingInfo.actorInfo.pWeaponTemplate->collide.movesize;
    }

    rdMatrix_TransformVector34(&pLocal->weaponFirePos, &pOwner->thingInfo.actorInfo.fireOffset, &pOwner->orient);
    rdVector_Add3Acc(&pLocal->weaponFirePos, &pOwner->pos);

    rdVector3 viewPos = pLocal->weaponFirePos;
    if ( (pLocal->submode & SITHAI_SUBMODE_FIREADDEYEOFFSET) != 0 && sithAIUtil_HasEyeOffset(pOwner) )
    {
        viewPos.z = pOwner->thingInfo.actorInfo.eyeOffset.z * 0.5f + viewPos.z;
    }

    pLocal->mode &= ~SITHAI_MODE_TARGETVISIBLE;

    SithThing* pTarget = pLocal->pTargetThing;
    if ( !pTarget )
    {
        pLocal->targetSightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &viewPos, &pLocal->targetPos, pLocal->pClass->fov, pLocal->pClass->sightDistance, weaponSize, &pLocal->toTarget, &pLocal->distance);
        if ( pLocal->targetSightState != 0 )
        {
            goto update_distance;
        }

        pLocal->vecUnknown122 = pLocal->targetPos;
    }
    else
    {
        if ( sithAIUtil_IsTargetHidden(pOwner, pTarget) )
        {
            prevSightState = 3;
        }

        pLocal->targetPos = pTarget->pos;
        pLocal->targetSightState = sithAIUtil_GetDistanceToTarget(pOwner, &viewPos, pTarget, pLocal->pClass->fov, pLocal->pClass->sightDistance, weaponSize, &pLocal->toTarget, &pLocal->distance);
        if ( pLocal->targetSightState != 0 )
        {
            goto update_distance;
        }

        if ( prevSightState != 0 && !sithAIUtil_CanSeeTarget(pLocal, pTarget, pLocal->distance) )
        {
            pLocal->targetSightState = 3;
            return;
        }

        pLocal->vecUnknown122 = pTarget->pos;
    }

    pLocal->msecAttackStart = sithTime_g_msecGameTime;
    pLocal->mode |= SITHAI_MODE_TARGETVISIBLE;

update_distance:
    if ( (pLocal->submode & SITHAI_SUBMODE_FIREADDEYEOFFSET) != 0 && sithAIUtil_HasEyeOffset(pOwner) )
    {
        // Direction & distance to target from the fire position
        rdVector_Sub3(&pLocal->toTarget, &pLocal->targetPos, &pLocal->weaponFirePos);
        float dist = rdVector_Normalize3Acc(&pLocal->toTarget);
        if ( pTarget )
        {
            dist = dist - pTarget->collide.size;
        }

        pLocal->distance = dist > 0.0f ? dist : 0.0f;
    }
}

// Updates the sight state of AI goal thing (or move position when there's no goal thing)
void J3DAPI sithAIUtil_sub_49B640(SithAIControlBlock* pLocal)
{
    int prevSightState = pLocal->targetSightState2;
    SithThing* pOwner  = pLocal->pOwner;
    if ( (size_t)pLocal->unknown141 == sithMain_g_frameNumber )
    {
        return; // Already updated this frame
    }

    SithThing* pGoal = pLocal->goalThing;
    pLocal->unknown141 = sithMain_g_frameNumber;

    if ( !pGoal )
    {
        pLocal->vecUnknown6 = pLocal->movePos;
        pLocal->targetSightState2 = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &pLocal->vecUnknown6, -1.0f, pLocal->pClass->sightDistance, 0.0f, &pLocal->vecUnknown0, &pLocal->targetDistance);
        if ( pLocal->targetSightState2 == 0 )
        {
            pLocal->vecUnknown5 = pLocal->vecUnknown6;
            pLocal->unknown150  = sithTime_g_msecGameTime;
        }
        return;
    }

    if ( sithAIUtil_IsTargetHidden(pOwner, pGoal) )
    {
        prevSightState = 3;
    }

    pLocal->targetSightState2 = sithAIUtil_GetDistanceToTarget(pOwner, &pOwner->pos, pGoal, -1.0f, pLocal->pClass->sightDistance, 0.0f, &pLocal->vecUnknown0, &pLocal->targetDistance);
    if ( pLocal->targetSightState2 == 0 )
    {
        if ( prevSightState != 0 && !sithAIUtil_CanSeeTarget(pLocal, pGoal, pLocal->targetDistance) )
        {
            pLocal->targetSightState2 = 3;
            return;
        }

        pLocal->vecUnknown5 = pGoal->pos;
        pLocal->unknown150  = sithTime_g_msecGameTime;
    }
}

// Checks if direction to target (unit vector) is in field of view.
// fov >= 0: target must be in front and within the fov cone, fov < 0: target behind and to the side more than fov is not visible.
static int sithAIUtil_CheckFov(const SithThing* pViewer, const rdVector3* toTarget, float fov, bool bInclusive)
{
    if ( fov <= -1.0f )
    {
        return 0;
    }

    float fwdDot  = rdVector_Dot3(&pViewer->orient.lvec, toTarget);
    float sideDot = fabsf(rdVector_Dot3(&pViewer->orient.rvec, toTarget));
    if ( fov >= 0.0f )
    {
        if ( fwdDot < 0.0f )
        {
            return 2;
        }

        if ( bInclusive ? 1.0f - fov <= sideDot : 1.0f - fov < sideDot )
        {
            return 2;
        }
    }

    if ( fov < 0.0f && fwdDot < 0.0f && sideDot < fov - -1.0f )
    {
        return 2;
    }

    return 0;
}

// returns target sight state:
// 0 - target in sight
// 1 - target too far away (dist > sightDistance)
// 2 - target out of FOV view
// 3 - target not in sight
int J3DAPI sithAIUtil_GetDistanceToTarget(SithThing* pViewer, rdVector3* startPos, const SithThing* pTarget, float fov, float sightDistance, float collisionSize, rdVector3* toTarget, float* pDist)
{
    J3D_UNUSED(collisionSize);
    SITH_ASSERTREL(toTarget && pDist);
    SITH_ASSERTREL(pViewer && pTarget && startPos);

    rdVector_Sub3(toTarget, &pTarget->pos, startPos);
    float dist = rdVector_Normalize3Acc(toTarget) - pTarget->collide.size;
    *pDist = dist;
    dist = dist <= 0.0f ? 0.0f : dist;
    *pDist = dist;

    if ( sithAIUtil_IsTargetHidden(pViewer, pTarget) )
    {
        return 3;
    }

    // Can't see from air into water unless the target is on the water surface and vice versa
    if ( (pViewer->flags & SITH_TF_SUBMERGED) == 0 && (pTarget->flags & SITH_TF_SUBMERGED) != 0 )
    {
        if ( pTarget->moveType != SITH_MT_PHYSICS || (pTarget->moveInfo.physics.flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            return 3;
        }
    }

    if ( (pViewer->flags & SITH_TF_SUBMERGED) != 0 && (pTarget->flags & SITH_TF_SUBMERGED) == 0 )
    {
        return 3;
    }

    if ( dist - pTarget->collide.size > sightDistance )
    {
        return 1;
    }

    if ( sithAIUtil_CheckFov(pViewer, toTarget, fov, /*bInclusive=*/true) )
    {
        return 2;
    }

    if ( (pViewer->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) == 0 )
    {
        SithSector* pSector = sithCollision_FindSectorInRadius(pViewer->pInSector, &pViewer->pos, startPos, 0.0f);
        return sithCollision_CanMoveToPos(pSector, startPos, &pTarget->pos, 0.0f) ? 0 : 3;
    }

    return 0;
}

int J3DAPI sithAIUtil_GetDistanceToTargetPos(SithThing* pViewer, rdVector3* startPos, rdVector3* endPos, float fov, float sightDistance, float collisionSize, rdVector3* toTarget, float* pDist)
{
    J3D_UNUSED(collisionSize);
    SITH_ASSERTREL(toTarget && pDist);
    SITH_ASSERTREL(pViewer && startPos);

    rdVector_Sub3(toTarget, endPos, startPos);
    *pDist = rdVector_Normalize3Acc(toTarget);
    if ( *pDist > sightDistance )
    {
        return 1;
    }

    if ( sithAIUtil_CheckFov(pViewer, toTarget, fov, /*bInclusive=*/false) )
    {
        return 2;
    }

    if ( (pViewer->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) == 0 )
    {
        SithSector* pSector = sithCollision_FindSectorInRadius(pViewer->pInSector, &pViewer->pos, startPos, 0.0f);
        return sithCollision_CanMoveToPos(pSector, startPos, endPos, 0.0f) ? 0 : 3;
    }

    return 0;
}

int J3DAPI sithAIUtil_AIFire(SithAIControlBlock* pLocal, float minDist, float maxDist, float fireDot, float aimError, int weaponNum, SithAIUtilFireFlags flags, int burstCount)
{
    float extra = 1.0f;
    SithPuppetSubMode fireSubmode = 0;
    int bFired = 0;

    SITH_ASSERTREL(pLocal->pOwner && pLocal->pClass);
    SithThing* pOwner = pLocal->pOwner;

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_AIDISABLED) != 0 )
    {
        return 0;
    }

    if ( (pOwner->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 || !pLocal->pTargetThing )
    {
        return 0;
    }

    if ( (pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0 && (pOwner->thingInfo.actorInfo.flags & SITH_AF_NOUNDERWATERFIRE) != 0 )
    {
        return 0;
    }

    sithAI_EmitEvent(pLocal, SITHAI_EVENT_PREFIRE, (void*)flags);

    SithThing* pWeaponTemplate = pOwner->thingInfo.actorInfo.pWeaponTemplate;
    float projectileSpeed;
    if ( pWeaponTemplate )
    {
        projectileSpeed = pWeaponTemplate->moveInfo.physics.velocity.y;
    }
    else
    {
        // Inventory weapon
        int weaponId = pOwner->thingInfo.actorInfo.curWeaponID;
        if ( weaponId < 0x13 || weaponId > 0x18 )
        {
            goto done;
        }

        projectileSpeed = 5.0f;
    }

    sithAIUtil_sub_49B2E0(pLocal, flags);
    rdVector3 fireDir = pLocal->toTarget;

    SithAIUtilFireFlags bDelayedFire = flags & SITHAIUTIL_FIRE_UNKNOWN_8; // delayed fire triggered from puppet fire key
    if ( !bDelayedFire )
    {
        if ( sithTime_g_msecGameTime < pLocal->msecFireWaitTime || pLocal->targetSightState != 0 )
        {
            goto done;
        }

        if ( pLocal->distance < minDist || pLocal->distance > maxDist )
        {
            goto done;
        }

        if ( (pLocal->submode & SITHAI_SUBMODE_SKIPCHECKFIREFOV) == 0 && rdVector_Dot3(&pOwner->orient.lvec, &fireDir) < fireDot )
        {
            goto done;
        }

        if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_40) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE;
        }
        else if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_80) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE2;
        }
        else if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_100) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE3;
        }
        else if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_200) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE4;
        }

        if ( (flags & SITHAIUTIL_FIRE_STOP_ON_FIRE) != 0 )
        {
            sithAIMove_AIStop(pLocal);
        }
    }

    if ( weaponNum >= 0 )
    {
        pLocal->fireDot     = fireDot;
        pLocal->minFireDist = minDist;
        pLocal->maxFireDist = maxDist;
        if ( !bDelayedFire && (pOwner->thingInfo.actorInfo.flags & SITH_AF_DELAYFIRE) != 0 )
        {
            // Play fire animation, the projectile is fired when the puppet fire key frame is reached
            pLocal->fireFlags     = flags | SITHAIUTIL_FIRE_UNKNOWN_8;
            pLocal->aimError      = aimError;
            pLocal->fireWeaponNum = weaponNum;
            sithPuppet_PlayMode(pOwner, fireSubmode, NULL);
            sithAI_EmitEvent(pLocal, SITHAI_EVENT_FIRE, pLocal->pTargetThing);
            bFired = 1;
            goto done;
        }
    }

    // Lead the target
    SithThing* pTarget = pLocal->pTargetThing;
    if ( (flags & SITHAIUTIL_FIRE_LEAD) != 0 && pTarget->moveType == SITH_MT_PHYSICS
        && (pTarget->moveInfo.physics.velocity.x != 0.0f || pTarget->moveInfo.physics.velocity.y != 0.0f || pTarget->moveInfo.physics.velocity.z != 0.0f) )
    {
        rdVector3 leadDir;
        leadDir.x = projectileSpeed * fireDir.x + pTarget->moveInfo.physics.velocity.x;
        leadDir.y = pTarget->moveInfo.physics.velocity.y + projectileSpeed * fireDir.y;
        leadDir.z = pTarget->moveInfo.physics.velocity.z + projectileSpeed * fireDir.z;
        rdVector_Normalize3Acc(&leadDir);
        if ( rdVector_Dot3(&leadDir, &fireDir) > 0.5f )
        {
            fireDir = leadDir;
        }
    }

    // Lob the projectile
    int bLob = (flags & SITHAIUTIL_FIRE_LOB) != 0;
    if ( bLob )
    {
        rdVector3 lobVel;
        lobVel.x = pLocal->toTarget.x * projectileSpeed;
        lobVel.y = pLocal->toTarget.y * projectileSpeed;
        lobVel.z = (pLocal->distance / projectileSpeed) * 0.5f * sithWorld_g_pCurrentWorld->gravity + pLocal->toTarget.z * projectileSpeed;
        extra = rdVector_Normalize3(&fireDir, &lobVel) / projectileSpeed;
    }

    // Apply aim error
    float accuracy = sithGetHitAccuarancyScalar() * pLocal->pClass->accurancy;
    if ( aimError != 0.0f && pLocal->distance != 0.0f && accuracy < sithAIUtil_Random() )
    {
        sithAIUtil_RandomRotate(&fireDir, sithGetCombatDamageScalar() * aimError);
    }

    if ( !pWeaponTemplate )
    {
        // Fire inventory weapon via its cog
        SithInventoryType* pWeaponType = sithInventory_GetType(pOwner->thingInfo.actorInfo.curWeaponID);
        if ( !pWeaponType || !pWeaponType->pCog )
        {
            goto done;
        }

        rdVector_Sub3(&pOwner->thingInfo.actorInfo.vecUnknown0, &fireDir, &pLocal->toTarget); // aim offset
        if ( (sithWeapon_HasWeaponSelected(pOwner) && !sithWeapon_IsAiming(pOwner)) || sithWeapon_IsMountingWeapon(pOwner) )
        {
            goto done;
        }

        sithCog_SendMessageEx(pWeaponType->pCog, SITHCOG_MSG_ACTIVATE, SITHCOG_SYM_REF_NONE, 0, SITHCOG_SYM_REF_THING, pOwner->idx, 0, burstCount, 0, 0, 0);
    }
    else
    {
        if ( pOwner->pAttachedThing && (pOwner->pAttachedThing->attach.flags & SITH_ATTACH_TAIL) != 0 )
        {
            sithAIMove_sub_49AA60(pLocal);
        }

        if ( weaponNum < 0 )
        {
            weaponNum = 0;
        }

        sithSoundClass_PlayModeRandom(pOwner, (SithSoundClassMode)(SITHSOUNDCLASS_FIRE1 + weaponNum));
        pOwner->thingInfo.actorInfo.vecUnknown0 = fireDir;

        SithThing* pProjectile = sithWeapon_WeaponFire(pOwner, pWeaponTemplate, &fireDir, &pLocal->weaponFirePos, 0, fireSubmode, extra, (SithFireProjectileFlags)bLob, 0.0f);
        if ( !pProjectile )
        {
            return 0;
        }

        sithCog_ThingSendMessageEx(pOwner, pProjectile, SITHCOG_MSG_FIRE, (int)pWeaponTemplate->thingInfo.weaponInfo.damage, pWeaponTemplate->thingInfo.weaponInfo.damageType, weaponNum, 0);
    }

    if ( !bDelayedFire )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_FIRE, pLocal->pTargetThing);
    }

    bFired = 1;

done:
    sithAI_EmitEvent(pLocal, SITHAI_EVENT_POSTFIRE, (void*)flags);
    return bFired;
}

void J3DAPI sithAIUtil_AIPauseMove(SithAIControlBlock* pLocal, int msecPause)
{
    pLocal->msecPauseMoveUntil = msecPause + sithTime_g_msecGameTime;
}

void sithAIUtil_AIResetWaypoints(void)
{
    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++i )
    {
        sithAIUtil_aAIWpnts[i].thingNum     = -1;
        sithAIUtil_aAIWpnts[i].flags        = SITH_AIWPNT_LAYER0;
        sithAIUtil_aAIWpnts[i].numUsedLinks = 0;
        sithAIUtil_aAIWpnts[i].pathDepth    = 0;
    }

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aWpntOwners); ++i )
    {
        sithAIUtil_aWpntOwners[i].thingNum          = -1;
        sithAIUtil_aWpntOwners[i].waypointNum       = 0;
        sithAIUtil_aWpntOwners[i].cosTurnAlignAngle = 0.0f;
    }

    sithAIUtil_activeWpntLayer   = SITH_AIWPNT_LAYER0;
    sithAIUtil_g_bRenderAIWpnts  = 0;
}

int J3DAPI sithAIUtil_SyncWpnts(DPID idTo, unsigned int outstream)
{
    SITHDSS_STARTOUT(SITHDSS_SYNCWPNT);

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++i )
    {
        SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[i];
        SITHDSS_PUSHINT32(pWpnt->thingNum);
        SITHDSS_PUSHUINT32(pWpnt->flags);
        SITHDSS_PUSHINT32(pWpnt->numUsedLinks);
        for ( size_t j = 0; j < STD_ARRAYLEN(pWpnt->aWpntLinks); ++j )
        {
            SITHDSS_PUSHINT32(pWpnt->aWpntLinks[j]);
        }

        SITHDSS_PUSHINT32(pWpnt->pathDepth);
    }

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aWpntOwners); ++i )
    {
        SITHDSS_PUSHINT32(sithAIUtil_aWpntOwners[i].thingNum);
        SITHDSS_PUSHINT32(sithAIUtil_aWpntOwners[i].waypointNum);
        SITHDSS_PUSHFLOAT(sithAIUtil_aWpntOwners[i].cosTurnAlignAngle);
    }

    SITHDSS_PUSHUINT32(sithAIUtil_activeWpntLayer);
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 1u);
}

int J3DAPI sithAIUtil_ProcessSyncWpnts(const SithMessage* pMsg)
{
    SITH_ASSERTREL(pMsg);
    SITHDSS_STARTIN(pMsg);

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++i )
    {
        SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[i];
        pWpnt->thingNum     = SITHDSS_POPINT32();
        pWpnt->flags        = SITHDSS_POPUINT32();
        pWpnt->numUsedLinks = SITHDSS_POPINT32();
        for ( size_t j = 0; j < STD_ARRAYLEN(pWpnt->aWpntLinks); ++j )
        {
            pWpnt->aWpntLinks[j] = SITHDSS_POPINT32();
        }

        pWpnt->pathDepth = SITHDSS_POPINT32();
    }

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aWpntOwners); ++i )
    {
        sithAIUtil_aWpntOwners[i].thingNum          = SITHDSS_POPINT32();
        sithAIUtil_aWpntOwners[i].waypointNum       = SITHDSS_POPINT32();
        sithAIUtil_aWpntOwners[i].cosTurnAlignAngle = SITHDSS_POPFLOAT();
    }

    sithAIUtil_activeWpntLayer = SITHDSS_POPUINT32();
    SITHDSS_ENDIN;
    return 1;
}

void J3DAPI sithAIUtil_AISetWpnt(SithThing* pGhost, unsigned int wpntIdx)
{
    if ( wpntIdx >= STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", wpntIdx, STD_ARRAYLEN(sithAIUtil_aAIWpnts));
        return;
    }

    if ( !pGhost || pGhost->type != SITH_THING_GHOST )
    {
        SITHLOG_ERROR("AISetWpnt: Non-GHOST type THING passed as waypoint.\n");
        return;
    }

    sithAIUtil_aAIWpnts[wpntIdx].thingNum     = pGhost->idx;
    sithAIUtil_aAIWpnts[wpntIdx].flags        = SITH_AIWPNT_LAYER0;
    sithAIUtil_aAIWpnts[wpntIdx].numUsedLinks = 0;
}

SithThing* J3DAPI sithAIUtil_AIGetWpntThing(int wpntIdx)
{
    return sithAIUtil_GetWpntGhost(wpntIdx);
}

void J3DAPI sithAIUtil_AISetWpntRank(unsigned int wpntIdx, unsigned int rank)
{
    if ( wpntIdx >= STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", wpntIdx, STD_ARRAYLEN(sithAIUtil_aAIWpnts));
        return;
    }

    if ( rank > SITHAIUTIL_WPNT_RANK_MASK )
    {
        rank = SITHAIUTIL_WPNT_RANK_MASK;
    }

    sithAIUtil_aAIWpnts[wpntIdx].flags = (sithAIUtil_aAIWpnts[wpntIdx].flags & ~SITHAIUTIL_WPNT_RANK_MASK) | rank;
}

void J3DAPI sithAIUtil_AISetWpntFlags(unsigned int wpntIdx, SithAIWaypointLayerFlag flags)
{
    if ( wpntIdx >= STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", wpntIdx, STD_ARRAYLEN(sithAIUtil_aAIWpnts));
        return;
    }

    // Clamp to valid flags range
    if ( (uint32_t)flags < SITH_AIWPNT_DISABLED )
    {
        flags = SITH_AIWPNT_DISABLED;
    }
    else if ( (uint32_t)flags > 0xFFFFFF00 )
    {
        flags = (SithAIWaypointLayerFlag)0xFFFFFF00;
    }

    sithAIUtil_aAIWpnts[wpntIdx].flags |= flags;
}

void J3DAPI sithAIUtil_AIClearWpntFlags(unsigned int tIdx, SithAIWaypointLayerFlag flags)
{
    if ( tIdx >= STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", tIdx, STD_ARRAYLEN(sithAIUtil_aAIWpnts));
        return;
    }

    if ( (uint32_t)flags < SITH_AIWPNT_DISABLED )
    {
        flags = SITH_AIWPNT_DISABLED;
    }
    else if ( (uint32_t)flags > 0xFFFFFF00 )
    {
        flags = (SithAIWaypointLayerFlag)0xFFFFFF00;
    }

    sithAIUtil_aAIWpnts[tIdx].flags &= ~flags;
}

void J3DAPI sithAIUtil_AISetActiveWpntLayer(SithAIWaypointLayerFlag layer)
{
    if ( (layer & (SITH_AIWPNT_LAYER0 | SITH_AIWPNT_LAYER1 | SITH_AIWPNT_LAYER2 | SITH_AIWPNT_LAYER3 | SITH_AIWPNT_LAYER4 | SITH_AIWPNT_LAYER5)) == 0 )
    {
        SITHLOG_ERROR("AISetWpntLayer: Invalid layer flag (%x).\n", layer);
        return;
    }

    sithAIUtil_activeWpntLayer = layer;
}

void J3DAPI sithAIUtil_AIConnectWpnts(unsigned int wpntIdx1, unsigned int wpntIdx2, int bConnectWpnt1To2)
{
    if ( wpntIdx1 >= STD_ARRAYLEN(sithAIUtil_aAIWpnts) || wpntIdx2 >= STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d or %d) out of allowed range (0 - %d).\n", wpntIdx1, wpntIdx2, STD_ARRAYLEN(sithAIUtil_aAIWpnts));
        return;
    }

    SithAIWaypoint* pWpnt1 = &sithAIUtil_aAIWpnts[wpntIdx1];
    SithAIWaypoint* pWpnt2 = &sithAIUtil_aAIWpnts[wpntIdx2];
    if ( pWpnt1->thingNum == -1 )
    {
        SITHLOG_ERROR("AIConnectWpnts: Invalid waypoint (#%d).\n", wpntIdx1);
        return;
    }

    if ( pWpnt2->thingNum == -1 )
    {
        SITHLOG_ERROR("AIConnectWpnts: Invalid waypoint (#%d).\n", wpntIdx2);
        return;
    }

    if ( (size_t)pWpnt1->numUsedLinks < STD_ARRAYLEN(pWpnt1->aWpntLinks) )
    {
        pWpnt1->aWpntLinks[pWpnt1->numUsedLinks++] = wpntIdx2;
    }
    else
    {
        SITHLOG_ERROR("AIConnectWpnts: Cant connect (#%d) to (#%d). Too many links\n", wpntIdx2, wpntIdx1);
    }

    // Connect waypoint 2 back to waypoint 1
    if ( bConnectWpnt1To2 != 1 )
    {
        return;
    }

    if ( (size_t)pWpnt2->numUsedLinks >= STD_ARRAYLEN(pWpnt2->aWpntLinks) )
    {
        SITHLOG_ERROR("AIConnectWpnts: Cannot connect (#%d) to (#%d). Too many links.\n", wpntIdx1, wpntIdx2);
        return;
    }

    pWpnt2->aWpntLinks[pWpnt2->numUsedLinks++] = wpntIdx1;
}

int J3DAPI sithAIUtil_AIMoveToNextWpnt(SithAIControlBlock* pLocal, float moveSpeed, float degTurn, int mode)
{
    int wpntIdx = sithAIUtil_AIFindNearestWpnt(pLocal->pOwner, 1);
    if ( wpntIdx < 0 )
    {
        return 0;
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0 )
    {
        degTurn = 180.0f;
    }

    sithAIUtil_AITraverseWpnts(pLocal, wpntIdx, moveSpeed, degTurn, mode);
    return 1;
}

void J3DAPI sithAIUtil_AITraverseWpnts(SithAIControlBlock* pLocal, int wpntIdx, float moveSpeed, float degTurn, int mode)
{
    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    SITH_ASSERTREL(pLocal);

    if ( !pWorld || (unsigned int)wpntIdx >= STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
    {
        SITHLOG_ERROR("AITraverseWpnts: Index out of allowed range.\n");
        return;
    }

    int thingNum = sithAIUtil_aAIWpnts[wpntIdx].thingNum;
    if ( thingNum < 0 // Added: Check for unset waypoint
        || pWorld->lastThingIdx < thingNum
        || pWorld->aThings[thingNum].type != SITH_THING_GHOST )
    {
        SITHLOG_ERROR("AITraverseWpnts: Invalid waypoint (#%d).\n", wpntIdx);
        return;
    }

    if ( degTurn == 0.0f )
    {
        degTurn = pLocal->pClass->degTurnAlign;
        if ( degTurn == 0.0f )
        {
            degTurn = 45.0f;
        }
    }

    int ownerNum = sithAIUtil_CreateAIWaypoint(pLocal, degTurn);
    if ( ownerNum == -1 )
    {
        SITHLOG_ERROR("AITraverseWpnts: Too many AI's using waypoints (max = %d).\n", STD_ARRAYLEN(sithAIUtil_aWpntOwners));
        sithAIMove_AIStop(pLocal);
        return;
    }

    pLocal->mode |= SITHAI_MODE_TRAVERSEWPNTS;
    if ( mode == 1 )
    {
        pLocal->submode |= SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION;
    }
    else if ( mode >= 2 )
    {
        pLocal->submode |= SITHAI_SUBMODE_CONTINUOUSWPNTMOTION;
    }

    // Set current waypoint
    SithAIWaypointOwner* pOwnerWpnt = &sithAIUtil_aWpntOwners[ownerNum];
    pOwnerWpnt->waypointNum = (pOwnerWpnt->waypointNum & 0xFFFF00FF) | (wpntIdx << 8);

    const rdVector3* pWpntPos = &pWorld->aThings[thingNum].pos;
    if ( sithAIMove_AISetMovePos(pLocal, pWpntPos, moveSpeed) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, pWpntPos);
        sithAIUtil_sub_49D170(pLocal);
        return;
    }

    // Already at waypoint
    pLocal->moveSpeed = moveSpeed;
    sithAIUtil_AIAdvanceToNextWpnt(pLocal, 0);
}

int J3DAPI sithAIUtil_AIAdvanceToNextWpnt(SithAIControlBlock* pLocal, int bNotify)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    int ownerNum = sithAIUtil_GetAIWaypointNum(pLocal);
    if ( ownerNum == -1 )
    {
        SITHLOG_ERROR("AIAdvanceToNextWpnt: AI '%s' not in waypoint tracking list.\n", pLocal->pOwner->aName);
        sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
        return 0;
    }

    SithAIWaypointOwner* pOwnerWpnt = &sithAIUtil_aWpntOwners[ownerNum];
    unsigned int curWpnt = SITHAIUTIL_WPNT_CUR(pOwnerWpnt->waypointNum);
    sithCog_ThingSendMessageEx(pLocal->pOwner, NULL, SITHCOG_MSG_UPDATEWPNTS, 0, 0, 0, 0);

    if ( curWpnt < STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
    {
        SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[curWpnt];
        int numLinks = pWpnt->numUsedLinks;
        if ( numLinks != 0 )
        {
            // Find first active link
            bool bFound = false;
            int linkNum = 0;
            for ( ; linkNum < numLinks; ++linkNum )
            {
                if ( sithAIUtil_IsWpntActive(pWpnt->aWpntLinks[linkNum]) )
                {
                    bFound = true;
                    break;
                }
            }

            // Don't go back to the previous waypoint if it's the only link, unless in continuous waypoint motion
            if ( bFound
                && (numLinks != 1 || (unsigned int)pWpnt->aWpntLinks[0] != SITHAIUTIL_WPNT_PREV(pOwnerWpnt->waypointNum)
                    || (pLocal->submode & (SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION | SITHAI_SUBMODE_CONTINUOUSWPNTMOTION)) != 0) )
            {
                int nextWpnt = sithAIUtil_sub_49CC60(pLocal, numLinks, pWpnt->aWpntLinks, linkNum, ownerNum);
                const rdVector3* pWpntPos = &sithAIUtil_GetWpntGhost(nextWpnt)->pos;
                if ( sithAIMove_AISetMovePos(pLocal, pWpntPos, pLocal->moveSpeed) )
                {
                    sithAIMove_AISetLookPosEyeLevel(pLocal, pWpntPos);
                    sithAIUtil_sub_49D170(pLocal);

                    pOwnerWpnt->waypointNum = (pOwnerWpnt->waypointNum & 0xFFFFFF00) | curWpnt;
                    pOwnerWpnt->waypointNum = (pOwnerWpnt->waypointNum & 0xFFFF00FF) | (nextWpnt << 8);
                    if ( bNotify )
                    {
                        sithCog_ThingSendMessageEx(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVEDWPNT, curWpnt, nextWpnt, 0, 0);
                    }

                    return 1;
                }
            }
        }
    }

    // Reached the end of waypoint path
    sithCog_ThingSendMessageEx(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVEDWPNT, curWpnt, -1, 0, 0);
    sithCog_ThingSendMessage(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVED);
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
    return 0;
}

// Returns dot of the direction from AI owner to waypoint with pDir
static float sithAIUtil_GetWpntDirDot(const SithAIControlBlock* pLocal, size_t wpntIdx, const rdVector3* pDir)
{
    rdVector3 toWpnt;
    rdVector_Sub3(&toWpnt, &sithAIUtil_GetWpntGhost(wpntIdx)->pos, &pLocal->pOwner->pos);
    rdVector_Normalize3Acc(&toWpnt);
    return rdVector_Dot3(&toWpnt, pDir);
}

// Selects the next waypoint from links aWypointLinks starting at linkNum by the waypoint rank and AI mode (hunting, fleeing, searching).
// Returns waypoint index.
int J3DAPI sithAIUtil_sub_49CC60(SithAIControlBlock* pLocal, unsigned int numLinks, int* aWypointLinks, int linkNum, int wpntOwnerNum)
{
    SithAIMode mode = pLocal->mode;
    if ( (mode & SITHAI_MODE_HUNTING) != 0 )
    {
        sithAIUtil_sub_49D640();
    }
    else if ( (mode & SITHAI_MODE_FLEEING) != 0 )
    {
        if ( (mode & SITHAI_MODE_FLEEINGTOWAYPOINT) != 0 )
        {
            sithAIUtil_sub_49D750(pLocal->pFleeFromThing);
        }
        else
        {
            sithAIUtil_sub_49D640();
        }
    }

    int best = aWypointLinks[linkNum];
    for ( unsigned int i = linkNum + 1; i < numLinks; ++i )
    {
        int cand = aWypointLinks[i];
        if ( !sithAIUtil_IsWpntActive(cand) )
        {
            continue;
        }

        const SithAIWaypoint* pBest = &sithAIUtil_aAIWpnts[best];
        const SithAIWaypoint* pCand = &sithAIUtil_aAIWpnts[cand];
        int candScore = pCand->flags & SITHAIUTIL_WPNT_RANK_MASK;
        int bestScore = pBest->flags & SITHAIUTIL_WPNT_RANK_MASK;

        // Previously visited waypoint
        int prevWpnt = -1;
        if ( wpntOwnerNum != -1 && SITHAIUTIL_WPNT_PREV(sithAIUtil_aWpntOwners[wpntOwnerNum].waypointNum) < STD_ARRAYLEN(sithAIUtil_aAIWpnts) )
        {
            prevWpnt = SITHAIUTIL_WPNT_PREV(sithAIUtil_aWpntOwners[wpntOwnerNum].waypointNum);
        }

        mode = pLocal->mode;
        if ( (mode & SITHAI_MODE_FLEEING) != 0 && pLocal->pFleeFromThing && (mode & SITHAI_MODE_FLEEINGTOWAYPOINT) == 0 )
        {
            // Fleeing from thing, avoid waypoints closer to the player and in the direction of the thing
            if ( pBest->pathDepth == 0xFF )
            {
                bestScore -= 14;
            }
            else if ( pCand->pathDepth == 0xFF )
            {
                candScore -= 14;
            }

            rdVector3 fleeDir;
            rdVector_Sub3(&fleeDir, &pLocal->pFleeFromThing->pos, &pLocal->pOwner->pos);
            float fleeDist = rdVector_Normalize3Acc(&fleeDir);

            float maxDot = (fleeDist - 0.5f) * ((0.984f - 0.923f) / (1.5f - 0.5f)) + 0.923f;
            maxDot = STDMATH_CLAMP(maxDot, 0.923f, 0.984f);

            if ( sithAIUtil_GetWpntDirDot(pLocal, best, &fleeDir) > maxDot )
            {
                bestScore -= 10;
            }

            if ( sithAIUtil_GetWpntDirDot(pLocal, cand, &fleeDir) > maxDot )
            {
                candScore -= 10;
            }

            int depthDiff = pCand->pathDepth - pBest->pathDepth;
            if ( depthDiff < 0 )
            {
                candScore += depthDiff * -3;
            }
            else if ( depthDiff > 0 )
            {
                bestScore += depthDiff * 3;
            }

            if ( prevWpnt != -1 )
            {
                if ( cand == prevWpnt )
                {
                    candScore -= 1;
                }

                if ( best == prevWpnt )
                {
                    bestScore -= 1;
                }
            }
        }
        else if ( ((mode & SITHAI_MODE_FLEEING) != 0 && pLocal->pFleeFromThing && (mode & SITHAI_MODE_FLEEINGTOWAYPOINT) != 0)
            || ((mode & SITHAI_MODE_HUNTING) != 0 && pLocal->pTargetThing) )
        {
            // Fleeing to waypoint or hunting player, follow the waypoint path depth
            bool bFleeing = (mode & SITHAI_MODE_FLEEING) != 0 && pLocal->pFleeFromThing && (mode & SITHAI_MODE_FLEEINGTOWAYPOINT) != 0;
            bestScore = 0;
            candScore = 0;

            bool bCompareDepth = true;
            if ( prevWpnt != -1 )
            {
                if ( bFleeing && cand == prevWpnt )
                {
                    candScore = -6;
                    if ( best == prevWpnt )
                    {
                        bestScore = -6;
                        if ( pBest->pathDepth < pCand->pathDepth )
                        {
                            candScore     = 1;
                            bCompareDepth = false;
                        }
                        else
                        {
                            if ( pCand->pathDepth < pBest->pathDepth )
                            {
                                bestScore += 7;
                            }

                            bCompareDepth = false;
                        }
                    }
                }
                else
                {
                    if ( !bFleeing && cand == prevWpnt )
                    {
                        candScore = -6;
                    }

                    bestScore = best == prevWpnt ? -6 : 0;
                }
            }

            if ( bCompareDepth )
            {
                if ( pCand->pathDepth > pBest->pathDepth )
                {
                    candScore += 7;
                }
                else if ( pCand->pathDepth < pBest->pathDepth )
                {
                    bestScore += 7;
                }
            }
        }
        else if ( (mode & (SITHAI_MODE_SEARCHING | SITHAI_MODE_ATTACKING)) != 0 )
        {
            // Searching or attacking, prefer higher rank and avoid going back
            if ( prevWpnt != -1 )
            {
                if ( cand == prevWpnt )
                {
                    candScore -= 6;
                }

                if ( best == prevWpnt )
                {
                    bestScore -= 6;
                }
            }

            if ( candScore < bestScore && (float)(bestScore - candScore) * 0.1f - -0.5f < sithAIUtil_Random() )
            {
                bestScore = -bestScore;
            }
        }

        if ( candScore > bestScore || (candScore == bestScore && sithAIUtil_Random() > 0.5f) )
        {
            best = cand;
        }
    }

    return best;
}

// Stops the AI when it should first turn towards the waypoint
void J3DAPI sithAIUtil_sub_49D170(SithAIControlBlock* pLocal)
{
    int ownerNum = sithAIUtil_GetAIWaypointNum(pLocal);
    if ( ownerNum < 0 )
    {
        sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
        return;
    }

    // Project goal look vector onto the owner's plane
    SithThing* pOwner = pLocal->pOwner;
    rdVector3 vec;
    rdVector_Sub3(&vec, &pLocal->goalLVec, &rdroid_g_zeroVector3);
    float dist = -rdVector_Dot3(&vec, &pOwner->orient.uvec);

    rdVector3 goalDir;
    rdVector_ScaleAdd3(&goalDir, &pOwner->orient.uvec, dist, &pLocal->goalLVec);
    rdVector_Normalize3Acc(&goalDir);

    float alignDot;
    if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
    {
        alignDot = pOwner->orient.lvec.z * goalDir.z + (pOwner->orient.lvec.y * goalDir.y + pOwner->orient.lvec.x * goalDir.x);
    }
    else
    {
        alignDot = pOwner->orient.lvec.y * goalDir.y + pOwner->orient.lvec.x * goalDir.x;
    }

    if ( alignDot < sithAIUtil_aWpntOwners[ownerNum].cosTurnAlignAngle && (pLocal->mode & SITHAI_MODE_TURNING) != 0 )
    {
        memset(&pOwner->moveInfo.physics.velocity, 0, sizeof(pOwner->moveInfo.physics.velocity));
        return;
    }

    pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_10;
}

int J3DAPI sithAIUtil_AIFindNearestWpnt(SithThing* pThing, int a2)
{
    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    if ( pThing != sithPlayer_g_pLocalPlayerThing )
    {
        sithCog_ThingSendMessageEx(pThing, NULL, SITHCOG_MSG_UPDATEWPNTS, 0, 0, 0, 0);
    }

    // Collect distances to the active waypoints
    bool bFound = false;
    size_t numDistances = 0;
    size_t wpntIdx = 0;
    for ( ; wpntIdx < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++wpntIdx )
    {
        int thingNum = sithAIUtil_aAIWpnts[wpntIdx].thingNum;
        if ( !sithAIUtil_IsWpntActive(wpntIdx) || thingNum == -1 )
        {
            continue;
        }

        if ( pWorld->lastThingIdx < thingNum || pWorld->aThings[thingNum].type != SITH_THING_GHOST )
        {
            SITHLOG_ERROR("AIFindNearestWpnt: Invalid waypoint (#%d).\n", wpntIdx);
            continue;
        }

        rdVector3 vec;
        rdVector_Sub3(&vec, &pThing->pos, &pWorld->aThings[thingNum].pos);
        float distSqr = vec.z * vec.z + vec.y * vec.y + vec.x * vec.x;

        sithAIUtil_aWpntDistances[numDistances].num      = wpntIdx;
        sithAIUtil_aWpntDistances[numDistances].distance = distSqr;
        bFound = true;

        float reachDist = pThing->collide.movesize - -0.03f;
        if ( distSqr <= reachDist * reachDist )
        {
            return wpntIdx; // At waypoint
        }

        ++numDistances;
    }

    if ( !bFound )
    {
        return -1;
    }

    qsort(sithAIUtil_aWpntDistances, numDistances, sizeof(sithAIUtil_aWpntDistances[0]), (int (*)(const void*, const void*))sithAIUtil_CompareAIWpntDistances);

    // Get nearest waypoint, optionally the nearest reachable
    int nearest = sithAIUtil_aWpntDistances[0].num;
    float hitDist;
    rdVector3 hitNorm;
    if ( a2 )
    {
        bFound = false;
        for ( wpntIdx = 0; wpntIdx < numDistances; ++wpntIdx )
        {
            nearest = sithAIUtil_aWpntDistances[wpntIdx].num;
            if ( !sithAIUtil_CheckPathToPoint(pThing, &sithAIUtil_GetWpntGhost(nearest)->pos, pThing->collide.movesize, &hitDist, &hitNorm, 1, 0) )
            {
                bFound = true;
                break;
            }
        }

        if ( !bFound )
        {
            return -1;
        }
    }

    if ( pThing == sithPlayer_g_pLocalPlayerThing )
    {
        return nearest;
    }

    // Collect the reachable waypoints linked to the nearest one and select one of them
    // Note, when a2 is not set wpntIdx is past the last waypoint
    int aCandidates[1 + STD_ARRAYLEN(sithAIUtil_aAIWpnts[0].aWpntLinks)];
    aCandidates[0] = nearest;
    size_t numCandidates = 1;

    const SithAIWaypoint* pNearest = &sithAIUtil_aAIWpnts[nearest];
    for ( size_t i = wpntIdx + 1; i < numDistances; ++i )
    {
        int distWpnt = sithAIUtil_aWpntDistances[i].num;
        for ( int j = 0; j < pNearest->numUsedLinks; ++j )
        {
            if ( pNearest->aWpntLinks[j] == distWpnt
                && !sithAIUtil_CheckPathToPoint(pThing, &sithAIUtil_GetWpntGhost(distWpnt)->pos, pThing->collide.movesize, &hitDist, &hitNorm, 1, 0) )
            {
                aCandidates[numCandidates++] = distWpnt;
                break;
            }
        }

        if ( numCandidates >= (size_t)pNearest->numUsedLinks + 1 )
        {
            break;
        }
    }

    return sithAIUtil_sub_49CC60(pThing->controlInfo.aiControl.pLocal, numCandidates, aCandidates, 0, -1);
}

// Flood fills the waypoint path depth (255 at start waypoint decreasing with each link)
static void sithAIUtil_FillWpntPathDepth(int startWpnt)
{
    int aStack[STD_ARRAYLEN(sithAIUtil_aAIWpnts)];
    int top = 0;
    aStack[0] = startWpnt;
    sithAIUtil_aAIWpnts[startWpnt].pathDepth = 0xFF;
    do
    {
        int wpnt = aStack[top--];
        const SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[wpnt];
        unsigned int depth = pWpnt->pathDepth;
        for ( int i = 0; i < pWpnt->numUsedLinks; ++i )
        {
            int link = pWpnt->aWpntLinks[i];
            SithAIWaypoint* pLink = &sithAIUtil_aAIWpnts[link];
            if ( sithAIUtil_IsWpntActive(link) && (unsigned int)pLink->pathDepth < depth - 1 )
            {
                pLink->pathDepth = depth - 1;
                if ( pLink->numUsedLinks > 1 )
                {
                    aStack[++top] = link;
                }
            }
        }
    } while ( top >= 0 );
}

// Updates waypoint path depth from the waypoint nearest to the local player
void J3DAPI sithAIUtil_sub_49D640()
{
    SithThing* pPlayer = sithPlayer_g_pLocalPlayerThing;
    if ( !pPlayer || (pPlayer->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
    {
        return;
    }

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++i )
    {
        sithAIUtil_aAIWpnts[i].pathDepth = 0;
    }

    int nearest = sithAIUtil_AIFindNearestWpnt(pPlayer, 0);
    if ( nearest != -1 )
    {
        sithAIUtil_FillWpntPathDepth(nearest);
    }
}

// Updates waypoint path depth from the waypoint thing pThing
void J3DAPI sithAIUtil_sub_49D750(SithThing* pThing)
{
    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++i )
    {
        sithAIUtil_aAIWpnts[i].pathDepth = 0;
    }

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++i )
    {
        if ( sithAIUtil_aAIWpnts[i].thingNum == pThing->idx )
        {
            sithAIUtil_FillWpntPathDepth(i);
            return;
        }
    }
}

// Adds AI to the waypoint tracking list (or updates existing entry), returns waypoint owner num or -1 if list is full
int J3DAPI sithAIUtil_CreateAIWaypoint(SithAIControlBlock* pLocal, float degTurn)
{
    int freeNum  = -1;
    int ownerNum = -1;
    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aWpntOwners); ++i )
    {
        if ( sithAIUtil_aWpntOwners[i].thingNum == -1 && freeNum == -1 )
        {
            freeNum = i;
        }

        if ( sithAIUtil_aWpntOwners[i].thingNum == pLocal->pOwner->idx )
        {
            ownerNum = i;
            break;
        }
    }

    if ( ownerNum == -1 )
    {
        ownerNum = freeNum;
    }

    if ( ownerNum >= 0 )
    {
        degTurn = STDMATH_CLAMP(degTurn, 5.0f, 180.0f);

        float sinTurn, cosTurn;
        stdMath_SinCos(degTurn, &sinTurn, &cosTurn);
        sithAIUtil_aWpntOwners[ownerNum].thingNum          = pLocal->pOwner->idx;
        sithAIUtil_aWpntOwners[ownerNum].cosTurnAlignAngle = cosTurn;
    }

    return ownerNum;
}

void J3DAPI sithAIUtil_ClearAIWaypoint(SithAIControlBlock* pLocal)
{
    int ownerNum = sithAIUtil_GetAIWaypointNum(pLocal);
    if ( ownerNum >= 0 )
    {
        sithAIUtil_aWpntOwners[ownerNum].thingNum          = -1;
        sithAIUtil_aWpntOwners[ownerNum].waypointNum       = 0;
        sithAIUtil_aWpntOwners[ownerNum].cosTurnAlignAngle = 0.0f;
    }
}

int J3DAPI sithAIUtil_GetAIWaypointNum(SithAIControlBlock* pLocal)
{
    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aWpntOwners); ++i )
    {
        if ( sithAIUtil_aWpntOwners[i].thingNum == pLocal->pOwner->idx )
        {
            return i;
        }
    }

    return -1;
}

int J3DAPI sithAIUtil_CompareAIWpntDistances(const SithAIWaypointDistance* pDist1, const SithAIWaypointDistance* pDist2)
{
    if ( pDist1 && pDist2 )
    {
        if ( pDist1->distance < pDist2->distance )
        {
            return -1;
        }

        if ( pDist2->distance < pDist1->distance )
        {
            return 1;
        }
    }

    return 0;
}

// Draws the links between active waypoints (debug)
void sithAIUtil_RenderAIWaypoints(void)
{
    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    if ( !sithAIUtil_g_bRenderAIWpnts )
    {
        sithAIUtil_secLastUpdate = 0.0f;
        return;
    }

    if ( sithTime_g_secGameTime - sithAIUtil_secLastUpdate < 1.0f )
    {
        return;
    }

    for ( size_t i = 0; i < STD_ARRAYLEN(sithAIUtil_aAIWpnts); ++i )
    {
        const SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[i];
        int thingNum = pWpnt->thingNum;
        if ( !sithAIUtil_IsWpntActive(i) )
        {
            continue;
        }

        for ( int j = 0; j < pWpnt->numUsedLinks; ++j )
        {
            unsigned int link = pWpnt->aWpntLinks[j];
            if ( i <= link && sithAIUtil_IsWpntActive(link) ) // Each connection is drawn only once
            {
                rdVector3 offset = { 0.0f, 0.0f, 0.1f };
                rdVector3 endPos = sithAIUtil_GetWpntGhost(link)->pos;
                endPos.z = endPos.z - -0.1f;
                sithFX_CreateLaserThing(&pWorld->aThings[thingNum], &offset, &endPos, 0.05f, 0.05f, 1.0f - -0.1f);
                sithAIUtil_secLastUpdate = sithTime_g_secGameTime;
            }
        }
    }
}

// Returns 1 if path from viewer to target is blocked, pDistance is set to the distance to hit and pHitNorm to hit normal.
// Note, when bSkipFloor is 0 floor surfaces are ignored.
int J3DAPI sithAIUtil_CheckPathToPoint(SithThing* pViewer, const rdVector3* pTarget, float radius, float* pDistance, rdVector3* pHitNorm, int bDetectThings, int bSkipFloor)
{
    int bBlocked = 0;
    SITH_ASSERTREL(pViewer && pTarget);

    rdVector3 dir;
    rdVector_Sub3(&dir, pTarget, &pViewer->pos);
    float dist = rdVector_Normalize3Acc(&dir);
    *pDistance = dist;

    sithCollision_SearchForCollisions(pViewer->pInSector, pViewer, &pViewer->pos, &dir, dist, radius, bDetectThings ? 0x102 : 0x103);
    SithCollision* pCollision = sithCollision_PopStack();
    if ( pCollision )
    {
        while ( (!bSkipFloor && (pCollision->type & SITHCOLLISION_WORLD) != 0 && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0)
            || ((pCollision->type & (SITHCOLLISION_ADJOINTOUCH | SITHCOLLISION_ADJOINCROSS)) != 0 && (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_MOVE) != 0) )
        {
            pCollision = sithCollision_PopStack();
            if ( !pCollision )
            {
                sithCollision_DecreaseStackLevel();
                return 0;
            }
        }

        *pDistance = pCollision->distance;
        bBlocked   = 1;
        *pHitNorm  = pCollision->hitNorm;
    }

    sithCollision_DecreaseStackLevel();
    return bBlocked;
}

// Checks if AI can stand at endPos. Returns 0 if it can't, otherwise ground type (see sithAIUtil_sub_49DD20).
int J3DAPI sithAIUtil_sub_49DC90(const SithAIControlBlock* pLocal, rdVector3* endPos, int* pbNoCollision)
{
    SithThing* pOwner = pLocal->pOwner;
    SITH_ASSERTREL(pOwner);

    SithSector* pSector = sithCollision_FindSectorInRadius(pOwner->pInSector, &pOwner->pos, endPos, 0.0f);
    if ( !pSector || (pSector->flags & SITH_SECTOR_NOACTORENTER) != 0 )
    {
        return 0;
    }

    if ( (pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) == 0 && (pSector->flags & SITH_SECTOR_UNDERWATER) != 0 && (pOwner->flags & SITH_TF_WATERDESTROYED) != 0 )
    {
        return 0;
    }

    return sithAIUtil_sub_49DD20(pLocal, endPos, pSector, pbNoCollision);
}

// Probes the ground under the startPos. Returns:
// 0 - can't stand there, 1 - floor, 2 - steep floor or not floor surface, 4 - thing, 8 - no ground (cliff)
// When pbNoCollision is set it's set to 0 if the ground is the same as the surface/thing AI is attached to, otherwise 1.
int J3DAPI sithAIUtil_sub_49DD20(const SithAIControlBlock* pLocal, const rdVector3* startPos, SithSector* pStartSector, int* pbNoCollision)
{
    SithThing* pOwner = pLocal->pOwner;
    int result = 8;
    SITH_ASSERTREL(pOwner);

    rdVector3 downDir;
    if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
    {
        rdVector_Neg3(&downDir, &pOwner->orient.uvec);
    }
    else if ( rdVector_Dot3(&pOwner->orient.uvec, &rdroid_g_zVector3) <= -0.707f )
    {
        downDir = rdroid_g_zVector3; // upside down
    }
    else
    {
        rdVector_Neg3(&downDir, &rdroid_g_zVector3);
    }

    sithCollision_SearchForCollisions(pStartSector, pOwner, startPos, &downDir, sithPhysics_GetThingHeight(pOwner) + pLocal->pClass->maxStep, 0.001f, 0);
    SithCollision* pCollision = sithCollision_PopStack();
    if ( !pCollision )
    {
        sithCollision_DecreaseStackLevel();
        return result;
    }

    for ( ; pCollision; pCollision = sithCollision_PopStack() )
    {
        SithCollisionType type = pCollision->type;
        if ( (type & (SITHCOLLISION_ADJOINTOUCH | SITHCOLLISION_ADJOINCROSS)) != 0 )
        {
            SithSectorFlag sectorFlags = pCollision->pSurfaceCollided->pAdjoin->pAdjoinSector->flags;
            if ( (sectorFlags & SITH_SECTOR_NOACTORENTER) != 0
                || ((pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) == 0 && (sectorFlags & SITH_SECTOR_UNDERWATER) != 0 && (pOwner->flags & SITH_TF_WATERDESTROYED) != 0) )
            {
                sithCollision_DecreaseStackLevel();
                return 0;
            }
        }

        if ( (type & SITHCOLLISION_WORLD) != 0 )
        {
            SITH_ASSERTREL(pCollision->pSurfaceCollided);
            SithSurface* pSurface = pCollision->pSurfaceCollided;
            SithSurfaceFlag surfFlags = pSurface->flags;
            if ( (surfFlags & SITH_SURFACE_NOAIMOVE) != 0 )
            {
                sithCollision_DecreaseStackLevel();
                return 0;
            }

            if ( pLocal->allowedSurfaceTypes == 0 )
            {
                if ( (surfFlags & SITH_SURFACE_LAVA) != 0 )
                {
                    sithCollision_DecreaseStackLevel();
                    return 0;
                }
            }
            else if ( (surfFlags & pLocal->allowedSurfaceTypes) == 0 )
            {
                sithCollision_DecreaseStackLevel();
                return 0;
            }

            result = 2;
            if ( (surfFlags & SITH_SURFACE_ISFLOOR) != 0 )
            {
                result = 1;
                if ( (pOwner->moveInfo.physics.flags & SITH_PF_ALIGNUP) != 0
                    && (pOwner->attach.flags & SITH_ATTACH_SURFACE) != 0
                    && pOwner->attach.attachedToStructure.pSurfaceAttached != pSurface
                    && rdVector_Dot3(&pSurface->face.normal, &rdroid_g_zVector3) < 0.788f )
                {
                    result = 2;
                }
            }

            if ( pbNoCollision )
            {
                if ( (pOwner->attach.flags & SITH_ATTACH_SURFACE) != 0 && pOwner->attach.attachedToStructure.pSurfaceAttached == pSurface )
                {
                    *pbNoCollision = 0;
                }
                else
                {
                    *pbNoCollision = 1;
                }
            }

            sithCollision_DecreaseStackLevel();
            return result;
        }

        if ( (type & SITHCOLLISION_THING) != 0 )
        {
            SITH_ASSERTREL(pCollision->pThingCollided);
            SithThing* pHitThing = pCollision->pThingCollided;

            // Note, the original checks type bits 0xA (SITH_THING_PLAYER)
            if ( ((pHitThing->attach.flags & SITH_ATTACH_THING) == 0 || pHitThing->attach.attachedToStructure.pThingAttached != pOwner)
                && (!pOwner->pAttachedThing || (pOwner->pAttachedThing->attach.flags & SITH_ATTACH_TAIL) == 0)
                && (pHitThing->flags & SITH_TF_STANDON) == 0
                && (pHitThing->type & SITH_THING_PLAYER) == 0 )
            {
                sithCollision_DecreaseStackLevel();
                return 4;
            }

            result = 1;
            if ( pbNoCollision )
            {
                if ( (pOwner->attach.flags & SITH_ATTACH_THINGFACE) != 0 && pOwner->attach.attachedToStructure.pThingAttached == pHitThing )
                {
                    *pbNoCollision = 0;
                }
                else
                {
                    *pbNoCollision = 1;
                }
            }

            sithCollision_DecreaseStackLevel();
            return result;
        }
    }

    sithCollision_DecreaseStackLevel();
    return 8;
}

// Makes a random point at distance *pDownCos from pPos in direction derived from pDir.
// Flags:
//   0x1    - direction perpendicular to pDir (sideways)
//   0x2    - rotate pDir by yaw angle
//   0x4    - rotate pDir by pitch angle (keeps pDir z component)
//   0x800  - sideways direction in the move direction
//   0x4000 - random angle within angle
//   0x8000 - random direction sign
//   0x100500/0x1200 - direction towards/away from move position (see sithAIUtil_AIGetMovePos)
//   0x10000 - distance is the distance to move position along the direction
// The made direction is stored so sithAIUtil_RetryMakeRandPoint can try the opposite direction.
int J3DAPI sithAIUtil_MakeRandPoint(SithAIControlBlock* pLocal, rdVector3* pPos, int flags, float angle, float* pDownCos, rdVector3* pDir, rdVector3* pDestPoint)
{
    int bMade = 0;
    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;

    if ( !pPos || !pDir || !pDestPoint )
    {
        SITHLOG_ERROR("sithAIUtil_MakeRandPoint: Bad parameters.\n");
        return 0;
    }

    sithAIUtil_mkPoinCurFlags   = 0;
    sithAIUtil_pMkPointCurLocal = NULL;

    if ( (flags & 0x4) == 0 )
    {
        pDir->z = 0.0f;
        rdVector_Normalize3Acc(pDir);
    }

    rdVector3 newDir;
    rdVector3 pyr = { 0 };
    if ( (flags & 0x6) == 0 )
    {
        if ( (flags & 0x1) == 0 )
        {
            return 0;
        }

        // Sideways
        rdVector_Cross3(&newDir, &pOwner->orient.uvec, pDir);
        if ( (flags & 0x800) == 0 )
        {
            if ( (flags & 0x8000) != 0 && sithAIUtil_Random() > 0.5f )
            {
                rdVector_Neg3Acc(&newDir);
            }

            if ( (flags & 0x4000) != 0 )
            {
                pyr.x = 0.0f;
                pyr.y = sithAIUtil_Random() * angle;
                pyr.z = 0.0f;
                if ( sithAIUtil_Random() > 0.5f )
                {
                    rdVector_Neg3Acc(&pyr);
                }

                rdVector_Rotate3Acc(&newDir, &pyr);
            }
        }
        else if ( rdVector_Dot3(&pLocal->moveDirection, &newDir) < 0.0f )
        {
            rdVector_Neg3Acc(&newDir);
        }
    }
    else
    {
        float pitch = (flags & 0x4) != 0 ? angle : 0.0f;
        float yaw   = (flags & 0x2) != 0 ? angle : 0.0f;
        if ( (flags & 0x4000) != 0 )
        {
            if ( pitch != 0.0f )
            {
                pitch = sithAIUtil_Random() * pitch;
            }

            if ( yaw != 0.0f )
            {
                yaw = sithAIUtil_Random() * yaw;
            }
        }

        pyr.x = pitch;
        pyr.y = yaw;
        pyr.z = 0.0f;
        if ( (flags & 0x8000) != 0 && sithAIUtil_Random() > 0.5f )
        {
            rdVector_Neg3Acc(&pyr);
        }

        rdVector_Rotate3(&newDir, pDir, &pyr);
    }

    bMade = 1;

    if ( (flags & 0x101700) != 0 )
    {
        // Direction relative to move position
        rdVector3 movePos = pOwner->pos; // Added: Init
        sithAIUtil_AIGetMovePos(pLocal, flags, &movePos);

        rdVector3 toMovePos;
        rdVector_Sub3(&toMovePos, &movePos, &pOwner->pos);
        rdVector_Normalize3Acc(&toMovePos);

        bool bAway = rdVector_Dot3(&toMovePos, &newDir) <= 0.0f;
        if ( ((flags & 0x100500) != 0 && bAway) || ((flags & 0x1200) != 0 && !bAway) )
        {
            if ( (flags & 0x2) == 0 )
            {
                rdVector_Neg3Acc(&newDir);
            }
            else
            {
                rdVector_Neg3Acc(&pyr);
                rdVector_Rotate3(&newDir, pDir, &pyr);
            }
        }

        if ( (flags & 0x10000) != 0 )
        {
            rdVector3 vec;
            rdVector_Sub3(&vec, pPos, &movePos);
            *pDownCos = -rdVector_Dot3(&vec, &newDir);
        }
    }

    if ( stdMath_ClipNearZero(*pDownCos) == 0.0f )
    {
        return 0;
    }

    // Save for retry
    sithAIUtil_mkPoinCurFlags   = flags;
    sithAIUtil_pMkPointCurLocal = pLocal;
    sithAIUtil_vec_585490       = *pDir;
    if ( (flags & 0x2) == 0 )
    {
        sithAIUtil_vec_585718 = newDir;
    }
    else
    {
        sithAIUtil_mkPointCurPYR = pyr;
    }

    rdVector_ScaleAdd3(pDestPoint, &newDir, *pDownCos, pPos);
    *pDir     = newDir;
    *pDownCos = fabsf(*pDownCos);
    return bMade;
}

// Makes the point in the opposite direction of the last point made by sithAIUtil_MakeRandPoint
int J3DAPI sithAIUtil_RetryMakeRandPoint(SithAIControlBlock* pLocal, rdVector3* pPos, float a3, rdVector3* pDirection, rdVector3* pDestPoint)
{
    if ( !sithAIUtil_mkPoinCurFlags || !sithAIUtil_pMkPointCurLocal || !pLocal )
    {
        return 0;
    }

    if ( pLocal != sithAIUtil_pMkPointCurLocal || !pPos || !pDirection || !pDestPoint )
    {
        SITHLOG_ERROR("sithAIUtil_RetryMakeRandPoint: Bad parameters.\n");
        return 0;
    }

    if ( (sithAIUtil_mkPoinCurFlags & 0x10400) != 0 )
    {
        sithAIUtil_mkPoinCurFlags   = 0;
        sithAIUtil_pMkPointCurLocal = NULL;
        return 0;
    }

    rdVector3 dir;
    if ( (sithAIUtil_mkPoinCurFlags & 0x6) == 0 )
    {
        if ( (sithAIUtil_mkPoinCurFlags & 0x1) == 0 )
        {
            sithAIUtil_mkPoinCurFlags   = 0;
            sithAIUtil_pMkPointCurLocal = NULL;
            return 0;
        }

        rdVector_Neg3(&dir, &sithAIUtil_vec_585718);
        sithAIUtil_mkPoinCurFlags &= ~0x1;
    }
    else
    {
        rdVector_Neg3Acc(&sithAIUtil_mkPointCurPYR);
        rdVector_Rotate3(&dir, &sithAIUtil_vec_585490, &sithAIUtil_mkPointCurPYR);
        sithAIUtil_mkPoinCurFlags &= ~0x6;
    }

    rdVector_ScaleAdd3(pDestPoint, &dir, a3, pPos);
    *pDirection = dir;
    return 1;
}

// Checks path from startPos in pDirection. Returns 0 if path is blocked, otherwise:
// 1 - path is clear, 2 - wall hit, 4 - thing hit
// Flags:
//   0x10     - check the path straight ahead of moveDist
//   0x20     - check the path sideways to the left and right of the direction and pick the better side
//   0x2000   - visibility from goal thing, 0x800000 - visibility from target thing
//   0x100    - prefer side closer to goal, 0x200 - prefer side farther from goal
//   0x20000  - ignore player, 0x40000 - ignore things, 0x80000 - ignore floor
int J3DAPI sithAIUtil_CheckPathToPos(SithAIControlBlock* pLocal, SithSector* pStartSector, rdVector3* startPos, int flags, float minMoveDist, float moveDist, float radius, rdVector3* pDirection, rdVector3* a9)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;

    if ( !pDirection )
    {
        SITHLOG_ERROR("sithAIUtil_CheckPathToPos: Bad direction vector.\n");
        return 0;
    }

    if ( radius == 0.0f )
    {
        radius = pOwner->collide.movesize;
    }

    if ( (flags & 0x10) != 0 )
    {
        rdVector3 endPos;
        rdVector_ScaleAdd3(&endPos, pDirection, moveDist, startPos);

        int result = 1;
        sithCollision_SearchForCollisions(pStartSector, pOwner, startPos, pDirection, moveDist, radius, 0x102);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            if ( ((flags & 0x20000) != 0 && (pCollision->type & SITHCOLLISION_THING) != 0 && pCollision->pThingCollided->type == SITH_THING_PLAYER)
                || ((flags & 0x40000) != 0 && (pCollision->type & SITHCOLLISION_THING) != 0)
                || ((flags & 0x80000) != 0 && (pCollision->type & SITHCOLLISION_WORLD) != 0 && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0) )
            {
                continue; // ignore
            }

            if ( pCollision->distance < minMoveDist )
            {
                result = 0;
                break;
            }

            rdVector_ScaleAdd3(&endPos, pDirection, pCollision->distance, startPos);
            if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
            {
                result = 4;
                break;
            }

            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                result = 2;
                break;
            }
        }

        sithCollision_DecreaseStackLevel();
        if ( result )
        {
            SithThing* pViewer = NULL;
            if ( (flags & 0x2000) != 0 && pLocal->goalThing )
            {
                pViewer = pLocal->goalThing;
            }

            if ( (flags & 0x800000) != 0 && pLocal->pTargetThing )
            {
                pViewer = pLocal->pTargetThing;
            }

            // End position must be visible from the viewer
            rdVector3 toTarget;
            float dist;
            if ( pViewer && sithAIUtil_GetDistanceToTargetPos(pViewer, &pViewer->pos, &endPos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &toTarget, &dist) == 3 )
            {
                return 0;
            }

            if ( a9 )
            {
                *a9 = endPos;
            }
        }

        return result;
    }

    if ( (flags & 0x20) == 0 )
    {
        return 0;
    }

    // Probe sideways, [0] - one side, [1] - the other side
    int aResults[2] = { 0 };
    int aScores[2]  = { 0 };
    rdVector3 aPos[2];
    aPos[0] = *startPos; // Added: Init
    aPos[1] = *startPos; // Added: Init

    rdVector3 sideDir;
    rdVector_Cross3(&sideDir, &pOwner->orient.uvec, pDirection);
    sithAIUtil_ClipVectorNearZero(&sideDir);
    if ( rdVector_Normalize3Acc(&sideDir) == 0.0f )
    {
        rdVector_Cross3(&sideDir, pDirection, &pLocal->pOwner->orient.rvec);
        sithAIUtil_ClipVectorNearZero(&sideDir);
        rdVector_Normalize3Acc(&sideDir);
    }

    int side   = 1;
    bool bDone = false;
    float dist = minMoveDist;
    do
    {
        if ( bDone )
        {
            if ( --side < 0 )
            {
                break;
            }

            rdVector_Neg3Acc(&sideDir);
            bDone = false;
            dist  = minMoveDist;
        }

        dist = dist - -0.03f;
        if ( dist > moveDist )
        {
            bDone = true;
            continue;
        }

        rdVector_ScaleAdd3(&aPos[side], &sideDir, dist, startPos);
        SithSector* pSector = sithCollision_FindSectorInRadius(pStartSector, startPos, &aPos[side], radius);
        if ( !pSector )
        {
            bDone = true;
            continue;
        }

        sithCollision_SearchForCollisions(pStartSector, pOwner, startPos, &sideDir, dist, radius, 0x102);
        SithCollision* pCollision = sithCollision_PopStack();
        sithCollision_DecreaseStackLevel();
        if ( !pCollision )
        {
            // Check the path in the direction from the side position
            aResults[side] = sithAIUtil_CheckPathToPos(pLocal, pSector, &aPos[side], 0x80010, pOwner->collide.movesize - -0.03f, radius, 0.0f, pDirection, NULL);
            if ( aResults[side] != 1 )
            {
                continue; // move further out
            }
        }
        else
        {
            rdVector_ScaleAdd3(&aPos[side], &sideDir, pCollision->distance, startPos);
            if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
            {
                aResults[side] = 4;
            }
            else if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                aResults[side] = 2;
            }
        }

        bDone = true;
    } while ( side >= 0 );

    // Score the sides by visibility & distance to goal
    if ( pLocal->goalThing )
    {
        for ( size_t i = 0; i < 2; ++i )
        {
            if ( sithAIUtil_GetDistanceToTargetPos(pLocal->goalThing, &pLocal->goalThing->pos, &aPos[i], -1.0f, pLocal->pClass->sightDistance, 0.0f, &pLocal->vecUnknown0, &pLocal->targetDistance) == 3 )
            {
                if ( (flags & 0x2000) != 0 )
                {
                    aScores[i]--;
                }
            }
            else
            {
                aScores[i]++;
            }
        }

        const rdVector3* pGoalPos = &pLocal->goalThing->pos;
        float dx0 = aPos[0].x - pGoalPos->x;
        float dy0 = aPos[0].y - pGoalPos->y;
        float dx1 = aPos[1].x - pGoalPos->x;
        float dy1 = aPos[1].y - pGoalPos->y;

        size_t closer = sqrtf(dy1 * dy1 + dx1 * dx1) <= sqrtf(dy0 * dy0 + dx0 * dx0) ? 1 : 0;
        if ( (flags & 0x100) != 0 )
        {
            aScores[closer]++;
        }
        else if ( (flags & 0x200) != 0 )
        {
            aScores[closer]--;
        }
    }

    size_t best = aScores[0] < aScores[1] ? 1 : 0;
    if ( a9 )
    {
        *a9 = aPos[best];
    }

    return aResults[best];
}

// Searches for a point in direction of pPos within angle (increased by deltaAngle up to maxAngle) which path from AI is blocked.
// If found, AI is set to move to that point.
int J3DAPI sithAIUtil_sub_49EE50(SithAIControlBlock* pLocal, rdVector3* pPos, float angle, float maxAngle, float deltaAngle, int flags)
{
    rdVector3 dir;
    dir.x = pPos->x - pLocal->pOwner->pos.x;
    dir.y = pPos->y - pLocal->pOwner->pos.y;
    dir.z = 0.0f;
    float dist = rdVector_Normalize3Acc(&dir);

    int mkFlags = flags & 0x0FFF7F00;
    while ( angle <= maxAngle )
    {
        rdVector3 mkDir = dir;
        rdVector3 destPos;
        if ( !sithAIUtil_MakeRandPoint(pLocal, &pLocal->pOwner->pos, mkFlags | 0x8002, angle, &dist, &mkDir, &destPos) )
        {
            return 0;
        }

        float hitDist;
        rdVector3 hitNorm;
        int bHit = sithAIUtil_CheckPathToPoint(pLocal->pOwner, &destPos, pLocal->pOwner->collide.movesize, &hitDist, &hitNorm, 1, 0);
        if ( !bHit )
        {
            while ( angle != 0.0f && angle != 180.0f )
            {
                if ( !sithAIUtil_RetryMakeRandPoint(pLocal, &pLocal->pOwner->pos, dist, &dir, &destPos) )
                {
                    break;
                }

                bHit = sithAIUtil_CheckPathToPoint(pLocal->pOwner, &destPos, pLocal->pOwner->collide.movesize, &hitDist, &hitNorm, 1, 0);
                if ( bHit )
                {
                    break;
                }
            }
        }

        if ( bHit )
        {
            sithAIMove_StopAIMovement(pLocal);
            sithAIMove_AISetLookPosEyeLevel(pLocal, &destPos);
            sithAIMove_AISetMoveTargetPos(pLocal, &destPos, pLocal->moveSpeed);
            return 1;
        }

        angle = angle + deltaAngle;
        if ( angle > maxAngle )
        {
            return 0;
        }
    }

    return 0;
}

// Makes random reachable position around pPos at distance moveDist
int J3DAPI sithAIUtil_sub_49F010(SithAIControlBlock* pLocal, rdVector3* pPos, float minDist, float moveDist, int bSomeBool, int maxRetries, rdVector3* pDestPos)
{
    SithThing* pOwner = pLocal->pOwner;
    rdVector3 dir     = pOwner->orient.lvec;
    int result        = 0;

    unsigned int numTries = 0;
    do
    {
        if ( !sithAIUtil_MakeRandPoint(pLocal, pPos, bSomeBool ? 0xC006 : 0xC002, 360.0f, &moveDist, &dir, pDestPos) )
        {
            return 0;
        }

        do
        {
            result = sithAIUtil_CheckPathToPos(pLocal, pLocal->pOwner->pInSector, &pLocal->pOwner->pos, bSomeBool ? 0x10 : 0x80010, minDist, moveDist, 0.0f, &dir, pDestPos);
            if ( result )
            {
                return 1;
            }
        } while ( sithAIUtil_RetryMakeRandPoint(pLocal, &pLocal->pOwner->pos, moveDist, &dir, pDestPos) );

        ++numTries;
    } while ( numTries < (unsigned int)maxRetries );

    return result != 0;
}

int J3DAPI sithAIUtil_MakePathPos(SithAIControlBlock* pLocal, rdVector3* pPos, float minDist, float moveDist, float angle, int flags, rdVector3* pDirection, rdVector3* pDestPos)
{
    if ( !sithAIUtil_MakeRandPoint(pLocal, pPos, flags, angle, &moveDist, pDirection, pDestPos) )
    {
        return 0;
    }

    int pathFlags = (flags & 0x4) != 0 ? ((flags & 0x8E2000) | 0x10) : ((flags & 0x8E2000) | 0x80010);
    if ( sithAIUtil_CheckPathToPos(pLocal, pLocal->pOwner->pInSector, &pLocal->pOwner->pos, pathFlags, minDist, moveDist, 0.0f, pDirection, pDestPos) )
    {
        return 1;
    }

    do
    {
        if ( !sithAIUtil_RetryMakeRandPoint(pLocal, &pLocal->pOwner->pos, moveDist, pDirection, pDestPos) )
        {
            return 0;
        }
    } while ( !sithAIUtil_CheckPathToPos(pLocal, pLocal->pOwner->pInSector, &pLocal->pOwner->pos, pathFlags, minDist, moveDist, 0.0f, pDirection, pDestPos) );

    return 1;
}

// Returns 0 if any other human AI which is closer to the target is in the line of fire from pPos to the target
int J3DAPI sithAIUtil_sub_49F1F0(const SithAIControlBlock* pLocal, const rdVector3* pPos)
{
    if ( !pLocal || !pLocal->pOwner || pLocal->pOwner->controlType != SITH_CT_AI || !sithWeapon_HasWeaponSelected(pLocal->pOwner) || !pLocal->pTargetThing )
    {
        return 1;
    }

    rdVector3 toTarget;
    rdVector_Sub3(&toTarget, &pLocal->pTargetThing->pos, pPos);
    float dist = rdVector_Normalize3Acc(&toTarget);

    for ( int i = 0; i <= sithAI_g_lastUsedAIIndex; ++i )
    {
        const SithAIControlBlock* pOther = &sithAI_g_aControlBlocks[i];
        const SithThing* pOtherOwner = pOther->pOwner;
        if ( pOtherOwner
            && pOtherOwner->controlType == SITH_CT_AI
            && (pOtherOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) != 0
            && (pOther->mode & SITHAI_MODE_TARGETVISIBLE) != 0
            && pOther != pLocal
            && rdVector_Dot3(&pOther->toTarget, &toTarget) >= 0.966f
            && dist > pOther->distance )
        {
            return 0;
        }
    }

    return 1;
}

void J3DAPI sithAIUtil_AIPlaySoundMode(SithAIControlBlock* pLocal, SithSoundClassMode mode)
{
    if ( !pLocal || !pLocal->pOwner )
    {
        return;
    }

    // Talk sound modes are handled by the talk instinct
    if ( mode > SITHSOUNDCLASS_FIRE4 && mode < SITHSOUNDCLASS_STAND2WALK && sithAI_HasInstinct(pLocal, "talk") )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_TALK, (void*)mode);
        return;
    }

    sithSoundClass_PlayModeRandom(pLocal->pOwner, mode);
}

void J3DAPI sithAIUtil_AISetMode(SithAIControlBlock* pLocal, SithAIMode mode)
{
    switch ( mode )
    {
        case SITHAI_MODE_MOVING:
            pLocal->mode |= SITHAI_MODE_MOVING;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4000;
            break;

        case SITHAI_MODE_ATTACKING:
            pLocal->mode = (pLocal->mode & ~(SITHAI_MODE_FLEEINGTOWAYPOINT | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_FLEEING | SITHAI_MODE_BLOCK | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_SEARCHING))
                | (SITHAI_MODE_ACTIVE | SITHAI_MODE_ATTACKING);
            break;

        case SITHAI_MODE_SEARCHING:
            pLocal->mode = (pLocal->mode & ~(SITHAI_MODE_FLEEINGTOWAYPOINT | SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_FLEEING
                | SITHAI_MODE_TARGETVISIBLE | SITHAI_MODE_ACTIVE | SITHAI_MODE_BLOCK | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_ATTACKING))
                | SITHAI_MODE_SEARCHING;
            sithActor_SetHeadPYR(pLocal->pOwner, &rdroid_g_zeroVector3);
            break;

        case SITHAI_MODE_BLOCK:
            pLocal->mode = (pLocal->mode & ~(SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_TARGETVISIBLE | SITHAI_MODE_ACTIVE | SITHAI_MODE_ATTACKING))
                | (SITHAI_MODE_BLOCK | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_SEARCHING);
            break;

        case SITHAI_MODE_FLEEING:
            pLocal->mode = (pLocal->mode & ~(SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_SLEEPING | SITHAI_MODE_TARGETVISIBLE
                | SITHAI_MODE_ACTIVE | SITHAI_MODE_BLOCK | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_SEARCHING | SITHAI_MODE_ATTACKING))
                | SITHAI_MODE_FLEEING;
            pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_1;
            break;

        case SITHAI_MODE_WALLCRAWLING:
            pLocal->mode |= SITHAI_MODE_WALLCRAWLING;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_100;
            break;

        case SITHAI_MODE_HUNTING:
            pLocal->mode = (pLocal->mode & ~(SITHAI_MODE_FLEEINGTOWAYPOINT | SITHAI_MODE_SLEEPING | SITHAI_MODE_FLEEING | SITHAI_MODE_BLOCK | SITHAI_MODE_NOCHECKFORCLIFF))
                | SITHAI_MODE_HUNTING;
            break;

        case SITHAI_MODE_FLEEINGTOWAYPOINT:
            pLocal->mode = (pLocal->mode & ~(SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_HUNTING | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_SLEEPING
                | SITHAI_MODE_TARGETVISIBLE | SITHAI_MODE_ACTIVE | SITHAI_MODE_BLOCK | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_SEARCHING | SITHAI_MODE_ATTACKING))
                | (SITHAI_MODE_FLEEINGTOWAYPOINT | SITHAI_MODE_FLEEING);
            break;

        default:
            break;
    }
}

void J3DAPI sithAIUtil_AIClearMode(SithAIControlBlock* pLocal, SithAIMode mode)
{
    switch ( mode )
    {
        case SITHAI_MODE_MOVING:
            pLocal->mode &= ~(SITHAI_MODE_UNKNOWN_10 | SITHAI_MODE_MOVING);
            pLocal->submode &= ~(SITHAI_SUBMODE_UNKNOWN_4000 | SITHAI_SUBMODE_UNKNOWN_10 | SITHAI_SUBMODE_UNKNOWN_8 | SITHAI_SUBMODE_UNKNOWN_4 | SITHAI_SUBMODE_UNKNOWN_2 | SITHAI_SUBMODE_UNKNOWN_1);
            break;

        case SITHAI_MODE_TURNING:
            pLocal->mode &= ~(SITHAI_MODE_UNKNOWN_80 | SITHAI_MODE_TURNING);
            pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_10;
            break;

        case SITHAI_MODE_WALLCRAWLING:
            pLocal->mode &= ~SITHAI_MODE_WALLCRAWLING;
            pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_100;
            break;

        case SITHAI_MODE_HUNTING:
        {
            SithAIMode curMode = pLocal->mode;
            pLocal->mode = curMode & ~SITHAI_MODE_HUNTING;
            if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) == 0 )
            {
                pLocal->mode = curMode & ~(SITHAI_MODE_FLEEINGTOWAYPOINT | SITHAI_MODE_TRAVERSEWPNTS | SITHAI_MODE_HUNTING);
                pLocal->submode &= ~SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION;
                sithAIUtil_ClearAIWaypoint(pLocal);
            }
        } break;

        case SITHAI_MODE_TRAVERSEWPNTS:
            pLocal->mode &= ~(SITHAI_MODE_FLEEINGTOWAYPOINT | SITHAI_MODE_TRAVERSEWPNTS | SITHAI_MODE_HUNTING);
            pLocal->submode &= ~SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION;
            sithAIUtil_ClearAIWaypoint(pLocal);
            break;

        default:
            break;
    }
}

void J3DAPI sithAIUtil_SetWeaponFireFlags(int weaponNum, SithAIUtilFireFlags* pFlags)
{
    switch ( weaponNum )
    {
        case 0:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_40;
            break;

        case 1:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_80;
            break;

        case 2:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_100;
            break;

        case 3:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_200;
            break;

        default:
            break;
    }
}

// Gets the move position by flags:
// 0x100000 - AI position, 0x400 - move position, 0x200000 - home position, 0x300 - goal thing position,
// 0x1000 - predicted position of the flee from thing (returns 0)
int J3DAPI sithAIUtil_AIGetMovePos(const SithAIControlBlock* pLocal, int flags, rdVector3* pDestPos)
{
    if ( (flags & 0x100000) != 0 )
    {
        *pDestPos = pLocal->pOwner->pos;
        return 1;
    }

    if ( (flags & 0x400) != 0 )
    {
        *pDestPos = pLocal->movePos;
        return 1;
    }

    if ( (flags & 0x200000) != 0 )
    {
        *pDestPos = pLocal->homePos;
        return 1;
    }

    if ( (flags & 0x300) != 0 )
    {
        if ( pLocal->goalThing )
        {
            *pDestPos = pLocal->goalThing->pos;
        }
        else
        {
            *pDestPos = pLocal->vecUnknown6;
        }

        return 1;
    }

    if ( (flags & 0x1000) != 0 && pLocal->pFleeFromThing )
    {
        rdVector_ScaleAdd3(pDestPos, &pLocal->pFleeFromThing->moveInfo.physics.velocity, sithTime_g_frameTimeFlex, &pLocal->pFleeFromThing->pos);
    }

    return 0;
}

signed int J3DAPI sithAIUtil_GetXYHeadingVector(const SithThing* pThing, rdVector3* pDest)
{
    if ( !pThing || !pDest )
    {
        SITHLOG_ERROR("Bad args passed to sithAIUtil_GetXYHeadingVector().\n");
        return 0;
    }

    if ( sithAIUtil_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        *pDest = pThing->orient.lvec;
    }
    else
    {
        *pDest = pThing->moveInfo.physics.velocity;
    }

    pDest->x = stdMath_ClipNearZero(pDest->x);
    pDest->y = stdMath_ClipNearZero(pDest->y);
    pDest->z = 0.0f;
    rdVector_Normalize3Acc(pDest);
    return 1;
}

signed int J3DAPI sithAIUtil_GetXYZHeadingVector(const SithThing* pThing, rdVector3* pDest)
{
    if ( !pThing || !pDest )
    {
        SITHLOG_ERROR("Bad args passed to sithAIUtil_GetXYHeadingVector().\n");
        return 0;
    }

    if ( sithAIUtil_IsZeroVector(&pThing->moveInfo.physics.velocity) )
    {
        *pDest = pThing->orient.lvec;
        return 1;
    }

    *pDest = pThing->moveInfo.physics.velocity;
    sithAIUtil_ClipVectorNearZero(pDest);
    rdVector_Normalize3Acc(pDest);
    return 1;
}

int J3DAPI sithAIUtil_CanAttackTarget(const SithThing* pThing)
{
    if ( !pThing || (pThing->flags & (SITH_TF_DISABLED | SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 )
    {
        return 0;
    }

    if ( pThing->type == SITH_THING_PLAYER && (pThing->thingInfo.actorInfo.flags & SITH_AF_CONTROLSDISABLED) != 0 )
    {
        return 0;
    }

    return 1;
}

int J3DAPI sithAIUtil_IsThingMoving(const SithThing* pThing)
{
    if ( !pThing || pThing->type == SITH_THING_FREE )
    {
        return 0;
    }

    if ( pThing->type == SITH_THING_ACTOR )
    {
        const SithAIControlBlock* pLocal = pThing->controlInfo.aiControl.pLocal;
        return pLocal ? (pLocal->mode & SITHAI_MODE_MOVING) : 0;
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        return !sithAIUtil_IsZeroVector(&pThing->moveInfo.physics.velocity) || !sithAIUtil_IsZeroVector(&pThing->moveInfo.physics.angularVelocity);
    }

    if ( pThing->moveType == SITH_MT_PATH )
    {
        return pThing->moveInfo.pathMovement.mode & SITH_PATHMOVE_MOVE;
    }

    return 0;
}

void J3DAPI sithAIUtil_ApplyForce(SithAIControlBlock* pLocal, const rdVector3* pDirection, float force)
{
    SithThing* pThing = pLocal->pOwner;
    rdVector3 vecForce;
    vecForce.x = pThing->moveInfo.physics.mass * force * pDirection->x;
    vecForce.y = pDirection->y * pThing->moveInfo.physics.mass * force;
    vecForce.z = pThing->moveInfo.physics.mass * force * pDirection->z;
    sithPhysics_ApplyForce(pThing, &vecForce);
}

// Randomly offsets unit vector by scalar and re-normalizes it
void J3DAPI sithAIUtil_RandomRotate(rdVector3* pOutPYR, float scalar)
{
    pOutPYR->x = (sithAIUtil_Random() - 0.5f) * scalar + pOutPYR->x;
    pOutPYR->y = (sithAIUtil_Random() - 0.5f) * scalar + pOutPYR->y;
    pOutPYR->z = (sithAIUtil_Random() - 0.5f) * scalar + pOutPYR->z;
    rdVector_Normalize3Acc(pOutPYR);
}

size_t J3DAPI sithAIUtil_GetThingsInView(const SithSector* pStartSector, const rdMatrix34* orient, float fovX, float fovY, int sizeResult, SithThing** pResult, int thingTypeMask, float maxDistance)
{
    SITH_ASSERTREL(pStartSector != NULL);
    if ( fovX < 0.0f || fovY < 0.0f )
    {
        return 0;
    }

    sithAIUtil_thingInViewTypeMask        = thingTypeMask;
    sithAIUtil_sizeThingsInView           = sizeResult;
    sithAIUtil_maxDistanceToThingsInView  = maxDistance;
    sithAIUtil_aThingsInView              = pResult;

    float sinAngle;
    stdMath_SinCos(90.0f - fovX * 0.5f, &sinAngle, &sithAIUtil_thingsInViewFovX);
    stdMath_SinCos(90.0f - fovY * 0.5f, &sinAngle, &sithAIUtil_thingsInViewFovY);

    sithAdvanceRenderTick();
    sithAIUtil_numThingsInView   = 0;
    sithAIUtil_numVisitedSectors = 0;
    sithAIUtil_GetNextThingsInView(pStartSector, orient, 0.0f);
    return sithAIUtil_numThingsInView;
}

void J3DAPI sithAIUtil_GetNextThingsInView(const SithSector* pSector, const rdMatrix34* pOrient, float distance)
{
    SithSector* pCurSector = (SithSector*)pSector; // render tick is updated
    if ( pCurSector->renderTick == sithMain_g_curRenderTick )
    {
        return;
    }

    pCurSector->renderTick = sithMain_g_curRenderTick;
    if ( sithAIUtil_numVisitedSectors >= 128 )
    {
        return;
    }

    ++sithAIUtil_numVisitedSectors;
    for ( SithThing* pThing = pCurSector->pFirstThingInSector; pThing && sithAIUtil_numThingsInView < (size_t)sithAIUtil_sizeThingsInView; pThing = pThing->pNextThingInSector )
    {
        if ( ((1 << (pThing->type & 0x1F)) & sithAIUtil_thingInViewTypeMask) == 0 || (pThing->flags & (SITH_TF_DISABLED | SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 )
        {
            continue;
        }

        rdVector3 dir;
        rdVector_Sub3(&dir, &pThing->pos, &pOrient->dvec);
        rdVector_Normalize3Acc(&dir);

        rdVector3 up, right, look;
        rdVector_Normalize3(&up, &pOrient->uvec);
        rdVector_Normalize3(&right, &pOrient->rvec);
        rdVector_Normalize3(&look, &pOrient->lvec);

        float upDot    = rdVector_Dot3(&up, &dir);
        float rightDot = rdVector_Dot3(&right, &dir);
        if ( upDot <= sithAIUtil_thingsInViewFovY && -sithAIUtil_thingsInViewFovY <= upDot
            && rightDot <= sithAIUtil_thingsInViewFovX && -sithAIUtil_thingsInViewFovX <= rightDot
            && rdVector_Dot3(&look, &dir) >= 0.0f )
        {
            if ( sithAIUtil_numThingsInView >= (size_t)sithAIUtil_sizeThingsInView )
            {
                return;
            }

            sithAIUtil_aThingsInView[sithAIUtil_numThingsInView++] = pThing;
        }
    }

    if ( distance > sithAIUtil_maxDistanceToThingsInView )
    {
        return;
    }

    // Continue through see-through adjoins facing the viewer
    for ( SithSurfaceAdjoin* pAdjoin = pCurSector->pFirstAdjoin; pAdjoin; pAdjoin = pAdjoin->pNextAdjoin )
    {
        SithSurface* pSurface = pAdjoin->pAdjoinSurface;
        rdMaterial* pMat      = pSurface->face.pMaterial;
        if ( (pAdjoin->flags & SITH_ADJOIN_VISIBLE) != 0
            && (!pMat || pSurface->face.geometryMode == RD_GEOMETRY_NONE || (pSurface->face.flags & RD_FF_TEX_TRANSLUCENT) != 0 || pMat->formatType != STDCOLOR_FORMAT_RGB)
            && rdVector_Dot3(&pSurface->face.normal, &pOrient->lvec) < 0.0f )
        {
            sithAIUtil_GetNextThingsInView(pAdjoin->pAdjoinSector, pOrient, pAdjoin->distance + pAdjoin->pMirrorAdjoin->distance + distance);
        }
    }
}

// Randomly decides if AI notices the target based on distance, target light, crouching, moving and invisibility
int J3DAPI sithAIUtil_CanSeeTarget(const SithAIControlBlock* pLocal, SithThing* pTarget, float distance)
{
    const SithThing* pOwner = pLocal->pOwner;
    float chance = 1.0f;
    if ( !pTarget || (pTarget->type != SITH_THING_ACTOR && pTarget->type != SITH_THING_PLAYER) )
    {
        return 1;
    }

    if ( distance >= 2.0f )
    {
        if ( (pLocal->mode & SITHAI_MODE_ACTIVE) == 0 )
        {
            chance = 0.5f;
        }

        if ( (pTarget->thingInfo.actorInfo.flags & SITH_AF_HEADLIGHT) == 0 && (pTarget->unknownFlags & 0x1) == 0 )
        {
            // Farther the target harder to see
            float distFactor = (distance - 2.0f) * 0.1f;
            distFactor = distFactor >= 0.0f ? (distFactor > 0.6f ? 0.6f : distFactor) : 0.0f;
            chance = (1.0f - distFactor) * chance;

            // Target in dark sector
            if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_SEEINDARK) == 0 && rdLight_GetIntensity(&pTarget->pInSector->ambientLight) < 0.5f )
            {
                chance = (rdLight_GetIntensity(&pTarget->pInSector->ambientLight) - -0.2f) * chance;
            }

            if ( pTarget->moveType == SITH_MT_PHYSICS )
            {
                if ( (pTarget->moveInfo.physics.flags & SITH_PF_CROUCHING) != 0 )
                {
                    chance = chance * 0.75f;
                }

                if ( sithAIUtil_IsZeroVector(&pTarget->moveInfo.physics.velocity) )
                {
                    chance = chance * 0.5f;
                }
            }
        }
    }

    if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) == 0 )
    {
        if ( (pTarget->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            chance = chance * 0.05f;
        }

        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
        {
            chance = chance * 0.05f;
        }
    }

    chance = STDMATH_CLAMP(chance, 0.05f, 1.0f);
    return chance > sithAIUtil_Random();
}
