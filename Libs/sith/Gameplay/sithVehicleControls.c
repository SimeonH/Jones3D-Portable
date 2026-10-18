#include "sithVehicleControls.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdVector.h>
#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithControl.h>
#include <sith/Engine/sithCamera.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithFX.h>
#include <sith/Gameplay/sithInventory.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerControls.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWorld.h>
#include <std/General/stdMath.h>
#include <std/General/stdUtil.h>

#include <math.h>
#include <stdbool.h>
#include <string.h>

static SithMineCarControlState sithVehicleControls_curMineCarState = { 0.0f, 0, 0.0f, -1, 0, 0, -1, 0u, 0.0f, 0.0f, 0 };
static SithRaftControlState sithVehicleControls_curRaftState = {
      SITHPLAYERMOVE_RAFT_STILL,
      SITHPUPPETSUBMODE_STAND,
      { { 0.0f }, { 0.0f }, { 0.0f } },
      { { 0.0f }, { 0.0f }, { 0.0f } },
      0.0f,
      0.0f,
      0,
      0.0f,
      0.0f
    };
static const float sithVehicleControls_jeepSurfDrag = 2.0f;
static const float sithVehicleControls_jeepStaticDrag = 0.02f;
static const float sithVehicleControls_jeepMaxVelocity = 1.0f;
static const float sithVehicleControls_jeepHeight = 0.18000001f;
static const float sithVehicleControls_jeepMoveSize = 0.16500001f;
static float sithVehicleControls_jeepWheelRadius = 0.037500001f;
static float sithVehicleControls_raftRotThrust = 60.0f;
static int sithVehicleControls_dword_53B188 = -1;
static float sithVehicleControls_flt_586294;
static int sithVehicleControls_bJeepStartMoveSndFxPlayed;
static float sithVehicleControls_raftStaticDrag;
static float sithVehicleControls_raftRotSpeed;

// Defined in sithPlayerControls.c
extern int sithPlayerControls_bActionActivated;

void sithVehicleControls_InstallHooks(void)
{
    J3D_HOOKFUNC(sithVehicleControls_GetMineCarControlState);
    J3D_HOOKFUNC(sithVehicleControls_SetMineCarControlState);
    J3D_HOOKFUNC(sithVehicleControls_GetRaftControlState);
    J3D_HOOKFUNC(sithVehicleControls_SetRaftControlState);
    J3D_HOOKFUNC(sithVehicleControls_PuppetCallback);
    J3D_HOOKFUNC(sithVehicleControls_ProcessMineCarPlayerMove);
    J3D_HOOKFUNC(sithVehicleControls_ProcessJeepPlayerMove);
    J3D_HOOKFUNC(sithVehicleControls_PlayPuppetMode);
    J3D_HOOKFUNC(sithVehicleControls_ProcessRaftPlayerMove);
    J3D_HOOKFUNC(sithVehicleControls_FindRaftExitSurfacePos);
    J3D_HOOKFUNC(sithVehicleControls_sub_4A5AE0);
    J3D_HOOKFUNC(sithVehicleControls_FadeJeepMoveSounds);
    J3D_HOOKFUNC(sithVehicleControls_CanMoveInDirection);
    J3D_HOOKFUNC(sithVehicleControls_ExitRaft);
    J3D_HOOKFUNC(sithVehicleControls_UpdateRaftPaddleSoundFx);
    J3D_HOOKFUNC(sithVehicleControls_SendMessageUser0);
    J3D_HOOKFUNC(sithVehicleControls_SendMessageUser1);
}

int sithVehicleControls_g_bJeepForceStopped;
int sithVehicleControls_g_bJeepAccMoveSndFxPlayed;

// Raft exit search entry
typedef struct sSithRaftExitSearch
{
    SithSurface* pSurface; // Exit wall surface, null if not found
    float dot;             // Dot product between search direction and wall normal
    rdVector3 dir;         // Search direction
    float distance;        // Distance to wall
} SithRaftExitSearch;
J3D_ASSERT_SIZE32(SithRaftExitSearch, 24);

static inline bool sithVehicleControls_IsZeroVector(const rdVector3* pVec)
{
    return pVec->x == 0.0f && pVec->y == 0.0f && pVec->z == 0.0f;
}

static inline float sithVehicleControls_Len(const rdVector3* pVec)
{
    return sqrtf(pVec->z * pVec->z + pVec->y * pVec->y + pVec->x * pVec->x);
}

static inline float sithVehicleControls_Dot(const rdVector3* pA, const rdVector3* pB)
{
    return pA->z * pB->z + pA->y * pB->y + pA->x * pB->x;
}

SithMineCarControlState* J3DAPI sithVehicleControls_GetMineCarControlState()
{
    return &sithVehicleControls_curMineCarState;
}

void J3DAPI sithVehicleControls_SetMineCarControlState(SithMineCarControlState* pState)
{
    sithVehicleControls_curMineCarState = *pState;
}

SithRaftControlState* J3DAPI sithVehicleControls_GetRaftControlState()
{
    return &sithVehicleControls_curRaftState;
}

void J3DAPI sithVehicleControls_SetRaftControlState(SithRaftControlState* pState)
{
    sithVehicleControls_curRaftState = *pState;
}

static inline bool sithVehicleControls_IsRaftPaddleSwitchMove(SithThingMoveStatus moveStatus)
{
    switch ( moveStatus )
    {
        case SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT:
        case SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT:
        case SITHPLAYERMOVE_RAFT_ENDPADDLE_LEFT:
        case SITHPLAYERMOVE_RAFT_ENDPADDLE_RIGHT:
        case SITHPLAYERMOVE_RAFT_STARTPADDLE_RIGHT:
        case SITHPLAYERMOVE_RAFT_STARTPADDLE_LEFT:
            return true;

        default:
            return false;
    }
}

