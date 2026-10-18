#include "sithAIMove.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Math/rdMath.h>
#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdVector.h>
#include <sith/AI/sithAI.h>
#include <sith/AI/sithAIUtil.h>
#include <sith/Cog/sithCog.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/Gameplay/sithPlayerControls.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithActor.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWeapon.h>
#include <std/General/stdMath.h>
#include <std/General/stdUtil.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


void sithAIMove_InstallHooks(void)
{
    J3D_HOOKFUNC(sithAIMove_Update);
    J3D_HOOKFUNC(sithAIMove_UpdateMineCar);
    J3D_HOOKFUNC(sithAIMove_sub_4958B0);
    J3D_HOOKFUNC(sithAIMove_sub_495CD0);
    J3D_HOOKFUNC(sithAIMove_sub_4961A0);
    J3D_HOOKFUNC(sithAIMove_sub_496200);
    J3D_HOOKFUNC(sithAIMove_sub_496550);
    J3D_HOOKFUNC(sithAIMove_sub_4966D0);
    J3D_HOOKFUNC(sithAIMove_sub_496820);
    J3D_HOOKFUNC(sithAIMove_AIGetMoveState);
    J3D_HOOKFUNC(sithAIMove_sub_496A50);
    J3D_HOOKFUNC(sithAIMove_UpdateAIMove);
    J3D_HOOKFUNC(sithAIMove_GetAIMoveModes);
    J3D_HOOKFUNC(sithAIMove_SetSubMode);
    J3D_HOOKFUNC(sithAIMove_PuppetCallback);
    J3D_HOOKFUNC(sithAIMove_AISetLookThing);
    J3D_HOOKFUNC(sithAIMove_AISetLookPos);
    J3D_HOOKFUNC(sithAIMove_AISetLookPosEyeLevel);
    J3D_HOOKFUNC(sithAIMove_AISetMovePos);
    J3D_HOOKFUNC(sithAIMove_AISetMoveTargetPos);
    J3D_HOOKFUNC(sithAIMove_sub_497FF0);
    J3D_HOOKFUNC(sithAIMove_sub_498890);
    J3D_HOOKFUNC(sithAIMove_AIJump);
    J3D_HOOKFUNC(sithAIMove_AIStop);
    J3D_HOOKFUNC(sithAIMove_SetGoalReached);
    J3D_HOOKFUNC(sithAIMove_Unreachable);
    J3D_HOOKFUNC(sithAIMove_StopAIMovement);
    J3D_HOOKFUNC(sithAIMove_ResetAILook);
    J3D_HOOKFUNC(sithAIMove_UpdateBoss);
    J3D_HOOKFUNC(sithAIMove_sub_499090);
    J3D_HOOKFUNC(sithAIMove_sub_4996C0);
    J3D_HOOKFUNC(sithAIMove_sub_499A80);
    J3D_HOOKFUNC(sithAIMove_sub_499CA0);
    J3D_HOOKFUNC(sithAIMove_sub_49A020);
    J3D_HOOKFUNC(sithAIMove_sub_49A1B0);
    J3D_HOOKFUNC(sithAIMove_sub_49A450);
    J3D_HOOKFUNC(sithAIMove_sub_49A630);
    J3D_HOOKFUNC(sithAIMove_sub_49A810);
    J3D_HOOKFUNC(sithAIMove_UpdateQuetzTail);
    J3D_HOOKFUNC(sithAIMove_sub_49AA60);
    J3D_HOOKFUNC(sithAIMove_sub_49AB80);
    J3D_HOOKFUNC(sithAIMove_sub_49AC50);
    J3D_HOOKFUNC(sithAIMove_sub_49AF80);
    J3D_HOOKFUNC(sithAIMove_sub_49B1B0);
    J3D_HOOKFUNC(sithAIMove_UpdateMardukTail);
}

float sithAIMove_g_flt_585464;
float sithAIMove_g_flt_585468;
float sithAIMove_g_flt_58546C;
float sithAIMove_g_flt_585470;

// Puppet submodes of AI special turns and moves (re-used submode slots)
#define SITHAIMOVE_SUBMODE_ROLLLEFT     SITHPUPPETSUBMODE_HOPLEFT
#define SITHAIMOVE_SUBMODE_ROLLRIGHT    SITHPUPPETSUBMODE_HOPRIGHT
#define SITHAIMOVE_SUBMODE_TURN180      (SithPuppetSubMode)0x28
#define SITHAIMOVE_SUBMODE_TURNLEFT45   (SithPuppetSubMode)0x2A
#define SITHAIMOVE_SUBMODE_TURNRIGHT45  (SithPuppetSubMode)0x2B
#define SITHAIMOVE_SUBMODE_TURNLEFT90   (SithPuppetSubMode)0x2C
#define SITHAIMOVE_SUBMODE_TURNRIGHT90  (SithPuppetSubMode)0x2D
#define SITHAIMOVE_SUBMODE_TURNLEFT135  (SithPuppetSubMode)0x2E
#define SITHAIMOVE_SUBMODE_TURNRIGHT135 (SithPuppetSubMode)0x2F

// Special move flags (see sithAIMove_sub_497FF0)
#define SITHAIMOVE_SPECIAL_ROLL         0x01
#define SITHAIMOVE_SPECIAL_STRAFE       0x02
#define SITHAIMOVE_SPECIAL_TURN45       0x04
#define SITHAIMOVE_SPECIAL_TURN90       0x08
#define SITHAIMOVE_SPECIAL_TURN135      0x10
#define SITHAIMOVE_SPECIAL_TURN180      0x20
#define SITHAIMOVE_SPECIAL_MOUNTWALL    0x40
#define SITHAIMOVE_SPECIAL_RANDOMSIDE   0x100
#define SITHAIMOVE_SPECIAL_LEFT         0x200
#define SITHAIMOVE_SPECIAL_RIGHT        0x400

static float sithAIMove_Random(void)
{
    return (float)rand() * (1.0f / 32767.0f);
}

static float sithAIMove_Sign(float value)
{
    return (float)(value >= 0.0f ? 1 : -1);
}

// Projects vector onto the plane with normal n
static void sithAIMove_ProjectOnPlane(rdVector3* pOut, const rdVector3* pVec, const rdVector3* n)
{
    float dist = -rdVector_Dot3(pVec, n);
    rdVector_ScaleAdd3(pOut, n, dist, pVec);
}

// Sets thing orientation look vector to pLook keeping the up vector
static void sithAIMove_SetOrientLook(rdMatrix34* pOrient, const rdVector3* pLook)
{
    pOrient->lvec = *pLook;
    rdVector_Cross3(&pOrient->rvec, &pOrient->lvec, &pOrient->uvec);
    rdVector_Normalize3Acc(&pOrient->rvec);
    rdVector_Cross3(&pOrient->uvec, &pOrient->rvec, &pOrient->lvec);
}

// Returns the eye position of AI owner (eye offset is applied only along the up vector)
static void sithAIMove_GetEyePos(const SithThing* pThing, rdVector3* pOut)
{
    pOut->x = 0.0f;
    pOut->y = 0.0f;
    pOut->z = pThing->thingInfo.actorInfo.eyeOffset.z;
    rdMatrix_TransformVector34Acc(pOut, &pThing->orient);
    rdVector_Add3Acc(pOut, &pThing->pos);
}

// Returns the eye position of actor or player, otherwise thing position
static void sithAIMove_GetThingEyePos(const SithThing* pThing, rdVector3* pOut)
{
    if ( pThing->type == SITH_THING_ACTOR || pThing->type == SITH_THING_PLAYER )
    {
        rdMatrix_TransformVector34(pOut, &pThing->thingInfo.actorInfo.eyeOffset, &pThing->orient);
        rdVector_Add3Acc(pOut, &pThing->pos);
    }
    else
    {
        *pOut = pThing->pos;
    }
}

// Returns turn step for the turn angle. Step is smoothly decreased when getting close to the target angle.
static float sithAIMove_GetTurnStep(const SithThing* pThing, float angle, float secDeltaTime, float minSmoothAngle)
{
    float step     = pThing->thingInfo.actorInfo.maxRotVelocity * secDeltaTime;
    float fullTurn = pThing->thingInfo.actorInfo.maxRotVelocity * 0.07f;
    float absAngle = fabsf(angle);
    if ( absAngle <= fullTurn && absAngle >= minSmoothAngle )
    {
        step = (absAngle / fullTurn) * step;
    }

    return step;
}

static bool sithAIMove_IsZeroVector(const rdVector3* pVec)
{
    return pVec->x == 0.0f && pVec->y == 0.0f && pVec->z == 0.0f;
}

void J3DAPI sithAIMove_Update(SithThing* pThing, float secDeltatTime)
{
    SITH_ASSERTREL(pThing);
    SITH_ASSERTREL(pThing->controlType == SITH_CT_AI);

    if ( pThing->type != SITH_THING_ACTOR || pThing->thingInfo.actorInfo.health <= 0.0f )
    {
        return;
    }

    SithAIControlBlock* pLocal = pThing->controlInfo.aiControl.pLocal;
    SITH_ASSERTREL(pLocal);

    SithAIMode mode = pLocal->mode;
    if ( (mode & SITHAI_MODE_SLEEPING) != 0 && (mode & SITHAI_MODE_BLOCK) == 0 && (pThing->thingInfo.actorInfo.flags & SITH_AF_NOSLOPEMOVE) == 0 )
    {
        return;
    }

    if ( (pThing->flags & (SITH_TF_DISABLED | SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 )
    {
        return;
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0 && (mode & SITHAI_MODE_MOVING) != 0 && (mode & SITHAI_MODE_DISABLED) == 0 )
    {
        // Look at move position
        rdVector3 lookPos = pLocal->movePos;
        lookPos.z = pLocal->pOwner->thingInfo.actorInfo.eyeOffset.z + lookPos.z;
        sithAIMove_AISetLookPos(pLocal, &lookPos);
    }

    SithAIMode bTurning = pLocal->mode & SITHAI_MODE_TURNING;
    if ( bTurning || (pLocal->submode & SITHAI_SUBMODE_BODYTRACKINGMOTION) != 0 )
    {
        if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_BOSS) != 0 )
        {
            sithAIMove_UpdateBoss(pLocal, secDeltatTime);
        }
        else if ( pLocal->pOwner->moveType == SITH_MT_PHYSICS && (pThing->moveInfo.physics.flags & SITH_PF_MINECAR) != 0 )
        {
            sithAIMove_UpdateMineCar(pLocal, secDeltatTime);
        }
        else
        {
            if ( (pLocal->submode & SITHAI_SUBMODE_BODYTRACKINGMOTION) != 0 && pLocal->pTargetThing )
            {
                // Turn body towards target when head can't turn any further
                if ( bTurning || (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400) == 0 || fabsf(pThing->thingInfo.actorInfo.headPYR.y) > 20.0f )
                {
                    sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
                }
            }

            sithAIMove_sub_4958B0(pLocal, secDeltatTime);
        }
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_10) != 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 )
        {
            sithAIUtil_sub_49D170(pLocal);
        }
        else
        {
            sithAIMove_sub_4961A0(pLocal);
        }
    }

    if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 && (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_10) == 0 )
    {
        sithAIMove_sub_495CD0(pLocal, secDeltatTime);
    }

    if ( sithWeapon_HasWeaponSelected(pLocal->pOwner) )
    {
        sithPlayerControls_ProcessWeaponAim(pLocal->pOwner, secDeltatTime);
    }

    // Head tracking
    if ( !sithWeapon_IsAiming(pLocal->pOwner)
        && ((pLocal->mode & (SITHAI_MODE_UNKNOWN_80 | SITHAI_MODE_TURNING)) != 0 || (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400) != 0) )
    {
        if ( ((pLocal->mode & SITHAI_MODE_BLOCK) == 0 && (pThing->thingInfo.actorInfo.flags & SITH_AF_NOSLOPEMOVE) == 0)
            || (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400) != 0 )
        {
            sithAIMove_sub_499090(pLocal, secDeltatTime);
        }
        else
        {
            sithAIMove_sub_4996C0(pLocal, secDeltatTime);
        }
    }

    SithThing* pAttached = pLocal->pOwner->pAttachedThing;
    if ( pAttached && (pAttached->attach.flags & SITH_ATTACH_TAIL) != 0 )
    {
        sithAIMove_UpdateQuetzTail(pLocal, secDeltatTime);
        return;
    }

    if ( strncmp(pThing->aName, "marduk", 6) == 0 )
    {
        sithAIMove_UpdateMardukTail(pThing, secDeltatTime);
    }
}

