#include "sithAIInstinct.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Math/rdMath.h>
#include <rdroid/Math/rdVector.h>
#include <sith/AI/sithAI.h>
#include <sith/AI/sithAIAwareness.h>
#include <sith/AI/sithAIMove.h>
#include <sith/AI/sithAIUtil.h>
#include <sith/Devices/sithSound.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/Gameplay/sithPlayerControls.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithSector.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>
#include <std/General/stdMath.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static float sithAIInstinct_flt_539AB8 = 1.0f;

void sithAIInstinct_InstallHooks(void)
{
    J3D_HOOKFUNC(sithAIInstinct_InitInstincts);
    J3D_HOOKFUNC(sithAIInstinct_Listen);
    J3D_HOOKFUNC(sithAIInstinct_LookForTarget);
    J3D_HOOKFUNC(sithAIInstinct_SenseDanger);
    J3D_HOOKFUNC(sithAIInstinct_FearGunshot);
    J3D_HOOKFUNC(sithAIInstinct_Hop);
    J3D_HOOKFUNC(sithAIInstinct_HoverDrift);
    J3D_HOOKFUNC(sithAIInstinct_Talk);
    J3D_HOOKFUNC(sithAIInstinct_RandomTurn);
    J3D_HOOKFUNC(sithAIInstinct_OpenDoors);
    J3D_HOOKFUNC(sithAIInstinct_Jump);
    J3D_HOOKFUNC(sithAIInstinct_LobFire);
    J3D_HOOKFUNC(sithAIInstinct_PrimaryFire);
    J3D_HOOKFUNC(sithAIInstinct_AlternateFire);
    J3D_HOOKFUNC(sithAIInstinct_CircleStrafe);
    J3D_HOOKFUNC(sithAIInstinct_Dodge);
    J3D_HOOKFUNC(sithAIInstinct_HitAndRun);
    J3D_HOOKFUNC(sithAIInstinct_Retreat);
    J3D_HOOKFUNC(sithAIInstinct_RandomMove);
    J3D_HOOKFUNC(sithAIInstinct_HumanCombatMove);
    J3D_HOOKFUNC(sithAIInstinct_Roam);
    J3D_HOOKFUNC(sithAIInstinct_ReturnHome);
    J3D_HOOKFUNC(sithAIInstinct_Flee);
    J3D_HOOKFUNC(sithAIInstinct_BasicFallow);
    J3D_HOOKFUNC(sithAIInstinct_WallCrawl);
    J3D_HOOKFUNC(sithAIInstinct_sub_494360);
    J3D_HOOKFUNC(sithAIInstinct_SnakeMungeTestCheck);
    J3D_HOOKFUNC(sithAIInstinct_SnakeFollow);
    J3D_HOOKFUNC(sithAIInstinct_sub_494FF0);
}


// Physics flags of thing riding vehicle (mine car, raft, jeep...)
#define SITHAIINSTINCT_PF_VEHICLE (SITH_PF_MINECAR | SITH_PF_RAFT | SITH_PF_JEEP | SITH_PF_UNKNOWN_8000000)

static float sithAIInstinct_Random(void)
{
    return (float)rand() * (1.0f / 32767.0f);
}

static bool sithAIInstinct_IsZeroVector(const rdVector3* pVec)
{
    return pVec->x == 0.0f && pVec->y == 0.0f && pVec->z == 0.0f;
}

static SithAIAwarenessSector* sithAIInstinct_GetAwarenessSector(const SithThing* pThing)
{
    return &sithAIAwareness_g_aSectors[sithSector_GetSectorIndex(pThing->pInSector)];
}

static bool sithAIInstinct_IsInVehicle(const SithThing* pThing)
{
    return (pThing->moveInfo.physics.flags & SITHAIINSTINCT_PF_VEHICLE) != 0;
}

// Returns non-zero when both things are under water or both are not
static bool sithAIInstinct_IsSameMedium(const SithThing* pThing1, const SithThing* pThing2)
{
    return ((pThing1->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0) == ((pThing2->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0);
}

// Handles target lost: plays victory if target died, and puts AI to search mode
static void sithAIInstinct_LoseTarget(SithAIControlBlock* pLocal, SithThing* pTarget)
{
    if ( pTarget && (pTarget->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 && (pLocal->mode & SITHAI_MODE_TARGETVISIBLE) != 0 )
    {
        sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_VICTORY);
        sithPuppet_PlayMode(pLocal->pOwner, SITHPUPPETSUBMODE_VICTORY, NULL);
    }

    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
    pLocal->pTargetThing = NULL;
}

void J3DAPI sithAIInstinct_InitInstincts()
{
    sithAI_RegisterInstinct("listen", (SithAIInstinctFunc)sithAIInstinct_Listen, 0, 0, (SithAIEventType)0x7);
    sithAI_RegisterInstinct("lookfortarget", (SithAIInstinctFunc)sithAIInstinct_LookForTarget, (SithAIMode)0x204, (SithAIMode)0x800, (SithAIEventType)0x100);
    sithAI_RegisterInstinct("sensedanger", (SithAIInstinctFunc)sithAIInstinct_SenseDanger, (SithAIMode)0x804, 0, (SithAIEventType)0x7);
    sithAI_RegisterInstinct("primaryfire", sithAIInstinct_PrimaryFire, (SithAIMode)0x2, (SithAIMode)0x800, 0);
    sithAI_RegisterInstinct("alternatefire", sithAIInstinct_AlternateFire, (SithAIMode)0x2, (SithAIMode)0x800, 0);
    sithAI_RegisterInstinct("lobfire", sithAIInstinct_LobFire, (SithAIMode)0x2, (SithAIMode)0x800, 0);
    sithAI_RegisterInstinct("opendoors", sithAIInstinct_OpenDoors, (SithAIMode)0x2, 0, 0);
    sithAI_RegisterInstinct("jump", sithAIInstinct_Jump, 0, 0, (SithAIEventType)0x604);
    sithAI_RegisterInstinct("randomturn", sithAIInstinct_RandomTurn, (SithAIMode)0x4, 0, 0);
    sithAI_RegisterInstinct("roam", (SithAIInstinctFunc)sithAIInstinct_Roam, (SithAIMode)0x4, (SithAIMode)0xA12, (SithAIEventType)0xF04);
    sithAI_RegisterInstinct("flee", (SithAIInstinctFunc)sithAIInstinct_Flee, (SithAIMode)0x800, 0, (SithAIEventType)0xF04);
    sithAI_RegisterInstinct("retreat", sithAIInstinct_Retreat, (SithAIMode)0x2, (SithAIMode)0x1000800, (SithAIEventType)0x1);
    sithAI_RegisterInstinct("circlestrafe", (SithAIInstinctFunc)sithAIInstinct_CircleStrafe, (SithAIMode)0x2, (SithAIMode)0x1000800, (SithAIEventType)0x88E04);
    sithAI_RegisterInstinct("returnhome", sithAIInstinct_ReturnHome, 0, 0, (SithAIEventType)0x900);
    sithAI_RegisterInstinct("talk", sithAIInstinct_Talk, (SithAIMode)0xFFFFFFFF, 0, (SithAIEventType)0x400104);
    sithAI_RegisterInstinct("hitandrun", sithAIInstinct_HitAndRun, (SithAIMode)0xC00, (SithAIMode)0x4, (SithAIEventType)0x4000);
    sithAI_RegisterInstinct("dodge", (SithAIInstinctFunc)sithAIInstinct_Dodge, 0, (SithAIMode)0x100000, (SithAIEventType)0x81803);
    sithAI_RegisterInstinct("hop", sithAIInstinct_Hop, (SithAIMode)0x1, 0, 0);
    sithAI_RegisterInstinct("hoverdrift", sithAIInstinct_HoverDrift, (SithAIMode)0x6FE, (SithAIMode)0xF001, 0);
    sithAI_RegisterInstinct("basicfollow", sithAIInstinct_BasicFallow, (SithAIMode)0x202, (SithAIMode)0x1004814, (SithAIEventType)0x8CE04);
    sithAI_RegisterInstinct("wallcrawl", (SithAIInstinctFunc)sithAIInstinct_WallCrawl, (SithAIMode)0x202, (SithAIMode)0x3004014, (SithAIEventType)0x10A604);
    sithAI_RegisterInstinct("snakefollow", (SithAIInstinctFunc)sithAIInstinct_SnakeFollow, (SithAIMode)0x6, 0, (SithAIEventType)0x8E04);
    sithAI_RegisterInstinct("randommove", sithAIInstinct_RandomMove, (SithAIMode)0x2, (SithAIMode)0x104805, (SithAIEventType)0x4800);
    sithAI_RegisterInstinct("humancombatmove", sithAIInstinct_HumanCombatMove, (SithAIMode)0x2, (SithAIMode)0x1104805, (SithAIEventType)0x80901);
    sithAI_RegisterInstinct("feargunshot", (SithAIInstinctFunc)sithAIInstinct_FearGunshot, (SithAIMode)0x6, (SithAIMode)0x800, (SithAIEventType)0x2);
}

// Reacts to sounds (alerts, curious), being hit or touched
int J3DAPI sithAIInstinct_Listen(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    if ( pThing && (pThing->flags & SITH_TF_DISABLED) != 0 )
    {
        return 0;
    }

    SithThing* pOwner = pLocal->pOwner;
    SithAIAwarenessSector* pAISector = sithAIInstinct_GetAwarenessSector(pOwner);
    if ( !pAISector )
    {
        SITHLOG_ERROR("Listen: Bad sector data passed!\n");
        return 0;
    }

    if ( event == SITHAI_EVENT_SOUND )
    {
        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_DEAF) != 0 || pAISector->aLevelAtTransmittingPos[0] == 0.0f )
        {
            return 0;
        }

        if ( pInstinct->fltArg[0] <= sithAIInstinct_Random() )
        {
            return 0;
        }

        SithThing* pSource = pAISector->aTransmittingThing[0];
        bool bStop = false, bMove = false, bLook = false, bHunt = false;
        if ( pSource && (pSource->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            return 0;
        }

        if ( !sithAIAwareness_IsInTransmittingRange(pAISector, 0, pOwner) )
        {
            return 0;
        }

        rdVector3 dir;
        float dist;
        int posSightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &pAISector->aStartPos[0], -1.0f, pLocal->pClass->sightDistance, 0.0f, &dir, &dist);
        if ( dist > pLocal->pClass->heardDistance )
        {
            return 0; // too far to hear
        }

        int sourceSightState = 3;
        if ( pSource && pSource->type != SITH_THING_FREE )
        {
            rdVector3 sourceDir;
            float sourceDist;
            sourceSightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &pSource->pos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &sourceDir, &sourceDist);
        }

        // Note, the original uses the pInstinct pointer bits as the alert level in hunt case
        float level;
        memcpy(&level, &pInstinct, sizeof(level));
        if ( !pSource || pSource->type == SITH_THING_FREE || (pLocal->mode & (SITHAI_MODE_HUNTING | SITHAI_MODE_INSTINCTUSEWPNTS)) == 0 )
        {
            if ( sourceSightState == 0 )
            {
                // Source visible, go to it
                bMove = true;
                bLook = true;
                pLocal->alertPos = pSource->pos;
                level = pAISector->aLevelAtTransmittingPos[0];
            }
            else if ( posSightState == 0 )
            {
                // Sound position visible
                bMove = true;
                bLook = true;
                pLocal->alertPos = pAISector->aStartPos[0];
                level = pAISector->aLevelAtTransmittingPos[0];
            }
            else if ( pSource && (pLocal->mode & SITHAI_MODE_HUNTING) == 0 )
            {
                bStop = true;
                bLook = true;
                level = pAISector->aLevelAtTransmittingPos[0];
            }
        }
        else
        {
            bHunt = true;
        }

        if ( !bStop && !bMove && !bLook && !bHunt )
        {
            return 0;
        }

        if ( pSource && pSource->type != SITH_THING_FREE && !sithAIInstinct_IsSameMedium(pSource, pLocal->pOwner) )
        {
            return 0;
        }

        if ( pState->aParams[0] < level )
        {
            pState->aParams[0] = level;
        }

        sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_CURIOUS);
        if ( bStop )
        {
            sithAIMove_StopAIMovement(pLocal);
        }

        if ( bMove && sithAIInstinct_Random() <= pInstinct->fltArg[1] )
        {
            sithAIMove_AISetMovePos(pLocal, &pLocal->alertPos, 0.75f);
        }

        if ( bLook )
        {
            sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->alertPos);
        }

        if ( !bHunt || pInstinct->fltArg[1] < sithAIInstinct_Random() )
        {
            return 0;
        }

        sithAIUtil_AISetMode(pLocal, SITHAI_MODE_HUNTING);
        pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
        sithAIUtil_AIMoveToNextWpnt(pLocal, 1.7f, 0.0f, 0);
        return 0;
    }

    if ( event != SITHAI_EVENT_HIT_SECTOR && event != SITHAI_EVENT_TOUCHED )
    {
        return 0;
    }

    if ( pState->aParams[0] == 0.0f )
    {
        sithAIUtil_AIPlaySoundMode(pLocal, event == SITHAI_EVENT_HIT_SECTOR ? SITHSOUNDCLASS_SURPRISE : SITHSOUNDCLASS_CURIOUS);
        pState->aParams[0] = 0.1f;
    }

    if ( !pThing )
    {
        return 0;
    }

    SithThing* pParent = sithThing_GetThingParent(pThing);
    if ( event == SITHAI_EVENT_HIT_SECTOR )
    {
        // Hit by something
        if ( pInstinct->fltArg[1] < sithAIInstinct_Random() )
        {
            sithAIMove_AISetLookThing(pLocal, pParent);
            return 0;
        }

        if ( (pLocal->mode & (SITHAI_MODE_HUNTING | SITHAI_MODE_INSTINCTUSEWPNTS)) == 0 )
        {
            sithAIMove_AISetLookThing(pLocal, pParent);
            sithAIMove_AISetMovePos(pLocal, &pParent->pos, 1.7f);
            return 1;
        }

        sithAIUtil_AISetMode(pLocal, SITHAI_MODE_HUNTING);
        pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
        if ( sithAIUtil_AIMoveToNextWpnt(pLocal, 1.7f, 0.0f, 0) )
        {
            return 1;
        }

        return 0;
    }

    // Touched
    if ( pParent->type == SITH_THING_PLAYER )
    {
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_AISetLookPosEyeLevel(pLocal, &pParent->pos);
        return 0;
    }

    if ( pInstinct->fltArg[1] < sithAIInstinct_Random() )
    {
        sithAIMove_AISetLookThing(pLocal, pParent);
        return 0;
    }

    if ( (pLocal->mode & (SITHAI_MODE_HUNTING | SITHAI_MODE_INSTINCTUSEWPNTS)) != 0 )
    {
        sithAIUtil_AIMoveToNextWpnt(pLocal, pLocal->moveSpeed, 0.0f, 0);
        return 0;
    }

    // Step away from the thing
    rdVector3 awayDir;
    rdVector_Sub3(&awayDir, &pLocal->pOwner->pos, &pParent->pos);
    rdVector_Normalize3Acc(&awayDir);
    if ( rdVector_Dot3(&pLocal->moveDirection, &awayDir) < 0.0f )
    {
        rdVector3 movePos;
        rdVector_ScaleAdd3(&movePos, &awayDir, pInstinct->fltArg[2], &pLocal->pOwner->pos);
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
        sithAIMove_AISetMoveTargetPos(pLocal, &movePos, 0.75f);
    }

    return 0;
}