void J3DAPI sithVehicleControls_PuppetCallback(SithThing* pThing, int track, rdKeyMarkerType markerType)
{
    if ( !pThing->thingInfo.actorInfo.bControlsDisabled )
    {
        return;
    }

    // Raft water ripples
    switch ( markerType )
    {
        case RDKEYMARKER_REACHRIGHTARMREST:
            sithFX_CreateRipple(pThing, 0.02f);
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_PICKUP:
            sithFX_CreateRipple(pThing, 0.039999999f);
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_DROP:
            sithFX_CreateRipple(pThing, 0.059999999f);
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_MOVE:
            sithFX_CreateRipple(pThing, 0.079999998f);
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_INVENTORYPULL:
            sithFX_CreateRipple(pThing, 0.1f);
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_INVENTORYPUT:
            sithFX_CreateRipple(pThing, 0.12f);
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_ATTACKFINISH:
            sithFX_CreateRipple(pThing, 0.14f);
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_ROW:
            if ( (pThing->flags & SITH_TF_DYING) == 0 )
            {
                sithVehicleControls_curRaftState.bRowing        = 1;
                sithVehicleControls_curRaftState.secRowStartTime = sithTime_g_secGameTime;
                sithFX_CreateRowWaterFx(pThing, sithTime_g_secGameTime);
                sithVehicleControls_UpdateRaftPaddleSoundFx(pThing);
            }
            else if ( (int)pThing->userval == 3 )
            {
                sithFX_CreateRaftRipple(pThing, 0);
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case RDKEYMARKER_ROWFINISH:
            sithVehicleControls_curRaftState.bRowing = 0;
            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        default:
            break;
    }

    SithThingMoveStatus moveStatus = pThing->moveStatus;
    if ( markerType == RDKEYMARKER_CLIMB )
    {
        // Raft paddle animation loop point
        switch ( moveStatus )
        {
            case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT:
            case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT:
            case SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT:
            case SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT:
            case SITHPLAYERMOVE_RAFT_TURN_LEFT:
            case SITHPLAYERMOVE_RAFT_TURN_RIGHT:
                if ( moveStatus == sithVehicleControls_curRaftState.moveStatus )
                {
                    sithPuppet_DefaultCallback(pThing, track, markerType);
                    return;
                }

                pThing->moveStatus = sithVehicleControls_curRaftState.moveStatus;
                break;

            case SITHPLAYERMOVE_UNKNOWN_45:
                break;

            default:
                if ( sithVehicleControls_IsRaftPaddleSwitchMove(moveStatus) )
                {
                    pThing->moveStatus = sithVehicleControls_curRaftState.moveStatus;
                    break;
                }

                sithPuppet_StopKey(pThing->renderData.pPuppet, track, 0.0f);
                sithPuppet_DefaultCallback(pThing, track, markerType);
                return;
        }

        sithVehicleControls_PlayPuppetMode(pThing, sithVehicleControls_curRaftState.puppetSubmode);
        sithPuppet_StopKey(pThing->renderData.pPuppet, track, 0.0f);
        sithPuppet_DefaultCallback(pThing, track, markerType);
        return;
    }

    if ( moveStatus == SITHPLAYERMOVE_UNKNOWN_45 && markerType == 0 )
    {
        if ( sithVehicleControls_curRaftState.moveStatus == SITHPLAYERMOVE_RAFT_UNBOARD_START )
        {
            pThing->moveStatus = SITHPLAYERMOVE_RAFT_UNBOARD_START;
        }
        else
        {
            sithVehicleControls_curRaftState.moveStatus = SITHPLAYERMOVE_RAFT_UNBOARD_START;
        }

        sithPuppet_DefaultCallback(pThing, track, 0);
        return;
    }

    if ( sithVehicleControls_IsRaftPaddleSwitchMove(moveStatus) )
    {
        if ( markerType == 0 && moveStatus != sithVehicleControls_curRaftState.moveStatus )
        {
            pThing->moveStatus = sithVehicleControls_curRaftState.moveStatus;
            sithVehicleControls_PlayPuppetMode(pThing, sithVehicleControls_curRaftState.puppetSubmode);
        }

        sithPuppet_DefaultCallback(pThing, track, markerType);
        return;
    }

    switch ( moveStatus )
    {
        case SITHPLAYERMOVE_LAND:
            if ( markerType == 0 )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                sithPuppet_FreeTrackByIndex(pThing, track);
                if ( (pThing->moveInfo.physics.flags & SITH_PF_RAFT) != 0 )
                {
                    pThing->moveStatus = SITHPLAYERMOVE_RAFT_STILL;
                    sithVehicleControls_PlayPuppetMode(pThing, SITHPUPPETSUBMODE_STAND);
                }
                else
                {
                    pThing->moveStatus = SITHPLAYERMOVE_STILL;
                }

                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_RAFT_BOARDING:
            if ( markerType == 0 )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                sithCog_ThingSendMessageEx(pThing, NULL, SITHCOG_MSG_CALLBACK, 0, 22, 0, 0);
                sithCog_ThingSendMessageEx(pThing, NULL, SITHCOG_MSG_CALLBACK, 0, 30, 0, 0);
                pThing->moveStatus = SITHPLAYERMOVE_RAFT_STILL;
                sithVehicleControls_SendMessageUser1(pThing);
                sithPuppet_DefaultCallback(pThing, track, 0);
                return;
            }

            if ( markerType == RDKEYMARKER_DUCK )
            {
                // Push raft away from the shore
                const float pushSpeed = -0.039999999f;
                pThing->moveInfo.physics.velocity.x = pushSpeed * pThing->orient.rvec.x;
                pThing->moveInfo.physics.velocity.y = pushSpeed * pThing->orient.rvec.y;
                pThing->moveInfo.physics.velocity.z = pushSpeed * pThing->orient.rvec.z;
                pThing->moveInfo.physics.angularVelocity.y = 67.0f;
                if ( (int)pThing->userval == 3 )
                {
                    sithFX_CreateRaftRipple(pThing, 0);
                    sithPuppet_DefaultCallback(pThing, track, markerType);
                    return;
                }
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_RAFT_UNBOARDING_LEFT:
        case SITHPLAYERMOVE_RAFT_UNBOARDING_RIGHT:
            if ( markerType == 0 )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                sithVehicleControls_ExitRaft(pThing);
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_MINECAR_BOARDING:
            sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)3);
            if ( markerType == 0 )
            {
                sithVehicleControls_SendMessageUser1(pThing);
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                pThing->thingInfo.actorInfo.flags |= SITH_AF_HEADLIGHT;
                pThing->moveStatus = SITHPLAYERMOVE_MINECAR_IDLE;
                pThing->thingInfo.actorInfo.headLightIntensity.w = 2.0f;
                sithPuppet_DefaultCallback(pThing, track, 0);
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_JEEP_BOARDING:
            sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)3);
            if ( markerType == 0 )
            {
                sithVehicleControls_SendMessageUser1(pThing);
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                pThing->moveStatus = SITHPLAYERMOVE_JEEP_STILL;
                sithPuppet_DefaultCallback(pThing, track, 0);
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_JEEP_UNBOARDING:
            if ( markerType == 0 )
            {
                sithPlayerControls_ExitVehicle(pThing);
                sithPuppet_DefaultCallback(pThing, track, 0);
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        default:
            break;
    }

    if ( (pThing->moveInfo.physics.flags & SITH_PF_JEEP) != 0 && markerType == RDKEYMARKER_DIED )
    {
        sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)0x62);
    }

    switch ( pThing->moveStatus )
    {
        case SITHPLAYERMOVE_MINECAR_UNBOARDING_LEFT:
        case SITHPLAYERMOVE_MINECAR_UNBOARDING_RIGHT:
            if ( markerType == 0 )
            {
                sithPlayerControls_ExitVehicle(pThing);
                sithPuppet_DefaultCallback(pThing, track, 0);
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_MINECAR_DUCKING:
            if ( markerType == RDKEYMARKER_DUCK )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                pThing->moveStatus = SITHPLAYERMOVE_MINECAR_DUCKED;
                sithVehicleControls_curMineCarState.secDuckTime = sithTime_g_secGameTime;
                sithPuppet_DefaultCallback(pThing, track, markerType);
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_MINECAR_DUCKED:
            if ( markerType == 0 )
            {
                pThing->moveStatus = SITHPLAYERMOVE_MINECAR_GETTING_UP;
                sithPuppet_DefaultCallback(pThing, track, 0);
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_MINECAR_GETTING_UP:
            if ( markerType == 0 )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                pThing->moveStatus = SITHPLAYERMOVE_MINECAR_IDLE;
                pThing->moveInfo.physics.flags &= ~SITH_PF_CROUCHING;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        default:
            return;
    }
}

void J3DAPI sithVehicleControls_ProcessMineCarPlayerMove(SithThing* pThing, float secDeltaTime)
{
    SithMineCarControlState* pState = &sithVehicleControls_curMineCarState;
    SithPhysicsInfo* pPhysics       = &pThing->moveInfo.physics;

    if ( !sithPhysics_CreateMineCarUserBlock(pThing) )
    {
        return;
    }

    SithMineCarState* pCarState = &pThing->userblock.pMinecar->state;
    SithThingMoveStatus moveStatus = pThing->moveStatus;
    if ( moveStatus == SITHPLAYERMOVE_MINECAR_BOARDING || moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_RIGHT || moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_LEFT )
    {
        pState->accelerationState = 0;
        pState->indyLeanState     = 0;
        pState->unbaordState      = 0;
        return;
    }

    // Acceleration states: 0 - none, 1 - stopped, 2 - braking, 3 - accelerating, 4 - moving
    if ( !sithVehicleControls_IsZeroVector(&pPhysics->velocity) && pState->accelerationState == 0 )
    {
        pState->accelerationState = 4;
    }

    if ( !sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, NULL) && pThing->moveStatus == SITHPLAYERMOVE_MINECAR_IDLE )
    {
        pState->bNotDucked = 1;
    }

    // Note, mine car parameters are stored in physics and actor fields
    float maxFwdSpeed  = pPhysics->maxVelocity * pPhysics->surfDrag;
    pPhysics->thrust.x = 0.0f;
    pPhysics->thrust.z = 0.0f;
    float accelRate    = pThing->thingInfo.actorInfo.maxThrust;
    float brakeRate    = pThing->thingInfo.actorInfo.maxRotVelocity;
    float secDuckTime  = pPhysics->airDrag;
    float maxBackSpeed = -(pPhysics->surfDrag * pPhysics->orientSpeed);
    float dragFactor   = pPhysics->surfDrag - -1.0f;
    float accelHigh    = pThing->thingInfo.actorInfo.jumpSpeed * dragFactor;
    float accelLow     = -(pPhysics->orientSpeed * dragFactor);

    pPhysics->velocity.x = fabsf(pPhysics->velocity.x) > 0.0000099999997f ? pPhysics->velocity.x : 0.0f;
    pPhysics->velocity.y = fabsf(pPhysics->velocity.y) > 0.0000099999997f ? pPhysics->velocity.y : 0.0f;
    pPhysics->velocity.z = fabsf(pPhysics->velocity.z) > 0.0000099999997f ? pPhysics->velocity.z : 0.0f;
    if ( sithVehicleControls_IsZeroVector(&pPhysics->velocity) && pState->accelerationState != 1 )
    {
        pState->accelerationState = 0;
    }

    // Indy leans in the turns
    moveStatus = pThing->moveStatus;
    bool bDucking = moveStatus == SITHPLAYERMOVE_MINECAR_GETTING_UP || moveStatus == SITHPLAYERMOVE_MINECAR_DUCKING || moveStatus == SITHPLAYERMOVE_MINECAR_DUCKED;
    if ( pState->indyLeanState == 0 && !bDucking )
    {
        if ( sithVehicleControls_Len(&pPhysics->velocity) > 0.69999999f )
        {
            if ( pPhysics->angularVelocity.y > 25.0f )
            {
                pState->curPuppetTrack = sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_STAND2WALK, NULL);
                pState->indyLeanState  = 1;
            }
            else if ( -25.0f > pPhysics->angularVelocity.y )
            {
                pState->curPuppetTrack = sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_WALK2STAND, NULL);
                pState->indyLeanState  = 2;
            }
        }
    }
    else if ( pState->indyLeanState == 1 || pState->indyLeanState == 2 )
    {
        bool bStillTurning = pState->indyLeanState == 1 ? pPhysics->angularVelocity.y >= 20.0f : -20.0f >= pPhysics->angularVelocity.y;
        if ( !bStillTurning || sithVehicleControls_Len(&pPhysics->velocity) < 0.60000002f || pThing->moveStatus == SITHPLAYERMOVE_MINECAR_DUCKING )
        {
            sithPuppet_StopKey(pThing->renderData.pPuppet, pState->curPuppetTrack, 0.15000001f);
            pState->indyLeanState = 0;
        }
    }

    if ( pState->accelerationState == 1 )
    {
        pState->secStoppingElapsedTime = secDeltaTime + pState->secStoppingElapsedTime;
        if ( pState->secStoppingElapsedTime > 0.40000001f )
        {
            pState->accelerationState = 0;
        }

        return;
    }

    if ( pThing->moveStatus == SITHPLAYERMOVE_MINECAR_DUCKED && sithTime_g_secGameTime - pState->secDuckTime > secDuckTime )
    {
        // Get up
        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_SLIDEDOWNBACK, sithVehicleControls_PuppetCallback);
        if ( pState->curDuckPuppetTrackNum != -1 )
        {
            sithPuppet_StopKey(pThing->renderData.pPuppet, pState->curDuckPuppetTrackNum, 0.1f);
        }

        pThing->thingInfo.actorInfo.bControlsDisabled = 1;
        return;
    }

    // Unboard states: 0 - act key held, 1 - ready, 3 - waiting for side, 4 - can't exit
    if ( pState->unbaordState == 0 )
    {
        if ( !sithControl_GetKey(SITHCONTROL_ACT2, NULL) )
        {
            pState->unbaordState = 1;
        }
    }
    else if ( pState->unbaordState == 1 )
    {
        moveStatus = pThing->moveStatus;
        if ( sithControl_GetKey(SITHCONTROL_ACT2, NULL)
            && (pState->accelerationState == 0 || pState->accelerationState == 1)
            && moveStatus != SITHPLAYERMOVE_MINECAR_DUCKING
            && moveStatus != SITHPLAYERMOVE_MINECAR_GETTING_UP
            && moveStatus != SITHPLAYERMOVE_MINECAR_DUCKED )
        {
            pState->secUnboardingElapsedTime = 0.0f;
            pState->unbaordState = 3;
            sithPlayerControls_bActionActivated = 1;
        }
    }
    else if ( pState->unbaordState >= 3 )
    {
        pState->secUnboardingElapsedTime = secDeltaTime + pState->secUnboardingElapsedTime;
        if ( pState->secUnboardingElapsedTime < 0.2f
            && !sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL)
            && !sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL)
            && sithControl_GetKey(SITHCONTROL_ACT2, NULL) )
        {
            return;
        }

        // Exit to the left or right side
        rdVector3 dir;
        rdVector_Copy3(&dir, &pThing->orient.rvec);
        int bCanExitRight = sithVehicleControls_CanMoveInDirection(pThing, &dir);
        rdVector_Neg3(&dir, &pThing->orient.rvec);
        int bCanExitLeft = sithVehicleControls_CanMoveInDirection(pThing, &dir);

        bool bExit  = false;
        bool bLeft  = false;
        bool bVoice = false;
        if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
        {
            if ( bCanExitLeft )
            {
                bExit = true;
                bLeft = true;
            }
            else
            {
                pState->unbaordState = 4;
                bVoice = true;
            }
        }
        else if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
        {
            if ( bCanExitRight )
            {
                bExit = true;
            }
            else
            {
                pState->unbaordState = 4;
                bVoice = true;
            }
        }
        else if ( pState->unbaordState == 3 )
        {
            if ( bCanExitLeft )
            {
                bExit = true;
                bLeft = true;
            }
            else if ( bCanExitRight )
            {
                bExit = true;
            }
            else
            {
                pState->unbaordState = 0;
                bVoice = true;
            }
        }

        if ( bVoice )
        {
            sithSoundClass_PlayPlayerVoiceModeRandom(pThing, (SithSoundClassMode)0x1C);
        }

        if ( bExit )
        {
            if ( bLeft )
            {
                pThing->moveStatus = SITHPLAYERMOVE_MINECAR_UNBOARDING_LEFT;
                sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_STRAFELEFT, sithVehicleControls_PuppetCallback);
            }
            else
            {
                pThing->moveStatus = SITHPLAYERMOVE_MINECAR_UNBOARDING_RIGHT;
                sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_STRAFERIGHT, sithVehicleControls_PuppetCallback);
            }

            sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)4);
            pThing->thingInfo.actorInfo.bControlsDisabled = 1;
            pState->unbaordState = 0;
            sithVehicleControls_SendMessageUser0(pThing);
            return;
        }

        if ( !sithControl_GetKey(SITHCONTROL_ACT2, NULL) )
        {
            pState->unbaordState = 1;
        }

        return;
    }

    if ( pState->unbaordState >= 3 )
    {
        return;
    }

    // Accelerate, brake or duck
    bool bApply   = false;
    bool bThrust  = false;
    float delta   = 0.0f;
    if ( sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, NULL) && pState->bNotDucked )
    {
        if ( pThing->moveStatus == SITHPLAYERMOVE_MINECAR_IDLE )
        {
            pState->curDuckPuppetTrackNum = sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_SLIDEDOWNFWD, sithVehicleControls_PuppetCallback);
            pThing->moveStatus = SITHPLAYERMOVE_MINECAR_DUCKING;
            pPhysics->flags |= SITH_PF_CROUCHING;
            pThing->thingInfo.actorInfo.bControlsDisabled = 1;
            pState->bNotDucked = 0;
        }
    }
    else if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
    {
        moveStatus = pThing->moveStatus;
        if ( pPhysics->thrust.y < maxFwdSpeed
            && moveStatus != SITHPLAYERMOVE_MINECAR_DUCKING
            && moveStatus != SITHPLAYERMOVE_MINECAR_DUCKED
            && moveStatus != SITHPLAYERMOVE_MINECAR_GETTING_UP
            && pState->indyLeanState == 0 )
        {
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_ACTIVATEHIGH, NULL);
        }

        bApply = true;
        if ( pPhysics->thrust.y < 0.0f )
        {
            delta = brakeRate;
        }
        else
        {
            delta   = accelRate;
            bThrust = true;
        }
    }
    else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
    {
        moveStatus = pThing->moveStatus;
        if ( pPhysics->thrust.y > maxBackSpeed
            && moveStatus != SITHPLAYERMOVE_MINECAR_DUCKING
            && moveStatus != SITHPLAYERMOVE_MINECAR_DUCKED
            && moveStatus != SITHPLAYERMOVE_MINECAR_GETTING_UP
            && pState->indyLeanState == 0 )
        {
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_ACTIVATE, NULL);
        }

        bApply = true;
        if ( pPhysics->thrust.y > 0.0f )
        {
            delta = -brakeRate;
        }
        else
        {
            delta   = -accelRate;
            bThrust = true;
        }
    }
    else if ( pState->accelerationState == 3 )
    {
        // Keep accelerating to the min speed
        bApply  = true;
        bThrust = true;
        delta   = pPhysics->thrust.y < 0.0f ? -accelRate : accelRate;
    }
    else if ( pState->accelerationState == 2 )
    {
        // Keep braking until stopped
        bApply = true;
        delta  = brakeRate * 0.5f;
        if ( !(pPhysics->thrust.y < 0.0f) )
        {
            delta = -delta;
        }
    }

    if ( bApply )
    {
        delta = delta * secDeltaTime;
        if ( bThrust )
        {
            pState->secBreakingElapsedTime = 0.0f;
            float speed = pPhysics->thrust.y;
            if ( speed < accelHigh && speed > accelLow )
            {
                pState->accelerationState = 3;
            }

            if ( speed == 0.0f )
            {
                pPhysics->thrust.y = delta;
            }
            else
            {
                speed = speed + delta;
                if ( speed > maxFwdSpeed )
                {
                    speed = maxFwdSpeed;
                }
                else if ( speed < maxBackSpeed )
                {
                    speed = maxBackSpeed;
                }

                pPhysics->thrust.y = speed;
            }

            // Note, original checks the old speed when the car was standing still
            if ( pState->accelerationState == 3 && (speed >= accelHigh || speed <= accelLow) )
            {
                pState->accelerationState = 4;
            }
        }
        else
        {
            // Braking
            pState->secBreakingElapsedTime = secDeltaTime + pState->secBreakingElapsedTime;
            if ( pState->secBreakingElapsedTime > 0.2f && !pState->hBreakSnd )
            {
                pState->hBreakSnd = sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)0xD);
                pCarState->bWheelsLocked = 1;
            }

            float speed = pPhysics->thrust.y;
            if ( fabsf(speed) < fabsf(delta) )
            {
                // Stopped
                rdVector_Set3(&pPhysics->thrust, 0.0f, 0.0f, 0.0f);
                rdVector_Set3(&pPhysics->velocity, 0.0f, 0.0f, 0.0f);
                pState->accelerationState      = 1;
                pState->secStoppingElapsedTime = 0.0f;
                pState->secBreakingElapsedTime = 0.0f;
            }
            else
            {
                speed = speed + delta;
                pPhysics->thrust.y = speed;
                if ( speed < accelHigh && speed > accelLow )
                {
                    pState->accelerationState = 2;
                }
            }
        }
    }
    else
    {
        pState->secBreakingElapsedTime = 0.0f;
    }

    if ( pState->secBreakingElapsedTime == 0.0f && pState->hBreakSnd )
    {
        sithSoundClass_FadeModeVolume(pThing, (SithSoundClassMode)0xD, 0.0f, 0.60000002f);
        pState->hBreakSnd = 0;
        if ( pCarState->bWheelsLocked )
        {
            pCarState->bWheelsLocked      = 0;
            pCarState->bUpdateLeftSparks  = 0;
            pCarState->bUpdateRightSparks = 0;
        }
    }
}