// Turns the torso mesh 'redarmy' (e.g. vehicle turret) towards the look position
void J3DAPI sithAIMove_UpdateMineCar(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0 )
    {
        return;
    }

    int meshNum = sithThing_GetThingMeshIndex(pOwner, "redarmy");
    const rdVector3* pJointPos = &pOwner->renderData.paJointMatrices[meshNum].dvec;

    rdVector3 toLook;
    rdVector_Sub3(&toLook, &pLocal->lookPos, pJointPos);

    // Current torso direction
    rdVector3 torsoDir;
    rdVector_Rotate3(&torsoDir, &pOwner->orient.lvec, &pOwner->renderData.apTweakedAngles[meshNum]);
    rdVector_Normalize3Acc(&torsoDir);

    rdVector3 torsoDirFlat;
    sithAIMove_ProjectOnPlane(&torsoDirFlat, &torsoDir, &pOwner->orient.uvec);
    rdVector_Normalize3Acc(&torsoDirFlat);

    // Note, the original also calculates the torso right vector here, but doesn't use it

    float angle = 0.0f;
    if ( fabsf(rdVector_Dot3(&pOwner->orient.uvec, &toLook)) <= 0.999f )
    {
        rdVector3 lookDir;
        sithAIMove_ProjectOnPlane(&lookDir, &toLook, &pOwner->orient.uvec);
        rdVector_Normalize3Acc(&lookDir);

        angle = rdMath_DeltaAngleNormalized(&lookDir, &torsoDirFlat, &pOwner->orient.uvec);
        float step = sithAIMove_GetTurnStep(pOwner, angle, secDeltaTime, 1.0f);
        if ( fabsf(angle) > step * 1.05f )
        {
            rdVector3 pyr = { 0.0f, -(sithAIMove_Sign(angle) * step), 0.0f };
            rdVector_Rotate3Acc(&torsoDirFlat, &pyr);

            rdVector3* pTweak = &pOwner->renderData.apTweakedAngles[meshNum];
            pTweak->y = 90.0f - stdMath_ArcSin3(rdVector_Dot3(&pOwner->orient.lvec, &torsoDirFlat));
            if ( rdVector_Dot3(&pOwner->orient.rvec, &torsoDirFlat) > 0.0f )
            {
                pTweak->y = -pTweak->y;
            }

            // Note, the original then sets turn move status when angle is 0 which never happens here
            return;
        }

        rdVector3* pTweak = &pOwner->renderData.apTweakedAngles[meshNum];
        pTweak->y = 90.0f - stdMath_ArcSin3(rdVector_Dot3(&pOwner->orient.lvec, &lookDir));
        if ( rdVector_Dot3(&pOwner->orient.rvec, &lookDir) > 0.0f )
        {
            pTweak->y = -pTweak->y;
        }

        pLocal->mode &= ~SITHAI_MODE_TURNING;
        if ( angle == 0.0f )
        {
            pOwner->moveStatus = SITHPLAYERMOVE_STILL;
        }

        return;
    }

    if ( (pOwner->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
    {
        SITHLOG_ERROR("Strange... AI '%s' is looking straight up or down.\n", pOwner->aName);
    }

    pLocal->mode &= ~SITHAI_MODE_TURNING;
}

// Turns the AI body towards the goal look vector
void J3DAPI sithAIMove_sub_4958B0(SithAIControlBlock* pLocal, float secDeltatTime)
{
    SithThing* pOwner = pLocal->pOwner;
    rdVector3 goalLVec = pLocal->goalLVec;
    if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0 )
    {
        return;
    }

    int moveState = sithAIMove_AIGetMoveState(pLocal);
    if ( moveState == 2 )
    {
        return; // special turn in progress
    }

    if ( fabsf(rdVector_Dot3(&pOwner->orient.uvec, &goalLVec)) <= 0.999f )
    {
        rdVector3 lookDir;
        sithAIMove_ProjectOnPlane(&lookDir, &goalLVec, &pOwner->orient.uvec);
        rdVector_Normalize3Acc(&lookDir);

        float angle = rdMath_DeltaAngleNormalized(&lookDir, &pOwner->orient.lvec, &pOwner->orient.uvec);
        if ( sithAIMove_sub_496A50(pLocal, angle) )
        {
            return; // special turn started
        }

        float step = sithAIMove_GetTurnStep(pOwner, angle, secDeltatTime, 1.0f);
        if ( fabsf(angle) > step * 1.05f )
        {
            rdVector3 pyr = { 0.0f, -(sithAIMove_Sign(angle) * step), 0.0f };
            rdMatrix_PreRotate34(&pOwner->orient, &pyr);
            rdMatrix_Normalize34(&pOwner->orient);

            if ( moveState != 0 )
            {
                return;
            }

            if ( angle >= 0.0f )
            {
                pOwner->moveStatus = angle <= 45.0f ? SITHPLAYERMOVE_TURNING_RIGHT : SITHPLAYERMOVE_UNKNOWN_106;
            }
            else
            {
                pOwner->moveStatus = angle >= -45.0f ? SITHPLAYERMOVE_TURNING_LEFT : SITHPLAYERMOVE_UNKNOWN_105;
            }

            return;
        }

        sithAIMove_SetOrientLook(&pOwner->orient, &lookDir);
    }
    else if ( (pOwner->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
    {
        SITHLOG_ERROR("Strange... AI '%s' is looking straight up or down.\n", pOwner->aName);
    }

    pLocal->mode &= ~SITHAI_MODE_TURNING;
    if ( moveState == 0 )
    {
        pOwner->moveStatus = SITHPLAYERMOVE_STILL;
    }
}

// Moves AI towards the move position
void J3DAPI sithAIMove_sub_495CD0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    int bReached = 0;
    SITH_ASSERTREL(pLocal);

    SithThing* pOwner = pLocal->pOwner;
    SITH_ASSERTREL(pOwner && pOwner->type == SITH_THING_ACTOR);

    if ( (pOwner->flags & (SITH_TF_DISABLED | SITH_TF_DYING | SITH_TF_DESTROYED)) != 0
        || (pOwner->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0
        || pLocal->msecPauseMoveUntil > sithTime_g_msecGameTime )
    {
        return;
    }

    pLocal->submode &= ~(SITHAI_SUBMODE_UNKNOWN_4000 | SITHAI_SUBMODE_UNKNOWN_4);

    rdVector3 fromPos = pOwner->pos;
    rdVector3 toPos   = pLocal->movePos;
    float thrust      = pOwner->thingInfo.actorInfo.maxThrust * secDeltaTime * pLocal->moveSpeed;
    float reachDist   = (float)sithAIMove_sub_496200(pLocal, &fromPos, &toPos);

    rdVector_Sub3(&pLocal->moveDirection, &toPos, &fromPos);
    pLocal->moveDistance = rdVector_Normalize3Acc(&pLocal->moveDirection);

    rdVector3 deltaVel;
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) == 0 )
    {
        rdVector_Scale3(&deltaVel, &pLocal->moveDirection, thrust);
    }
    else
    {
        // Move only forward
        float fwd = rdVector_Dot3(&pOwner->orient.lvec, &pLocal->moveDirection);
        fwd = fwd >= 0.3f ? (fwd > 1.0f ? 1.0f : fwd) : 0.3f;
        rdVector_Scale3(&deltaVel, &pOwner->orient.lvec, fwd * thrust);
    }

    deltaVel.x = stdMath_ClipNearZero(deltaVel.x);
    deltaVel.y = stdMath_ClipNearZero(deltaVel.y);
    deltaVel.z = stdMath_ClipNearZero(deltaVel.z);

    rdVector3* pVelocity = &pOwner->moveInfo.physics.velocity;
    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_8000) == 0 || !pLocal->goalThing )
    {
        rdVector_Add3Acc(pVelocity, &deltaVel);
    }
    else
    {
        // Match goal speed
        rdVector3 goalVel = pLocal->goalThing->moveInfo.physics.velocity;
        float goalSpeed = rdVector_Normalize3Acc(&goalVel);

        rdVector3 dir = *pVelocity;
        rdVector_Normalize3Acc(&dir);
        rdVector_Scale3(pVelocity, &dir, goalSpeed);
    }

    if ( sithAIMove_AIGetMoveState(pLocal) == 2 )
    {
        memset(&pLocal->pOwner->moveInfo.physics.velocity, 0, sizeof(pLocal->pOwner->moveInfo.physics.velocity));
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0 )
    {
        float speed = rdVector_Len3(pVelocity);
        rdVector_Scale3(pVelocity, &pOwner->orient.lvec, speed);
    }

    // Check for cliff, water etc.
    if ( (pLocal->mode & SITHAI_MODE_NOCHECKFORCLIFF) == 0 && !sithAIMove_IsZeroVector(pVelocity) )
    {
        bool bFly = (pOwner->moveInfo.physics.flags & SITH_PF_FLY) != 0;
        if ( !bFly && pOwner->attach.flags != 0 )
        {
            bReached = sithAIMove_sub_496550(pLocal, secDeltaTime);
        }
        else if ( !bFly || (pOwner->thingInfo.actorInfo.flags & SITH_AF_BREATHEUNDERWATER) != 0 )
        {
            if ( (pOwner->flags & (SITH_TF_AIRDESTROYED | SITH_TF_SUBMERGED)) != 0 )
            {
                bReached = sithAIMove_sub_496820(pLocal, secDeltaTime);
            }
        }
        else
        {
            bReached = sithAIMove_sub_4966D0(pLocal, secDeltaTime);
        }

        if ( (pLocal->mode & SITHAI_MODE_MOVING) == 0 )
        {
            return;
        }
    }

    if ( pLocal->moveDistance <= pOwner->collide.movesize * 3.0f )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_SET, NULL);
    }

    if ( !bReached )
    {
        if ( pLocal->moveDistance <= reachDist )
        {
            bReached = 1;
        }

        if ( !bReached )
        {
            return;
        }
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_2) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_AISetMovePos(pLocal, &pLocal->vecUnknown3, pLocal->moveSpeed);
        return;
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_8) != 0 )
    {
        int flags = (pLocal->mode & SITHAI_MODE_ACTIVE) != 0 ? 0x100 : 0;
        if ( sithAIUtil_sub_49EE50(pLocal, &pLocal->vecUnknown3, 45.0f, 90.0f, 45.0f, flags) )
        {
            return;
        }
    }

    if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) == 0 || !sithAIUtil_AIAdvanceToNextWpnt(pLocal, 1) )
    {
        sithAIMove_SetGoalReached(pLocal);
    }
}

// Clears the turn wait submode when AI is facing the goal (or doesn't need to)
void J3DAPI sithAIMove_sub_4961A0(SithAIControlBlock* pLocal)
{
    if ( (pLocal->submode & SITHAI_SUBMODE_NOMOVEBACKWARDS) == 0 || rdVector_Dot3(&pLocal->pOwner->orient.lvec, &pLocal->goalLVec) >= 0.0f )
    {
        pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_10;
    }
}

// Returns the distance at which the move position is considered reached, may adjust the positions
double J3DAPI sithAIMove_sub_496200(SithAIControlBlock* pLocal, const rdVector3* a2, rdVector3* a3)
{
    SithThing* pOwner = pLocal->pOwner;
    SITH_ASSERTREL(pOwner);

    if ( (pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0 || (pOwner->moveInfo.physics.flags & SITH_PF_FLY) != 0 )
    {
        return 0.05f <= pOwner->collide.movesize ? 0.05f : pOwner->collide.movesize;
    }

    float reachDist = 0.03f <= pOwner->collide.movesize ? 0.03f : pOwner->collide.movesize;
    rdVector3* pFrom = (rdVector3*)a2;
    if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) == 0 )
    {
        // Compare horizontally
        pFrom->z = pOwner->pos.z;
        a3->z    = pOwner->pos.z;

        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0 )
        {
            // Move position on the side is reached
            float dist1 = stdMath_Dist2D1((pOwner->orient.rvec.x * 0.05f + pFrom->x) - a3->x, (pOwner->orient.rvec.y * 0.05f + pFrom->y) - a3->y);
            float dist2 = stdMath_Dist2D1((pOwner->orient.rvec.x * -0.05f + pFrom->x) - a3->x, (pOwner->orient.rvec.y * -0.05f + pFrom->y) - a3->y);
            if ( dist1 < 0.05f || dist2 < 0.05f )
            {
                *a3 = *pFrom;
            }
        }

        return reachDist;
    }

    // Wall crawling
    if ( pOwner->moveStatus == SITHPLAYERMOVE_MOUNTING_WALL )
    {
        return reachDist;
    }

    rdVector3 delta;
    rdVector_Sub3(&delta, a3, pFrom);

    int numReachedAxes = 0;
    if ( fabsf(delta.x) <= reachDist )
    {
        a3->x   = pFrom->x;
        delta.x = 0.0f;
        ++numReachedAxes;
    }

    if ( fabsf(delta.y) <= reachDist )
    {
        ++numReachedAxes;
        a3->y   = pFrom->y;
        delta.y = 0.0f;
    }

    if ( fabsf(delta.z) <= reachDist )
    {
        ++numReachedAxes;
        a3->z   = pFrom->z;
        delta.z = 0.0f;
    }

    bool bAlongSurface = false;
    float dist = 0.0f;
    if ( (pOwner->attach.flags & SITH_ATTACH_SURFACE) != 0 )
    {
        dist = rdVector_Normalize3Acc(&delta);
        const rdVector3* pNormal = &pOwner->attach.attachedToStructure.pSurfaceAttached->face.normal;
        float normalDot = pNormal->x * delta.x + pNormal->z * delta.z + pNormal->y * delta.y;
        if ( fabsf(normalDot) < 0.996f )
        {
            bAlongSurface = true;
        }
    }

    if ( !bAlongSurface )
    {
        if ( numReachedAxes != 2 && pOwner->collide.movesize + pOwner->collide.movesize < dist )
        {
            sithAIMove_Unreachable(pLocal);
            return reachDist;
        }

        *a3 = *pFrom;
    }

    return reachDist;
}