// Looks for player as the target and starts attacking when in sight. Ends hunting when target is lost.
int J3DAPI sithAIInstinct_LookForTarget(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithAIMode newMode)
{
    if ( event == SITHAI_EVENT_MODECHANGED && newMode && (pLocal->mode & SITHAI_MODE_HUNTING) != 0 && (newMode & SITHAI_MODE_HUNTING) == 0 )
    {
        pState->aParams[0] = sithTime_g_secGameTime; // hunt start time
    }

    if ( event != 0 )
    {
        return 0;
    }

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_AINOTARGET) != 0 || sithPlayerControls_g_bCutsceneMode == 1 )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_HUNTING) != 0 )
    {
        if ( !pLocal->pTargetThing )
        {
            SITHLOG_WARNING("LookForTarget: AI '%s' lost track of its target.  Ending HUNT mode.\n", pLocal->pOwner->aName);
            sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_HUNTING);
        }
        else
        {
            float secHuntTime = pInstinct->fltArg[3] == 0.0f ? 20.0f : pInstinct->fltArg[3];
            if ( sithTime_g_secGameTime > pState->aParams[0] + secHuntTime
                || ((pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) == 0 && sithAIInstinct_IsInVehicle(pLocal->pTargetThing)) )
            {
                sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_HUNTING);
            }
        }
    }

    if ( (pLocal->mode & SITHAI_MODE_ACTIVE) != 0 )
    {
        // Give up attack after a while
        if ( pInstinct->intArg[1] && pLocal->msecAttackStart + pInstinct->intArg[1] < sithTime_g_msecGameTime )
        {
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
            if ( (pLocal->mode & SITHAI_MODE_INSTINCTUSEWPNTS) == 0 )
            {
                return 1;
            }

            if ( (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) != 0 || !sithAIInstinct_IsInVehicle(pLocal->pTargetThing) )
            {
                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_HUNTING);
                sithAIUtil_AIMoveToNextWpnt(pLocal, 1.7f, 0.0f, 0);
                return 1;
            }
        }

        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    pState->msecNextUpdate = pInstinct->intArg[0] + sithTime_g_msecGameTime;
    if ( (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
    {
        return 0;
    }

    pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
    SITH_ASSERTREL(pLocal->pTargetThing);

    if ( !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
    {
        pLocal->pTargetThing = NULL;
        return 0;
    }

    if ( (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) == 0 && !sithAIInstinct_IsInVehicle(pLocal->pOwner) && sithAIInstinct_IsInVehicle(pLocal->pTargetThing) )
    {
        pLocal->pTargetThing = NULL;
        return 0;
    }

    sithAIUtil_sub_49B2E0(pLocal, 0);
    if ( pLocal->targetSightState == 0 )
    {
        // Target spotted
        SithThing* pTarget = pLocal->pTargetThing;
        if ( pLocal->goalThing == pTarget )
        {
            // Update goal sight state
            pLocal->unknown141        = sithMain_g_frameNumber;
            pLocal->targetSightState2 = 0;
            pLocal->vecUnknown0       = pLocal->toTarget;
            pLocal->targetDistance    = pLocal->distance;
            pLocal->vecUnknown5       = pTarget->pos;
            pLocal->vecUnknown6       = pTarget->pos;
            pLocal->unknown150        = sithTime_g_msecGameTime;
        }

        if ( pInstinct->fltArg[2] == 0.0f )
        {
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_ATTACKING);
            if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) == 0 )
            {
                sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
            }

            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_ALERT);
            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_ACTIVATE);
            sithAIAwareness_CreateTransmittingEvent(pLocal->pTargetThing->pInSector, &pLocal->pOwner->pos, 0, 1.5f, pLocal->pTargetThing);
            pLocal->goalThing = pLocal->pTargetThing;
            return 1;
        }

        sithAIMove_AISetLookThing(pLocal, pTarget);
        return 0;
    }

    if ( pInstinct->fltArg[0] == 0.0f )
    {
        pInstinct->fltArg[0] = 500.0f;
    }

    return 0;
}

// Flees when seeing player, being hit or hearing danger
int J3DAPI sithAIInstinct_SenseDanger(SithAIControlBlock* pLocal, SithAIInstinct* pInstance, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    SithThing* pOwner = pLocal->pOwner;
    SithAIAwarenessSector* pAISector = sithAIInstinct_GetAwarenessSector(pOwner);
    if ( event == 0 && pInstance->intArg[0] )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + pInstance->intArg[0];
    }

    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 || (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    if ( event == 0 )
    {
        if ( sithAIInstinct_Random() <= pInstance->fltArg[2] )
        {
            pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
            if ( !sithAIUtil_CanAttackTarget(sithPlayer_g_pLocalPlayerThing) )
            {
                pLocal->pTargetThing = NULL;
                return 0;
            }

            sithAIUtil_sub_49B2E0(pLocal, 0);
            if ( pLocal->targetSightState == 0 )
            {
                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
                sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
                sithAIAwareness_CreateTransmittingEvent(pLocal->pOwner->pInSector, &pLocal->pOwner->pos, 1, 3.0f, pLocal->pOwner);
                pLocal->pFleeFromThing = pLocal->pTargetThing;
            }
        }
    }
    else if ( event == SITHAI_EVENT_HIT_SECTOR )
    {
        if ( sithAIInstinct_Random() <= pInstance->fltArg[3] )
        {
            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_SURPRISE);
            if ( pThing )
            {
                pLocal->pFleeFromThing = sithThing_GetThingParent(pThing);
            }

            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
            return 1;
        }
    }
    else if ( event == SITHAI_EVENT_SOUND && pInstance->fltArg[1] < pAISector->aLevelAtTransmittingPos[2] )
    {
        SithThing* pSource = pAISector->aTransmittingThing[2];
        if ( pSource && pSource != pLocal->pOwner )
        {
            rdVector3 dir;
            float dist;
            if ( sithAIUtil_GetDistanceToTarget(pOwner, &pOwner->pos, pSource, -1.0f, pLocal->pClass->heardDistance, 0.0f, &dir, &dist) == 0 )
            {
                pLocal->pFleeFromThing = pSource;
                if ( (pLocal->mode & SITHAI_MODE_FLEEING) == 0 )
                {
                    sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
                    sithAIAwareness_CreateTransmittingEvent(pLocal->pOwner->pInSector, &pLocal->pOwner->pos, 2, 4.0f, pLocal->pOwner);
                }

                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
                return 1;
            }
        }
    }

    return 0;
}

// Flees from gunshot sound source when close to it or when it's aiming at AI
signed int J3DAPI sithAIInstinct_FearGunshot(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(pState);
    J3D_UNUSED(pObject);

    SithAIAwarenessSector* pAISector = sithAIInstinct_GetAwarenessSector(pLocal->pOwner);
    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 || event != SITHAI_EVENT_SOUND || pAISector->aLevelAtTransmittingPos[3] <= 0.0f )
    {
        return 0;
    }

    SithThing* pSource = pAISector->aTransmittingThing[3];
    SithThing* pOwner  = pLocal->pOwner;
    if ( !pSource || pSource == pOwner )
    {
        return 0;
    }

    rdVector3 dir;
    rdVector_Sub3(&dir, &pOwner->pos, &pSource->pos);
    float dist = rdVector_Normalize3Acc(&dir);
    if ( dist < pInstinct->fltArg[0]
        || (dist < pInstinct->fltArg[1] && pInstinct->fltArg[3] < rdVector_Dot3(&pSource->orient.lvec, &dir) && sithAIInstinct_Random() < pInstinct->fltArg[2]) )
    {
        sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
        pLocal->pFleeFromThing = pSource;
        sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
        return 1;
    }

    return 0;
}