static void sithVehicleControls_RotateJeepWheel(SithThing* pThing, const char* pJointName, float angle)
{
    int jointNum = sithThing_GetThingJointIndex(pThing, pJointName);
    if ( jointNum == -1 )
    {
        SITHLOG_WARNING("Jeep wheel node missing.\n");
        return;
    }

    pThing->renderData.apTweakedAngles[jointNum].x = pThing->renderData.apTweakedAngles[jointNum].x - angle;
}

void J3DAPI sithVehicleControls_ProcessJeepPlayerMove(SithThing* pThing, float secDeltaTime)
{
    SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;
    switch ( pThing->moveStatus )
    {
        case SITHPLAYERMOVE_JEEP_BOARDING:
        case SITHPLAYERMOVE_JEEP_UNBOARDING:
        case SITHPLAYERMOVE_UNKNOWN_83:
        case SITHPLAYERMOVE_UNKNOWN_84:
        case SITHPLAYERMOVE_JEEP_IMPACT:
            return;

        default:
            break;
    }

    pPhysics->height              = sithVehicleControls_jeepHeight;
    pThing->collide.movesize      = sithVehicleControls_jeepMoveSize;
    pPhysics->surfDrag            = sithVehicleControls_jeepSurfDrag;
    pPhysics->staticDrag          = sithVehicleControls_jeepStaticDrag;
    pPhysics->maxVelocity         = sithVehicleControls_jeepMaxVelocity;
    pPhysics->maxRotationVelocity = 150.0f;
    pPhysics->flags |= SITH_PF_USEANGULARTHRUST;

    if ( sithVehicleControls_g_bJeepForceStopped )
    {
        pPhysics->surfDrag   = 2.0f;
        pPhysics->staticDrag = 0.050000001f;
        if ( sithVehicleControls_IsZeroVector(&pPhysics->velocity) )
        {
            sithVehicleControls_flt_586294 = sithVehicleControls_flt_586294 + secDeltaTime;
            if ( sithVehicleControls_flt_586294 > 0.1f )
            {
                sithVehicleControls_g_bJeepForceStopped = 0;
            }
        }
        else
        {
            sithVehicleControls_flt_586294 = 0.0f;
        }
    }
    else
    {
        sithVehicleControls_flt_586294 = 0.0f;
    }

    // Spin wheels
    float wheelCircumference = sithVehicleControls_jeepWheelRadius * 3.1415901f;
    wheelCircumference = wheelCircumference + wheelCircumference;
    if ( !sithVehicleControls_IsZeroVector(&pPhysics->velocity) && !sithVehicleControls_g_bJeepForceStopped )
    {
        float speed = sithVehicleControls_Len(&pPhysics->velocity);
        float angle = stdMath_NormalizeAngle(speed / wheelCircumference * 360.0f * secDeltaTime);
        if ( sithVehicleControls_Dot(&pThing->orient.lvec, &pPhysics->velocity) < 0.0f )
        {
            angle = angle * -1.0f;
        }

        sithVehicleControls_RotateJeepWheel(pThing, "brwheel", angle);
        sithVehicleControls_RotateJeepWheel(pThing, "blwheel", angle);
        sithVehicleControls_RotateJeepWheel(pThing, "frwheel", angle);
        sithVehicleControls_RotateJeepWheel(pThing, "flwheel", angle);
    }

    float speedFactor;
    float accelRate;
    if ( !sithControl_GetKey(SITHCONTROL_ACT1, NULL) )
    {
        speedFactor = pPhysics->maxVelocity;
        accelRate   = 0.34999999f;
        sithVehicleControls_g_bJeepAccMoveSndFxPlayed = 0;
    }
    else
    {
        // Turbo
        accelRate   = 0.5f;
        speedFactor = 1.98f;
        if ( !sithVehicleControls_g_bJeepAccMoveSndFxPlayed )
        {
            sithSoundClass_PlayModeFirstEx(pThing, (SithSoundClassMode)0x1B, 0.5f);
            sithVehicleControls_g_bJeepAccMoveSndFxPlayed = 1;
        }
    }

    float maxFwdSpeed  = pPhysics->surfDrag * speedFactor;
    float maxRotVel    = pPhysics->maxRotationVelocity;
    float brakeFwd     = -2.0f;
    float brakeBack    = 2.0f;
    float maxBackSpeed = -(pPhysics->surfDrag * 0.34999999f);

    float steerScale = sithVehicleControls_Len(&pPhysics->velocity) / 0.40000001f;
    if ( steerScale < 0.0f )
    {
        steerScale = 0.0f;
    }
    else if ( steerScale > 1.0f )
    {
        steerScale = 1.0f;
    }

    float maxSteer = steerScale * maxRotVel * pPhysics->surfDrag;

    // Exit jeep
    if ( sithControl_GetKey(SITHCONTROL_ACT2, NULL) && !sithPlayerControls_bActionActivated )
    {
        rdVector3 exitDir;
        rdVector_Neg3(&exitDir, &pThing->orient.rvec);

        if ( sithVehicleControls_Len(&pPhysics->velocity) <= 0.2f )
        {
            rdVector_Set3(&pPhysics->velocity, 0.0f, 0.0f, 0.0f);
        }

        if ( fabsf(pPhysics->angularVelocity.x) <= 3.0f )
        {
            pPhysics->angularVelocity.x = 0.0f;
        }

        if ( fabsf(pPhysics->angularVelocity.y) <= 3.0f )
        {
            pPhysics->angularVelocity.y = 0.0f;
        }

        if ( fabsf(pPhysics->angularVelocity.z) <= 3.0f )
        {
            pPhysics->angularVelocity.z = 0.0f;
        }

        sithPlayerControls_bActionActivated = 1;
        if ( sithVehicleControls_IsZeroVector(&pPhysics->velocity)
            && sithVehicleControls_IsZeroVector(&pPhysics->angularVelocity)
            && sithVehicleControls_CanMoveInDirection(pThing, &exitDir) )
        {
            sithVehicleControls_FadeJeepMoveSounds(pThing);
            sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)4);
            pThing->thingInfo.actorInfo.bControlsDisabled = 1;
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_PULLITEM, sithVehicleControls_PuppetCallback);
            pThing->moveStatus = SITHPLAYERMOVE_JEEP_UNBOARDING;
            sithVehicleControls_SendMessageUser0(pThing);
            return;
        }

        sithSoundClass_PlayPlayerVoiceModeRandom(pThing, (SithSoundClassMode)0x1C);
    }

    float fwdSpeed = sithVehicleControls_IsZeroVector(&pPhysics->velocity) ? 0.0f : sithVehicleControls_Dot(&pThing->orient.lvec, &pPhysics->velocity);

    // Throttle
    if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
    {
        if ( fwdSpeed < 0.0f || sithVehicleControls_g_bJeepForceStopped )
        {
            // Changing direction, stop first
            sithVehicleControls_g_bJeepForceStopped = 1;
            pPhysics->thrust.y = 0.0f;
        }
        else
        {
            if ( !sithVehicleControls_bJeepStartMoveSndFxPlayed )
            {
                sithSoundClass_PlayModeFirstEx(pThing, (SithSoundClassMode)0x1B, 0.5f);
            }

            sithVehicleControls_bJeepStartMoveSndFxPlayed = 1;
            float thrust = maxFwdSpeed * secDeltaTime * accelRate + pPhysics->thrust.y;
            pPhysics->thrust.y = thrust;
            if ( maxFwdSpeed < thrust )
            {
                pPhysics->thrust.y = maxFwdSpeed;
            }
        }
    }
    else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
    {
        if ( fwdSpeed > 0.0f || sithVehicleControls_g_bJeepForceStopped )
        {
            sithVehicleControls_g_bJeepForceStopped = 1;
            pPhysics->thrust.y = 0.0f;
        }
        else
        {
            float thrust = maxBackSpeed * secDeltaTime * accelRate + pPhysics->thrust.y;
            pPhysics->thrust.y = thrust;
            if ( thrust < maxBackSpeed )
            {
                pPhysics->thrust.y = maxBackSpeed;
            }
        }
    }
    else
    {
        // Decelerate
        if ( fwdSpeed > 0.0f )
        {
            float delta = brakeFwd * 0.5f * secDeltaTime;
            if ( pPhysics->thrust.y <= -delta )
            {
                pPhysics->thrust.y = 0.0f;
            }
            else
            {
                pPhysics->thrust.y = delta + pPhysics->thrust.y;
            }
        }
        else if ( fwdSpeed < 0.0f )
        {
            float delta = brakeBack * 0.5f * secDeltaTime;
            if ( -delta <= pPhysics->thrust.y )
            {
                pPhysics->thrust.y = 0.0f;
            }
            else
            {
                pPhysics->thrust.y = delta + pPhysics->thrust.y;
            }
        }

        sithVehicleControls_g_bJeepForceStopped       = 0;
        sithVehicleControls_bJeepStartMoveSndFxPlayed = 0;
    }

    // Steering
    if ( pThing->moveStatus != SITHPLAYERMOVE_UNKNOWN_82 && !sithVehicleControls_IsZeroVector(&pPhysics->velocity) )
    {
        if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
        {
            float delta = maxSteer * secDeltaTime * 3.0f;
            if ( sithVehicleControls_Dot(&pThing->orient.lvec, &pPhysics->velocity) < 0.0f )
            {
                pPhysics->rotThrust.y = delta + pPhysics->rotThrust.y;
            }
            else
            {
                pPhysics->rotThrust.y = pPhysics->rotThrust.y - delta;
            }
        }
        else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
        {
            float delta = maxSteer * secDeltaTime * 3.0f;
            if ( sithVehicleControls_Dot(&pThing->orient.lvec, &pPhysics->velocity) < 0.0f )
            {
                pPhysics->rotThrust.y = pPhysics->rotThrust.y - delta;
            }
            else
            {
                pPhysics->rotThrust.y = delta + pPhysics->rotThrust.y;
            }
        }
        else
        {
            // Center steering
            float center = 600.0f * secDeltaTime;
            pPhysics->rotThrust.y = 0.0f;
            if ( fabsf(center) < fabsf(pPhysics->angularVelocity.y) )
            {
                float sign = pPhysics->angularVelocity.y < 0.0f ? -1.0f : 1.0f;
                pPhysics->angularVelocity.y = pPhysics->angularVelocity.y - sign * center;
            }
            else
            {
                pPhysics->angularVelocity.y = 0.0f;
            }
        }
    }

    if ( pPhysics->rotThrust.y < -maxSteer )
    {
        pPhysics->rotThrust.y = -maxSteer;
    }
    else if ( pPhysics->rotThrust.y > maxSteer )
    {
        pPhysics->rotThrust.y = maxSteer;
    }

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 && !sithVehicleControls_IsZeroVector(&pPhysics->velocity) )
    {
        float axisRot = sithControl_GetAxis(SITHCONTROL_MOUSETURN) * sithTime_g_fps;
        pPhysics->angularVelocity.y = sithControl_GetKeyAsAxis(SITHCONTROL_MOUSETURN) * pThing->thingInfo.actorInfo.maxRotVelocity + axisRot + pPhysics->angularVelocity.y;
    }
}