// Checks the ground ahead of walking AI. Note, always returns 0.
int J3DAPI sithAIMove_sub_496550(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;

    rdVector3 nextPos;
    rdVector_ScaleAdd3(&nextPos, &pOwner->moveInfo.physics.velocity, secDeltaTime, &pOwner->pos);

    if ( ((pOwner->idx + sithMain_g_frameNumber) & 1) != 0 )
    {
        // Check in front of AI every other frame
        rdVector3 heading;
        float dist;
        if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
        {
            sithAIUtil_GetXYZHeadingVector(pOwner, &heading);
            dist = pLocal->pOwner->collide.movesize;
            if ( dist - 0.03f > 0.0f )
            {
                dist = dist - 0.03f;
            }
        }
        else
        {
            sithAIUtil_GetXYHeadingVector(pOwner, &heading);
            dist = pLocal->pOwner->collide.movesize - -0.03f;
        }

        rdVector_ScaleAdd3Acc(&nextPos, &heading, dist);
    }

    switch ( sithAIUtil_sub_49DC90(pLocal, &nextPos, NULL) )
    {
        case 1:
            break;

        case 2:
            if ( sithAI_HasInstinct(pLocal, "wallcrawl") )
            {
                return 0;
            }
            // fallthrough

        case 0:
        case 8:
            sithAI_EmitEvent(pLocal, SITHAI_EVENT_HIT_CLIFF, NULL);
            return 0;

        case 4:
            if ( (pLocal->submode & SITHAI_SUBMODE_ALLOWSTEPTHING) == 0 )
            {
                sithAI_EmitEvent(pLocal, SITHAI_EVENT_HIT_THING, NULL);
                return 0;
            }
            break;

        default:
            sithAIMove_Unreachable(pLocal);
            break;
    }

    return 0;
}

// Checks flying AI doesn't fly into water or sector it can't enter. Returns 1 if AI was stopped.
int J3DAPI sithAIMove_sub_4966D0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( (pOwner->flags & SITH_TF_SUBMERGED) != 0 )
    {
        // Fly out of water
        sithAIMove_StopAIMovement(pLocal);
        float velZ = pOwner->moveInfo.physics.velocity.z - -0.5f;
        pOwner->moveStatus = SITHPLAYERMOVE_WALKING;
        pOwner->moveInfo.physics.velocity.z = velZ;
        return 0;
    }

    rdVector3 nextPos;
    rdVector_ScaleAdd3(&nextPos, &pOwner->moveInfo.physics.velocity, secDeltaTime, &pOwner->pos);

    rdVector3 heading;
    sithAIUtil_GetXYHeadingVector(pOwner, &heading);
    rdVector_ScaleAdd3Acc(&nextPos, &heading, pOwner->collide.movesize - -0.05f);

    SithSector* pSector = sithCollision_FindSectorInRadius(pOwner->pInSector, &pOwner->pos, &nextPos, 0.0f);
    if ( !pSector || (pSector->flags & SITH_SECTOR_NOACTORENTER) != 0 )
    {
        sithAIMove_Unreachable(pLocal);
        return 1;
    }

    if ( (pSector->flags & SITH_SECTOR_UNDERWATER) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        float velZ = pOwner->moveInfo.physics.velocity.z - -0.5f;
        pOwner->moveStatus = SITHPLAYERMOVE_WALKING;
        pOwner->moveInfo.physics.velocity.z = velZ;
        return 1;
    }

    return 0;
}

// Keeps swimming AI under water. Note, always returns 0.
int J3DAPI sithAIMove_sub_496820(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( pOwner->moveInfo.physics.velocity.z != 0.0f )
    {
        // Don't swim out of water
        rdVector3 checkPos = pOwner->pos;
        float scale = (pLocal->submode & SITHAI_SUBMODE_SWIMNEARSURFACE) != 0 ? 0.2f : 0.5f;
        checkPos.z = pOwner->collide.movesize * scale + pOwner->pos.z;

        SithSector* pSector = sithCollision_FindSectorInRadius(pOwner->pInSector, &pOwner->pos, &checkPos, 0.0f);
        if ( pSector && (pSector->flags & SITH_SECTOR_UNDERWATER) == 0 )
        {
            pOwner->moveInfo.physics.velocity.z = -0.05f;
            if ( pOwner->pos.z < pLocal->movePos.z )
            {
                pLocal->movePos.z = pOwner->pos.z - 0.05f;
            }
        }
    }

    rdVector3 nextPos;
    rdVector_ScaleAdd3(&nextPos, &pOwner->moveInfo.physics.velocity, secDeltaTime, &pOwner->pos);

    rdVector3 heading;
    sithAIUtil_GetXYHeadingVector(pLocal->pOwner, &heading);
    rdVector_ScaleAdd3Acc(&nextPos, &heading, pLocal->pOwner->collide.movesize - -0.03f);

    SithSector* pSector = sithCollision_FindSectorInRadius(pOwner->pInSector, &pOwner->pos, &nextPos, 0.0f);
    if ( !pSector || (pSector->flags & SITH_SECTOR_NOACTORENTER) != 0 || (pSector->flags & SITH_SECTOR_UNDERWATER) == 0 )
    {
        sithAIMove_Unreachable(pLocal);
    }

    return 0;
}

// Returns 2 if AI is in special turn or wall mount, 1 if in other special move (roll, strafe, transition...), otherwise 0
int J3DAPI sithAIMove_AIGetMoveState(const SithAIControlBlock* pLocal)
{
    switch ( pLocal->pOwner->moveStatus )
    {
        case SITHPLAYERMOVE_MOUNTING_WALL:
        case SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES:
        case SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES:
        case SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES:
        case SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES:
        case SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES:
        case SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES:
        case SITHPLAYERMOVE_TURNING_180_DEGREES:
            return 2;

        case SITHPLAYERMOVE_ROLLING_LEFT:
        case SITHPLAYERMOVE_ROLLING_RIGHT:
        case SITHPLAYERMOVE_STRAFING_LEFT:
        case SITHPLAYERMOVE_STRAFING_RIGHT:
        case SITHPLAYERMOVE_SLIDEDOWNFORWARD:
        case SITHPLAYERMOVE_STAND_TO_CRAWL:
        case SITHPLAYERMOVE_CRAWL_TO_STAND:
        case SITHPLAYERMOVE_WALK2STAND:
        case SITHPLAYERMOVE_STAND2WALK:
        case SITHPLAYERMOVE_UNKNOWN_78:
        case SITHPLAYERMOVE_STAND2RUN:
        case SITHPLAYERMOVE_UNKNOWN_80:
        case SITHPLAYERMOVE_UNKNOWN_81:
        case SITHPLAYERMOVE_SLIDEDOWNBACK:
        case SITHPLAYERMOVE_KNOCKEDOUT:
        case SITHPLAYERMOVE_RUNOVER:
            return 1;

        default:
            return 0;
    }
}

// Starts special turn animation for big turn angles. Returns 1 if special turn is in progress or started.
int J3DAPI sithAIMove_sub_496A50(SithAIControlBlock* pLocal, float angle)
{
    int bStarted = 0;
    if ( (pLocal->submode & SITHAI_SUBMODE_SPECIALTURNS) == 0 )
    {
        return 0;
    }

    if ( sithAIMove_AIGetMoveState(pLocal) != 0 )
    {
        return 1;
    }

    float absAngle = fabsf(angle);
    if ( absAngle < 22.5f )
    {
        return 0;
    }

    int side = angle >= 0.0f ? SITHAIMOVE_SPECIAL_RIGHT : SITHAIMOVE_SPECIAL_LEFT;
    if ( absAngle >= 112.5f && absAngle <= 180.0f )
    {
        if ( absAngle <= 157.5f && sithAIMove_sub_497FF0(pLocal, side | SITHAIMOVE_SPECIAL_TURN135) )
        {
            bStarted = 1;
        }

        if ( bStarted )
        {
            return bStarted;
        }

        bStarted = sithAIMove_sub_497FF0(pLocal, SITHAIMOVE_SPECIAL_TURN180) ? 1 : 0;
    }
    else if ( absAngle >= 67.5f && absAngle <= 112.5f )
    {
        bStarted = sithAIMove_sub_497FF0(pLocal, side | SITHAIMOVE_SPECIAL_TURN90) ? 1 : 0;
    }

    if ( !bStarted && sithAIMove_sub_497FF0(pLocal, side | SITHAIMOVE_SPECIAL_TURN45) )
    {
        bStarted = 1;
    }

    return bStarted;
}

// Updates AI move animation by its local velocity. Returns the move speed.
float J3DAPI sithAIMove_UpdateAIMove(SithAIControlBlock* pLocal)
{
    float moveSpeed               = 0.0f;
    SithPuppetSubMode submode     = SITHPUPPETSUBMODE_STAND;
    SithThingMoveStatus moveStatus = SITHPLAYERMOVE_STILL;

    SithThing* pThing = pLocal->pOwner;
    SITH_ASSERTREL(pThing);

    if ( pThing->pInSector
        && (!sithAIMove_IsZeroVector(&pThing->moveInfo.physics.velocity) || (pLocal->mode & SITHAI_MODE_TURNING) != 0)
        && (pThing->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) == 0 )
    {
        rdVector3 localVel;
        rdMatrix_TransformVectorOrtho34(&localVel, &pThing->moveInfo.physics.velocity, &pThing->orient);

        float absX = fabsf(localVel.x);
        float absY = fabsf(localVel.y);
        float absZ = fabsf(localVel.z);
        if ( (pThing->moveInfo.physics.flags & SITH_PF_FLY) != 0 && absZ > absY && absZ > absX )
        {
            // Flying up or down
            moveSpeed = localVel.z;
            if ( localVel.z > 0.01f )
            {
                moveStatus = SITHPLAYERMOVE_UNKNOWN_103;
                submode    = SITHPUPPETSUBMODE_RISING;
            }
            else if ( localVel.z < -0.01f )
            {
                moveStatus = SITHPLAYERMOVE_UNKNOWN_104;
                submode    = SITHPUPPETSUBMODE_FALLFORWARD;
            }
        }
        else
        {
            int moveDirection;
            if ( absZ > absY && absZ > absX )
            {
                moveDirection = 2;
                moveSpeed     = localVel.z;
            }
            else if ( absY > absX )
            {
                moveSpeed     = localVel.y;
                moveDirection = 1;
            }
            else
            {
                moveDirection = 0;
                moveSpeed     = localVel.x;
            }

            sithAIMove_GetAIMoveModes(pThing, moveSpeed, moveDirection, &moveStatus, &submode);
        }
    }

    if ( !pThing->thingInfo.actorInfo.bControlsDisabled && !pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        int moveState = sithAIMove_AIGetMoveState(pLocal);
        if ( moveState == 1 )
        {
            return moveSpeed;
        }

        if ( moveState == 2 )
        {
            return 0.0f;
        }

        if ( pThing->pPuppetState->submode != submode )
        {
            sithAIMove_SetSubMode(pLocal, moveStatus, submode);
        }
    }

    return moveSpeed;
}