// Hops while moving
int J3DAPI sithAIInstinct_Hop(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(event);
    J3D_UNUSED(pObject);

    float msecInterval = (float)pInstinct->intArg[0];
    pState->msecNextUpdate = (int)(sithAIInstinct_Random() * msecInterval) + sithTime_g_msecGameTime;
    if ( pLocal->pOwner->attach.flags != 0 && (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
    {
        sithThing_DetachThing(pLocal->pOwner);

        rdVector3 force;
        force.x = 0.0f;
        force.y = 0.0f;
        force.z = sithAIInstinct_Random() * pInstinct->fltArg[1];
        sithPhysics_ApplyForce(pLocal->pOwner, &force);
        sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_JUMP);
    }

    return 0;
}

// Randomly drifts flying AI
int J3DAPI sithAIInstinct_HoverDrift(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(event);
    J3D_UNUSED(pObject);
    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    if ( (pLocal->pOwner->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
    {
        return 0;
    }

    float secInterval = pInstinct->fltArg[0];
    pState->msecNextUpdate = (int)((sithAIInstinct_Random() - 0.5f) * secInterval + secInterval) + sithTime_g_msecGameTime;
    if ( (pLocal->mode & SITHAI_MODE_MOVING) == 0 && (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) == 0 )
    {
        float dirX = sithAIInstinct_Random() - 0.5f;
        float dirY = sithAIInstinct_Random() - 0.5f;
        float dirZ = sithAIInstinct_Random() - 0.5f;

        rdVector3 force;
        force.x = sithAIInstinct_Random() * pInstinct->fltArg[1] * (dirX + dirX);
        force.y = sithAIInstinct_Random() * pInstinct->fltArg[1] * (dirY + dirY);
        force.z = sithAIInstinct_Random() * pInstinct->fltArg[1] * (dirZ + dirZ);
        sithPhysics_ApplyForce(pLocal->pOwner, &force);
    }

    return 0;
}

// Plays talk sounds depending on the AI state
int J3DAPI sithAIInstinct_Talk(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    float chance        = pInstinct->fltArg[1];
    SithThing* pOwner   = pLocal->pOwner;
    tSoundChannelHandle hSnd = 0;
    int bHandled        = 0;

    float secNextTalk = -1.0f;
    if ( pInstinct->fltArg[2] != 0.0f )
    {
        secNextTalk = pState->aParams[0] + pInstinct->fltArg[2];
    }

    if ( event == SITHAI_EVENT_TOUCHED )
    {
        if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 || sithTime_g_secGameTime < secNextTalk || !pObject )
        {
            return 0;
        }

        const SithThing* pThing = (const SithThing*)pObject;
        if ( (pThing->type == SITH_THING_PLAYER || pThing->type == SITH_THING_ACTOR) && sithAIInstinct_Random() < chance )
        {
            hSnd = sithSoundClass_PlayModeRandom(pLocal->pOwner, SITHSOUNDCLASS_ALERT);
        }
    }
    else if ( event == 0 )
    {
        float secInterval = pInstinct->fltArg[0];
        if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
        {
            chance      = chance + chance;
            secInterval = secInterval * 0.5f;
        }

        pState->msecNextUpdate = (int)(secInterval * 1000.0f) + sithTime_g_msecGameTime;
        if ( sithTime_g_secGameTime < secNextTalk )
        {
            return 0;
        }

        // Talk only when in the same medium as player
        int bPlayerUnderwater = 0;
        if ( sithPlayer_g_pLocalPlayerThing )
        {
            bPlayerUnderwater = (sithPlayer_g_pLocalPlayerThing->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0;
        }

        int bUnderwater = (pLocal->pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0;
        if ( bPlayerUnderwater != bUnderwater )
        {
            return 0;
        }

        if ( sithAIInstinct_Random() < chance )
        {
            float health = pOwner->thingInfo.actorInfo.health / pOwner->thingInfo.actorInfo.maxHealth;
            SithAIMode mode = pLocal->mode;
            if ( (mode & SITHAI_MODE_ACTIVE) == 0 )
            {
                hSnd = sithSoundClass_PlayModeRandom(pOwner, SITHSOUNDCLASS_IDLE);
            }
            else if ( (mode & SITHAI_MODE_FLEEING) != 0 )
            {
                hSnd = sithSoundClass_PlayModeRandom(pOwner, health > 0.5f ? SITHSOUNDCLASS_GLOAT : SITHSOUNDCLASS_FEAR);
            }
            else if ( (mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
            {
                if ( sithAIInstinct_Random() >= 0.6f )
                {
                    hSnd = sithSoundClass_PlayModeRandom(pLocal->pOwner, health <= 0.25f ? SITHSOUNDCLASS_FEAR : SITHSOUNDCLASS_GLOAT);
                }
                else
                {
                    hSnd = sithSoundClass_PlayModeFirst(pLocal->pOwner, SITHSOUNDCLASS_RESERVED8);
                }
            }
            else if ( (mode & SITHAI_MODE_TARGETVISIBLE) == 0 )
            {
                hSnd = sithSoundClass_PlayModeRandom(pOwner, SITHSOUNDCLASS_SEARCH);
            }
            else
            {
                hSnd = sithSoundClass_PlayModeRandom(pOwner, health >= 0.25f ? SITHSOUNDCLASS_BOAST : SITHSOUNDCLASS_FEAR);
            }
        }
    }
    else if ( event == SITHAI_EVENT_MODECHANGED )
    {
        if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 && ((uintptr_t)pObject & SITHAI_MODE_UNKNOWN_100000) == 0 )
        {
            sithSoundMixer_StopAllSoundsThing(pOwner);
            hSnd = sithSoundClass_PlayModeFirst(pLocal->pOwner, SITHSOUNDCLASS_RESERVED8);
        }
    }
    else if ( event == SITHAI_EVENT_TALK )
    {
        if ( sithTime_g_secGameTime < secNextTalk )
        {
            return 0;
        }

        hSnd = sithSoundClass_PlayModeRandom(pOwner, (SithSoundClassMode)(uintptr_t)pObject);
        bHandled = 1;
    }
    else
    {
        return 0;
    }

    if ( (int)hSnd > 0 )
    {
        pState->aParams[0] = sithTime_g_secGameTime;
    }

    return bHandled;
}

// Looks at random direction
int J3DAPI sithAIInstinct_RandomTurn(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(event);
    J3D_UNUSED(pObject);

    pState->msecNextUpdate = sithTime_g_msecGameTime + (pInstinct->intArg[0] ? pInstinct->intArg[0] : 5000);
    if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    rdVector3 pyr = { 0.0f, sithAIInstinct_Random() * 360.0f, 0.0f };
    rdVector3 dir = rdroid_g_yVector3;
    rdVector_Rotate3Acc(&dir, &pyr);

    SithThing* pOwner = pLocal->pOwner;
    rdVector3 lookPos;
    rdVector_ScaleAdd3(&lookPos, &dir, pInstinct->fltArg[1], &pOwner->pos);

    rdVector3 toPos;
    float dist;
    int sightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &lookPos, -1.0f, pInstinct->fltArg[1], 0.0f, &toPos, &dist);
    if ( sightState == 0 )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
        return 1; // Note, the original returns the leftover new AI mode value from sithAIMove_AISetLookPos (non-zero)
    }

    return sightState;
}

int J3DAPI sithAIInstinct_OpenDoors(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(pInstinct);
    J3D_UNUSED(event);
    J3D_UNUSED(pObject);

    if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
    {
        sithPlayerActions_Activate(pLocal->pOwner);
        pState->msecNextUpdate = sithTime_g_msecGameTime + 1000;
    }

    return 0;
}

// Jumps over obstacles and cliffs
int J3DAPI sithAIInstinct_Jump(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(pObject);
    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    SithThing* pOwner = pLocal->pOwner;
    SithSector* pSector = pOwner->pInSector;
    if ( pOwner->attach.flags == 0 || (pLocal->mode & SITHAI_MODE_MOVING) == 0 || pOwner->thingInfo.actorInfo.bForceMovePlay )
    {
        return 0;
    }

    const rdVector3* pVel = &pOwner->moveInfo.physics.velocity;
    if ( pLocal->vecUnknown0.x * pVel->x + pVel->y * pLocal->vecUnknown0.y + pVel->z * pLocal->vecUnknown0.z > 0.02f )
    {
        return 0;
    }

    if ( (float)sithTime_g_msecGameTime < pState->aParams[0] )
    {
        return 0;
    }

    pState->aParams[0] = pInstinct->fltArg[0] + (float)sithTime_g_msecGameTime;
    if ( event == SITHAI_EVENT_TOUCHED || event == SITHAI_EVENT_HIT_WALL )
    {
        // Jump on top of obstacle
        rdVector3 upPos;
        rdVector_ScaleAdd3(&upPos, &rdroid_g_zVector3, pInstinct->fltArg[1], &pOwner->pos);
        SithSector* pUpSector = sithCollision_FindSectorInRadius(pSector, &pOwner->pos, &upPos, 0.0f);
        if ( !pUpSector )
        {
            return 0;
        }

        rdVector3 jumpPos;
        jumpPos.x = pLocal->moveDirection.x * 0.1f + upPos.x;
        jumpPos.y = pLocal->moveDirection.y * 0.1f + upPos.y;
        jumpPos.z = upPos.z;
        SithSector* pJumpSector = sithCollision_FindSectorInRadius(pUpSector, &upPos, &jumpPos, 0.0f);
        if ( !pJumpSector )
        {
            return 0;
        }

        int bNoCollision;
        if ( sithAIUtil_sub_49DD20(pLocal, &jumpPos, pJumpSector, &bNoCollision) == 1 && bNoCollision )
        {
            sithAIMove_AIJump(pLocal, &jumpPos, 1.0f);
        }

        return 1;
    }

    if ( event != SITHAI_EVENT_HIT_CLIFF )
    {
        return 0;
    }

    // Jump over cliff
    rdVector3 landPos;
    rdVector_ScaleAdd3(&landPos, &pLocal->moveDirection, pInstinct->fltArg[2], &pOwner->pos);
    if ( sithAIUtil_sub_49DC90(pLocal, &landPos, NULL) )
    {
        rdVector_ScaleAdd3Acc(&pOwner->moveInfo.physics.velocity, &pLocal->moveDirection, 0.1f);
        sithAIMove_AIJump(pLocal, &pLocal->movePos, 1.0f);
        return 1;
    }

    return 1;
}

int J3DAPI sithAIInstinct_LobFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(event);
    J3D_UNUSED(pObject);

    SithAIUtilFireFlags fireFlags = SITHAIUTIL_FIRE_LOB;
    SITH_ASSERTREL(pLocal);

    SithThing* pOwner  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;
    SITH_ASSERTREL(pOwner);

    if ( pOwner->thingInfo.actorInfo.bForceMovePlay )
    {
        return 0;
    }

    if ( !sithAIUtil_CanAttackTarget(pTarget) )
    {
        sithAIInstinct_LoseTarget(pLocal, pTarget);
        return 1;
    }

    if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 150;
        return 0;
    }

    int weaponNum = pInstinct->intArg[6];
    if ( sithAIInstinct_Random() < pInstinct->fltArg[5] )
    {
        weaponNum = weaponNum + 1;
    }

    sithAIUtil_SetWeaponFireFlags(weaponNum, &fireFlags);
    if ( sithAIUtil_AIFire(pLocal, pInstinct->fltArg[2], pInstinct->fltArg[3], pInstinct->fltArg[1], pInstinct->fltArg[4], weaponNum, fireFlags, 1) )
    {
        pState->msecNextUpdate  = sithTime_g_msecGameTime + pInstinct->intArg[0];
        pLocal->msecFireWaitTime = sithTime_g_msecGameTime + pInstinct->intArg[0];
        return 0;
    }

    sithAIMove_AISetLookThing(pLocal, pTarget);
    pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
    return 0;
}

// Returns burst count from instinct arg (negative value - random burst up to the value)
static int sithAIInstinct_GetBurstCount(const SithAIControlBlock* pLocal, int burstCount, bool bAltFire)
{
    if ( burstCount == 0 )
    {
        burstCount = 1;
    }

    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) == 0 )
    {
        if ( burstCount < 0 )
        {
            burstCount = rand() % burstCount + 1;
        }
    }
    else if ( bAltFire )
    {
        burstCount = burstCount * (rand() % 8 + 3);
    }
    else
    {
        int absBurst = burstCount < 0 ? -burstCount : burstCount;
        burstCount = burstCount * (absBurst + rand() % (6 - absBurst));
    }

    return burstCount;
}

int J3DAPI sithAIInstinct_PrimaryFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(event);
    J3D_UNUSED(pObject);

    SithAIUtilFireFlags fireFlags = SITHAIUTIL_FIRE_PRIMARY;
    SITH_ASSERTREL(pLocal);

    SithThing* pOwner = pLocal->pOwner;
    SITH_ASSERTREL(pOwner);

    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 || pOwner->thingInfo.actorInfo.bForceMovePlay )
    {
        return 0;
    }

    SithThing* pTarget = pLocal->pTargetThing;
    if ( !sithAIUtil_CanAttackTarget(pTarget) )
    {
        sithAIInstinct_LoseTarget(pLocal, pTarget);
        return 1;
    }

    if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 250;
        return 0;
    }

    if ( pInstinct->fltArg[17] != 0.0f && rdVector_Dot3(&pOwner->orient.lvec, &pLocal->toTarget) > pInstinct->fltArg[17] )
    {
        if ( pInstinct->fltArg[16] == 0.0f )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
        }

        return 0;
    }

    if ( pInstinct->fltArg[6] != 0.0f && sithAIInstinct_Random() <= pInstinct->fltArg[6] )
    {
        fireFlags |= SITHAIUTIL_FIRE_LEAD;
    }

    // Select weapon
    int weaponNum = pInstinct->intArg[11];
    if ( (pLocal->mode & SITHAI_MODE_MOVING) == 0 )
    {
        if ( pInstinct->fltArg[10] != 0.0f )
        {
            float minDist = pInstinct->fltArg[15];
            if ( minDist == 0.0f )
            {
                weaponNum = weaponNum + 1;
            }
            else
            {
                sithAIUtil_sub_49B2E0(pLocal, 0);
                if ( minDist < pLocal->distance )
                {
                    weaponNum = pInstinct->intArg[11] + 1;
                }
            }
        }
    }
    else if ( pInstinct->intArg[14] == 1 )
    {
        weaponNum = -1;
    }
    else if ( pInstinct->intArg[14] == 2 )
    {
        sithAIUtil_sub_49B2E0(pLocal, 0);
        return 0;
    }

    if ( weaponNum == pInstinct->intArg[11] && pInstinct->fltArg[7] != 0.0f && sithAIInstinct_Random() <= pInstinct->fltArg[7] )
    {
        weaponNum = pInstinct->intArg[11] + 1;
    }

    sithAIUtil_SetWeaponFireFlags(weaponNum, &fireFlags);
    int burstCount = sithAIInstinct_GetBurstCount(pLocal, pInstinct->intArg[8], false);

    float minDist = pInstinct->fltArg[4];
    float maxDist = pInstinct->fltArg[2];
    if ( sithWeapon_HasWeaponSelected(pOwner) )
    {
        maxDist = sithWeapon_GetWeaponMaxAimDistance(pOwner->thingInfo.actorInfo.curWeaponID);
    }

    if ( ((pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 || sithAIUtil_sub_49F1F0(pLocal, &pLocal->pOwner->pos))
        && sithAIUtil_AIFire(pLocal, minDist, maxDist, pInstinct->fltArg[1], pInstinct->fltArg[3], weaponNum, fireFlags, burstCount) )
    {
        if ( pInstinct->fltArg[16] == 0.0f && !sithWeapon_IsAiming(pLocal->pOwner) )
        {
            sithAIMove_AISetLookThing(pLocal, pTarget);
        }

        pState->msecNextUpdate = (int)((sithAIInstinct_Random() * 0.4f - 0.2f - -1.0f) * pInstinct->fltArg[0]) + sithTime_g_msecGameTime;
        return 0;
    }

    // Didn't fire, look at target
    if ( pInstinct->fltArg[16] == 0.0f && (pLocal->mode & SITHAI_MODE_MOVING) == 0 )
    {
        int weaponId = pLocal->pOwner->thingInfo.actorInfo.curWeaponID;
        if ( pLocal->targetSightState == 0 )
        {
            SithThing* pCurTarget = pLocal->pTargetThing;
            if ( sithAIInstinct_IsZeroVector(&pCurTarget->moveInfo.physics.velocity) )
            {
                if ( weaponId > 0x12 && weaponId < 0x19 )
                {
                    sithAIMove_AISetLookThing(pLocal, pCurTarget);
                }
            }
            else
            {
                rdVector3 lookPos;
                rdVector_ScaleAdd3(&lookPos, &pCurTarget->moveInfo.physics.velocity, 0.5f, &pCurTarget->pos);
                sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
            }
        }
        else if ( pLocal->targetSightState == 2 )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
        }
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + 750;
    return 0;
}