void J3DAPI sithVehicleControls_PlayPuppetMode(SithThing* pThing, SithPuppetSubMode submode)
{
    sithPuppet_PlayMode(pThing, submode, sithVehicleControls_PuppetCallback);
    pThing->thingInfo.actorInfo.bControlsDisabled = submode != SITHPUPPETSUBMODE_STAND;
    sithVehicleControls_curRaftState.bRowing = 0;
}

// Returns next raft paddle state for paddling on the left side
static SithThingMoveStatus sithVehicleControls_GetRaftLeftPaddleState(bool bFwd, bool bBack, bool bLeft, bool bRight)
{
    if ( bLeft && !bFwd && !bBack )
    {
        return SITHPLAYERMOVE_RAFT_TURN_LEFT;
    }

    if ( bRight && !bFwd && !bBack )
    {
        return SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT;
    }

    if ( bFwd && bLeft )
    {
        return SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT;
    }

    if ( bFwd && bRight )
    {
        return SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT;
    }

    if ( bBack && bLeft )
    {
        return SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT;
    }

    if ( bBack && bRight )
    {
        return SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT;
    }

    return SITHPLAYERMOVE_RAFT_ENDPADDLE_LEFT;
}

// Returns next raft paddle state for paddling on the right side
static SithThingMoveStatus sithVehicleControls_GetRaftRightPaddleState(bool bFwd, bool bBack, bool bLeft, bool bRight)
{
    if ( bLeft && !bFwd && !bBack )
    {
        return SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT;
    }

    if ( bRight && !bFwd && !bBack )
    {
        return SITHPLAYERMOVE_RAFT_TURN_RIGHT;
    }

    if ( bFwd && bLeft )
    {
        return SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT;
    }

    if ( bFwd && bRight )
    {
        return SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT;
    }

    if ( bBack && bLeft )
    {
        return SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT;
    }

    if ( bBack && bRight )
    {
        return SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT;
    }

    return SITHPLAYERMOVE_RAFT_ENDPADDLE_RIGHT;
}