// Gets the move status & puppet submode for move speed along the moveDirection axis (0 - x, 1 - y, 2 - z)
void J3DAPI sithAIMove_GetAIMoveModes(const SithThing* pThing, float moveSpeed, int moveDirection, SithThingMoveStatus* pOutMoveStatus, SithPuppetSubMode* pOutSubmode)
{
    const SithAIControlBlock* pLocal = pThing->controlInfo.aiControl.pLocal;
    float walkSpeed = 0.001f;
    float runSpeed  = 0.27f;
    float turnSpeed = 0.005f;
    SITH_ASSERTREL(pLocal);

    SithActorFlag flags = pThing->thingInfo.actorInfo.flags;
    float speedScale = 1.15f;
    if ( (flags & SITH_AF_FASTMOVE15) != 0 || (speedScale = 1.1f, (flags & SITH_AF_FASTMOVE10) != 0) )
    {
        walkSpeed = 0.001f * speedScale;
        runSpeed  = 0.27f * speedScale;
    }

    if ( (pLocal->mode & SITHAI_MODE_TURNING) != 0 )
    {
        if ( (flags & SITH_AF_FLYERMOVE) != 0 )
        {
            if ( pThing->moveStatus == SITHPLAYERMOVE_UNKNOWN_105 )
            {
                *pOutMoveStatus = SITHPLAYERMOVE_TURNING_LEFT;
                *pOutSubmode    = SITHPUPPETSUBMODE_TURNLEFT;
                return;
            }

            if ( pThing->moveStatus == SITHPLAYERMOVE_UNKNOWN_106 )
            {
                *pOutMoveStatus = SITHPLAYERMOVE_TURNING_RIGHT;
                *pOutSubmode    = SITHPUPPETSUBMODE_TURNRIGHT;
                return;
            }
        }

        speedScale = 1.15f;
        if ( (flags & SITH_AF_FASTMOVE15) != 0 || (speedScale = 1.1f, (flags & SITH_AF_FASTMOVE10) != 0) )
        {
            turnSpeed = 0.005f * speedScale;
        }

        if ( fabsf(moveSpeed) < turnSpeed )
        {
            // Turning in place
            if ( pThing->moveStatus != SITHPLAYERMOVE_TURNING_LEFT && pThing->moveStatus != SITHPLAYERMOVE_UNKNOWN_105 )
            {
                *pOutMoveStatus = SITHPLAYERMOVE_TURNING_RIGHT;
                *pOutSubmode    = SITHPUPPETSUBMODE_TURNRIGHT;
                return;
            }

            *pOutMoveStatus = SITHPLAYERMOVE_TURNING_LEFT;
            *pOutSubmode    = SITHPUPPETSUBMODE_TURNLEFT;
            return;
        }
    }

    if ( moveDirection == 0 )
    {
        // Sideways
        if ( fabsf(moveSpeed) >= runSpeed )
        {
            *pOutMoveStatus = SITHPLAYERMOVE_RUNNING;
            *pOutSubmode    = SITHPUPPETSUBMODE_RUN;
            return;
        }

        if ( fabsf(moveSpeed) >= walkSpeed && fabsf(moveSpeed) < runSpeed )
        {
            *pOutMoveStatus = SITHPLAYERMOVE_WALKING;
            *pOutSubmode    = SITHPUPPETSUBMODE_WALK;
            return;
        }

        *pOutMoveStatus = SITHPLAYERMOVE_STILL;
        *pOutSubmode    = SITHPUPPETSUBMODE_STAND;
        return;
    }

    if ( moveDirection != 1 )
    {
        if ( moveDirection != 2 )
        {
            *pOutMoveStatus = SITHPLAYERMOVE_STILL;
            *pOutSubmode    = SITHPUPPETSUBMODE_STAND;
            return;
        }

        if ( pThing->attach.flags == 0 )
        {
            // Vertical move in air
            if ( moveSpeed >= -walkSpeed && moveSpeed <= walkSpeed )
            {
                *pOutMoveStatus = SITHPLAYERMOVE_STILL;
                *pOutSubmode    = SITHPUPPETSUBMODE_STAND;
                return;
            }

            if ( fabsf(moveSpeed) > walkSpeed )
            {
                *pOutMoveStatus = SITHPLAYERMOVE_FALLING;
                *pOutSubmode    = SITHPUPPETSUBMODE_FALL;
            }

            return;
        }
    }

    // Forward/backward
    if ( moveSpeed >= runSpeed )
    {
        *pOutMoveStatus = SITHPLAYERMOVE_RUNNING;
        *pOutSubmode    = SITHPUPPETSUBMODE_RUN;
    }
    else if ( moveSpeed >= walkSpeed )
    {
        *pOutMoveStatus = SITHPLAYERMOVE_WALKING;
        *pOutSubmode    = SITHPUPPETSUBMODE_WALK;
    }
    else if ( moveSpeed > -walkSpeed && moveSpeed < walkSpeed )
    {
        *pOutMoveStatus = SITHPLAYERMOVE_STILL;
        *pOutSubmode    = SITHPUPPETSUBMODE_STAND;
    }
    else if ( moveSpeed <= -walkSpeed )
    {
        *pOutMoveStatus = SITHPLAYERMOVE_WALKING;
        *pOutSubmode    = SITHPUPPETSUBMODE_WALKBACK;
    }
}

// Sets puppet submode, playing transition animations (stand to walk, walk to stand, land) when available. Returns new move status.
int J3DAPI sithAIMove_SetSubMode(SithAIControlBlock* pLocal, SithThingMoveStatus moveStatus, SithPuppetSubMode submode)
{
    SithThing* pOwner = pLocal->pOwner;
    SithPuppetSubMode transitionSubmode = 0;
    rdPuppetTrackCallback pfCallback   = NULL;

    if ( !pOwner || !pOwner->pPuppetClass || !pOwner->pPuppetState )
    {
        SITHLOG_ERROR("sithAIMove_SetSubMode: Bad puppet data for AI '%s'.\n", pOwner->aName);
        return 0;
    }

    SithPuppetState* pState = pOwner->pPuppetState;
    int majorMode = pState->majorMode;
    SithPuppetSubMode curSubmode = pState->submode;
    SithThingMoveStatus transitionStatus = moveStatus;
    switch ( curSubmode )
    {
        case SITHPUPPETSUBMODE_STAND:
        case SITHPUPPETSUBMODE_TURNLEFT:
        case SITHPUPPETSUBMODE_TURNRIGHT:
            if ( submode >= SITHPUPPETSUBMODE_WALK && submode <= SITHPUPPETSUBMODE_WALKBACK )
            {
                transitionSubmode = SITHPUPPETSUBMODE_STAND2WALK;
                transitionStatus  = SITHPLAYERMOVE_STAND2WALK;
            }
            break;

        case SITHPUPPETSUBMODE_WALK:
        case SITHPUPPETSUBMODE_RUN:
            if ( submode == SITHPUPPETSUBMODE_STAND || (submode > SITHPUPPETSUBMODE_STRAFERIGHT && submode < SITHPUPPETSUBMODE_SLIDEDOWNFWD) )
            {
                transitionSubmode = SITHPUPPETSUBMODE_WALK2STAND;
                transitionStatus  = SITHPLAYERMOVE_WALK2STAND;
            }
            break;

        case SITHPUPPETSUBMODE_FALL:
            if ( submode == SITHPUPPETSUBMODE_STAND && sithPuppet_PlayMode(pOwner, SITHPUPPETSUBMODE_LAND, sithAIMove_PuppetCallback) >= 0 )
            {
                moveStatus       = SITHPLAYERMOVE_LAND;
                transitionStatus = SITHPLAYERMOVE_LAND;
            }
            break;

        default:
            break;
    }

    if ( transitionSubmode && pOwner->pPuppetClass->aModes[majorMode][transitionSubmode].pKeyframe )
    {
        sithPuppet_ClearMode(pOwner, curSubmode);
        pfCallback = sithAIMove_PuppetCallback;
        submode    = transitionSubmode;
        moveStatus = transitionStatus;
        if ( transitionSubmode == SITHPUPPETSUBMODE_STAND2WALK )
        {
            memset(&pOwner->moveInfo.physics.velocity, 0, sizeof(pOwner->moveInfo.physics.velocity));
            sithSoundClass_PlayModeFirst(pOwner, SITHSOUNDCLASS_STAND2WALK);
        }
        else if ( transitionSubmode == SITHPUPPETSUBMODE_WALK2STAND )
        {
            sithSoundClass_PlayModeFirst(pOwner, SITHSOUNDCLASS_WALK2STAND);
        }
    }

    rdKeyframe* pKeyframe = pOwner->pPuppetClass->aModes[majorMode][submode].pKeyframe;
    if ( pKeyframe )
    {
        SithPuppetTrack* pTrack = sithPuppet_FindActiveTrack(pOwner, pKeyframe);
        if ( pTrack )
        {
            sithPuppet_SynchMode(pOwner, pTrack->submode, submode, -1.0f, 0);
        }
        else
        {
            sithPuppet_SetSubMode(pOwner, submode, pfCallback);
        }
    }

    pOwner->pPuppetState->submode = submode;
    pOwner->moveStatus = moveStatus;
    return moveStatus;
}

void J3DAPI sithAIMove_PuppetCallback(SithThing* pThing, int trackNum, rdKeyMarkerType marker)
{
    SITH_ASSERTREL(pThing);
    SithAIControlBlock* pLocal = pThing->controlInfo.aiControl.pLocal;
    if ( !pLocal )
    {
        sithPuppet_DefaultCallback(pThing, trackNum, marker);
        return;
    }

    switch ( marker )
    {
        case 0: // animation finished
        {
            sithPuppet_FreeTrackByIndex(pThing, trackNum);
            if ( (pThing->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 )
            {
                return;
            }

            switch ( pThing->moveStatus )
            {
                case SITHPLAYERMOVE_LAND:
                case SITHPLAYERMOVE_WALK2STAND:
                case SITHPLAYERMOVE_UNKNOWN_78:
                    pThing->moveStatus = SITHPLAYERMOVE_STILL;
                    break;

                case SITHPLAYERMOVE_MOUNTING_WALL:
                    sithAIMove_sub_498890(pLocal, 0x40);
                    pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                    break;

                case SITHPLAYERMOVE_ROLLING_LEFT:
                case SITHPLAYERMOVE_ROLLING_RIGHT:
                    sithAIMove_sub_498890(pLocal, 1);
                    // fallthrough

                case SITHPLAYERMOVE_STRAFING_LEFT:
                case SITHPLAYERMOVE_STRAFING_RIGHT:
                    pThing->moveStatus = SITHPLAYERMOVE_STILL;
                    sithAIMove_SetGoalReached(pLocal);
                    break;

                case SITHPLAYERMOVE_STAND2WALK:
                case SITHPLAYERMOVE_STAND2RUN:
                    pThing->moveStatus = pThing->moveStatus != SITHPLAYERMOVE_STAND2WALK ? SITHPLAYERMOVE_RUNNING : SITHPLAYERMOVE_WALKING;
                    break;

                case SITHPLAYERMOVE_KNOCKEDOUT:
                    pThing->collide.type = SITH_COLLIDE_FACE;
                    pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                    pThing->moveStatus = SITHPLAYERMOVE_STILL;
                    pThing->controlInfo.aiControl.pLocal->mode &= ~SITHAI_MODE_DISABLED;
                    pThing->thingInfo.actorInfo.flags &= ~(SITH_AF_IMMOBILE | SITH_AF_INVULNERABLE);
                    break;

                case SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES:
                case SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES:
                    sithAIMove_sub_498890(pLocal, 4);
                    pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                    break;

                case SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES:
                case SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES:
                    sithAIMove_sub_498890(pLocal, 8);
                    pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                    break;

                case SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES:
                case SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES:
                    sithAIMove_sub_498890(pLocal, 0x10);
                    pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                    break;

                case SITHPLAYERMOVE_TURNING_180_DEGREES:
                    sithAIMove_sub_498890(pLocal, 0x20);
                    pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                    break;

                default:
                    return;
            }

            rdVector3* pVelocity = &pThing->moveInfo.physics.velocity;
            if ( pThing->moveStatus == SITHPLAYERMOVE_STILL )
            {
                sithPuppet_SetSubMode(pThing, SITHPUPPETSUBMODE_STAND, NULL);
            }
            else if ( pThing->moveStatus == SITHPLAYERMOVE_WALKING )
            {
                if ( sithAIMove_IsZeroVector(pVelocity) )
                {
                    pVelocity->y = 0.06f;
                    sithPuppet_SetSubMode(pThing, SITHPUPPETSUBMODE_WALK, NULL);
                    return;
                }

                if ( rdVector_Len3(pVelocity) < 0.06f )
                {
                    rdVector_Scale3Acc(pVelocity, 1.06f);
                }

                sithPuppet_SetSubMode(pThing, SITHPUPPETSUBMODE_WALK, NULL);
            }
            else if ( pThing->moveStatus == SITHPLAYERMOVE_RUNNING )
            {
                if ( sithAIMove_IsZeroVector(pVelocity) )
                {
                    pVelocity->y = 0.27f;
                }

                if ( rdVector_Len3(pVelocity) < 0.27f )
                {
                    rdVector_Scale3Acc(pVelocity, 1.27f);
                }

                sithPuppet_SetSubMode(pThing, SITHPUPPETSUBMODE_RUN, NULL);
            }
        } break;

        case RDKEYMARKER_LEFTFOOT:
            if ( pThing->moveStatus != SITHPLAYERMOVE_STRAFING_RIGHT )
            {
                sithPuppet_DefaultCallback(pThing, trackNum, RDKEYMARKER_LEFTFOOT);
                return;
            }

            sithAIMove_sub_498890(pLocal, 2);
            break;

        case RDKEYMARKER_RIGHTFOOT:
            if ( pThing->moveStatus != SITHPLAYERMOVE_STRAFING_LEFT )
            {
                sithPuppet_DefaultCallback(pThing, trackNum, RDKEYMARKER_RIGHTFOOT);
                return;
            }

            sithAIMove_sub_498890(pLocal, 2);
            break;

        case RDKEYMARKER_DIED:
            switch ( pThing->moveStatus )
            {
                case SITHPLAYERMOVE_ROLLING_LEFT:
                case SITHPLAYERMOVE_ROLLING_RIGHT:
                    sithSoundClass_PlayModeRandom(pThing, SITHSOUNDCLASS_RESERVED3);
                    break;

                case SITHPLAYERMOVE_KNOCKEDOUT:
                    sithSoundClass_PlayModeRandom(pThing, SITHSOUNDCLASS_RESERVED1);
                    break;

                case SITHPLAYERMOVE_RUNOVER:
                    sithSoundClass_PlayModeRandom(pThing, SITHSOUNDCLASS_RESERVED2);
                    break;

                default:
                    sithPuppet_DefaultCallback(pThing, trackNum, RDKEYMARKER_DIED);
                    break;
            }
            break;

        default:
            sithPuppet_DefaultCallback(pThing, trackNum, marker);
            break;
    }
}

void J3DAPI sithAIMove_AISetLookThing(SithAIControlBlock* pLocal, const SithThing* pThing)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner && pLocal->pClass);
    SITH_ASSERTREL(pThing);

    rdVector3 lookPos;
    sithAIMove_GetThingEyePos(pThing, &lookPos);
    sithAIMove_AISetLookPos(pLocal, &lookPos);
}