int J3DAPI sithAIInstinct_AlternateFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(event);
    J3D_UNUSED(pObject);

    SithAIUtilFireFlags fireFlags = SITHAIUTIL_FIRE_ALT;
    SITH_ASSERTREL(pLocal);

    SithThing* pTarget = pLocal->pTargetThing;
    if ( !sithAIUtil_CanAttackTarget(pTarget) )
    {
        return 1;
    }

    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0 )
    {
        pState->msecNextUpdate = pInstinct->intArg[1] + sithTime_g_msecGameTime;
        return 0;
    }

    if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 150;
        return 0;
    }

    int weaponNum = pInstinct->intArg[0];
    if ( weaponNum < 0 )
    {
        weaponNum = rand() % (1 - weaponNum);
    }

    sithAIUtil_SetWeaponFireFlags(weaponNum, &fireFlags);
    if ( pInstinct->fltArg[7] != 0.0f && sithAIInstinct_Random() <= pInstinct->fltArg[7] )
    {
        fireFlags |= SITHAIUTIL_FIRE_LEAD;
    }

    int burstCount = sithAIInstinct_GetBurstCount(pLocal, pInstinct->intArg[8], true);
    if ( pInstinct->intArg[9] )
    {
        fireFlags |= SITHAIUTIL_FIRE_STOP_ON_FIRE;
    }

    if ( !sithAIUtil_AIFire(pLocal, pInstinct->fltArg[4], pInstinct->fltArg[5], pInstinct->fltArg[3], pInstinct->fltArg[6], weaponNum, fireFlags, burstCount) )
    {
        if ( pLocal->targetSightState == 2 )
        {
            sithAIMove_AISetLookThing(pLocal, pTarget);
        }
        else if ( pLocal->targetSightState == 0 && pTarget->moveType == SITH_MT_PHYSICS && !sithAIInstinct_IsZeroVector(&pTarget->moveInfo.physics.velocity) )
        {
            rdVector3 lookPos;
            rdVector_ScaleAdd3(&lookPos, &pTarget->moveInfo.physics.velocity, 0.5f, &pTarget->pos);
            sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
        }

        pState->msecNextUpdate = pInstinct->intArg[1] + sithTime_g_msecGameTime;
        return 0;
    }

    pState->msecNextUpdate = (int)((sithAIInstinct_Random() * 0.4f - 0.2f - -1.0f) * pInstinct->fltArg[2]) + sithTime_g_msecGameTime;
    return 0;
}

// Circles around the goal thing
int J3DAPI sithAIInstinct_CircleStrafe(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    float moveSpeed = 1.25f;
    int bHandled    = 0;
    SITH_ASSERTREL(pLocal);

    pState->msecNextUpdate = pInstinct->intArg[0] + sithTime_g_msecGameTime;
    SithAIMode mode = pLocal->mode;
    if ( (mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_FLEEING | SITHAI_MODE_SEARCHING)) != 0 || (mode & SITHAI_MODE_ATTACKING) == 0 )
    {
        pLocal->mode = mode & ~SITHAI_MODE_CIRCLESTRAFING;
        return 0;
    }

    if ( !sithAIUtil_CanAttackTarget(pLocal->goalThing) )
    {
        sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
        pLocal->goalThing = NULL;
        return 0;
    }

    if ( pInstinct->fltArg[7] != 0.0f && sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
        return 0;
    }

    if ( pInstinct->fltArg[8] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[8];
    }

    switch ( event )
    {
        case SITHAI_EVENT_TOUCHED:
            if ( (pLocal->mode & SITHAI_MODE_CIRCLESTRAFING) == 0 || !pThing || pThing->type == SITH_THING_PLAYER )
            {
                return 0;
            }
            goto stop_strafe;

        case 0:
            break;

        case SITHAI_EVENT_GOAL_REACHED:
            bHandled = 1;
            break;

        case SITHAI_EVENT_HIT_CLIFF:
        case SITHAI_EVENT_HIT_WALL:
        case SITHAI_EVENT_HIT_THING:
        case SITHAI_EVENT_GOAL_UNREACHABLE:
            if ( (pLocal->mode & SITHAI_MODE_CIRCLESTRAFING) == 0 )
            {
                return 0;
            }
            goto stop_strafe;

        default:
            return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_CIRCLESTRAFING) != 0 && pInstinct->intArg[6] == 0 )
    {
        sithAIMove_AISetLookThing(pLocal, pLocal->goalThing);
    }

    int bRandomAngle = pInstinct->intArg[4];
    sithAIUtil_sub_49B640(pLocal);
    if ( pLocal->targetDistance <= pInstinct->fltArg[2] && pLocal->targetSightState2 == 0 )
    {
        rdVector3 fromGoal;
        rdVector_Scale3(&fromGoal, &pLocal->vecUnknown0, -pLocal->targetDistance);
        if ( bRandomAngle || rdVector_Dot3(&pLocal->goalThing->orient.lvec, &fromGoal) >= 0.0f )
        {
            float prevCount = pState->aParams[1];
            pState->aParams[1] = pState->aParams[1] - -1.0f;
            if ( prevCount <= 4.0f )
            {
                if ( pState->aParams[0] == 0.0f || bRandomAngle )
                {
                    pState->aParams[0] = sithAIInstinct_Random() >= 0.5f ? 1.0f : -1.0f;
                }

                float dirSign = pState->aParams[0];
                rdVector3 pyr = { 0.0f, 0.0f, 0.0f };
                if ( bRandomAngle )
                {
                    pyr.y = (sithAIInstinct_Random() - -0.5f) * dirSign * pInstinct->fltArg[1];
                }
                else
                {
                    pyr.y = pInstinct->fltArg[1] * dirSign;
                }

                rdVector3 toGoal;
                rdVector_Sub3(&toGoal, &pLocal->goalThing->pos, &pLocal->pOwner->pos);
                rdVector_Normalize3Acc(&toGoal);

                rdVector3 dir;
                rdVector_Rotate3(&dir, &toGoal, &pyr);

                SithThing* pOwner = pLocal->pOwner;
                rdVector3 movePos;
                rdVector_ScaleAdd3(&movePos, &dir, pOwner->collide.movesize * 3.0f, &pOwner->pos);
                if ( pInstinct->fltArg[5] != 0.0f )
                {
                    // Min move distance
                    rdVector3 vec;
                    rdVector_Sub3(&vec, &pOwner->pos, &movePos);
                    if ( rdVector_Normalize3Acc(&vec) < pInstinct->fltArg[5] )
                    {
                        rdVector_ScaleAdd3Acc(&movePos, &vec, pInstinct->fltArg[5]);
                    }
                }

                rdVector3 tmpDir;
                float tmpDist;
                if ( sithAIUtil_GetDistanceToTargetPos(pLocal->goalThing, &pLocal->goalThing->pos, &movePos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &tmpDir, &tmpDist) == 0 )
                {
                    pLocal->mode |= SITHAI_MODE_CIRCLESTRAFING;
                    sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed);
                    if ( pInstinct->intArg[6] == 0 )
                    {
                        sithAIMove_AISetLookThing(pLocal, pLocal->goalThing);
                    }
                    else
                    {
                        sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
                    }

                    pState->msecNextUpdate = pInstinct->intArg[3] + sithTime_g_msecGameTime;
                    return bHandled;
                }

                // Can't strafe this way, try the other direction next time
                pState->aParams[0] = -dirSign;
                pState->aParams[1] = 0.0f;
                pLocal->mode &= ~SITHAI_MODE_CIRCLESTRAFING;
                return bHandled;
            }
        }
    }

stop_strafe:
    sithAIMove_AISetLookThing(pLocal, pLocal->goalThing);
    pLocal->mode &= ~SITHAI_MODE_CIRCLESTRAFING;
    pState->aParams[0] = 0.0f;
    pState->aParams[1] = 0.0f;
    return bHandled;
}

// Dodges projectiles and attacks
int J3DAPI sithAIInstinct_Dodge(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    float moveSpeed    = 1.5f;
    float secNextDodge = -1.0f;
    float angle        = 0.0f;
    int bHandled       = 0;
    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    if ( event == 0 )
    {
        return 0;
    }

    if ( (event & (SITHAI_EVENT_GOAL_UNREACHABLE | SITHAI_EVENT_GOAL_REACHED)) != 0 && (pLocal->mode & SITHAI_MODE_UNKNOWN_10) != 0 )
    {
        // Dodge finished
        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_10;
        if ( pLocal->pTargetThing )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
        }

        return 0;
    }

    if ( (pLocal->mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_UNKNOWN_100000 | SITHAI_MODE_FLEEING | SITHAI_MODE_UNKNOWN_10)) != 0 )
    {
        return 0;
    }

    float minMoveDist = pInstinct->fltArg[0] - -0.03f;
    float maxMoveDist = pInstinct->fltArg[1] - -0.03f;
    float moveDist    = (maxMoveDist - minMoveDist) * sithAIInstinct_Random() + minMoveDist;
    float minDist     = pInstinct->fltArg[4];
    float minPathDist = pInstinct->fltArg[10];
    if ( pInstinct->fltArg[2] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[2];
    }

    if ( pInstinct->fltArg[3] != 0.0f )
    {
        secNextDodge = pState->aParams[0] + pInstinct->fltArg[3];
    }

    rdVector3 dir;
    float dist;
    int flags;
    if ( event == SITHAI_EVENT_TARGETED )
    {
        // Targeted with weapon
        if ( (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 || !pThing )
        {
            return 0;
        }

        if ( (pThing->thingInfo.weaponInfo.damageType & (SITH_DAMAGE_WHIP | SITH_DAMAGE_ELECTROWHIP)) != 0 )
        {
            return 0;
        }

        if ( (pThing->thingInfo.weaponInfo.flags & SITH_WF_AITARGETNODODGE) == 0 )
        {
            if ( sithTime_g_secGameTime < secNextDodge || pInstinct->fltArg[5] <= sithAIInstinct_Random() )
            {
                return 0;
            }
        }

        bHandled = 1;
        SithThing* pShooter = sithThing_GetThingParent(pThing);
        if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pShooter, pLocal->pClass->fov, pLocal->pClass->sightDistance, 0.0f, &dir, &dist) != 0 )
        {
            return 0;
        }

        if ( dist < minDist )
        {
            return 0;
        }

        if ( pInstinct->fltArg[8] != 0.0f && sithAIInstinct_Random() < pInstinct->fltArg[8]
            && sithAIMove_sub_497FF0(pLocal, 0x101) /* roll to random side */ )
        {
            goto dodged;
        }

        flags = 0x8001;
    }
    else if ( event == SITHAI_EVENT_HIT_SECTOR )
    {
        // Hit with projectile
        if ( sithTime_g_secGameTime < secNextDodge || pInstinct->fltArg[6] <= sithAIInstinct_Random() )
        {
            return 0;
        }

        if ( !pThing || pThing->type != SITH_THING_WEAPON || pThing->moveType != SITH_MT_PHYSICS )
        {
            return 0;
        }

        SithThing* pShooter = sithThing_GetThingParent(pThing);
        SithThing* pOwner   = pLocal->pOwner;
        if ( pShooter == pOwner )
        {
            return 0;
        }

        if ( pShooter->controlType == SITH_CT_AI && pShooter->controlInfo.aiControl.pLocal )
        {
            // Move out of the line of fire
            rdVector_Sub3(&dir, &pShooter->controlInfo.aiControl.pLocal->pTargetThing->pos, &pShooter->pos);
            rdVector_Normalize3Acc(&dir);
            pLocal->pFleeFromThing = pShooter;
            flags    = 0x9001;
            moveDist = 1.0f;
        }
        else
        {
            if ( sithAIUtil_GetDistanceToTarget(pOwner, &pOwner->pos, pShooter, pLocal->pClass->fov, pLocal->pClass->sightDistance, 0.0f, &dir, &dist) != 0 )
            {
                return 0;
            }

            if ( dist < minDist )
            {
                return 0;
            }

            angle = 60.0f;
            flags = 0x8002;
        }
    }
    else if ( event == SITHAI_EVENT_SOUND )
    {
        // Heard projectile or vehicle
        SithAIAwarenessSector* pAISector = sithAIInstinct_GetAwarenessSector(pLocal->pOwner);
        if ( pAISector->aLevelAtTransmittingPos[2] == 0.0f )
        {
            return 0;
        }

        SithThing* pSource = pAISector->aTransmittingThing[2];
        if ( !pSource || pSource->moveType != SITH_MT_PHYSICS )
        {
            return 0;
        }

        if ( pSource->type == SITH_THING_WEAPON && (pSource->thingInfo.weaponInfo.damageType & SITH_DAMAGE_WHIP) != 0 )
        {
            return 0;
        }

        SithThing* pShooter = sithThing_GetThingParent(pSource);
        SithThing* pOwner   = pLocal->pOwner;
        if ( pShooter == pOwner )
        {
            return 0;
        }

        if ( !sithAIInstinct_IsInVehicle(pShooter) )
        {
            if ( sithAIUtil_GetDistanceToTarget(pOwner, &pOwner->pos, pShooter, pLocal->pClass->fov, 1.0f, 0.0f, &dir, &dist) != 0 )
            {
                return 0;
            }
        }
        else
        {
            float range = rdVector_Len3(&pShooter->moveInfo.physics.velocity);
            range = range + range;
            if ( range > pLocal->pClass->sightDistance )
            {
                range = pLocal->pClass->sightDistance;
            }

            if ( range <= 1.0f )
            {
                range = 1.0f;
            }

            if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pShooter, -1.0f, range, 0.0f, &dir, &dist) != 0 )
            {
                return 0;
            }
        }

        bHandled = 1;
        if ( !sithAIInstinct_IsInVehicle(pShooter) )
        {
            if ( sithTime_g_secGameTime < secNextDodge || pInstinct->fltArg[7] <= sithAIInstinct_Random() )
            {
                return 0;
            }

            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
            pLocal->pFleeFromThing = pShooter;
            angle = 45.0f;
            flags = 0xC002;
        }
        else
        {
            // Get out of the way of the vehicle
            rdVector3 heading;
            sithAIUtil_GetXYHeadingVector(pSource, &heading);
            if ( rdVector_Dot3(&heading, &dir) > -0.5f )
            {
                return 0;
            }

            moveDist = 0.5f;
            flags    = 0x8001;

            float chance = pInstinct->fltArg[9] == 0.0f ? 0.98f : pInstinct->fltArg[9];
            if ( rdVector_Len3(&pShooter->moveInfo.physics.velocity) > 1.4f )
            {
                chance = chance - 0.2f;
            }

            if ( sithAIInstinct_Random() < chance )
            {
                flags = 0x9001;
            }

            pLocal->pFleeFromThing = pShooter;
        }
    }
    else
    {
        return 0;
    }

    // Note, the original clears the flee thing before making the dodge position
    pLocal->pFleeFromThing = NULL;

    rdVector3 dodgePos;
    if ( !sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minPathDist, moveDist, angle, flags, &dir, &dodgePos) )
    {
        return 0;
    }

    sithAIMove_AISetLookPosEyeLevel(pLocal, &dodgePos);
    sithAIMove_AISetMoveTargetPos(pLocal, &dodgePos, moveSpeed);