void J3DAPI sithVehicleControls_ProcessRaftPlayerMove(SithThing* pThing, float secDeltaTime)
{
    SithRaftControlState* pState = &sithVehicleControls_curRaftState;
    SithPhysicsInfo* pPhysics    = &pThing->moveInfo.physics;

    pPhysics->height              = 0.029999999f;
    pPhysics->surfDrag            = 0.40000001f;
    pPhysics->staticDrag          = sithVehicleControls_raftStaticDrag;
    pThing->thingInfo.actorInfo.maxThrust = 0.2f;
    pPhysics->orientSpeed         = sithVehicleControls_raftRotSpeed;
    pPhysics->maxRotationVelocity = 135.0f;

    float rotThrust     = sithVehicleControls_raftRotThrust;
    float fwdThrust     = pThing->thingInfo.actorInfo.maxThrust;
    float backRotThrust = sithVehicleControls_raftRotThrust;
    float backThrust    = -(fwdThrust * 0.5f);

    // Raft wake; state velocity field is used as wake distance counter
    bool bWake = false;
    if ( !(pState->velocity > 0.0f) )
    {
        float speed = sithVehicleControls_Len(&pPhysics->velocity);
        if ( speed >= 0.050000001f && (int)pThing->userval == 3 )
        {
            sithFX_CreateRaftWake(pThing);
            pState->velocity = 0.050000001f;
            bWake = true;
        }
    }

    if ( !bWake )
    {
        if ( pState->velocity > 0.0f )
        {
            pState->velocity = pState->velocity - sithVehicleControls_Len(&pPhysics->velocity) * secDeltaTime;
        }
        else if ( pState->velocity < 0.0f )
        {
            pState->velocity = 0.0f;
        }
    }

    if ( pState->bRowing )
    {
        sithFX_CreateRowWaterFx(pThing, pState->secRowStartTime);
    }

    // Exit raft
    switch ( pThing->moveStatus )
    {
        case SITHPLAYERMOVE_FALLING:
        case SITHPLAYERMOVE_LAND:
        case SITHPLAYERMOVE_UNKNOWN_45:
        case SITHPLAYERMOVE_RAFT_BOARDING:
        case SITHPLAYERMOVE_RAFT_UNBOARDING_LEFT:
        case SITHPLAYERMOVE_RAFT_UNBOARDING_RIGHT:
        case SITHPLAYERMOVE_RAFT_UNBOARD_START:
            break;

        default:
            if ( sithControl_GetKey(SITHCONTROL_ACT2, NULL) && !sithPlayerControls_bActionActivated )
            {
                sithPlayerControls_bActionActivated = 1;
                if ( sithVehicleControls_FindRaftExitSurfacePos(pThing, 0.2f, &pState->exitSurfPos, &pState->exitSurfPYR) == 8 )
                {
                    sithSoundClass_PlayPlayerVoiceModeRandom(pThing, (SithSoundClassMode)0x1C);
                }
                else
                {
                    pState->moveSize       = pThing->collide.movesize;
                    pState->secUnboardTime = 0.0f;
                    pThing->collide.movesize = pState->moveSize * 0.5f;
                    sithPhysics_ResetThingMovement(pThing);
                    sithVehicleControls_SendMessageUser0(pThing);

                    switch ( pThing->moveStatus )
                    {
                        case SITHPLAYERMOVE_RAFT_STILL:
                            pThing->moveStatus = SITHPLAYERMOVE_RAFT_UNBOARD_START;
                            break;

                        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT:
                        case SITHPLAYERMOVE_RAFT_TURN_LEFT:
                        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT:
                        case SITHPLAYERMOVE_RAFT_STARTPADDLE_LEFT:
                        case SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT:
                            pState->moveStatus = SITHPLAYERMOVE_RAFT_ENDPADDLE_LEFT;
                            pThing->moveStatus = SITHPLAYERMOVE_UNKNOWN_45;
                            break;

                        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT:
                        case SITHPLAYERMOVE_RAFT_TURN_RIGHT:
                        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT:
                        case SITHPLAYERMOVE_RAFT_STARTPADDLE_RIGHT:
                        case SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT:
                            pState->moveStatus = SITHPLAYERMOVE_RAFT_ENDPADDLE_RIGHT;
                            pThing->moveStatus = SITHPLAYERMOVE_UNKNOWN_45;
                            break;

                        default:
                            pThing->moveStatus = SITHPLAYERMOVE_UNKNOWN_45;
                            pState->moveStatus = SITHPLAYERMOVE_RAFT_UNBOARD_START;
                            break;
                    }
                }
            }
            break;
    }

    bool bFwd   = sithControl_GetKey(SITHCONTROL_FORWARD, NULL) != 0;
    bool bBack  = sithControl_GetKey(SITHCONTROL_BACK, NULL) != 0;
    bool bLeft  = sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) != 0;
    bool bRight = sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) != 0;
    bool bFwdOnly  = bFwd && !bLeft && !bRight;
    bool bBackOnly = bBack && !bLeft && !bRight;

    // Update paddle state
    SithThingMoveStatus moveStatus = pThing->moveStatus;
    switch ( moveStatus )
    {
        case SITHPLAYERMOVE_FALLING:
            sithCog_ThingSendMessageEx(pThing, NULL, SITHCOG_MSG_CALLBACK, 0, 22, 0, 0);
            pState->moveStatus = pThing->moveStatus;
            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            break;

        case SITHPLAYERMOVE_LAND:
            pState->moveStatus = moveStatus;
            break;

        case SITHPLAYERMOVE_RAFT_STILL:
        {
            // Start paddling on left or right side
            bool bLeftOnly  = bLeft && !bFwd && !bBack;
            bool bRightOnly = bRight && !bFwd && !bBack;
            int side = 0; // 1 - left, 2 - right
            if ( bFwdOnly || bBackOnly )
            {
                side = 2;
            }
            else if ( bLeftOnly )
            {
                side = 1;
            }
            else if ( bRightOnly || (bFwd && bLeft) )
            {
                side = 2;
            }
            else if ( (bFwd && bRight) || (bBack && bLeft) )
            {
                side = 1;
            }
            else if ( bBack && bRight )
            {
                side = 2;
            }

            if ( side == 2 )
            {
                pThing->moveStatus = SITHPLAYERMOVE_RAFT_STARTPADDLE_RIGHT;
                sithVehicleControls_PlayPuppetMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLRIGHT);
            }
            else if ( side == 1 )
            {
                pThing->moveStatus = SITHPLAYERMOVE_RAFT_STARTPADDLE_LEFT;
                sithVehicleControls_PlayPuppetMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLLEFT);
            }
        } break;

        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT:
            if ( bFwdOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT;
            }
            else if ( bBackOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT;
            }
            else
            {
                pState->moveStatus = sithVehicleControls_GetRaftLeftPaddleState(bFwd, bBack, bLeft, bRight);
            }
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT:
            if ( bFwdOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT;
            }
            else if ( bBackOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT;
            }
            else
            {
                pState->moveStatus = sithVehicleControls_GetRaftRightPaddleState(bFwd, bBack, bLeft, bRight);
            }
            break;

        case SITHPLAYERMOVE_RAFT_TURN_LEFT:
        case SITHPLAYERMOVE_RAFT_STARTPADDLE_LEFT:
        case SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT:
            if ( bFwdOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT;
            }
            else if ( bBackOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT;
            }
            else
            {
                pState->moveStatus = sithVehicleControls_GetRaftLeftPaddleState(bFwd, bBack, bLeft, bRight);
            }
            break;

        case SITHPLAYERMOVE_RAFT_TURN_RIGHT:
            if ( bFwdOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT;
            }
            else if ( bBackOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT;
            }
            else
            {
                pState->moveStatus = sithVehicleControls_GetRaftRightPaddleState(bFwd, bBack, bLeft, bRight);
            }
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT:
            if ( bFwdOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT;
            }
            else if ( bBackOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT;
            }
            else
            {
                pState->moveStatus = sithVehicleControls_GetRaftLeftPaddleState(bFwd, bBack, bLeft, bRight);
            }
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT:
            if ( bFwdOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT;
            }
            else if ( bBackOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT;
            }
            else
            {
                pState->moveStatus = sithVehicleControls_GetRaftRightPaddleState(bFwd, bBack, bLeft, bRight);
            }
            break;

        case SITHPLAYERMOVE_RAFT_ENDPADDLE_LEFT:
        case SITHPLAYERMOVE_RAFT_ENDPADDLE_RIGHT:
        case SITHPLAYERMOVE_RAFT_BOARDING:
            pState->moveStatus = SITHPLAYERMOVE_RAFT_STILL;
            break;

        case SITHPLAYERMOVE_RAFT_STARTPADDLE_RIGHT:
        case SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT:
            if ( bFwdOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT;
            }
            else if ( bBackOnly )
            {
                pState->moveStatus = SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT;
            }
            else
            {
                pState->moveStatus = sithVehicleControls_GetRaftRightPaddleState(bFwd, bBack, bLeft, bRight);
            }
            break;

        case SITHPLAYERMOVE_UNKNOWN_45:
        case SITHPLAYERMOVE_RAFT_UNBOARD_START:
            pState->secUnboardTime = secDeltaTime + pState->secUnboardTime;
            if ( pState->secUnboardTime < 5.0f )
            {
                if ( pThing->moveStatus == SITHPLAYERMOVE_RAFT_UNBOARD_START )
                {
                    // Turn the raft side to the exit surface and jump out
                    rdMatrix34* pOrient = &pThing->orient;
                    float dot = sithVehicleControls_Dot(&pOrient->rvec, &pState->exitSurfPYR);
                    rdVector3 toExit;
                    rdVector_Sub3(&toExit, &pState->exitSurfPos, &pThing->pos);
                    if ( sithVehicleControls_Len(&toExit) <= 0.029999999f && fabsf(dot) > 0.98000002f )
                    {
                        pThing->collide.movesize = pState->moveSize;
                        sithPhysics_ResetThingMovement(pThing);
                        if ( dot >= 0.0f )
                        {
                            sithVehicleControls_PlayPuppetMode(pThing, SITHPUPPETSUBMODE_HOPLEFT);
                            pThing->moveStatus = SITHPLAYERMOVE_RAFT_UNBOARDING_LEFT;
                            rdVector_Copy3(&pOrient->rvec, &pState->exitSurfPYR);
                        }
                        else
                        {
                            sithVehicleControls_PlayPuppetMode(pThing, SITHPUPPETSUBMODE_HOPRIGHT);
                            pThing->moveStatus = SITHPLAYERMOVE_RAFT_UNBOARDING_RIGHT;
                            rdVector_Neg3(&pOrient->rvec, &pState->exitSurfPYR);
                        }

                        pOrient->lvec.x = pOrient->uvec.y * pOrient->rvec.z - pOrient->uvec.z * pOrient->rvec.y;
                        pOrient->lvec.y = pOrient->uvec.z * pOrient->rvec.x - pOrient->uvec.x * pOrient->rvec.z;
                        pOrient->lvec.z = pOrient->uvec.x * pOrient->rvec.y - pOrient->uvec.y * pOrient->rvec.x;
                        rdVector_Normalize3Acc(&pOrient->lvec);

                        sithCog_ThingSendMessageEx(pThing, NULL, SITHCOG_MSG_CALLBACK, 0, 21, 0, 0);
                        sithCog_ThingSendMessageEx(pThing, NULL, SITHCOG_MSG_CALLBACK, 0, 30, 0, 0);
                    }
                }
            }
            else
            {
                // Timeout
                pThing->moveStatus = SITHPLAYERMOVE_RAFT_STILL;
                pThing->collide.movesize = pState->moveSize;
                sithVehicleControls_SendMessageUser1(pThing);
            }
            break;

        case SITHPLAYERMOVE_RAFT_UNBOARDING_LEFT:
        case SITHPLAYERMOVE_RAFT_UNBOARDING_RIGHT:
            break;

        default:
            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            pState->moveStatus = moveStatus;
            break;
    }

    // Apply paddle thrust
    switch ( pThing->moveStatus )
    {
        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT:
            if ( pState->bRowing )
            {
                if ( !sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
                {
                    if ( !sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
                    {
                        rotThrust = rotThrust * 0.5f;
                    }

                    pPhysics->rotThrust.y = -rotThrust;
                }
                else
                {
                    pPhysics->rotThrust.y = 0.0f;
                }

                pPhysics->thrust.y = fwdThrust;
            }
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT:
            if ( pState->bRowing )
            {
                if ( !sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
                {
                    if ( !sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
                    {
                        rotThrust = rotThrust * 0.5f;
                    }

                    pPhysics->rotThrust.y = rotThrust;
                }
                else
                {
                    pPhysics->rotThrust.y = 0.0f;
                }

                pPhysics->thrust.y = fwdThrust;
            }
            break;

        case SITHPLAYERMOVE_RAFT_TURN_LEFT:
            if ( !pState->bRowing )
            {
                break;
            }

            if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
            {
                pPhysics->thrust.y    = 0.0f;
                pPhysics->rotThrust.y = 1.5f * rotThrust;
                break;
            }

            pPhysics->rotThrust.y = 0.0f;
            pPhysics->thrust.y    = 0.0f;
            break;

        case SITHPLAYERMOVE_RAFT_TURN_RIGHT:
            if ( !pState->bRowing )
            {
                break;
            }

            if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
            {
                pPhysics->thrust.y    = 0.0f;
                pPhysics->rotThrust.y = -(1.5f * rotThrust);
                break;
            }

            pPhysics->rotThrust.y = 0.0f;
            pPhysics->thrust.y    = 0.0f;
            break;

        case SITHPLAYERMOVE_RAFT_STARTPADDLE_LEFT:
        case SITHPLAYERMOVE_RAFT_ENDPADDLE_LEFT:
        case SITHPLAYERMOVE_RAFT_ENDPADDLE_RIGHT:
        case SITHPLAYERMOVE_RAFT_STARTPADDLE_RIGHT:
        case SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT:
        case SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT:
            pPhysics->rotThrust.y = 0.0f;
            pPhysics->thrust.y    = 0.0f;
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT:
            if ( pState->bRowing )
            {
                if ( !sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
                {
                    if ( !sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
                    {
                        backRotThrust = backRotThrust * 0.5f;
                    }

                    pPhysics->rotThrust.y = backRotThrust;
                }
                else
                {
                    pPhysics->rotThrust.y = 0.0f;
                }

                pPhysics->thrust.y = backThrust;
            }
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT:
            if ( pState->bRowing )
            {
                if ( !sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
                {
                    if ( !sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
                    {
                        backRotThrust = backRotThrust * 0.5f;
                    }

                    pPhysics->rotThrust.y = -backRotThrust;
                }
                else
                {
                    pPhysics->rotThrust.y = 0.0f;
                }

                pPhysics->thrust.y = backThrust;
            }
            break;

        case SITHPLAYERMOVE_UNKNOWN_45:
        case SITHPLAYERMOVE_RAFT_UNBOARD_START:
        {
            // Move raft towards exit position and turn its side to the exit wall
            rdVector3 toExit;
            rdVector_Sub3(&toExit, &pState->exitSurfPos, &pThing->pos);
            if ( sithVehicleControls_Len(&toExit) <= 0.029999999f )
            {
                rdVector_Set3(&pPhysics->thrust, 0.0f, 0.0f, 0.0f);
                rdVector_Set3(&pPhysics->velocity, 0.0f, 0.0f, 0.0f);
            }
            else
            {
                rdVector_Normalize3Acc(&toExit);
                pPhysics->thrust.y = sithVehicleControls_Dot(&pThing->orient.lvec, &toExit) * 0.15000001f;
                pPhysics->thrust.x = 0.15000001f * sithVehicleControls_Dot(&pThing->orient.rvec, &toExit);
            }

            float rotVel = 120.0f * sithVehicleControls_Dot(&pThing->orient.lvec, &pState->exitSurfPYR);
            if ( sithVehicleControls_Dot(&pThing->orient.rvec, &pState->exitSurfPYR) < 0.0f )
            {
                rotVel = -rotVel;
            }

            pPhysics->angularVelocity.y = rotVel;
            if ( pPhysics->angularVelocity.y < 5.0f && -5.0f < pPhysics->angularVelocity.y )
            {
                pPhysics->angularVelocity.y = (pPhysics->angularVelocity.y < 0.0f ? -1.0f : 1.0f) * 5.0f;
            }
        } break;

        case SITHPLAYERMOVE_RAFT_UNBOARDING_LEFT:
        case SITHPLAYERMOVE_RAFT_UNBOARDING_RIGHT:
            break;

        default:
            rdVector_Set3(&pPhysics->rotThrust, 0.0f, 0.0f, 0.0f);
            rdVector_Set3(&pPhysics->thrust, 0.0f, 0.0f, 0.0f);
            break;
    }

    // Set puppet mode for next paddle state
    switch ( pState->moveStatus )
    {
        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_STRAFELEFT;
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_STRAFERIGHT;
            break;

        case SITHPLAYERMOVE_RAFT_TURN_LEFT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_TURNLEFT;
            break;

        case SITHPLAYERMOVE_RAFT_TURN_RIGHT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_TURNRIGHT;
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_LEFT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_HANGSHIMLEFT;
            break;

        case SITHPLAYERMOVE_RAFT_PADDLE_BACK_RIGHT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_HANGSHIMRIGHT;
            break;

        case SITHPLAYERMOVE_RAFT_STARTPADDLE_LEFT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_CLIMBWALLLEFT;
            break;

        case SITHPLAYERMOVE_RAFT_ENDPADDLE_LEFT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_WHIPCLIMBLEFT;
            break;

        case SITHPLAYERMOVE_RAFT_ENDPADDLE_RIGHT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_WHIPCLIMBRIGHT;
            break;

        case SITHPLAYERMOVE_RAFT_STARTPADDLE_RIGHT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_CLIMBWALLRIGHT;
            break;

        case SITHPLAYERMOVE_RAFT_PADDLERIGHT_STARTPADDLE_LEFT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_WHIPCLIMBDOWN;
            break;

        case SITHPLAYERMOVE_RAFT_PADDLEL_LEFT_STARTPADDLE_RIGHT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_WHIPCLIMBUP;
            break;

        case SITHPLAYERMOVE_RAFT_UNBOARDING_LEFT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_HOPLEFT;
            break;

        case SITHPLAYERMOVE_RAFT_UNBOARDING_RIGHT:
            pState->puppetSubmode = SITHPUPPETSUBMODE_HOPRIGHT;
            break;

        default:
            pState->puppetSubmode = SITHPUPPETSUBMODE_STAND;
            break;
    }
}

int J3DAPI sithVehicleControls_FindRaftExitSurfacePos(SithThing* pThing, float moveDist, rdVector3* pOutPos, rdVector3* pOutPYR)
{
    if ( pThing->orient.uvec.z < 0.99860001f )
    {
        // Raft is tilted
        return 8;
    }

    // Search for exit wall in 7 horizontal directions around the raft
    SithRaftExitSearch aSearch[7];
    const rdVector3* pRight = &pThing->orient.rvec;
    const rdVector3* pLook  = &pThing->orient.lvec;
    rdVector_Set3(&aSearch[2].dir, pLook->x, pLook->y, 0.0f);
    rdVector_Normalize3Acc(&aSearch[2].dir);
    sithVehicleControls_sub_4A5AE0(pThing, &aSearch[2], moveDist);

    rdVector_Set3(&aSearch[3].dir, -pRight->x + pLook->x, -pRight->y + pLook->y, 0.0f);
    rdVector_Normalize3Acc(&aSearch[3].dir);
    sithVehicleControls_sub_4A5AE0(pThing, &aSearch[3], moveDist);

    rdVector_Set3(&aSearch[4].dir, pRight->x + pLook->x, pRight->y + pLook->y, 0.0f);
    rdVector_Normalize3Acc(&aSearch[4].dir);
    sithVehicleControls_sub_4A5AE0(pThing, &aSearch[4], moveDist);

    rdVector_Set3(&aSearch[0].dir, -pRight->x, -pRight->y, 0.0f);
    rdVector_Normalize3Acc(&aSearch[0].dir);
    sithVehicleControls_sub_4A5AE0(pThing, &aSearch[0], moveDist);

    rdVector_Set3(&aSearch[1].dir, pRight->x, pRight->y, 0.0f);
    rdVector_Normalize3Acc(&aSearch[1].dir);
    sithVehicleControls_sub_4A5AE0(pThing, &aSearch[1], moveDist);

    rdVector_Set3(&aSearch[5].dir, pLook->x * -1.0f + -pRight->x, pLook->y * -1.0f + -pRight->y, 0.0f);
    rdVector_Normalize3Acc(&aSearch[5].dir);
    sithVehicleControls_sub_4A5AE0(pThing, &aSearch[5], moveDist);

    rdVector_Set3(&aSearch[6].dir, pLook->x * -1.0f + pRight->x, pLook->y * -1.0f + pRight->y, 0.0f);
    rdVector_Normalize3Acc(&aSearch[6].dir);
    sithVehicleControls_sub_4A5AE0(pThing, &aSearch[6], moveDist);

    // Find wall which has floor on the other side
    bool bFound = false;
    int exitNum;
    for ( ;; )
    {
        exitNum = -1;
        for ( int i = 0; i < (int)STD_ARRAYLEN(aSearch); ++i )
        {
            if ( aSearch[i].pSurface && aSearch[i].dot < 1.0f )
            {
                exitNum = i;
            }
        }

        if ( exitNum == -1 )
        {
            break;
        }

        rdVector3 startPos;
        startPos.x = pThing->pos.x;
        startPos.y = pThing->pos.y;
        startPos.z = ((0.050000001f - sithPhysics_GetThingHeight(pThing)) - -0.059999999f) + pThing->pos.z;
        SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.001f);
        if ( pSector )
        {
            // Check that nothing blocks the way above the wall
            rdVector3 dir;
            rdVector_Neg3(&dir, &aSearch[exitNum].pSurface->face.normal);
            sithCollision_SearchForCollisions(pSector, pThing, &startPos, &dir, moveDist, 0.050000001f, 2);
            SithCollision* pCollision = sithCollision_PopStack();
            sithCollision_DecreaseStackLevel();
            if ( !pCollision )
            {
                // Search for floor behind the wall
                float dist = moveDist - -0.050000001f;
                rdVector3 floorPos;
                floorPos.x = dir.x * dist + startPos.x;
                floorPos.y = dir.y * dist + startPos.y;
                floorPos.z = dir.z * dist + startPos.z;

                rdVector3 downDir;
                rdVector_Neg3(&downDir, &rdroid_g_zVector3);

                pSector = sithCollision_FindSectorInRadius(pSector, &startPos, &floorPos, 0.001f);
                if ( pSector )
                {
                    sithCollision_SearchForCollisions(pSector, pThing, &floorPos, &downDir, 0.050000001f - -0.02f, 0.001f, 2);
                    for ( pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
                    {
                        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0
                            && (pCollision->type & SITHCOLLISION_FACE) != 0
                            && pCollision->pSurfaceCollided
                            && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0
                            && pCollision->pSurfaceCollided->face.normal.z >= 0.99980003f )
                        {
                            bFound = true;
                            break;
                        }
                    }

                    sithCollision_DecreaseStackLevel();
                }
            }
        }

        if ( bFound )
        {
            break;
        }

        aSearch[exitNum].pSurface = NULL;
    }

    if ( !bFound )
    {
        return 8;
    }

    // Return 0 when exit is on the right side or 1 when on the left side
    const SithSurface* pExitSurf = aSearch[exitNum].pSurface;
    int result = pThing->orient.rvec.y * pExitSurf->face.normal.y + pExitSurf->face.normal.x * pThing->orient.rvec.x + pExitSurf->face.normal.z * pThing->orient.rvec.z <= 0.0f;

    // Exit position is in front of the center of exit wall
    rdVector_Copy3(pOutPYR, &pExitSurf->face.normal);
    rdVector_Set3(pOutPos, 0.0f, 0.0f, 0.0f);
    size_t i = 0;
    for ( ; i < pExitSurf->face.numVertices; ++i )
    {
        const rdVector3* pVert = &sithWorld_g_pCurrentWorld->aVertices[pExitSurf->face.aVertices[i]];
        pOutPos->x = pVert->x + pOutPos->x;
        pOutPos->y = pVert->y + pOutPos->y;
        pOutPos->z = pVert->z + pOutPos->z;
    }

    float invNum = 1.0f / (float)i;
    float x = pOutPos->x * invNum;
    float y = pOutPos->y * invNum;
    float z = pOutPos->z * invNum;
    pOutPos->x = 0.050000001f * pOutPYR->x + x;
    pOutPos->y = pOutPYR->y * 0.050000001f + y;
    pOutPos->z = pOutPYR->z * 0.050000001f + z;
    return result;
}

void J3DAPI sithVehicleControls_sub_4A5AE0(SithThing* pThing, void* a2, float moveDist)
{
    // Searches for vertical wall surface in direction of pSearch->dir
    SithRaftExitSearch* pSearch = (SithRaftExitSearch*)a2;
    pSearch->pSurface = NULL;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &pSearch->dir, moveDist, 0.001f, 2);

    SithCollision* pCollision = sithCollision_PopStack();
    if ( pCollision && (pCollision->type & SITHCOLLISION_WORLD) != 0 && (pCollision->type & SITHCOLLISION_FACE) != 0 )
    {
        SithSurface* pSurface = pCollision->pSurfaceCollided;
        if ( (pSurface->flags & SITH_SURFACE_ISFLOOR) == 0 && pSurface->face.normal.z <= 0.02f )
        {
            float dot = pSearch->dir.x * pSurface->face.normal.x + pSurface->face.normal.z * pSearch->dir.z + pSurface->face.normal.y * pSearch->dir.y;
            if ( dot < -0.90600002f )
            {
                pSearch->dot      = dot;
                pSearch->pSurface = pSurface;
                pSearch->distance = pCollision->distance;
            }
        }
    }

    sithCollision_DecreaseStackLevel();
}

void J3DAPI sithVehicleControls_FadeJeepMoveSounds(SithThing* pThing)
{
    sithSoundClass_FadeModeVolume(pThing, (SithSoundClassMode)10, 0.0f, 0.1f);
    sithSoundClass_FadeModeVolume(pThing, (SithSoundClassMode)11, 0.0f, 0.1f);
}

int J3DAPI sithVehicleControls_CanMoveInDirection(SithThing* pThing, rdVector3* pDir)
{
    // Check that nothing blocks the way
    bool bCanMove = true;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, pDir, 0.15000001f, 0.07f, 0x200);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        SithCollisionType type = pCollision->type;
        if ( (type & SITHCOLLISION_THING) != 0 )
        {
            if ( pThing != pCollision->pThingCollided )
            {
                bCanMove = false;
            }
        }
        else if ( ((type & SITHCOLLISION_ADJOINTOUCH) == 0
                || (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_NOPLAYERMOVE) != 0
                || (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_MOVE) == 0)
            && (type & SITHCOLLISION_ADJOINCROSS) == 0 )
        {
            bCanMove = false;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bCanMove )
    {
        return 0;
    }

    // Check for floor at destination
    rdVector3 pos;
    pos.x = pDir->x * 0.15000001f + pThing->pos.x;
    pos.y = pDir->y * 0.15000001f + pThing->pos.y;
    pos.z = pDir->z * 0.15000001f + pThing->pos.z;

    rdVector3 downDir;
    rdVector_Set3(&downDir, 0.0f, 0.0f, -1.0f);

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &pos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    int bFloor = 0;
    sithCollision_SearchForCollisions(pSector, NULL, &pos, &downDir, sithPhysics_GetThingHeight(pThing) - -0.0049999999f, 0.001f, 0x200);
    SithCollision* pCollision = sithCollision_PopStack();
    if ( !pCollision )
    {
        sithCollision_DecreaseStackLevel();
        return 0;
    }

    for ( ; pCollision; pCollision = sithCollision_PopStack() )
    {
        SithCollisionType type = pCollision->type;
        if ( (type & SITHCOLLISION_THING) != 0 )
        {
            if ( pThing != pCollision->pThingCollided )
            {
                break;
            }
        }
        else if ( (type & SITHCOLLISION_WORLD) != 0 && (type & SITHCOLLISION_FACE) != 0 )
        {
            // Jeep can't exit onto the floor with no-move adjoin
            const SithSurfaceAdjoin* pAdjoin = pCollision->pSurfaceCollided->pAdjoin;
            if ( ((pThing->moveInfo.physics.flags & SITH_PF_JEEP) != 0 && pAdjoin && (pAdjoin->flags & SITH_ADJOIN_MOVE) == 0 && (pAdjoin->flags & SITH_ADJOIN_VISIBLE) != 0)
                || pCollision->pSurfaceCollided->face.normal.z < 0.88999999f )
            {
                break;
            }

            bFloor = 1;
        }
    }

    sithCollision_DecreaseStackLevel();
    return pCollision ? 0 : bFloor;
}

void J3DAPI sithVehicleControls_ExitRaft(SithThing* pRaftPlayer)
{
    sithVehicleControls_SendMessageUser1(pRaftPlayer);
    sithInventory_SetSwimmingInventory(pRaftPlayer, 1);
    sithInventory_SetInventoryDisabled(pRaftPlayer, 56, 1); // Patch kit

    // Indy faces the wall on the exit side
    const rdVector3* pLook = &pRaftPlayer->orient.lvec;
    rdMatrix34 orient;
    rdVector_Copy3(&orient.uvec, &rdroid_g_zVector3);

    rdVector3 exitPos;
    if ( pRaftPlayer->moveStatus == SITHPLAYERMOVE_RAFT_UNBOARDING_LEFT )
    {
        orient.lvec.x = pLook->y * orient.uvec.z - pLook->z * orient.uvec.y;
        orient.lvec.y = pLook->z * orient.uvec.x - pLook->x * orient.uvec.z;
        orient.lvec.z = pLook->x * orient.uvec.y - pLook->y * orient.uvec.x;
        rdVector_Normalize3Acc(&orient.lvec);

        rdVector_Scale3(&exitPos, &pRaftPlayer->orient.rvec, -0.1f);
    }
    else
    {
        orient.lvec.x = pLook->z * orient.uvec.y - pLook->y * orient.uvec.z;
        orient.lvec.y = pLook->x * orient.uvec.z - pLook->z * orient.uvec.x;
        orient.lvec.z = pLook->y * orient.uvec.x - pLook->x * orient.uvec.y;
        rdVector_Normalize3Acc(&orient.lvec);

        rdVector_Scale3(&exitPos, &pRaftPlayer->orient.rvec, 0.1f);
    }

    orient.rvec.x = orient.lvec.y * orient.uvec.z - orient.lvec.z * orient.uvec.y;
    orient.rvec.y = orient.lvec.z * orient.uvec.x - orient.lvec.x * orient.uvec.z;
    orient.rvec.z = orient.lvec.x * orient.uvec.y - orient.lvec.y * orient.uvec.x;
    rdVector_Normalize3Acc(&orient.rvec);

    SithThing* pPlayerThing = sithPlayer_g_aPlayers[0].pThing;
    exitPos.x = exitPos.x + pRaftPlayer->pos.x;
    exitPos.y = exitPos.y + pRaftPlayer->pos.y;
    exitPos.z = exitPos.z + pRaftPlayer->pos.z;
    exitPos.z = ((sithPhysics_GetThingHeight(pPlayerThing) - sithPhysics_GetThingHeight(pRaftPlayer)) - -0.050000001f) + exitPos.z;

    SithSector* pSector = sithCollision_FindSectorInRadius(pRaftPlayer->pInSector, &pRaftPlayer->pos, &exitPos, 0.0f);
    if ( !pSector )
    {
        SITHLOG_ERROR("This is really bad, trying to exit vehicle to non-existent sector..\n");
    }

    SithSector* pRaftSector = pRaftPlayer->pInSector;
    if ( pSector != pRaftSector )
    {
        sithCog_SectorSendMessage(pRaftSector, pRaftPlayer, SITHCOG_MSG_EXITED);
    }

    // Transfer control back to Indy
    sithPlayer_HidePlayer(sithPlayer_g_playerNum);
    *pPlayerThing->thingInfo.actorInfo.pPlayer = *pRaftPlayer->thingInfo.actorInfo.pPlayer;
    pPlayerThing->thingInfo.actorInfo.pPlayer->pThing = pPlayerThing;
    pPlayerThing->thingInfo.actorInfo.health = pRaftPlayer->thingInfo.actorInfo.health;

    if ( (pRaftPlayer->thingInfo.actorInfo.flags & SITH_AF_INVULNERABLE) != 0 )
    {
        pPlayerThing->thingInfo.actorInfo.flags |= SITH_AF_INVULNERABLE;
    }
    else
    {
        pPlayerThing->thingInfo.actorInfo.flags &= ~SITH_AF_INVULNERABLE;
    }

    if ( (pRaftPlayer->thingInfo.actorInfo.flags & SITH_AF_POISONED) != 0 )
    {
        pPlayerThing->thingInfo.actorInfo.flags |= SITH_AF_POISONED;
    }
    else
    {
        pPlayerThing->thingInfo.actorInfo.flags &= ~SITH_AF_POISONED;
    }

    sithThing_ExitSector(pPlayerThing);
    sithPlayer_SetLocalPlayer(0);
    sithPlayer_ShowPlayer(0, 0);
    sithThing_SetPositionAndOrient(pPlayerThing, &exitPos, &orient);
    sithThing_EnterSector(pPlayerThing, pSector, 1, pSector == pRaftSector);

    pPlayerThing->alpha      = 1.0f;
    pPlayerThing->moveStatus = SITHPLAYERMOVE_STILL;
    sithPuppet_SetMoveMode(pPlayerThing, SITHPUPPET_MOVEMODE_NORMAL);
    sithPhysics_FindFloor(pPlayerThing, 1);
    sithCamera_SetCameraFocus(&sithCamera_g_aCameras[1], pPlayerThing, NULL);

    sithCog_ThingSendMessageEx(pRaftPlayer, NULL, SITHCOG_MSG_CALLBACK, 0, 16, 0, 0);
    sithSoundClass_StopMode(pRaftPlayer, SITHSOUNDCLASS_JUMPWOOD);
}

void J3DAPI sithVehicleControls_UpdateRaftPaddleSoundFx(SithThing* pThing)
{
    bool bPaddleIdle = true;

    rdVector3 paddleDir;
    if ( pThing->moveStatus == SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_LEFT || pThing->moveStatus == SITHPLAYERMOVE_RAFT_PADDLE_FORWARD_RIGHT )
    {
        rdVector_Copy3(&paddleDir, &pThing->orient.lvec);
    }
    else
    {
        rdVector_Neg3(&paddleDir, &pThing->orient.lvec);
    }

    rdVector3 moveDir;
    float speed = rdVector_Normalize3(&moveDir, &pThing->moveInfo.physics.velocity);
    float minSpeed = pThing->orient.lvec.y * moveDir.y + pThing->orient.lvec.z * moveDir.z + pThing->orient.lvec.x * moveDir.x >= 0.0f ? 0.175f : 0.1f;
    if ( minSpeed < speed && minSpeed < moveDir.z * paddleDir.z + moveDir.y * paddleDir.y + moveDir.x * paddleDir.x )
    {
        bPaddleIdle = false;
    }

    // Alternate between two paddle sounds
    SithSoundClassMode mode;
    if ( sithVehicleControls_dword_53B188 < 1 )
    {
        mode = bPaddleIdle ? (SithSoundClassMode)0xF : (SithSoundClassMode)0x13;
    }
    else
    {
        mode = bPaddleIdle ? (SithSoundClassMode)0xE : (SithSoundClassMode)0x12;
    }

    sithSoundClass_PlayModeFirst(pThing, mode);
    sithVehicleControls_dword_53B188 = -sithVehicleControls_dword_53B188;
}

int J3DAPI sithVehicleControls_SendMessageUser0(SithThing* pThing)
{
    int result = sithCog_ThingSendMessage(pThing, pThing, SITHCOG_MSG_USER0);
    sithPlayerControls_g_bCutsceneMode = 0;
    return result;
}

void J3DAPI sithVehicleControls_SendMessageUser1(SithThing* pThing)
{
    sithCog_ThingSendMessage(pThing, pThing, SITHCOG_MSG_USER1);
}