void J3DAPI sithAIMove_AISetLookPos(SithAIControlBlock* pLocal, const rdVector3* targetPos)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner && pLocal->pClass);
    SITH_ASSERTREL(targetPos);

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_1) != 0 )
    {
        return;
    }

    if ( (pLocal->mode & SITHAI_MODE_DISABLED) != 0 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        SITHLOG_ERROR("WARNING!  Look target issued to '%s'. AI is DISABLED.\n", pLocal->pOwner->aName);
    }

    SithThing* pOwner = pLocal->pOwner;
    rdVector3 fromPos = pOwner->pos;
    if ( pOwner->type == SITH_THING_ACTOR || pOwner->type == SITH_THING_PLAYER )
    {
        fromPos.z = pOwner->thingInfo.actorInfo.eyeOffset.z + fromPos.z;
    }

    rdVector_Sub3(&pLocal->goalLVec, targetPos, &fromPos);
    float dist = rdVector_Normalize3Acc(&pLocal->goalLVec);
    if ( isnan(dist) || dist < 0.001f )
    {
        pLocal->goalLVec = pOwner->orient.lvec;
        dist = 0.0f;
    }

    pLocal->lookPos = *targetPos;
    if ( dist != 0.0f )
    {
        pLocal->mode |= SITHAI_MODE_TURNING;
        pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_10;
    }

    SithAIMode mode = pLocal->mode;
    pLocal->mode = mode | SITHAI_MODE_UNKNOWN_80;
    if ( (mode & SITHAI_MODE_WANTALLEVENTS) != 0 )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_UNKNOWN_40000, NULL);
    }
}

// Sets look position at the AI eye level
void J3DAPI sithAIMove_AISetLookPosEyeLevel(SithAIControlBlock* pLocal, const rdVector3* targetPos)
{
    SithThing* pOwner = pLocal->pOwner;

    rdVector3 eyePos;
    sithAIMove_GetEyePos(pOwner, &eyePos);

    rdVector3 vec;
    rdVector_Sub3(&vec, targetPos, &eyePos);
    float dist = -(vec.y * pOwner->orient.uvec.y + vec.z * pOwner->orient.uvec.z + vec.x * pOwner->orient.uvec.x);

    rdVector3 lookPos;
    rdVector_ScaleAdd3(&lookPos, &pOwner->orient.uvec, dist, targetPos);
    sithAIMove_AISetLookPos(pLocal, &lookPos);
}

int J3DAPI sithAIMove_AISetMovePos(SithAIControlBlock* pLocal, const rdVector3* moveToPos, float moveSpeed)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner && pLocal->pClass);
    SITH_ASSERTREL(moveToPos);

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_1) != 0 )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_DISABLED) != 0 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        SITHLOG_ERROR("WARNING!  Move target issued to '%s'. AI is DISABLED.\n", pLocal->pOwner->aName);
    }

    rdVector3 toPos   = *moveToPos;
    rdVector3 fromPos = pLocal->pOwner->pos;
    if ( pLocal->maxHomeDist > 0.0f )
    {
        // Limit the move pos to max home distance
        rdVector3 dir;
        rdVector_Sub3(&dir, moveToPos, &pLocal->homePos);
        if ( rdVector_Normalize3Acc(&dir) > pLocal->maxHomeDist )
        {
            rdVector_ScaleAdd3(&toPos, &dir, pLocal->maxHomeDist, &pLocal->homePos);
        }
    }

    // Note, the limited move position is used only to get the reach distance
    float reachDist = (float)sithAIMove_sub_496200(pLocal, &fromPos, &toPos);

    rdVector3 vec;
    rdVector_Sub3(&vec, moveToPos, &pLocal->pOwner->pos);
    if ( sqrtf(vec.z * vec.z + vec.y * vec.y + vec.x * vec.x) <= reachDist )
    {
        return 0; // already there
    }

    pLocal->moveSpeed  = moveSpeed;
    pLocal->movePos    = *moveToPos;
    pLocal->vecUnknown = pLocal->pOwner->pos;
    sithSoundClass_PlayModeFirst(pLocal->pOwner, SITHSOUNDCLASS_MOVING);
    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_MOVING);
    if ( (pLocal->mode & SITHAI_MODE_WANTALLEVENTS) != 0 )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_MOVE_TARGET_CHANGED, NULL);
    }

    return 1;
}

// Sets move position that can't be changed until reached
int J3DAPI sithAIMove_AISetMoveTargetPos(SithAIControlBlock* pLocal, const rdVector3* moveToPos, float moveSpeed)
{
    SITH_ASSERTREL(pLocal);
    SITH_ASSERTREL(moveToPos);

    if ( (pLocal->mode & SITHAI_MODE_DISABLED) != 0 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        SITHLOG_ERROR("WARNING!  Move target issued to '%s'. AI is DISABLED.\n", pLocal->pOwner->aName);
    }

    pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_1;
    if ( sithAIMove_AISetMovePos(pLocal, moveToPos, moveSpeed) )
    {
        pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_1;
        return 1;
    }

    return 0;
}

// Starts special move (see SITHAIMOVE_SPECIAL_* flags): roll, strafe, special turn or wall mount.
// Returns 1 if the move was started.
int J3DAPI sithAIMove_sub_497FF0(SithAIControlBlock* pLocal, int armedModeState)
{
    int flags = armedModeState;
    bool bHaveSecondAnim = false;
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;

    // Get the left & right animations
    SithPuppetSubMode leftSubmode, rightSubmode = 0;
    int bRoll = flags & SITHAIMOVE_SPECIAL_ROLL;
    if ( bRoll )
    {
        if ( rdVector_Dot3(&pOwner->orient.lvec, &pLocal->toTarget) <= 0.4f )
        {
            return 0; // not facing target
        }

        leftSubmode  = SITHAIMOVE_SUBMODE_ROLLLEFT;
        rightSubmode = SITHAIMOVE_SUBMODE_ROLLRIGHT;
    }
    else if ( (flags & SITHAIMOVE_SPECIAL_STRAFE) != 0 )
    {
        if ( rdVector_Dot3(&pOwner->orient.lvec, &pLocal->toTarget) <= 0.4f )
        {
            return 0;
        }

        leftSubmode  = SITHPUPPETSUBMODE_STRAFELEFT;
        rightSubmode = SITHPUPPETSUBMODE_STRAFERIGHT;
    }
    else if ( (flags & SITHAIMOVE_SPECIAL_TURN45) != 0 )
    {
        leftSubmode  = SITHAIMOVE_SUBMODE_TURNLEFT45;
        rightSubmode = SITHAIMOVE_SUBMODE_TURNRIGHT45;
    }
    else if ( (flags & SITHAIMOVE_SPECIAL_TURN90) != 0 )
    {
        leftSubmode  = SITHAIMOVE_SUBMODE_TURNLEFT90;
        rightSubmode = SITHAIMOVE_SUBMODE_TURNRIGHT90;
    }
    else if ( (flags & SITHAIMOVE_SPECIAL_TURN135) != 0 )
    {
        leftSubmode  = SITHAIMOVE_SUBMODE_TURNLEFT135;
        rightSubmode = SITHAIMOVE_SUBMODE_TURNRIGHT135;
    }
    else if ( (flags & SITHAIMOVE_SPECIAL_TURN180) != 0 )
    {
        leftSubmode     = SITHAIMOVE_SUBMODE_TURN180;
        bHaveSecondAnim = true;
    }
    else if ( (flags & SITHAIMOVE_SPECIAL_MOUNTWALL) != 0 )
    {
        leftSubmode     = SITHPUPPETSUBMODE_MOUNTWALL;
        bHaveSecondAnim = true;
    }
    else
    {
        return 0;
    }

    int majorMode = pOwner->pPuppetState->majorMode;
    if ( !bHaveSecondAnim )
    {
        bHaveSecondAnim = pOwner->pPuppetClass->aModes[majorMode][rightSubmode].pKeyframe != NULL;
    }

    if ( !pOwner->pPuppetClass->aModes[majorMode][leftSubmode].pKeyframe || !bHaveSecondAnim )
    {
        return 0;
    }

    int bRight;
    rdVector3 destPos;
    if ( (flags & (SITHAIMOVE_SPECIAL_ROLL | SITHAIMOVE_SPECIAL_STRAFE)) == 0 )
    {
        bRight = flags;
        if ( (flags & (SITHAIMOVE_SPECIAL_TURN45 | SITHAIMOVE_SPECIAL_TURN90 | SITHAIMOVE_SPECIAL_TURN135)) != 0 )
        {
            bRight = (flags & SITHAIMOVE_SPECIAL_LEFT) == 0;
        }
    }
    else
    {
        // Roll or strafe to the side
        float moveDist = bRoll ? 0.21f : 0.08f;
        bRight = 1;
        if ( (flags & SITHAIMOVE_SPECIAL_RANDOMSIDE) != 0 )
        {
            if ( sithAIMove_Random() < 0.5f )
            {
                bRight = 0;
            }
        }
        else if ( (flags & SITHAIMOVE_SPECIAL_LEFT) != 0 )
        {
            bRight = 0;
        }

        rdVector3 sideDir;
        if ( bRight )
        {
            sideDir = pOwner->orient.rvec;
        }
        else
        {
            rdVector_Neg3(&sideDir, &pOwner->orient.rvec);
        }

        rdVector3 halfPos;
        rdVector_ScaleAdd3(&halfPos, &sideDir, moveDist * 0.5f, &pOwner->pos);
        rdVector_ScaleAdd3(&destPos, &sideDir, moveDist, &pOwner->pos);

        float hitDist;
        rdVector3 hitNorm;
        int numTries = 0;
        int bBlocked = sithAIUtil_CheckPathToPoint(pOwner, &destPos, pOwner->collide.movesize, &hitDist, &hitNorm, 1, 0);
        while ( bBlocked )
        {
            if ( ++numTries > 1 )
            {
                return 0;
            }

            // Try the other side
            // Note, the original doesn't recalculate the destination position here
            rdVector_Neg3Acc(&sideDir);
            bRight   = 1 - bRight;
            bBlocked = sithAIUtil_CheckPathToPoint(pOwner, &destPos, pOwner->collide.movesize, &hitDist, &hitNorm, 1, 0);
        }

        if ( bRoll && sithAIUtil_sub_49DC90(pLocal, &halfPos, NULL) != 1 )
        {
            return 0;
        }

        if ( sithAIUtil_sub_49DC90(pLocal, &destPos, NULL) != 1 )
        {
            return 0;
        }
    }

    if ( (flags & SITHAIMOVE_SPECIAL_STRAFE) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        pLocal->pOwner->moveStatus = bRight ? SITHPLAYERMOVE_STRAFING_RIGHT : SITHPLAYERMOVE_STRAFING_LEFT;
        sithPuppet_SetSubMode(pLocal->pOwner, bRight ? SITHPUPPETSUBMODE_STRAFERIGHT : SITHPUPPETSUBMODE_STRAFELEFT, sithAIMove_PuppetCallback);
        if ( pLocal->pTargetThing )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
        }

        sithAIMove_AISetMoveTargetPos(pLocal, &destPos, 1.0f);
        return 1;
    }

    if ( bRoll )
    {
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_ResetAILook(pLocal);
        pLocal->pOwner->moveStatus = bRight ? SITHPLAYERMOVE_ROLLING_RIGHT : SITHPLAYERMOVE_ROLLING_LEFT;
        sithPuppet_SetSubMode(pLocal->pOwner, bRight ? SITHAIMOVE_SUBMODE_ROLLRIGHT : SITHAIMOVE_SUBMODE_ROLLLEFT, sithAIMove_PuppetCallback);
        sithAIMove_AISetMoveTargetPos(pLocal, &destPos, 2.0f);
        return 1;
    }

    if ( (flags & SITHAIMOVE_SPECIAL_TURN45) != 0 )
    {
        pLocal->pOwner->moveStatus = bRight ? SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES : SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES;
        sithPuppet_SetSubMode(pLocal->pOwner, bRight ? SITHAIMOVE_SUBMODE_TURNRIGHT45 : SITHAIMOVE_SUBMODE_TURNLEFT45, sithAIMove_PuppetCallback);
        return 1;
    }

    if ( (flags & SITHAIMOVE_SPECIAL_TURN90) != 0 )
    {
        pLocal->pOwner->moveStatus = bRight ? SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES : SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES;
        sithPuppet_SetSubMode(pLocal->pOwner, bRight ? SITHAIMOVE_SUBMODE_TURNRIGHT90 : SITHAIMOVE_SUBMODE_TURNLEFT90, sithAIMove_PuppetCallback);
        return 1;
    }

    if ( (flags & SITHAIMOVE_SPECIAL_TURN135) != 0 )
    {
        pLocal->pOwner->moveStatus = bRight ? SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES : SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES;
        sithPuppet_SetSubMode(pLocal->pOwner, bRight ? SITHAIMOVE_SUBMODE_TURNRIGHT135 : SITHAIMOVE_SUBMODE_TURNLEFT135, sithAIMove_PuppetCallback);
        return 1;
    }

    if ( (flags & SITHAIMOVE_SPECIAL_TURN180) != 0 )
    {
        pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_180_DEGREES;
        sithPuppet_SetSubMode(pLocal->pOwner, SITHAIMOVE_SUBMODE_TURN180, sithAIMove_PuppetCallback);
        return 1;
    }

    if ( (flags & SITHAIMOVE_SPECIAL_MOUNTWALL) == 0 )
    {
        return 0;
    }

    // Mount wall with normal vecUnknown3
    float angle = rdMath_DeltaAngleNormalized(&pLocal->vecUnknown3, &pOwner->orient.uvec, &pOwner->orient.lvec);
    float absAngle = fabsf(angle);
    if ( absAngle >= 80.0f && absAngle <= 100.0f )
    {
        pLocal->pOwner->moveStatus = SITHPLAYERMOVE_MOUNTING_WALL;
        sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_MOUNTWALL, sithAIMove_PuppetCallback);
        return 1;
    }

    if ( absAngle >= 60.0f && absAngle <= 79.0f )
    {
        // Tilt up vector away from the wall before mounting
        rdVector3 up;
        rdVector_ScaleAdd3(&up, &pLocal->vecUnknown3, -0.2f, &pOwner->orient.uvec);
        rdVector_Normalize3Acc(&up);

        // Note, the original also calculates the projected look vector here, but doesn't use it

        pOwner->orient.uvec = up;
        rdVector_Cross3(&pOwner->orient.rvec, &pOwner->orient.lvec, &pOwner->orient.uvec);
        rdVector_Normalize3Acc(&pOwner->orient.rvec);
        rdVector_Cross3(&pOwner->orient.lvec, &pOwner->orient.uvec, &pOwner->orient.rvec);

        pLocal->pOwner->moveStatus = SITHPLAYERMOVE_MOUNTING_WALL;
        sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_MOUNTWALL, sithAIMove_PuppetCallback);
        return 1;
    }

    if ( absAngle <= 40.0f )
    {
        return 0;
    }

    sithAIMove_sub_498890(pLocal, SITHAIMOVE_SPECIAL_MOUNTWALL);
    return 1;
}