dodged:
    pLocal->mode |= SITHAI_MODE_UNKNOWN_10;
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
    pState->aParams[0] = sithTime_g_secGameTime;
    return bHandled;
}

// Runs away after attacking
int J3DAPI sithAIInstinct_HitAndRun(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(pObject);
    if ( event == 0 )
    {
        if ( pState->aParams[1] == 1.0f )
        {
            if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
            {
                pLocal->mode &= ~SITHAI_MODE_ATTACKING;
                pState->msecNextUpdate = sithTime_g_msecGameTime + 100;
                return 0;
            }

            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
            pLocal->pFleeFromThing = pLocal->pTargetThing;
            goto start_run;
        }

        if ( (pLocal->mode & SITHAI_MODE_FLEEING) == 0 )
        {
            if ( (pLocal->mode & SITHAI_MODE_TARGETVISIBLE) != 0 && pState->aParams[3] != 0.0f && sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
            {
                float msecAttack = pState->aParams[0] - -1000.0f;
                if ( msecAttack <= pInstinct->fltArg[0] )
                {
                    pState->aParams[0]     = msecAttack;
                    pState->msecNextUpdate = sithTime_g_msecGameTime + 1000;
                    return 0;
                }

                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
                pLocal->pFleeFromThing = pLocal->pTargetThing;
                goto start_run;
            }

            pState->aParams[0]     = 0.0f;
            pState->msecNextUpdate = sithTime_g_msecGameTime + 1000;
        }
        else if ( pState->aParams[2] == 1.0f )
        {
            // Run finished, attack again
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_ATTACKING);
            pState->aParams[2] = 0.0f;
        }

        return 0;
    }

    if ( event == SITHAI_EVENT_FIRE )
    {
        pState->aParams[3] = 1.0f;
        if ( sithAIInstinct_Random() < pInstinct->fltArg[2] )
        {
            pState->aParams[1]     = 1.0f;
            pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
        }
    }

    return 0;

start_run:
    pState->aParams[2] = 1.0f;
    pState->aParams[0] = 0.0f;
    pState->aParams[1] = 0.0f;
    pState->aParams[3] = 0.0f;
    pState->msecNextUpdate = (int)pInstinct->fltArg[1] + sithTime_g_msecGameTime;
    return 0;
}

// Flees when health is low
int J3DAPI sithAIInstinct_Retreat(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(pObject);
    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0 )
    {
        return 0;
    }

    if ( !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
    {
        pState->msecNextUpdate = pInstinct->intArg[0] + sithTime_g_msecGameTime;
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
    {
        pState->flags |= SITHAI_INSTINCT_DISABLED;
        return 0;
    }

    if ( pInstinct->fltArg[3] != 0.0f && pInstinct->fltArg[3] < pState->aParams[0] )
    {
        return 0; // max retreats
    }

    SithThing* pOwner = pLocal->pOwner;
    if ( pOwner->thingInfo.actorInfo.health / pOwner->thingInfo.actorInfo.maxHealth < pInstinct->fltArg[1] && (event == 0 || event == SITHAI_EVENT_HIT_SECTOR) )
    {
        if ( sithAIInstinct_Random() < pInstinct->fltArg[2] )
        {
            pState->aParams[0] = pState->aParams[0] - -1.0f;
            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FLEE);
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
            pLocal->pFleeFromThing = pLocal->pTargetThing;
            pState->msecNextUpdate = pInstinct->intArg[0] + sithTime_g_msecGameTime;
            return 0;
        }

        if ( sithAIInstinct_Random() < 0.1f )
        {
            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
        }
    }

    pState->msecNextUpdate = pInstinct->intArg[0] + sithTime_g_msecGameTime;
    return 0;
}

// Moves randomly while attacking
int J3DAPI sithAIInstinct_RandomMove(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(pObject);
    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0
        || !sithAIUtil_CanAttackTarget(pLocal->pTargetThing)
        || (pLocal->mode & (SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_FLEEING | SITHAI_MODE_SEARCHING)) != 0
        || (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
    {
        return 0;
    }

    if ( pInstinct->intArg[6] && sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        return 0;
    }

    sithAIUtil_sub_49B2E0(pLocal, 0);
    if ( event == 0 )
    {
        int msecInterval = pInstinct->intArg[0];
        int msecScatter  = (int)((sithAIInstinct_Random() - 0.5f) * (float)(unsigned int)msecInterval);
        pState->msecNextUpdate = msecInterval + msecScatter + sithTime_g_msecGameTime;
        if ( pLocal->targetSightState != 0 )
        {
            return 0;
        }
    }
    else if ( event == SITHAI_EVENT_GOAL_REACHED )
    {
        if ( pLocal->targetSightState == 0 && pLocal->pTargetThing )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
            pState->aParams[3] = 0.0f;
            return 0;
        }

        if ( pState->aParams[3] != 0.0f )
        {
            // Look at last target position
            rdVector3 lookPos = { pState->aParams[0], pState->aParams[1], pState->aParams[2] };
            sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
        }

        pState->aParams[3] = 0.0f;
        return 0;
    }
    else if ( event != SITHAI_EVENT_FIRE || pInstinct->intArg[7] == 0 )
    {
        return 0;
    }

    float minDist    = pInstinct->fltArg[1] - -0.03f;
    float distRange  = (pInstinct->fltArg[2] - -0.03f) - minDist;
    float moveDist   = sithAIInstinct_Random() * distRange + minDist;
    float minSpeed   = pInstinct->fltArg[3];
    float maxSpeed   = pInstinct->fltArg[4];
    float speedRand  = sithAIInstinct_Random();

    rdVector3 dir;
    rdVector_Neg3(&dir, &pLocal->toTarget);

    int flags = pInstinct->intArg[9] ? 0x808006 : 0x808002;
    if ( pInstinct->fltArg[5] < 0.0f )
    {
        flags |= 0x4000;
    }

    rdVector3 movePos;
    if ( sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, moveDist, pInstinct->fltArg[5], flags, &dir, &movePos) )
    {
        if ( pLocal->pTargetThing )
        {
            pState->aParams[0] = pLocal->pTargetThing->pos.x;
            pState->aParams[1] = pLocal->pTargetThing->pos.y;
            pState->aParams[2] = pLocal->pTargetThing->pos.z;
            pState->aParams[3] = 1.0f;
        }
        else
        {
            pState->aParams[3] = 0.0f;
        }

        if ( pInstinct->intArg[8] || minDist - distRange * -0.5f <= moveDist )
        {
            sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
        }

        sithAIMove_AISetMoveTargetPos(pLocal, &movePos, (maxSpeed - minSpeed) * speedRand + minSpeed);
    }

    return 0;
}

// Human combat movement: advances, sidesteps and retreats while attacking, chases the goal when lost from sight
int J3DAPI sithAIInstinct_HumanCombatMove(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    SithThing* pOwner = pLocal->pOwner;
    int bLookAtMovePos = 1;
    int bCheckFireLine = 0;
    SITH_ASSERTREL(pLocal && pOwner);

    if ( !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0
        || (pLocal->mode & (SITHAI_MODE_UNKNOWN_100000 | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_FLEEING | SITHAI_MODE_SEARCHING)) != 0 )
    {
        goto reset;
    }

    sithAIUtil_sub_49B2E0(pLocal, 0);
    float health = pOwner->thingInfo.actorInfo.health / pOwner->thingInfo.actorInfo.maxHealth;

    if ( event == SITHAI_EVENT_GOAL_REACHED )
    {
        SithAIMode mode = pLocal->mode;
        if ( (mode & SITHAI_MODE_UNKNOWN_1000000) != 0 )
        {
            // Peeking from cover
            if ( pState->aParams[3] == 3.0f )
            {
                if ( pLocal->targetSightState != 0 )
                {
                    pLocal->mode = mode & ~SITHAI_MODE_UNKNOWN_1000000;
                    return 0;
                }

                sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->vecUnknown122);
                sithAIUtil_AIPauseMove(pLocal, rand() % 1001 + 2000);
                pState->aParams[3] = 2.0f;
            }
            else if ( pState->aParams[3] == 2.0f )
            {
                if ( pLocal->targetSightState == 0 )
                {
                    pLocal->mode = mode & ~SITHAI_MODE_UNKNOWN_1000000;
                    return 0;
                }

                sithAIUtil_AIPauseMove(pLocal, rand() % 2001 + 1000);
                pState->aParams[3] = 3.0f;
            }

            rdVector3 toTarget;
            rdVector_Sub3(&toTarget, &pLocal->vecUnknown122, &pLocal->pOwner->pos);
            rdVector_Normalize3Acc(&toTarget);

            rdVector3 toPrev;
            rdVector_Sub3(&toPrev, &pLocal->vecUnknown, &pLocal->pOwner->pos);
            rdVector_Normalize3Acc(&toPrev);

            pOwner->aiState = 1;
            if ( rdMath_DeltaAngleNormalized(&toPrev, &toTarget, &pOwner->orient.uvec) < 0.0f )
            {
                pOwner->aiArmedModeState = 0x201;
                return 1;
            }

            pOwner->aiArmedModeState = 0x401;
            return 1;
        }

        if ( (mode & SITHAI_MODE_CHASE_GOAL) == 0 )
        {
            if ( pLocal->pTargetThing )
            {
                sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
            }
        }
        else
        {
            // Move to the last seen goal position
            rdVector3 dir;
            float dist;
            if ( sithAIUtil_GetDistanceToTargetPos(pLocal->pOwner, &pLocal->pOwner->pos, &pLocal->vecUnknown5, -1.0f, pLocal->pClass->sightDistance, 0.0f, &dir, &dist) != 0 )
            {
                sithAIMove_AISetMovePos(pLocal, &pLocal->vecUnknown5, 1.7f);
                sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->vecUnknown5);
                return 1;
            }
        }

        goto reset;
    }

    if ( event == SITHAI_EVENT_GOAL_UNREACHABLE )
    {
        goto reset;
    }

    if ( (int)event > 0x100 )
    {
        return 0;
    }

    if ( event == SITHAI_EVENT_MODECHANGED )
    {
        if ( (pLocal->mode & SITHAI_MODE_CHASE_GOAL) == 0 || ((uintptr_t)pObject & SITHAI_MODE_CHASE_GOAL) != 0 || (pLocal->mode & SITHAI_MODE_UNKNOWN_1000000) != 0 )
        {
            return 0;
        }

        // Goal lost from sight, go back to previous position
        sithAIMove_StopAIMovement(pLocal);

        rdVector3 dir;
        float dist;
        int sightState = sithAIUtil_GetDistanceToTargetPos(pLocal->pOwner, &pLocal->pOwner->pos, &pLocal->vecUnknown5, -1.0f, pLocal->pClass->sightDistance, 0.0f, &dir, &dist);
        if ( health <= pInstinct->fltArg[6] && sithAIInstinct_Random() < pInstinct->fltArg[9] )
        {
            if ( sightState == 0 )
            {
                return 0;
            }

            // Take cover
            sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->vecUnknown);
            sithAIMove_AISetMovePos(pLocal, &pLocal->vecUnknown, pInstinct->fltArg[5]);
            pLocal->mode = (pLocal->mode & ~SITHAI_MODE_CHASE_GOAL) | SITHAI_MODE_UNKNOWN_1000000;
            pState->aParams[3] = 3.0f;
            return 1;
        }

        if ( sightState == 0 )
        {
            return 0;
        }

        sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->vecUnknown);
        sithAIMove_AISetMovePos(pLocal, &pLocal->vecUnknown, pInstinct->fltArg[5]);
        return 1;
    }

    if ( event == SITHAI_EVENT_HIT_SECTOR )
    {
        // Got hit
        if ( pInstinct->fltArg[6] < health )
        {
            return 0;
        }

        if ( sithAIInstinct_Random() < pInstinct->fltArg[7] )
        {
            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FLEE);
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
            pLocal->pFleeFromThing = pLocal->pTargetThing;
            return 0;
        }

        if ( sithAIInstinct_Random() < pInstinct->fltArg[8] && sithAI_HasInstinct(pLocal, "circlestrafe") )
        {
            sithAI_EnableInstinct(pLocal, "circlestrafe", 1);
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_400000;
        }

        if ( pInstinct->fltArg[10] <= sithAIInstinct_Random() )
        {
            return 0;
        }

        // Go berserk
        sithAIMove_StopAIMovement(pLocal);
        pLocal->mode |= SITHAI_MODE_UNKNOWN_100000;
        sithAI_ForceInstinctUpdate(pLocal, "primaryfire", 0);
        sithAI_ForceInstinctUpdate(pLocal, "lobfire", 0);
        sithAI_ForceInstinctUpdate(pLocal, "alternatefire", 0);
        return 0;
    }

    if ( event != 0 )
    {
        return 0;
    }

    if ( pLocal->targetSightState != 0 )
    {
        return 0;
    }

    if ( sithAIUtil_sub_49F1F0(pLocal, &pLocal->pOwner->pos) )
    {
        bCheckFireLine = 1;
        if ( !sithAIInstinct_IsInVehicle(pLocal->pTargetThing) && sithAIUtil_IsThingMoving(pLocal->pTargetThing) )
        {
            pState->aParams[1] = pState->aParams[1] - -0.2f;
        }

        if ( (float)sithTime_g_msecGameTime < pState->aParams[0] )
        {
            return 0;
        }

        float msecInterval = pInstinct->fltArg[0];
        pState->aParams[0] = (float)sithTime_g_msecGameTime + ((sithAIInstinct_Random() - 0.5f) * msecInterval + msecInterval);
        if ( pState->aParams[1] < sithAIInstinct_Random() )
        {
            pState->aParams[1] = pState->aParams[1] - -0.05f;
            return 0;
        }
    }

    // Select the move: 1 - forward, 2 - backward, 4 - sideways
    float minDist     = pInstinct->fltArg[2] - -0.03f;
    float maxDist     = pInstinct->fltArg[3] - -0.03f;
    float moveDist    = (maxDist - minDist) * sithAIInstinct_Random() + minDist;
    float minSpeed    = pInstinct->fltArg[4];
    float moveSpeed   = pInstinct->fltArg[5];
    unsigned int failedMoves = (unsigned int)(int)pState->aParams[2];

    unsigned int moveType;
    rdVector3 dir;
    float angle;
    int flags;
    if ( failedMoves == 0 )
    {
        float r = sithAIInstinct_Random();
        if ( r >= 0.0f && r <= 0.4f )
        {
            goto move_forward;
        }

        if ( !(r >= 0.5f && r <= 0.8f) )
        {
            goto move_sideways;
        }

        goto move_backward;
    }

    if ( (failedMoves & 1) == 0 )
    {
        goto move_forward;
    }

    if ( (failedMoves & 2) == 0 )
    {
        goto move_backward;
    }

    if ( (failedMoves & 4) != 0 )
    {
        pState->aParams[2] = 0.0f;
        return 0;
    }

move_sideways:
    moveType = 4;
    if ( pInstinct->intArg[1] == 0 )
    {
        return 1;
    }

    moveSpeed = (moveSpeed - minSpeed) * sithAIInstinct_Random() + minSpeed;
    angle     = 30.0f;
    flags     = 0xC002;
    dir       = pLocal->toTarget;
    goto make_move;

move_forward:
    moveType = 1;
    dir      = pLocal->toTarget;
    angle    = 0.0f;
    flags    = 0x8001;
    goto make_move;

move_backward:
    moveType = 2;
    if ( pInstinct->intArg[1] == 0 && pOwner->collide.movesize * 4.0f < pLocal->distance )
    {
        return 1;
    }

    rdVector_Neg3(&dir, &pLocal->toTarget);
    bLookAtMovePos = 0;
    angle     = 30.0f;
    flags     = 0xC002;
    moveSpeed = minSpeed;

make_move:
    {
        rdVector3 movePos;
        if ( sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, moveDist, angle, flags, &dir, &movePos)
            && (!bCheckFireLine || sithAIUtil_sub_49F1F0(pLocal, &movePos)) )
        {
            pState->aParams[1] = 0.0f;
            pState->aParams[2] = 0.0f;

            rdVector3 lookPos;
            if ( bLookAtMovePos )
            {
                lookPos = movePos;
            }
            else
            {
                // Moving backwards, look away from the move position
                lookPos.x = (pOwner->pos.x - movePos.x) + pOwner->pos.x;
                lookPos.y = (pOwner->pos.y - movePos.y) + pOwner->pos.y;
                lookPos.z = (pOwner->pos.z - movePos.z) + pOwner->pos.z;
            }

            sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
            sithAIMove_AISetMoveTargetPos(pLocal, &movePos, moveSpeed);
            return 0;
        }

        pState->aParams[2] = (float)(failedMoves | moveType);
        return 0;
    }

reset:
    pState->aParams[1] = 0.0f;
    pState->aParams[2] = 0.0f;
    return 0;
}

// Roams around randomly or around home position
int J3DAPI sithAIInstinct_Roam(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    float moveSpeed   = 0.75f;
    int bLimitArea    = 0;
    SithAIMode mode   = pLocal->mode;
    if ( (mode & (SITHAI_MODE_FLEEING | SITHAI_MODE_ACTIVE | SITHAI_MODE_UNKNOWN_10 | SITHAI_MODE_ATTACKING)) != 0 )
    {
        return 0;
    }

    if ( event == SITHAI_EVENT_MODECHANGED && pThing && (mode & SITHAI_MODE_SEARCHING) != 0 && ((uintptr_t)pThing & SITHAI_MODE_SEARCHING) == 0 )
    {
        // Started searching
        pState->msecNextUpdate = pInstinct->intArg[0] + sithTime_g_msecGameTime;
        return 0;
    }

    if ( (mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 )
    {
        return 0;
    }

    float minDist   = pLocal->pOwner->collide.movesize - -0.03f;
    float radius    = pInstinct->fltArg[1];
    float moveDist  = (fabsf(radius) - minDist) * sithAIInstinct_Random() + minDist;
    if ( pInstinct->fltArg[3] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[3];
    }

    rdVector3 centerPos = radius <= 0.0f ? pLocal->pOwner->pos : pLocal->homePos;
    if ( pInstinct->fltArg[2] != 0.0f )
    {
        bLimitArea = 1;
    }

    rdVector3 movePos;
    switch ( event )
    {
        case SITHAI_EVENT_GOAL_REACHED:
            if ( (pLocal->submode & (SITHAI_SUBMODE_SEMICONTINUOUSMOTION | SITHAI_SUBMODE_CONTINUOUSMOTION)) == 0 )
            {
                return 0;
            }
            // fallthrough

        case 0:
            pState->msecNextUpdate = pInstinct->intArg[0] + sithTime_g_msecGameTime;
            if ( (pLocal->mode & SITHAI_MODE_INSTINCTUSEWPNTS) != 0 && sithAIUtil_AIMoveToNextWpnt(pLocal, moveSpeed, 0.0f, 1) )
            {
                return 0;
            }

            if ( radius > 0.0f )
            {
                // Return home when too far
                rdVector3 vec;
                rdVector_Sub3(&vec, &pLocal->pOwner->pos, &pLocal->homePos);
                if ( radius < sqrtf(vec.z * vec.z + vec.y * vec.y + vec.x * vec.x) )
                {
                    sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->homePos);
                    sithAIMove_AISetMovePos(pLocal, &pLocal->homePos, moveSpeed);
                    return 0;
                }
            }
            break;

        case SITHAI_EVENT_TOUCHED:
        case SITHAI_EVENT_HIT_WALL:
        case SITHAI_EVENT_HIT_CLIFF:
        {
            // Turn back
            rdVector3 dir;
            sithAIUtil_GetXYHeadingVector(pLocal->pOwner, &dir);
            rdVector_Neg3Acc(&dir);
            if ( sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, moveDist, 45.0f, 0x20C002, &dir, &movePos) )
            {
                sithAIMove_StopAIMovement(pLocal);
                if ( sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed) )
                {
                    sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
                    return 1;
                }
            }
        } break;

        default:
            return 0;
    }

    if ( sithAIUtil_sub_49F010(pLocal, &centerPos, minDist, moveDist, bLimitArea, 2, &movePos) && sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
        return 1;
    }

    if ( !sithAIMove_AISetMovePos(pLocal, &pLocal->homePos, moveSpeed) )
    {
        return 0;
    }

    sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->homePos);
    return 1;
}

int J3DAPI sithAIInstinct_ReturnHome(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    J3D_UNUSED(pInstinct);
    J3D_UNUSED(pState);

    if ( event == SITHAI_EVENT_MODECHANGED )
    {
        if ( (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 && ((uintptr_t)pObject & SITHAI_MODE_SEARCHING) == 0 )
        {
            sithAIMove_AISetMovePos(pLocal, &pLocal->homePos, 1.2f);
            sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->homePos);
        }
    }
    else if ( event == SITHAI_EVENT_GOAL_REACHED && (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 )
    {
        // Look at home direction
        rdVector3 lookPos;
        rdVector_Add3(&lookPos, &pLocal->homeOrient, &pLocal->pOwner->pos);
        sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
    }

    return 0;
}

int J3DAPI sithAIInstinct_Flee(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    float secMaxFleeTime = pInstinct->fltArg[2];
    float minDist        = pLocal->pOwner->collide.movesize - -0.03f;
    float fleeDist       = pInstinct->fltArg[0];
    float moveSpeed      = 1.5f;

    if ( (pLocal->mode & SITHAI_MODE_FLEEING) == 0 )
    {
        if ( pState->aParams[0] <= 0.0f )
        {
            return 0;
        }

        goto stop_flee;
    }

    pLocal->goalThing    = NULL;
    pLocal->pTargetThing = NULL;

    bool bStart = pState->aParams[0] == 0.0f;
    if ( bStart )
    {
        pState->aParams[0] = sithTime_g_secGameTime; // flee start time
    }

    if ( secMaxFleeTime == 0.0f )
    {
        secMaxFleeTime = (pLocal->mode & SITHAI_MODE_FLEEINGTOWAYPOINT) != 0 ? 25.1f : 10.0f;
    }

    if ( !pLocal->pFleeFromThing || pState->aParams[0] + secMaxFleeTime < sithTime_g_secGameTime )
    {
        goto stop_flee;
    }

    if ( pInstinct->fltArg[4] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[4];
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + (pInstinct->intArg[1] ? pInstinct->intArg[1] : 5000);

    rdVector3 dir;
    float dist;
    if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pLocal->pFleeFromThing, -1.0f, fleeDist, 0.0f, &dir, &dist) != 0 && !bStart )
    {
        // Escaped
        if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) != 0 )
        {
            return 0;
        }

        goto stop_flee;
    }

    bool bAway = false;
    switch ( event )
    {
        case SITHAI_EVENT_TOUCHED:
            if ( pThing && pThing->type == SITH_THING_PLAYER )
            {
                return 1;
            }
            break;

        case 0:
        case SITHAI_EVENT_GOAL_REACHED:
            bAway = true;
            break;

        case SITHAI_EVENT_MODECHANGED:
            if ( !pThing || (pLocal->mode & SITHAI_MODE_FLEEING) == 0 || ((uintptr_t)pThing & SITHAI_MODE_FLEEING) != 0 )
            {
                return 0;
            }

            bAway = true;
            break;

        default:
            break;
    }

    if ( bAway )
    {
        // Move away from the flee thing
        rdVector_Neg3Acc(&dir);
    }
    else
    {
        // Keep moving forward
        sithAIUtil_GetXYHeadingVector(pLocal->pOwner, &dir);
        pLocal->mode &= ~SITHAI_MODE_TRAVERSEWPNTS;
    }

    if ( (pLocal->mode & SITHAI_MODE_INSTINCTUSEWPNTS) != 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 || sithAIUtil_AIMoveToNextWpnt(pLocal, moveSpeed, 0.0f, 0) )
        {
            return 1;
        }
    }

    sithAIMove_StopAIMovement(pLocal);

    int flags = 0x21002;
    if ( pInstinct->fltArg[5] > 0.0f )
    {
        minDist = pInstinct->fltArg[5];
    }

    float angle = pInstinct->fltArg[3];
    if ( angle != 0.0f )
    {
        flags = angle < 0.0f ? 0x2D002 : 0x29002;
    }

    rdVector3 movePos;
    if ( (sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, fleeDist, angle, flags, &dir, &movePos) && sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed))
        || (sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, fleeDist, angle, 0x29001, &dir, &movePos) && sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed)) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
        return 1;
    }

stop_flee:
    if ( pLocal->pFleeFromThing )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->pFleeFromThing->pos);
    }

    sithAIMove_StopAIMovement(pLocal);
    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
    pState->aParams[0] = 0.0f;
    pLocal->pFleeFromThing = NULL;
    return 0;
}