// Finishes special move
void J3DAPI sithAIMove_sub_498890(SithAIControlBlock* pLocal, int type)
{
    SithThing* pOwner = pLocal->pOwner;
    switch ( type )
    {
        case SITHAIMOVE_SPECIAL_STRAFE:
            sithAIMove_StopAIMovement(pLocal);
            sithAIUtil_ApplyForce(pLocal, &pLocal->moveDirection, -0.05f);
            break;

        case SITHAIMOVE_SPECIAL_TURN45:
        case SITHAIMOVE_SPECIAL_TURN90:
        case SITHAIMOVE_SPECIAL_TURN135:
        case SITHAIMOVE_SPECIAL_TURN180:
        {
            // Turn by the special turn angle
            rdVector3 pyr = { 0 }; // Added: Init
            switch ( pOwner->moveStatus )
            {
                case SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES:
                    pyr.y = 45.0f;
                    break;

                case SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES:
                    pyr.y = -45.0f;
                    break;

                case SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES:
                    pyr.y = 90.0f;
                    break;

                case SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES:
                    pyr.y = -90.0f;
                    break;

                case SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES:
                    pyr.y = 135.0f;
                    break;

                case SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES:
                    pyr.y = -135.0f;
                    break;

                case SITHPLAYERMOVE_TURNING_180_DEGREES:
                    pyr.y = 180.0f;
                    break;

                default:
                    break;
            }

            rdMatrix_PreRotate34(&pOwner->orient, &pyr);
            rdMatrix_Normalize34(&pOwner->orient);
        } break;

        case SITHAIMOVE_SPECIAL_MOUNTWALL:
            // Stick to the wall
            sithThing_DetachThing(pOwner);
            pOwner->orient.uvec = pLocal->vecUnknown3;
            rdVector_Cross3(&pOwner->orient.lvec, &pOwner->orient.uvec, &pOwner->orient.rvec);
            rdVector_Normalize3Acc(&pOwner->orient.lvec);

            sithPhysics_FindFloor(pOwner, 1);
            if ( pOwner->moveStatus == SITHPLAYERMOVE_MOUNTING_WALL )
            {
                sithCollision_MoveThing(pOwner, &pOwner->orient.lvec, pOwner->collide.movesize, 0);
            }
            break;

        default:
            break;
    }
}

void J3DAPI sithAIMove_AIJump(SithAIControlBlock* pLocal, rdVector3* movePos, float a3)
{
    pLocal->movePos   = *movePos;
    pLocal->moveSpeed = 2.0f;
    if ( sithPuppet_PlayMode(pLocal->pOwner, SITHPUPPETSUBMODE_JUMPUP, NULL) < 0 )
    {
        sithPlayerActions_Jump(pLocal->pOwner, a3, 0);
    }

    pLocal->msecPauseMoveUntil = sithTime_g_msecGameTime + 2000;
    pLocal->mode |= SITHAI_MODE_MOVING;
}

void J3DAPI sithAIMove_AIStop(SithAIControlBlock* pLocal)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    sithAIMove_StopAIMovement(pLocal);
    sithAIMove_ResetAILook(pLocal);

    pLocal->movePos = pLocal->pOwner->pos;
    sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_UNREACHABLE, NULL);
    sithAIMove_UpdateAIMove(pLocal);
}

void J3DAPI sithAIMove_SetGoalReached(SithAIControlBlock* pLocal)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_REACHED, NULL);
    if ( (pLocal->mode & SITHAI_MODE_BLOCK) != 0 )
    {
        sithCog_ThingSendMessage(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVED);
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_4000) == 0 && (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) == 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        if ( sithAIMove_AIGetMoveState(pLocal) == 0 )
        {
            sithAIMove_UpdateAIMove(pLocal);
        }
    }
}

void J3DAPI sithAIMove_Unreachable(SithAIControlBlock* pLocal)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    if ( (pLocal->mode & SITHAI_MODE_BLOCK) != 0 )
    {
        sithCog_ThingSendMessage(pLocal->pOwner, NULL, SITHCOG_MSG_BLOCKED);
        return;
    }

    sithAIMove_StopAIMovement(pLocal);
    sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_UNREACHABLE, NULL);
    sithAIMove_UpdateAIMove(pLocal);
}

void J3DAPI sithAIMove_StopAIMovement(SithAIControlBlock* pLocal)
{
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_MOVING);
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
    if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSMOTION) != 0 )
    {
        pLocal->mode |= SITHAI_MODE_MOVING;
        return;
    }

    sithSoundClass_StopMode(pLocal->pOwner, SITHSOUNDCLASS_MOVING);
    memset(&pLocal->pOwner->moveInfo.physics.velocity, 0, sizeof(pLocal->pOwner->moveInfo.physics.velocity));
}

void J3DAPI sithAIMove_ResetAILook(SithAIControlBlock* pLocal)
{
    sithActor_SetHeadPYR(pLocal->pOwner, &rdroid_g_zeroVector3);
    pLocal->goalLVec = pLocal->pOwner->orient.lvec;
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TURNING);
}

// Turns boss AI towards the goal look vector
void J3DAPI sithAIMove_UpdateBoss(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner  = pLocal->pOwner;
    rdVector3 goalLVec = pLocal->goalLVec;
    if ( fabsf(rdVector_Dot3(&pOwner->orient.uvec, &goalLVec)) > 0.99f )
    {
        pLocal->mode &= ~SITHAI_MODE_TURNING;
        SITHLOG_ERROR("WARNING! AI '%s' looking straight up or down.\n", pOwner->aName);
        return;
    }

    rdVector3 lookDir;
    sithAIMove_ProjectOnPlane(&lookDir, &goalLVec, &pOwner->orient.uvec);
    rdVector_Normalize3Acc(&lookDir);

    float angle = rdMath_DeltaAngleNormalized(&lookDir, &pOwner->orient.lvec, &pOwner->orient.uvec);
    float step  = pOwner->thingInfo.actorInfo.maxRotVelocity * secDeltaTime;
    if ( fabsf(angle) <= 1.05f * step )
    {
        sithAIMove_SetOrientLook(&pOwner->orient, &lookDir);
        pLocal->mode &= ~SITHAI_MODE_TURNING;
        return;
    }

    rdVector3 pyr = { 0.0f, -(sithAIMove_Sign(angle) * step), 0.0f };
    rdMatrix_PreRotate34(&pOwner->orient, &pyr);
    rdMatrix_Normalize34(&pOwner->orient);
}