// Follows the goal thing keeping the distance between min and max distance
int J3DAPI sithAIInstinct_BasicFallow(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    float moveSpeed = 1.0f;
    float moveDist  = 0.0f;
    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);

    SithAIMode mode = pLocal->mode;
    SithThing* pOwner = pLocal->pOwner;
    if ( (mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_FLEEING | SITHAI_MODE_UNKNOWN_10)) != 0 )
    {
        return 0;
    }

    if ( (mode & SITHAI_MODE_CIRCLESTRAFING) != 0 )
    {
        if ( (mode & SITHAI_MODE_MOVING) != 0 )
        {
            return 0;
        }

        pLocal->mode = mode & ~SITHAI_MODE_CIRCLESTRAFING;
    }

    mode = pLocal->mode;
    if ( (mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 )
    {
        return 0;
    }

    rdVector3 heading = { 0 }; // Added: Init
    if ( (event & (SITHAI_EVENT_HIT_THING | SITHAI_EVENT_HIT_WALL | SITHAI_EVENT_TOUCHED)) != 0 )
    {
        if ( (mode & SITHAI_MODE_SEARCHING) != 0 || (mode & (SITHAI_MODE_ACTIVE | SITHAI_MODE_MOVING)) == 0 )
        {
            return 0;
        }

        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_4) != 0 )
        {
            return 1;
        }

        sithAIUtil_GetXYHeadingVector(pOwner, &heading);
        if ( pState->aParams[0] > 3.0f )
        {
            // Stuck, turn back
            rdVector_Neg3Acc(&heading);
            pState->aParams[0] = 0.0f;
            pState->aParams[1] = 0.0f;
            pLocal->vecUnknown3 = pLocal->movePos;

            rdVector3 backPos;
            rdVector_ScaleAdd3(&backPos, &heading, pOwner->collide.movesize * 4.0f, &pOwner->pos);
            if ( !sithAIUtil_sub_49EE50(pLocal, &backPos, 0.0f, 60.0f, 30.0f, 0x100) )
            {
                return 0;
            }

            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_8;
            return 1;
        }
    }

    if ( (event & (SITHAI_EVENT_HIT_THING | SITHAI_EVENT_HIT_CLIFF | SITHAI_EVENT_HIT_WALL | SITHAI_EVENT_TOUCHED)) != 0 )
    {
        pState->aParams[1] = 1.0f;
    }

    if ( pInstinct->intArg[5] && sithPuppet_IsAnyModeOnTrack(pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
        return 0;
    }

    SithThing* pGoal = pLocal->goalThing;
    if ( !pGoal )
    {
        return 0;
    }

    sithAIUtil_sub_49B640(pLocal);
    switch ( event )
    {
        case SITHAI_EVENT_TOUCHED:
        case SITHAI_EVENT_HIT_THING:
        {
            if ( pObject )
            {
                const SithThing* pHitThing = (const SithThing*)pObject;
                if ( pHitThing->type == SITH_THING_PLAYER )
                {
                    return 0;
                }

                // Moving away from the thing
                rdVector3 awayDir;
                awayDir.x = pOwner->pos.x - pHitThing->pos.x;
                awayDir.y = pOwner->pos.y - pHitThing->pos.y;
                awayDir.z = 0.0f;
                rdVector_Normalize3Acc(&awayDir);
                if ( rdVector_Dot3(&heading, &awayDir) > 0.0f )
                {
                    return 1;
                }
            }

            pState->aParams[0] = pState->aParams[0] - -1.0f;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4;
            return sithAIUtil_sub_49EE50(pLocal, &pLocal->movePos, 45.0f, 135.0f, 45.0f, 0x100) != 0;
        }

        case 0:
            if ( pInstinct->intArg[0] )
            {
                pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
            }
            break;

        case SITHAI_EVENT_GOAL_REACHED:
        {
            bool bWasHit = pState->aParams[1] == 1.0f;
            pState->aParams[0] = 0.0f;
            if ( bWasHit )
            {
                pState->aParams[1] = 0.0f;
                sithAIMove_AISetLookPosEyeLevel(pLocal, &pGoal->pos);
            }

            if ( (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 || (pLocal->submode & (SITHAI_SUBMODE_SEMICONTINUOUSMOTION | SITHAI_SUBMODE_CONTINUOUSMOTION)) == 0 )
            {
                return 0;
            }
        } break;

        case SITHAI_EVENT_HIT_WALL:
        {
            if ( !pObject )
            {
                return 0;
            }

            // Wall normal on horizontal plane
            const SithSurface* pWall = (const SithSurface*)pObject;
            rdVector3 wallNormal;
            wallNormal.x = stdMath_ClipNearZero(pWall->face.normal.x);
            wallNormal.y = stdMath_ClipNearZero(pWall->face.normal.y);
            wallNormal.z = 0.0f;
            rdVector_Normalize3Acc(&wallNormal);

            if ( pLocal->targetSightState2 == 0 )
            {
                rdVector3 goalDir;
                goalDir.x = stdMath_ClipNearZero(pLocal->vecUnknown0.x);
                goalDir.y = stdMath_ClipNearZero(pLocal->vecUnknown0.y);
                goalDir.z = 0.0f;
                rdVector_Normalize3Acc(&goalDir);

                // Heading towards goal and not into the wall
                if ( heading.y * goalDir.y + heading.x * goalDir.x > 0.701f && heading.y * wallNormal.y + heading.x * wallNormal.x > -0.966f )
                {
                    return 1;
                }
            }

            if ( pLocal->pClass->rank < 0.5f && pLocal->pClass->rank < sithAIInstinct_Random() )
            {
                return 0;
            }

            pState->aParams[0] = pState->aParams[0] - -1.0f;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4;

            rdVector3 movePos;
            if ( pLocal->targetSightState2 == 0 )
            {
                // Move along the wall
                float minMoveDist = pOwner->collide.movesize;
                unsigned int result = sithAIUtil_CheckPathToPos(pLocal, pOwner->pInSector, &pOwner->pos, 0x2120, minMoveDist, minMoveDist + minMoveDist, 0.0f, &wallNormal, &movePos);
                if ( (result & 2) == 0 )
                {
                    goto move_target;
                }
            }

            {
                float minMoveDist = pOwner->collide.movesize <= 0.09f ? 0.09f : pOwner->collide.movesize;
                float maxMoveDist = sithAIInstinct_Random() * 0.1f + minMoveDist;
                if ( !sithAIUtil_MakePathPos(pLocal, &pOwner->pos, minMoveDist, maxMoveDist, 45.0f, 0x8102, &wallNormal, &movePos) )
                {
                    return 0;
                }

                sithAIMove_StopAIMovement(pLocal);
            }

        move_target:
            sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
            sithAIMove_AISetMoveTargetPos(pLocal, &movePos, 1.0f);
            return 1;
        }

        default:
            return 0;
    }

    // Follow the goal
    pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_2000000;
    if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) == 0 && sithAIInstinct_IsInVehicle(pGoal) )
    {
        return 0;
    }

    float minDist, maxDist, backDist;
    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) == 0 )
    {
        minDist  = pInstinct->fltArg[1];
        maxDist  = pInstinct->fltArg[2];
        backDist = pInstinct->fltArg[3];
        if ( sithWeapon_HasWeaponSelected(pOwner) )
        {
            backDist = 0.0f;
            maxDist  = sithWeapon_GetWeaponMaxAimDistance(pOwner->thingInfo.actorInfo.curWeaponID) - 0.1f;
        }

        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400000) != 0 )
        {
            minDist  = (float)(minDist * 0.5);
            maxDist  = (float)(maxDist * 0.5);
            backDist = (float)(backDist * 0.5);
        }
    }
    else
    {
        // Berserk, rush in
        moveSpeed = 2.0f;
        maxDist   = pOwner->collide.movesize * 3.0f;
        minDist   = pInstinct->fltArg[1];
        backDist  = 0.0f;
        if ( minDist <= maxDist )
        {
            maxDist = minDist;
        }
    }

    // Match goal velocity when close
    if ( (pLocal->submode & SITHAI_SUBMODE_USEMATCHVELOCITY) == 0
        || sithAIInstinct_IsZeroVector(&pLocal->goalThing->moveInfo.physics.velocity)
        || pLocal->targetDistance < 0.0f
        || pOwner->collide.movesize + pOwner->collide.movesize < pLocal->targetDistance )
    {
        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_8000;
    }
    else
    {
        pLocal->mode |= SITHAI_MODE_UNKNOWN_8000;
    }

    if ( pLocal->targetSightState2 == 0 || pLocal->targetSightState2 == 2 )
    {
        pLocal->mode &= ~SITHAI_MODE_CHASE_GOAL;
        if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) == 0 )
        {
            sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
        }

        sithAIMove_AISetLookPosEyeLevel(pLocal, &pGoal->pos);
        if ( pLocal->targetDistance > maxDist )
        {
            moveDist = pLocal->targetDistance - maxDist;
        }
        else if ( pLocal->targetDistance < minDist )
        {
            moveDist = backDist == 0.0f ? pLocal->targetDistance - minDist : pLocal->targetDistance - backDist;
        }

        rdVector3 movePos;
        if ( moveDist == 0.0f )
        {
            // In range
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_2000000;
            if ( (pOwner->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
            {
                return 0;
            }

            // Flyer moves to goal height
            movePos.z = pOwner->collide.movesize * 0.5f + pGoal->pos.z;
            if ( fabsf(pOwner->pos.z - movePos.z) <= 0.05f )
            {
                return 0;
            }

            movePos.x = pOwner->pos.x;
            movePos.y = pOwner->pos.y;
        }
        else
        {
            if ( moveDist < 0.0f )
            {
                pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_2000000;
            }

            rdVector_ScaleAdd3(&movePos, &pLocal->vecUnknown0, moveDist, &pOwner->pos);
            if ( (pOwner->moveInfo.physics.flags & SITH_PF_FLY) != 0 )
            {
                movePos.z = pOwner->collide.movesize + pGoal->pos.z;
            }
            else if ( (pOwner->flags & SITH_TF_SUBMERGED) != 0 )
            {
                movePos.z = pGoal->pos.z;
            }
        }

        rdVector3 tmpDir;
        float tmpDist;
        int sightState = sithAIUtil_GetDistanceToTargetPos(pGoal, &pGoal->pos, &movePos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &tmpDir, &tmpDist);
        if ( pInstinct->fltArg[4] != 0.0f || sightState == 0 )
        {
            sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed);
            return 1;
        }

        if ( (pLocal->mode & SITHAI_MODE_WANTALLEVENTS) != 0 )
        {
            sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_UNREACHABLE, NULL);
        }

        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_CHASE_GOAL) == 0 )
    {
        // Lost the goal from sight, chase
        if ( (pGoal->type == SITH_THING_PLAYER || pGoal->type == SITH_THING_ACTOR) && (pGoal->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            return 0;
        }

        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
        {
            return 0;
        }

        pLocal->mode |= SITHAI_MODE_CHASE_GOAL;
        return 1;
    }

    return 0;
}

// Follows goal crawling on walls
int J3DAPI sithAIInstinct_WallCrawl(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithSurface* pSurface)
{
    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;
    if ( (event & (SITHAI_EVENT_HIT_FLOOR | SITHAI_EVENT_HIT_WALL)) != 0 && (pLocal->mode & (SITHAI_MODE_ACTIVE | SITHAI_MODE_MOVING)) == 0 )
    {
        return 0;
    }

    float minHeight = pOwner->collide.movesize - -0.03f;
    int moveFlags;
    if ( (pLocal->mode & (SITHAI_MODE_ACTIVE | SITHAI_MODE_ATTACKING)) == 0 )
    {
        moveFlags = 0x400;
    }
    else
    {
        sithAIUtil_sub_49B640(pLocal);
        moveFlags = 0x100;
    }

    rdVector3 movePos;
    sithAIUtil_AIGetMovePos(pLocal, moveFlags, &movePos);

    switch ( event )
    {
        case 0:
            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
            {
                if ( (pOwner->attach.flags & SITH_ATTACH_SURFACE) != 0 && (pOwner->attach.attachedToStructure.pSurfaceAttached->flags & SITH_SURFACE_ISFLOOR) != 0 )
                {
                    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_WALLCRAWLING);
                }

                pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
                return 1;
            }
            break;

        case SITHAI_EVENT_TOUCHED:
        case SITHAI_EVENT_HIT_THING:
            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
            {
                return 1;
            }
            break;

        case SITHAI_EVENT_HIT_CLIFF:
            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) == 0 )
            {
                return 0;
            }

            sithAIMove_Unreachable(pLocal);
            return 1;

        case SITHAI_EVENT_HIT_WALL:
            if ( (pLocal->submode & (SITHAI_SUBMODE_WALLCRAWLLOCKED | SITHAI_SUBMODE_UNKNOWN_4)) != 0 )
            {
                return 1;
            }

            if ( !pSurface )
            {
                break;
            }

            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) == 0 )
            {
                if ( movePos.z - pOwner->pos.z <= minHeight )
                {
                    break; // move pos is not above
                }

                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_WALLCRAWLING);
            }
            goto mount_wall;

        case SITHAI_EVENT_HIT_FLOOR:
        case SITHAI_EVENT_LAND_FLOOR:
            if ( !pSurface )
            {
                break;
            }

            if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_400000) != 0 )
            {
                pLocal->mode &= ~SITHAI_MODE_UNKNOWN_400000;
                return 1;
            }

            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) == 0 )
            {
                break;
            }

            sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_WALLCRAWLING);
            goto mount_wall;

        default:
            return 0;
    }

    if ( (pLocal->mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_FLEEING | SITHAI_MODE_UNKNOWN_10 | SITHAI_MODE_SEARCHING)) != 0 )
    {
        return 0;
    }

    return sithAIInstinct_BasicFallow(pLocal, pInstinct, pState, event, pSurface);

mount_wall:
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_4) != 0 )
    {
        return 1;
    }

    if ( rdVector_Dot3(&pSurface->face.normal, &pOwner->orient.lvec) < 0.0f )
    {
        pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4;
        pLocal->vecUnknown3 = pSurface->face.normal;
        if ( sithAIMove_sub_497FF0(pLocal, 0x40) )
        {
            return 1;
        }

        sithThing_AttachThingToSurface(pOwner, pSurface, 1);
    }

    return 1;
}