// Turns AI head towards target (or look position)
void J3DAPI sithAIMove_sub_499090(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    SithThing* pOwner  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;
    rdVector3 headPYR  = pOwner->thingInfo.actorInfo.headPYR;

    SithActorFlag flags = pOwner->thingInfo.actorInfo.flags;
    if ( (flags & SITH_AF_IMMOBILE) != 0 || (flags & SITH_AF_CANROTATEHEAD) == 0 )
    {
        return;
    }

    rdVector3 eyePos;
    sithAIMove_GetEyePos(pOwner, &eyePos);

    if ( pTarget && (pOwner->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) == 0 )
    {
        if ( (pTarget->type == SITH_THING_ACTOR || pTarget->type == SITH_THING_PLAYER) && (pTarget->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            pTarget = NULL;
        }

        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
        {
            pTarget = NULL;
        }
    }

    rdVector3 toTarget, toTargetFlat;
    if ( pTarget )
    {
        rdVector3 targetPos;
        sithAIMove_GetThingEyePos(pTarget, &targetPos);

        rdVector_Sub3(&toTarget, &targetPos, &eyePos);
        rdVector_Normalize3Acc(&toTarget);
        sithAIMove_ProjectOnPlane(&toTargetFlat, &toTarget, &pOwner->orient.uvec);
        rdVector_Normalize3Acc(&toTargetFlat);
    }

    rdVector3 toLook, toLookFlat;
    rdVector_Sub3(&toLook, &pLocal->lookPos, &eyePos);
    rdVector_Normalize3Acc(&toLook);
    sithAIMove_ProjectOnPlane(&toLookFlat, &toLook, &pOwner->orient.uvec);
    rdVector_Normalize3Acc(&toLookFlat);

    // Decide whether to track target or look position
    float minDot = -0.2f;
    if ( !pTarget )
    {
        pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_800;
    }
    else if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_NOSLOPEMOVE) == 0 && (pLocal->mode & SITHAI_MODE_BLOCK) == 0 )
    {
        float targetDot = rdVector_Dot3(&pOwner->orient.lvec, &toTargetFlat);
        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_800) == 0 )
        {
            if ( targetDot > sithAIMove_g_flt_58546C )
            {
                pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_800;
            }
        }
        else if ( targetDot < -0.2f )
        {
            pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_800;
        }
    }
    else
    {
        minDot = -1.0f;
        pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_800;
    }

    rdVector3 lookFlat, lookDir;
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_800) != 0 )
    {
        lookFlat = toTargetFlat;
        lookDir  = toTarget;
    }
    else
    {
        lookFlat = toLookFlat;
        lookDir  = toLook;
    }

    float fwdDot    = rdVector_Dot3(&pOwner->orient.lvec, &lookFlat);
    float yawStep   = pOwner->thingInfo.actorInfo.maxHeadVelocity * secDeltaTime;
    float pitchStep = pOwner->thingInfo.actorInfo.maxHeadVelocity * 0.5f * secDeltaTime;

    // Yaw
    float yaw = fwdDot < minDot ? 0.0f : rdMath_DeltaAngleNormalized(&pOwner->orient.lvec, &lookFlat, &pOwner->orient.uvec);
    yaw = yaw - pOwner->thingInfo.actorInfo.headPYR.y;
    yaw = STDMATH_CLAMP(yaw, -yawStep, yawStep);

    headPYR.y = yaw + headPYR.y;
    float maxHeadYaw = pOwner->thingInfo.actorInfo.maxHeadYaw;
    if ( headPYR.y <= -maxHeadYaw || headPYR.y >= maxHeadYaw )
    {
        headPYR.y = sithAIMove_Sign(headPYR.y) * maxHeadYaw;
        pLocal->submode &= ~SITHAI_SUBMODE_HEADTRACKINGMOTION;
    }
    else
    {
        pLocal->submode |= SITHAI_SUBMODE_HEADTRACKINGMOTION;
    }

    // Pitch
    float pitch = fwdDot <= 0.0f ? 0.0f : rdMath_DeltaAngleNormalized(&lookFlat, &lookDir, &pOwner->orient.rvec);
    pitch = pitch - pOwner->thingInfo.actorInfo.headPYR.x;
    pitch = STDMATH_CLAMP(pitch, -pitchStep, pitchStep);
    pitch = pitch + headPYR.x;

    if ( pitch < pOwner->thingInfo.actorInfo.minHeadPitch )
    {
        headPYR.x = pOwner->thingInfo.actorInfo.minHeadPitch;
    }
    else if ( pitch > pOwner->thingInfo.actorInfo.maxHeadPitch )
    {
        headPYR.x = pOwner->thingInfo.actorInfo.maxHeadPitch;
    }
    else
    {
        headPYR.x = pitch;
    }

    sithActor_SetHeadPYR(pLocal->pOwner, &headPYR);
}

// Turns AI head towards the look position
void J3DAPI sithAIMove_sub_4996C0(SithAIControlBlock* pLocal, float a2)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    SithThing* pOwner  = pLocal->pOwner;
    rdVector3 headPYR  = pOwner->thingInfo.actorInfo.headPYR;
    rdVector3 lookPos  = pLocal->lookPos;
    if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_CANROTATEHEAD) == 0 || (pOwner->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0 )
    {
        return;
    }

    rdVector3 eyePos;
    sithAIMove_GetEyePos(pOwner, &eyePos);

    rdVector3 toLook, toLookFlat;
    rdVector_Sub3(&toLook, &lookPos, &eyePos);
    rdVector_Normalize3Acc(&toLook);
    sithAIMove_ProjectOnPlane(&toLookFlat, &toLook, &pOwner->orient.uvec);
    rdVector_Normalize3Acc(&toLookFlat);

    float yawStep   = pOwner->thingInfo.actorInfo.maxHeadVelocity * a2;
    float pitchStep = pOwner->thingInfo.actorInfo.maxHeadVelocity * 0.5f * a2;

    float yaw = rdMath_DeltaAngleNormalized(&pOwner->orient.lvec, &toLookFlat, &pOwner->orient.uvec) - pOwner->thingInfo.actorInfo.headPYR.y;
    if ( yaw < -yawStep )
    {
        yaw = -yawStep;
    }
    else if ( yaw > yawStep )
    {
        yaw = yawStep;
    }

    float headYaw    = yaw + headPYR.y;
    float maxHeadYaw = pOwner->thingInfo.actorInfo.maxHeadYaw;
    headPYR.y = headYaw >= -maxHeadYaw ? (headYaw > maxHeadYaw ? maxHeadYaw : headYaw) : -maxHeadYaw;

    float pitch = rdVector_Dot3(&pOwner->orient.lvec, &toLookFlat) <= 0.0f ? 0.0f : rdMath_DeltaAngleNormalized(&toLookFlat, &toLook, &pOwner->orient.rvec);
    pitch = pitch - pOwner->thingInfo.actorInfo.headPYR.x;
    if ( pitch < -pitchStep )
    {
        pitch = -pitchStep;
    }
    else if ( pitch > pitchStep )
    {
        pitch = pitchStep;
    }

    headPYR.x = pitch + headPYR.x;
    if ( headPYR.x < pOwner->thingInfo.actorInfo.minHeadPitch )
    {
        headPYR.x = pOwner->thingInfo.actorInfo.minHeadPitch;
    }
    else if ( headPYR.x > pOwner->thingInfo.actorInfo.maxHeadPitch )
    {
        headPYR.x = pOwner->thingInfo.actorInfo.maxHeadPitch;
    }

    sithActor_SetHeadPYR(pLocal->pOwner, &headPYR);

    // Head is aligned
    if ( fabsf(yaw) <= 0.5f && fabsf(pitch) <= 0.5f )
    {
        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_80;
    }
}

// Gets Quetzalcoatl strike aim angles towards target eyes: yaw (pDestAngle), head pitch (a3), neck yaw (a4) and neck pitch (a5) offsets
void J3DAPI sithAIMove_sub_499A80(SithAIControlBlock* pLocal, float* pDestAngle, float* a3, float* a4, float* a5)
{
    SithThing* pOwner  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;

    rdVector3 eyePos = pOwner->pos;
    eyePos.z = pOwner->thingInfo.actorInfo.eyeOffset.z + pOwner->pos.z;

    rdVector3 targetEyePos;
    rdMatrix_TransformVector34(&targetEyePos, &pTarget->thingInfo.actorInfo.eyeOffset, &pTarget->orient);
    rdVector_Add3Acc(&targetEyePos, &pTarget->pos);
    if ( (pTarget->moveInfo.physics.flags & SITH_PF_CROUCHING) != 0 )
    {
        targetEyePos.z = targetEyePos.z - 0.1f;
    }

    rdVector3 dir;
    rdVector_Sub3(&dir, &targetEyePos, &eyePos);
    rdVector_Normalize3Acc(&dir);

    rdVector3 dirFlat;
    sithAIMove_ProjectOnPlane(&dirFlat, &dir, &pOwner->orient.uvec);
    rdVector_Normalize3Acc(&dirFlat);

    rdVector3* aTweak = pOwner->renderData.apTweakedAngles;
    *pDestAngle = rdMath_DeltaAngleNormalized(&pOwner->orient.lvec, &dirFlat, &pOwner->orient.uvec);
    *a4 = -aTweak[2].y;

    float dx   = pOwner->pos.x - pTarget->pos.x;
    float dy   = pOwner->pos.y - pTarget->pos.y;
    float dist = sqrtf(dy * dy + dx * dx);
    dist = dist >= 0.3f ? (dist > 0.5f ? 0.5f : dist) : 0.3f;

    *a3 = (dist - 0.3f) * ((-60.0f - -15.0f) / (0.5f - 0.3f)) + -15.0f;
    *a5 = ((dist - 0.3f) * ((60.0f - sithAIMove_g_flt_585470) / (0.5f - 0.3f)) + sithAIMove_g_flt_585470) - aTweak[2].x;
}

// Advances Quetzalcoatl strike animation by secDeltaTime interpolating between snapshots.
// Returns 1 if the next snapshot was reached and there's time remaining (in pSecDeltaTime) to process.
static int sithAIMove_UpdateStrikeSnapShot(SithThing* pOwner, SithQuetzStrike* pStrike, float* pSecDeltaTime, float secRemaining)
{
    rdVector3* aTweak = pOwner->renderData.apTweakedAngles;
    SithQuetzSnapShot* pCur = &pStrike->aSnapShots[pStrike->snapShot];
    if ( *pSecDeltaTime <= secRemaining )
    {
        SITH_ASSERTREL(pStrike->snapShot != 0);
        pStrike->unknown1 = *pSecDeltaTime + pStrike->unknown1;

        SithQuetzSnapShot* pPrev = &pStrike->aSnapShots[pStrike->snapShot - 1];
        float t = (pStrike->unknown1 - pPrev->unknown0) / (pCur->unknown0 - pPrev->unknown0);
        t = t >= 0.0f ? (t > 1.0f ? 1.0f : t) : 0.0f;

        aTweak[1].x    = (pCur->angle1 - pPrev->angle1) * t + pPrev->angle1;
        aTweak[1].y    = (pCur->angle2 - pPrev->angle2) * t + pPrev->angle2;
        aTweak[2].x    = (pCur->angle3 - pPrev->angle3) * t + pPrev->angle3;
        aTweak[2].y    = (pCur->angle4 - pPrev->angle4) * t + pPrev->angle4;
        aTweak[0x1C].x = (pCur->angle5 - pPrev->angle5) * t + pPrev->angle5;
        return 0;
    }

    // Snapshot reached
    pStrike->snapShot++;
    pStrike->unknown1 = pCur->unknown0;
    aTweak[1].x    = pCur->angle1;
    aTweak[1].y    = pCur->angle2;
    aTweak[2].x    = pCur->angle3;
    aTweak[2].y    = pCur->angle4;
    aTweak[0x1C].x = pCur->angle5;
    if ( pStrike->snapShot == pStrike->unknown3 )
    {
        pStrike->unknown0 = 0; // strike finished
        return 0;
    }

    *pSecDeltaTime = *pSecDeltaTime - secRemaining;
    return 1;
}

// Sets the last snapshot to the current neck angles
static void sithAIMove_SetStrikeLastSnapShot(const SithThing* pOwner, SithQuetzStrike* pStrike)
{
    if ( pStrike->snapShot == pStrike->unknown3 - 1 )
    {
        pStrike->aSnapShots[pStrike->snapShot].angle3 = pOwner->renderData.apTweakedAngles[2].x;
        pStrike->aSnapShots[pStrike->snapShot].angle4 = pOwner->renderData.apTweakedAngles[2].y;
    }
}

// Updates Quetzalcoatl strike 1 animation aiming at target
void J3DAPI sithAIMove_sub_499CA0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    while ( sithThing_CreateQuetzUserBlock(pOwner) )
    {
        SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
        if ( pStrike->unknown0 != 1 )
        {
            return;
        }

        int snapShot = pStrike->snapShot;
        float secRemaining = pStrike->aSnapShots[snapShot].unknown0 - pStrike->unknown1;
        SithQuetzSnapShot* pCur = &pStrike->aSnapShots[snapShot];
        const rdVector3* aTweak = pOwner->renderData.apTweakedAngles;

        float yaw, headPitch, neckYaw, neckPitch;
        switch ( snapShot )
        {
            case 1:
                sithAIMove_sub_499A80(pLocal, &yaw, &headPitch, &neckYaw, &neckPitch);
                pCur->angle1 = 20.0f;
                pCur->angle2 = yaw * 0.5f;
                pCur->angle3 = aTweak[2].x - 20.0f;
                pCur->angle5 = pStrike->aSnapShots[0].angle5;
                pCur->angle4 = neckYaw * 0.5f + aTweak[2].y;
                break;

            case 2:
                sithAIMove_sub_499A80(pLocal, &yaw, &headPitch, &neckYaw, &neckPitch);
                pCur->angle1 = headPitch * 0.9f;
                pCur->angle2 = yaw * 0.9f;
                pCur->angle3 = neckPitch * 0.9f + aTweak[2].x;
                pCur->angle5 = -30.0f;
                pCur->angle4 = neckYaw * 0.9f + aTweak[2].y;
                break;

            case 3:
                sithAIMove_sub_499A80(pLocal, &yaw, &headPitch, &neckYaw, &neckPitch);
                pCur->angle1 = headPitch;
                pCur->angle2 = yaw;
                pCur->angle3 = aTweak[2].x + neckPitch;
                pCur->angle5 = 20.0f;
                pCur->angle4 = aTweak[2].y + neckYaw;
                break;

            case 4:
                sithAIMove_sub_499A80(pLocal, &yaw, &headPitch, &neckYaw, &neckPitch);
                pCur->angle1 = 10.0f;
                pCur->angle2 = yaw * 0.4f;
                pCur->angle3 = aTweak[2].x - 10.0f;
                pCur->angle5 = pStrike->aSnapShots[0].angle5;
                pCur->angle4 = neckYaw * 0.4f + aTweak[2].y;
                break;

            default:
                sithAIMove_SetStrikeLastSnapShot(pOwner, pStrike);
                break;
        }

        if ( !sithAIMove_UpdateStrikeSnapShot(pOwner, pStrike, &secDeltaTime, secRemaining) )
        {
            return;
        }
    }
}

// Gets Quetzalcoatl strike 2 aim angles towards target eyes
void J3DAPI sithAIMove_sub_49A020(SithAIControlBlock* pLocal, float* angle, float* a3, float* a4, float* a5)
{
    SithThing* pOwner  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;

    rdVector3 eyePos = pOwner->pos;
    eyePos.z = pOwner->thingInfo.actorInfo.eyeOffset.z + pOwner->pos.z;

    rdVector3 targetEyePos;
    rdMatrix_TransformVector34(&targetEyePos, &pTarget->thingInfo.actorInfo.eyeOffset, &pTarget->orient);
    rdVector_Add3Acc(&targetEyePos, &pTarget->pos);

    rdVector3 dir;
    rdVector_Sub3(&dir, &targetEyePos, &eyePos);
    rdVector_Normalize3Acc(&dir);

    rdVector3 dirFlat;
    sithAIMove_ProjectOnPlane(&dirFlat, &dir, &pOwner->orient.uvec);
    rdVector_Normalize3Acc(&dirFlat);

    *angle = rdMath_DeltaAngleNormalized(&pOwner->orient.lvec, &dirFlat, &pOwner->orient.uvec);
    *a4    = -pOwner->renderData.apTweakedAngles[2].y;
    *a5    = 50.0f;
    *a3    = -30.0f;
}

// Updates Quetzalcoatl strike 2 animation aiming at target
void J3DAPI sithAIMove_sub_49A1B0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    while ( sithThing_CreateQuetzUserBlock(pOwner) )
    {
        SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
        if ( pStrike->unknown0 != 2 )
        {
            return;
        }

        int snapShot = pStrike->snapShot;
        float secRemaining = pStrike->aSnapShots[snapShot].unknown0 - pStrike->unknown1;
        SithQuetzSnapShot* pCur = &pStrike->aSnapShots[snapShot];
        const rdVector3* aTweak = pOwner->renderData.apTweakedAngles;

        float yaw, headPitch, neckYaw, neckPitch;
        if ( snapShot == 1 )
        {
            sithAIMove_sub_49A020(pLocal, &yaw, &headPitch, &neckYaw, &neckPitch);
            pCur->angle1 = 20.0f;
            pCur->angle2 = yaw * 0.5f;
            pCur->angle3 = aTweak[2].x - -10.0f;
            pCur->angle5 = pStrike->aSnapShots[0].angle5;
            pCur->angle4 = neckYaw * 0.5f + aTweak[2].y;
        }
        else if ( snapShot == 2 )
        {
            sithAIMove_sub_49A020(pLocal, &yaw, &headPitch, &neckYaw, &neckPitch);
            pCur->angle1 = headPitch;
            pCur->angle2 = yaw;
            pCur->angle3 = aTweak[2].x + neckPitch;
            pCur->angle5 = -70.0f;
            pCur->angle4 = aTweak[2].y + neckYaw;
        }
        else
        {
            sithAIMove_SetStrikeLastSnapShot(pOwner, pStrike);
        }

        if ( !sithAIMove_UpdateStrikeSnapShot(pOwner, pStrike, &secDeltaTime, secRemaining) )
        {
            return;
        }
    }
}

// Updates Quetzalcoatl strike 3 animation
void J3DAPI sithAIMove_sub_49A450(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    while ( sithThing_CreateQuetzUserBlock(pOwner) )
    {
        SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
        if ( pStrike->unknown0 != 3 )
        {
            return;
        }

        float secRemaining = pStrike->aSnapShots[pStrike->snapShot].unknown0 - pStrike->unknown1;
        sithAIMove_SetStrikeLastSnapShot(pOwner, pStrike);
        if ( !sithAIMove_UpdateStrikeSnapShot(pOwner, pStrike, &secDeltaTime, secRemaining) )
        {
            return;
        }

        pOwner = pLocal->pOwner;
    }
}

// Updates Quetzalcoatl strike 5 animation
void J3DAPI sithAIMove_sub_49A630(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    while ( sithThing_CreateQuetzUserBlock(pOwner) )
    {
        SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
        if ( pStrike->unknown0 != 5 )
        {
            return;
        }

        float secRemaining = pStrike->aSnapShots[pStrike->snapShot].unknown0 - pStrike->unknown1;
        sithAIMove_SetStrikeLastSnapShot(pOwner, pStrike);
        if ( !sithAIMove_UpdateStrikeSnapShot(pOwner, pStrike, &secDeltaTime, secRemaining) )
        {
            return;
        }

        pOwner = pLocal->pOwner;
    }
}

// Updates Quetzalcoatl strike 4 animation
void J3DAPI sithAIMove_sub_49A810(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    while ( sithThing_CreateQuetzUserBlock(pOwner) )
    {
        SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
        if ( pStrike->unknown0 != 4 )
        {
            return;
        }

        float secRemaining = pStrike->aSnapShots[pStrike->snapShot].unknown0 - pStrike->unknown1;
        if ( !sithAIMove_UpdateStrikeSnapShot(pOwner, pStrike, &secDeltaTime, secRemaining) )
        {
            return;
        }

        pOwner = pLocal->pOwner;
    }
}

void J3DAPI sithAIMove_UpdateQuetzTail(SithAIControlBlock* pLocal, float secDeltaTime)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return;
    }

    switch ( pOwner->userblock.pQuetz->strike.unknown0 )
    {
        case 1:
            sithAIMove_sub_499CA0(pLocal, secDeltaTime);
            break;

        case 2:
            sithAIMove_sub_49A1B0(pLocal, secDeltaTime);
            break;

        case 3:
            sithAIMove_sub_49A450(pLocal, secDeltaTime);
            break;

        case 4:
            sithAIMove_sub_49A810(pLocal, secDeltaTime);
            break;

        case 5:
            sithAIMove_sub_49A630(pLocal, secDeltaTime);
            break;

        default:
            break;
    }
}

// Sets the first snapshot to the current head and neck angles
static void sithAIMove_SetStrikeSnapShot(const SithThing* pOwner, SithQuetzSnapShot* pSnapShot)
{
    const rdVector3* aTweak = pOwner->renderData.apTweakedAngles;
    pSnapShot->angle1 = aTweak[1].x;
    pSnapShot->angle2 = aTweak[1].y;
    pSnapShot->angle3 = aTweak[2].x;
    pSnapShot->angle4 = aTweak[2].y;
    pSnapShot->angle5 = aTweak[0x1C].x;
}

static void sithAIMove_SetStrikeSnapShotAngles(SithQuetzSnapShot* pSnapShot, float time, float angle1, float angle2, float angle3, float angle4, float angle5)
{
    pSnapShot->unknown0 = time;
    pSnapShot->angle1   = angle1;
    pSnapShot->angle2   = angle2;
    pSnapShot->angle3   = angle3;
    pSnapShot->angle4   = angle4;
    pSnapShot->angle5   = angle5;
}

// Starts Quetzalcoatl strike 1 (snapshots 1 - 4 are aimed at target during the strike)
void J3DAPI sithAIMove_sub_49AA60(SithAIControlBlock* pLocal)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;

    // Note, the aim angles are not used here
    float yaw, headPitch, neckYaw, neckPitch;
    sithAIMove_sub_499A80(pLocal, &yaw, &headPitch, &neckYaw, &neckPitch);

    pStrike->unknown1 = 0.0f;
    pStrike->snapShot = 0;
    pStrike->aSnapShots[0].unknown0 = 0.0f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[0]);
    pStrike->aSnapShots[1].unknown0 = 0.11f;
    pStrike->aSnapShots[2].unknown0 = 0.24f;
    pStrike->aSnapShots[3].unknown0 = 0.3f;
    pStrike->aSnapShots[4].unknown0 = 0.5f;
    pStrike->aSnapShots[5].unknown0 = 0.7f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[5]);
    pStrike->unknown0 = 1;
    pStrike->unknown3 = 6;
}

// Starts Quetzalcoatl strike 2 (snapshots 1 - 2 are aimed at target during the strike)
void J3DAPI sithAIMove_sub_49AB80(SithAIControlBlock* pLocal)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
    pStrike->unknown1 = 0.0f;
    pStrike->snapShot = 0;
    pStrike->aSnapShots[0].unknown0 = 0.0f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[0]);
    pStrike->aSnapShots[1].unknown0 = 0.11f;
    pStrike->aSnapShots[2].unknown0 = 0.24f;
    pStrike->aSnapShots[3].unknown0 = 0.5f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[3]);
    pStrike->unknown0 = 2;
    pStrike->unknown3 = 4;
}

// Starts Quetzalcoatl strike 3 (head shaking)
void J3DAPI sithAIMove_sub_49AC50(SithAIControlBlock* pLocal)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
    pStrike->unknown1 = 0.0f;
    pStrike->snapShot = 0;
    pStrike->aSnapShots[0].unknown0 = 0.0f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[0]);

    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[1], 0.1f, -10.0f, 0.0f, 50.0f, 0.0f, -40.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[2], 0.3f, -15.0f, 30.0f, 60.0f, 50.0f, -60.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[3], 0.6f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[4], 0.9f, -15.0f, 30.0f, 60.0f, 50.0f, -35.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[5], 1.2f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[6], 1.5f, -15.0f, 30.0f, 60.0f, 50.0f, -60.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[7], 1.8f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[8], 2.1f, -15.0f, 30.0f, 60.0f, 50.0f, -35.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[9], 2.4f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[10], 2.7f, -15.0f, 30.0f, 60.0f, 50.0f, -60.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[11], 3.0f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[12], 3.3f, -10.0f, 0.0f, 20.0f, 0.0f, -15.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[13], 3.7f, 0.0f, 0.0f, 5.0f, 0.0f, -10.0f);

    pStrike->aSnapShots[14].unknown0 = 5.0f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[14]);
    pStrike->unknown0 = 3;
    pStrike->unknown3 = 15;
}

// Starts Quetzalcoatl strike 5 (shorter head shaking)
void J3DAPI sithAIMove_sub_49AF80(SithAIControlBlock* pLocal)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
    pStrike->unknown1 = 0.0f;
    pStrike->snapShot = 0;
    pStrike->aSnapShots[0].unknown0 = 0.0f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[0]);

    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[1], 0.1f, -10.0f, 0.0f, 50.0f, 0.0f, -40.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[2], 0.3f, -15.0f, 30.0f, 60.0f, 50.0f, -60.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[3], 0.6f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[4], 0.9f, -15.0f, 30.0f, 60.0f, 50.0f, -35.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[5], 1.2f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[6], 1.5f, -15.0f, 30.0f, 60.0f, 50.0f, -60.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[7], 1.8f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[8], 2.1f, -15.0f, 30.0f, 60.0f, 50.0f, -35.0f);

    pStrike->aSnapShots[9].unknown0 = 3.0f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[9]);
    pStrike->unknown0 = 5;
    pStrike->unknown3 = 10;
}

// Starts Quetzalcoatl strike 4 (head down)
void J3DAPI sithAIMove_sub_49B1B0(SithAIControlBlock* pLocal)
{
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pOwner->userblock.pQuetz->strike;
    pStrike->unknown1 = 0.0f;
    pStrike->snapShot = 0;
    pStrike->aSnapShots[0].unknown0 = 0.0f;
    sithAIMove_SetStrikeSnapShot(pOwner, &pStrike->aSnapShots[0]);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[1], 1.6f, 0.0f, 0.0f, 0.0f, 0.0f, -60.0f);
    sithAIMove_SetStrikeSnapShotAngles(&pStrike->aSnapShots[2], 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, -60.0f);
    pStrike->unknown0 = 4;
    pStrike->unknown3 = 3;
}

// Spins Marduk's abdomen
void J3DAPI sithAIMove_UpdateMardukTail(SithThing* pThing, float secDeltaTime)
{
    if ( stdUtil_StrCmp(pThing->aName, "marduk") != 0 && stdUtil_StrCmp(pThing->aName, "marduk2") != 0 )
    {
        return;
    }

    int jointNum = sithThing_GetThingJointIndex(pThing, "wmabdomen");
    if ( jointNum != -1 )
    {
        rdVector3* pTweak = &pThing->renderData.apTweakedAngles[jointNum];
        pTweak->y = stdMath_NormalizeAngle(720.0f * secDeltaTime) + pTweak->y;
    }
}