// Selects snake move position, tries left & right positions and then sweeps around the look direction
void J3DAPI sithAIInstinct_sub_494360(SithAIControlBlock* pLocal, rdVector3* moveToPosLeft, rdVector3* moveToPosRight)
{
    SithThing* pOwner = pLocal->pOwner;

    rdVector3 vec;
    rdVector_Sub3(&vec, moveToPosLeft, &pOwner->pos);
    rdVector_Normalize3Acc(&vec);
    rdMath_DeltaAngleNormalized(&vec, &pOwner->orient.lvec, &rdroid_g_zVector3); // Note, result is not used

    rdVector3 dir;
    rdVector_Sub3(&vec, moveToPosLeft, &pOwner->pos);
    float dist = rdVector_Normalize3(&dir, &vec);
    if ( sithAIInstinct_SnakeMungeTestCheck(pLocal, &dir, dist, moveToPosLeft) )
    {
        return;
    }

    rdVector_Sub3(&vec, moveToPosRight, &pOwner->pos);
    dist = rdVector_Normalize3(&dir, &vec);
    if ( sithAIInstinct_SnakeMungeTestCheck(pLocal, &dir, dist, moveToPosLeft) )
    {
        return;
    }

    // Sweep around the look direction alternating sides
    int sign = sithAIInstinct_Random() - 0.5f >= 0.0f ? 1 : -1;
    rdVector3 pyr = { 0.0f, 0.0f, 0.0f };
    dir = pOwner->orient.lvec;
    for ( int numRetries = 24; ; --numRetries )
    {
        bool bFound = sithAIInstinct_SnakeMungeTestCheck(pLocal, &dir, 0.3f, moveToPosLeft) != 0;
        if ( pyr.y == 0.0f )
        {
            pyr.y = (float)sign * 15.0f;
        }
        else if ( (pyr.y >= 0.0f ? 1 : -1) == sign )
        {
            pyr.y = -pyr.y;
        }
        else
        {
            pyr.y = (float)sign * 15.0f - pyr.y;
        }

        rdVector_Rotate3(&dir, &pOwner->orient.lvec, &pyr);
        if ( bFound )
        {
            return; // Note, the original formats 'Retires %d' string here but doesn't use it
        }

        if ( numRetries == 0 )
        {
            break;
        }
    }

    *moveToPosLeft = pOwner->pos;
}

// Checks if snake can move from its position in direction for distance. pOutPoint is set to the reachable point.
int J3DAPI sithAIInstinct_SnakeMungeTestCheck(SithAIControlBlock* pLocal, rdVector3* pDirection, float distance, rdVector3* pOutPoint)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return 0;
    }

    float fudge = 0.0f;
    if ( pOwner->userblock.pQuetz->bPosFudged )
    {
        fudge = pOwner->userblock.pQuetz->unknown393;
    }

    rdVector_ScaleAdd3(pOutPoint, pDirection, distance, &pOwner->pos);

    rdVector3 startPos = pOwner->pos;
    startPos.z = (((0.3f - sithPhysics_GetThingHeight(pOwner)) - -0.002f) - fudge) + startPos.z;

    SithSector* pStartSector = sithCollision_FindSectorInRadius(pOwner->pInSector, &pOwner->pos, &startPos, 0.0f);
    if ( !pStartSector )
    {
        SITHLOG_ERROR("_SnakeMungeTestCheck - couldn't find sector of start pointt");
        return 0;
    }

    // Note, the original OR's the float bits of distance into the flags
    uint32_t distBits;
    memcpy(&distBits, &distance, sizeof(distBits));

    rdVector3 hitPos;
    int result = sithAIUtil_CheckPathToPos(pLocal, pStartSector, &startPos, (int)(distBits | 0x62010), 0.0f, 0.05f + distance, 0.3f, pDirection, &hitPos);
    if ( !result )
    {
        return 0;
    }

    if ( result != 1 )
    {
        // Blocked, move to before the hit position
        float hitDist = sqrtf((startPos.y - hitPos.y) * (startPos.y - hitPos.y) + (startPos.x - hitPos.x) * (startPos.x - hitPos.x));
        if ( hitDist - 0.05f < 0.2f )
        {
            return 0;
        }

        rdVector_ScaleAdd3(pOutPoint, pDirection, hitDist - 0.05f, &pOwner->pos);
    }

    return 1;
}

// Quetzalcoatl (snake) follows the goal slithering
signed int J3DAPI sithAIInstinct_SnakeFollow(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    J3D_UNUSED(pInstinct);
    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);

    SithThing* pOwner = pLocal->pOwner;
    bool bMove = true;
    switch ( event )
    {
        case SITHAI_EVENT_TOUCHED:
            if ( pThing->type == SITH_THING_PLAYER )
            {
                bMove = false;
            }
            break;

        case 0:
            if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
            {
                bMove = false;
            }
            break;

        case SITHAI_EVENT_HIT_CLIFF:
        case SITHAI_EVENT_HIT_WALL:
        case SITHAI_EVENT_HIT_THING:
            if ( (pLocal->mode & SITHAI_MODE_MOVING) == 0 || (pLocal->mode & SITHAI_MODE_ACTIVE) == 0 )
            {
                return 1;
            }
            break;

        case SITHAI_EVENT_GOAL_REACHED:
            break;

        default:
            return 0;
    }

    if ( !pLocal->goalThing )
    {
        pLocal->submode &= ~SITHAI_SUBMODE_CONTINUOUSMOTION;
        return 0;
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
    sithAIUtil_sub_49B640(pLocal);
    sithAIUtil_sub_49B2E0(pLocal, 0);

    rdVector3 goalDir;
    float goalDist;
    if ( pLocal->targetSightState2 == 0 || pLocal->targetSightState2 == 2 )
    {
        pState->aParams[0] = 0.0f;
        goalDir  = pLocal->vecUnknown0;
        goalDist = pLocal->targetDistance;
        pLocal->mode |= SITHAI_MODE_LOSTSIGHTOFGOAL;
    }
    else
    {
        // Move to last seen goal position
        pState->aParams[0] = 1.0f;
        rdVector_Sub3(&goalDir, &pLocal->vecUnknown5, &pLocal->pOwner->pos);
        goalDist = rdVector_Normalize3Acc(&goalDir);
        pLocal->mode &= ~SITHAI_MODE_LOSTSIGHTOFGOAL;
    }

    if ( bMove )
    {
        SithThing* pCurOwner = pLocal->pOwner;
        float rightDot = pCurOwner->orient.rvec.y * goalDir.y + pCurOwner->orient.rvec.x * goalDir.x + pCurOwner->orient.rvec.z * goalDir.z;
        float fwdDot   = rdVector_Dot3(&pCurOwner->orient.lvec, &goalDir);

        float yaw = rdMath_DeltaAngleNormalized(&pCurOwner->orient.lvec, &goalDir, &rdroid_g_zVector3);
        if ( yaw >= 0.0f )
        {
            yaw = rdMath_DeltaAngleNormalized(&pLocal->pOwner->orient.lvec, &goalDir, &rdroid_g_zVector3);
        }
        else
        {
            yaw = -rdMath_DeltaAngleNormalized(&pLocal->pOwner->orient.lvec, &goalDir, &rdroid_g_zVector3);
        }

        float nearDist, farDist;
        bool bAttacking = (pLocal->mode & SITHAI_MODE_ATTACKING) != 0;
        if ( !bAttacking )
        {
            nearDist = sithAIMove_g_flt_585468;
            farDist  = 0.65f;
        }
        else
        {
            nearDist = 0.3f;
            farDist  = 0.8f;
        }

        float leftYaw = yaw, rightYaw, leftDist, rightDist;
        if ( fwdDot > -0.98f )
        {
            if ( fwdDot >= 0.5f )
            {
                if ( goalDist <= farDist )
                {
                    if ( goalDist <= nearDist )
                    {
                        // Close to goal, coil
                        rightDist = goalDist - -0.1f;
                        rightYaw  = yaw + yaw;
                        leftYaw   = -yaw;
                        leftDist  = rightDist;
                    }
                    else
                    {
                        rightYaw = yaw;
                        if ( !bAttacking )
                        {
                            rightDist = goalDist - 0.05f;
                            leftDist  = goalDist;
                            if ( rightDist < 0.1f )
                            {
                                rightDist = 0.1f;
                            }
                            else if ( goalDist < rightDist )
                            {
                                rightDist = goalDist;
                            }
                        }
                        else
                        {
                            leftDist = goalDist - 0.1f;
                            if ( leftDist < 0.1f )
                            {
                                leftDist = 0.1f;
                            }
                            else if ( goalDist < leftDist )
                            {
                                leftDist = goalDist;
                            }

                            rightDist = goalDist - 0.45f;
                            if ( rightDist < 0.1f )
                            {
                                rightDist = 0.1f;
                            }
                            else if ( goalDist < rightDist )
                            {
                                rightDist = goalDist;
                            }
                        }
                    }
                }
                else if ( sithAIInstinct_flt_539AB8 >= 0.5f )
                {
                    // Was facing goal last time too, slither
                    float slitherYaw = yaw + 30.0f;
                    leftYaw   = slitherYaw >= 0.0f ? (slitherYaw > 60.0f ? 60.0f : slitherYaw) : 0.0f;
                    leftDist  = 0.5f;
                    rightDist = 0.5f;
                    rightYaw  = yaw - 10.0f;
                }
                else
                {
                    rightYaw  = 0.0f;
                    leftDist  = 0.2f;
                    rightDist = 0.2f;
                }
            }
            else
            {
                // Goal to the side, turn
                rightYaw  = 60.0f;
                leftDist  = 0.25f;
                rightDist = 0.25f;
                leftYaw   = 45.0f;
            }
        }
        else
        {
            // Goal right behind, turn to random side
            float sign = (float)(sithAIInstinct_Random() - 0.5f >= 0.0f ? 1 : -1);
            leftDist  = 0.25f;
            rightDist = 0.25f;
            rightYaw  = -(sign * 45.0f);
            leftYaw   = sign * 45.0f;
        }

        if ( rightDot > 0.0f )
        {
            leftYaw  = -leftYaw;
            rightYaw = -rightYaw;
        }

        sithAIInstinct_flt_539AB8 = fwdDot;

        rdVector3 pyrLeft = { 0.0f, leftYaw, 0.0f };
        rdVector3 dir;
        rdVector_Rotate3(&dir, &pLocal->pOwner->orient.lvec, &pyrLeft);

        rdVector3 leftPos;
        leftPos.x = leftDist * dir.x + pLocal->pOwner->pos.x;
        leftPos.y = leftDist * dir.y + pLocal->pOwner->pos.y;

        rdVector3 pyrRight = { 0.0f, rightYaw, 0.0f };
        rdVector_Rotate3(&dir, &pLocal->pOwner->orient.lvec, &pyrRight);

        rdVector3 rightPos;
        rightPos.x = rightDist * dir.x + pLocal->pOwner->pos.x;
        rightPos.y = rightDist * dir.y + pLocal->pOwner->pos.y;
        leftPos.z  = pLocal->pOwner->pos.z;
        rightPos.z = pLocal->pOwner->pos.z;

        sithAIInstinct_sub_494360(pLocal, &leftPos, &rightPos);
        sithAIMove_AISetMovePos(pLocal, &leftPos, 2.0f);

        rdVector3 lookPos = pLocal->movePos;
        lookPos.z = pLocal->pOwner->thingInfo.actorInfo.eyeOffset.z + pLocal->movePos.z;
        sithAIMove_AISetLookPos(pLocal, &lookPos);

        rdVector3 moveDir;
        rdVector_Sub3(&moveDir, &pLocal->movePos, &pOwner->pos);
        SithThing* pMoveOwner = pLocal->pOwner;
        sithAIInstinct_sub_494FF0(pOwner, pMoveOwner->orient.rvec.y * moveDir.y + pMoveOwner->orient.rvec.x * moveDir.x + pMoveOwner->orient.rvec.z * moveDir.z);

        SithQuetzUserBlock* pQuetz = pLocal->pOwner->userblock.pQuetz;
        if ( pQuetz )
        {
            pQuetz->unknown391 = 1;
        }
    }

    pLocal->mode &= ~SITHAI_MODE_UNKNOWN_80;
    pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_400 | SITHAI_SUBMODE_CONTINUOUSMOTION | SITHAI_SUBMODE_UNKNOWN_100;
    return event == SITHAI_EVENT_HIT_WALL || event == SITHAI_EVENT_TOUCHED || event == SITHAI_EVENT_HIT_CLIFF || event == SITHAI_EVENT_HIT_THING;
}

// Plays snake slither sound when changing the turn side
void J3DAPI sithAIInstinct_sub_494FF0(const SithThing* pThing, float a2)
{
    float side;
    if ( fabsf(a2) >= 0.1f )
    {
        side = a2 <= 0.0f ? -1.0f : 1.0f;
    }
    else
    {
        side = 0.0f;
    }

    if ( sithAIMove_g_flt_585464 == side )
    {
        return;
    }

    tSoundHandle hSnd = sithSound_Load(sithWorld_g_pCurrentWorld, side >= 0.0f ? "olv_boss_slither2.wav" : "olv_boss_slither1.wav");
    if ( hSnd )
    {
        sithSoundMixer_PlaySoundThing(hSnd, pThing, 1.0f, 0.01f, 7.0f, (SoundPlayFlag)0x80);
    }

    sithAIMove_g_flt_585464 = side;
}
