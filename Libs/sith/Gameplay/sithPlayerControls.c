#include "sithPlayerControls.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Engine/rdPuppet.h>
#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdVector.h>
#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithControl.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Engine/sithCamera.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithFX.h>
#include <sith/Gameplay/sithInventory.h>
#include <sith/Gameplay/sithOverlayMap.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Gameplay/sithVehicleControls.h>
#include <sith/Gameplay/sithWhip.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithActor.h>
#include <sith/World/sithModel.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>
#include <sound/Sound.h>
#include <std/General/stdMath.h>
#include <std/General/stdUtil.h>

#include <math.h>
#include <stdbool.h>
#include <string.h>

static rdVector3 sithPlayerControls_vecDebugCamDir = { { 0.0f }, { -1.0f }, { 0.0f } };
static float sithPlayerControls_cameraDistance = 0.2f;
int sithPlayerControls_bActionActivated;
static SithThing* sithPlayerControls_pCollidedThing;
static int sithPlayerControls_curJumpDirection;
static SithThing* sithPlayerControls_pCurActivatedThing;
static int sithPlayerControls_bJumpActivated;
static int sithPlayerControls_bLookMode;
static int sithPlayerControls_bHealthActivated;
static SithThing* sithPlayerControls_pVehicleBoardedThing;
static SithThing* sithPlayerControls_pTargetThing;
static int sithPlayerControls_dword_55EE00;
static int sithPlayerControls_curMoveStatus;
static int sithPlayerControls_bTurnRightActivated;
static int sithPlayerControls_bTurnLeftActivated;
static float sithPlayerControls_secSwimmingTime;
static float sithPlayerControls_secCommentWait;

// Defined in sithPlayerActions.c
extern int sithPlayerActions_climbPupTrackNum;

#define SITHPLAYERCONTROLS_COMMENTWAIT 3.0f   // Seconds before Indy can comment again
#define SITHCONTROL_OPTION_ALWAYSRUN   0x02

// Aim cone angles (pitch near, pitch far, yaw near, yaw far, vertical), the second set is for the mirror weapon
static const float sithPlayerControls_aAimAngles[5]       = { 20.5f, 22.5f, 87.0f, 89.0f, 50.0f };
static const float sithPlayerControls_aMirrorAimAngles[5] = { 3.0f, 5.0f, 14.0f, 16.0f, 5.0f };
static const float sithPlayerControls_aimNearDistance     = 0.3f;

static const float sithPlayerControls_exitMineCarOffset = 0.08f;
static const float sithPlayerControls_exitJeepOffset    = 0.12f;
static const float sithPlayerControls_rarmAimYawOffset  = 3.0f;
static const float sithPlayerControls_torsoAimYawOffset = 1.0f;

void sithPlayerControls_InstallHooks(void)
{
    J3D_HOOKFUNC(sithPlayerControls_PuppetCallback);
    J3D_HOOKFUNC(sithPlayerControls_Process);
    J3D_HOOKFUNC(sithPlayerControls_ProcessPlayerDebugControls);
    J3D_HOOKFUNC(sithPlayerControls_ProcessLookControls);
    J3D_HOOKFUNC(sithPlayerControls_ProcessGeneralMove);
    J3D_HOOKFUNC(sithPlayerControls_ProcessClimbMove);
    J3D_HOOKFUNC(sithPlayerControls_ProcessJewelFlyMove);
    J3D_HOOKFUNC(sithPlayerControls_ProcessFlyAndUnderwaterMove);
    J3D_HOOKFUNC(sithPlayerControls_ProcessGeneralControls);
    J3D_HOOKFUNC(sithPlayerControls_ProcessEditorDebugControls);
    J3D_HOOKFUNC(sithPlayerControls_ProcessDeadPlayer);
    J3D_HOOKFUNC(sithPlayerControls_ProcessJumpForwardMove);
    J3D_HOOKFUNC(sithPlayerControls_ProcessSwimMove);
    J3D_HOOKFUNC(sithPlayerControls_ProcessHangMove);
    J3D_HOOKFUNC(sithPlayerControls_ProcessWeaponAim);
    J3D_HOOKFUNC(sithPlayerControls_CanAimAtPos);
    J3D_HOOKFUNC(sithPlayerControls_GetPushPullMoveNorm);
    J3D_HOOKFUNC(sithPlayerControls_CheckWaterLedge);
    J3D_HOOKFUNC(sithPlayerControls_SearchForCollisions);
    J3D_HOOKFUNC(sithPlayerControls_SearchForClimbSurface);
    J3D_HOOKFUNC(sithPlayerControls_TryBoardVehicle);
    J3D_HOOKFUNC(sithPlayerControls_ExitVehicle);
    J3D_HOOKFUNC(sithPlayerControls_RotateAimJoints);
    J3D_HOOKFUNC(sithPlayerControls_CenterActorOrientation);
    J3D_HOOKFUNC(sithPlayerControls_GetTargetThing);
    J3D_HOOKFUNC(sithPlayerControls_GetVehicleBoardedThing);
    J3D_HOOKFUNC(sithPlayerControls_SetVehicleBoardedThing);
    J3D_HOOKFUNC(sithPlayerControls_CanStrafeMove);
}

int sithPlayerControls_g_bCutsceneMode;

static inline bool sithPlayerControls_IsZeroVector(const rdVector3* pVec)
{
    return pVec->x == 0.0f && pVec->y == 0.0f && pVec->z == 0.0f;
}

static inline float sithPlayerControls_MinSpeed(float speed)
{
    return speed < 1.0f ? speed : 1.0f;
}

// Returns yaw rotation velocity; scale is the turn direction (and speed) and speed is clamped to 1.0
static inline float sithPlayerControls_GetTurnVelocity(const SithThing* pThing, float scale, float speed)
{
    float fpsRot = sithTime_g_fps * scale;
    return pThing->thingInfo.actorInfo.maxRotVelocity * scale * sithPlayerControls_MinSpeed(speed) + fpsRot;
}

// Adds mouse turn to yaw rotation velocity, used only in editor mode
static void sithPlayerControls_ApplyMouseTurn(SithThing* pThing, float scale)
{
    float axisRot = sithControl_GetAxis(SITHCONTROL_MOUSETURN) * sithTime_g_fps;
    float keyRot  = sithControl_GetKeyAsAxis(SITHCONTROL_MOUSETURN);
    pThing->moveInfo.physics.angularVelocity.y = keyRot * pThing->thingInfo.actorInfo.maxRotVelocity * scale + axisRot + pThing->moveInfo.physics.angularVelocity.y;
}

// Rotates thrust by head orientation (e.g.: when looking up or down while flying)
static void sithPlayerControls_RotateThrustByHead(SithThing* pThing)
{
    rdMatrix34 mat;
    rdMatrix_BuildRotate34(&mat, &pThing->thingInfo.actorInfo.headPYR);
    rdMatrix_TransformVector34Acc(&pThing->moveInfo.physics.thrust, &mat);
}

static void sithPlayerControls_PlayComment(SithThing* pThing, SithSoundClassMode mode)
{
    if ( sithPlayerControls_secCommentWait == 0.0f )
    {
        sithSoundClass_PlayVoiceModeRandom(pThing, mode);
        sithPlayerControls_secCommentWait = SITHPLAYERCONTROLS_COMMENTWAIT;
    }
}

static void sithPlayerControls_StopPushPull(SithThing* pThing)
{
    sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_PUSHPULLREADY);
    pThing->moveStatus = SITHPLAYERMOVE_STILL;
    pThing->thingInfo.actorInfo.bControlsDisabled = 0;
    sithPlayerControls_pCollidedThing = NULL;
}

static inline float sithPlayerControls_GetWorldVertexZ(const int* aVertices)
{
    return sithWorld_g_pCurrentWorld->aVertices[aVertices[0]].z;
}

static void sithPlayerControls_StartActivate(SithThing* pThing, SithThing* pActivatedThing)
{
    pThing->thingInfo.actorInfo.flags |= SITH_AF_CONTROLSDISABLED;
    sithPlayerControls_pCurActivatedThing = pActivatedThing;
    sithCog_ThingSendMessage(pActivatedThing, pThing, SITHCOG_MSG_ACTIVATE);
    sithPhysics_ResetThingMovement(pThing);

    sithPlayerControls_curMoveStatus = pThing->moveStatus;
    pThing->moveStatus = SITHPLAYERMOVE_ACTIVATING;
    pThing->thingInfo.actorInfo.bControlsDisabled = 1;
}

static void sithPlayerControls_PlaySound(uint32_t soundIdx)
{
    tSoundHandle hSnd = Sound_GetSoundHandle(soundIdx);
    if ( hSnd )
    {
        sithSoundMixer_PlaySound(hSnd, 1.0f, 0.0f, (SoundPlayFlag)0);
    }
}

void J3DAPI sithPlayerControls_PuppetCallback(SithThing* pThing, int track, rdKeyMarkerType markerType)
{
    if ( !pThing->thingInfo.actorInfo.bControlsDisabled )
    {
        return;
    }

    SithPuppetMoveMode moveMode;
    switch ( pThing->moveStatus )
    {
        case SITHPLAYERMOVE_PUSHPULL_READY:
            if ( markerType == RDKEYMARKER_CRAWL )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            }
            return;

        case SITHPLAYERMOVE_WALK2STAND:
            if ( markerType == 0 )
            {
                pThing->moveStatus = SITHPLAYERMOVE_STILL;
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_STAND, NULL);
            }
            return;

        case SITHPLAYERMOVE_STAND2WALK:
            if ( markerType == 0 )
            {
                pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            }
            return;

        case SITHPLAYERMOVE_STAND2RUN:
            if ( markerType == 0 )
            {
                pThing->moveStatus = SITHPLAYERMOVE_RUNNING;
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            }
            return;

        case SITHPLAYERMOVE_JEEP_STILL:
        case SITHPLAYERMOVE_MINECAR_IDLE:
            sithPlayerControls_ExitVehicle(pThing);
            return;

        case SITHPLAYERMOVE_LAND:
            if ( markerType == 0 )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                sithPuppet_FreeTrackByIndex(pThing, track);
                pThing->moveStatus = SITHPLAYERMOVE_STILL;
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_ACTIVATING:
            if ( markerType == RDKEYMARKER_ACTIVATERIGHTARM )
            {
                if ( sithPlayerControls_pCurActivatedThing )
                {
                    sithCog_ThingSendMessage(sithPlayerControls_pCurActivatedThing, pThing, SITHCOG_MSG_TOUCHED);
                    return;
                }

                // Chalk writing
                sithFX_CreateChalkMark(&sithPlayerActions_g_posChalkMarkWrite, sithPlayerActions_g_pChalkWriteSurf, sithPlayerActions_g_pChalkWriteSector);
                sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)0x79);
                return;
            }

            if ( markerType == RDKEYMARKER_ACTIVATERIGHTARMREST )
            {
                sithCamera_RestoreExtCamera();
                return;
            }

            if ( markerType == 0 )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                sithPuppet_RemoveTrackByIndex(pThing, track);
                pThing->moveStatus = sithPlayerControls_curMoveStatus;
                if ( !sithPlayerControls_pCurActivatedThing )
                {
                    sithCog_ThingSendMessage(pThing, pThing, SITHCOG_MSG_USER7);
                }

                sithPlayerControls_pCurActivatedThing = NULL;
                pThing->thingInfo.actorInfo.flags &= ~SITH_AF_CONTROLSDISABLED;
                return;
            }

            sithPuppet_DefaultCallback(pThing, track, markerType);
            return;

        case SITHPLAYERMOVE_MOUNTING_WALL:
            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            pThing->moveStatus = SITHPLAYERMOVE_CLIMBIDLE;
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_CLIMBWALLIDLE, NULL);
            sithPuppet_FreeTrackByIndex(pThing, track);
            return;

        case SITHPLAYERMOVE_CLIMBIDLE:
            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            return;

        case SITHPLAYERMOVE_PULLINGUP_1M:
            if ( markerType != 0 )
            {
                sithPuppet_DefaultCallback(pThing, track, markerType);
                return;
            }

            pThing->moveStatus = SITHPLAYERMOVE_STILL;
            moveMode = pThing->pPuppetState->moveMode;
            if ( moveMode != SITHPUPPET_MOVEMODE_NORMAL )
            {
                sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_NORMAL);
            }
            return;

        case SITHPLAYERMOVE_JUMP_READY:
            if ( markerType != 0 )
            {
                sithPuppet_DefaultCallback(pThing, track, markerType);
                return;
            }

            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            sithPuppet_DefaultCallback(pThing, track, 0);
            if ( sithPlayerControls_curJumpDirection == 99 )
            {
                sithPlayerActions_ClimbOn2m(pThing);
            }
            else if ( sithPlayerControls_curJumpDirection != 0 )
            {
                // Note, original also has a branch for jump direction -1 (jump back) after this one which is dead code
                // because jump direction is compared as unsigned value.
                sithPlayerActions_Jump(pThing, 0.77f, sithPlayerControls_curJumpDirection);
            }
            else
            {
                sithPlayerActions_Jump(pThing, 0.60000002f, 0);
            }

            sithPlayerControls_curJumpDirection = 0;
            return;

        case SITHPLAYERMOVE_SLIDEDOWNFORWARD:
        case SITHPLAYERMOVE_SLIDEDOWNBACK:
        {
            if ( markerType != 0 )
            {
                sithPuppet_DefaultCallback(pThing, track, markerType);
                return;
            }

            SithSector* pSector = pThing->pInSector;
            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            if ( (pSector && (pSector->flags & SITH_SECTOR_UNDERWATER) != 0) || (pSector->flags & SITH_SECTOR_AETHERIUM) != 0 )
            {
                pThing->moveStatus = SITHPLAYERMOVE_SWIMIDLE;
                if ( pThing->pPuppetState->moveMode != SITHPUPPET_MOVEMODE_SWIM )
                {
                    sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_SWIM);
                }
                return;
            }

            pThing->moveStatus = SITHPLAYERMOVE_STILL;
            moveMode = pThing->pPuppetState->moveMode;
            if ( moveMode != SITHPUPPET_MOVEMODE_NORMAL )
            {
                sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_NORMAL);
            }
        } return;

        case SITHPLAYERMOVE_WHIPSWINGING:
            if ( markerType == RDKEYMARKER_ATTACK )
            {
                sithPlayerControls_PlaySound(0x8003);
            }
            else if ( markerType == RDKEYMARKER_SWING )
            {
                sithPlayerControls_PlaySound(0x8064);
            }
            else if ( markerType == RDKEYMARKER_ACTIVATERIGHTARMREST )
            {
                sithPlayerControls_PlaySound(0x8018);
                sithCamera_RestoreExtCamera();
            }
            else if ( markerType == 0 )
            {
                pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                sithWhip_DeactivateWhip(pThing);
            }
            return;

        case SITHPLAYERMOVE_WHIP_CLIMB_START:
            sithWhip_SetActorWhipClimbIdle(pThing);
            return;

        case SITHPLAYERMOVE_WHIPCLIMBIDLE:
            return;

        case SITHPLAYERMOVE_HANGING:
        case SITHPLAYERMOVE_STILL:
            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
            return;

        default:
            return;
    }
}

int J3DAPI sithPlayerControls_Process(SithThing* pPlayerThing, float secDeltaTime)
{
    if ( sithPlayerControls_g_bCutsceneMode )
    {
        return 0;
    }

    if ( sithPlayerControls_dword_55EE00 > 0 )
    {
        sithPlayerControls_dword_55EE00 += (int)(secDeltaTime * -1000.0f);
        if ( sithPlayerControls_dword_55EE00 < 0 )
        {
            sithPlayerControls_dword_55EE00 = 0;
        }
    }

    if ( pPlayerThing->moveType != SITH_MT_PHYSICS )
    {
        return 0;
    }

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 && sithPlayerControls_ProcessEditorDebugControls(pPlayerThing, secDeltaTime) )
    {
        return 1;
    }

    if ( (pPlayerThing->flags & SITH_TF_DYING) != 0 )
    {
        sithPlayerControls_ProcessDeadPlayer(pPlayerThing, secDeltaTime);
        return 0;
    }

    if ( sithPlayerControls_secCommentWait != 0.0f )
    {
        sithPlayerControls_secCommentWait = sithPlayerControls_secCommentWait - secDeltaTime;
        if ( sithPlayerControls_secCommentWait <= 0.0f )
        {
            sithPlayerControls_secCommentWait = 0.0f;
        }
    }

    if ( sithTime_g_bPaused == 1 )
    {
        return 1;
    }

    if ( pPlayerThing->moveStatus == SITHPLAYERMOVE_SWIMIDLE )
    {
        sithPlayerControls_secSwimmingTime = secDeltaTime + sithPlayerControls_secSwimmingTime;
    }
    else
    {
        sithPlayerControls_secSwimmingTime = 0.0f;
    }

    if ( sithPuppet_g_bPlayerLeapForward && pPlayerThing->moveStatus != SITHPLAYERMOVE_RUNNING )
    {
        sithPuppet_g_bPlayerLeapForward = 0;
    }

    if ( sithPlayerControls_ProcessPlayerDebugControls(pPlayerThing) )
    {
        return 1;
    }

    // Reset key latches when keys are released
    if ( sithPlayerControls_bActionActivated && !sithControl_GetKey(SITHCONTROL_ACT2, NULL) )
    {
        sithPlayerControls_bActionActivated = 0;
    }

    if ( sithPlayerControls_bTurnLeftActivated && !sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
    {
        sithPlayerControls_bTurnLeftActivated = 0;
    }

    if ( sithPlayerControls_bTurnRightActivated && !sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
    {
        sithPlayerControls_bTurnRightActivated = 0;
    }

    if ( sithPlayerControls_bJumpActivated && !sithControl_GetKey(SITHCONTROL_JUMP, NULL) )
    {
        sithPlayerControls_bJumpActivated = 0;
    }

    if ( sithPlayerControls_bHealthActivated && !sithControl_GetKey(SITHCONTROL_HEALTH, NULL) )
    {
        sithPlayerControls_bHealthActivated = 0;
    }

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        SithActorInfo* pActor = &pPlayerThing->thingInfo.actorInfo;
        if ( !sithControl_GetKey(SITHCONTROL_LOOK, NULL) )
        {
            if ( sithPlayerControls_bLookMode == 1 )
            {
                // Exit look mode
                pActor->flags |= SITH_AF_VIEWCENTRING;
                rdVector_Set3(&pActor->headPYR, 0.0f, 0.0f, 0.0f);
                sithActor_SetHeadPYR(pPlayerThing, &pActor->headPYR);

                sithPlayerControls_bLookMode = 0;
                sithCamera_RestoreExtCamera();
                sithCamera_g_bExtCameraLookMode = 0;
            }
        }
        else if ( !sithPlayerControls_g_bCutsceneMode
            && !sithPlayerControls_bLookMode
            && sithPlayerControls_IsZeroVector(&pPlayerThing->moveInfo.physics.velocity)
            && sithPlayerControls_IsZeroVector(&pPlayerThing->moveInfo.physics.angularVelocity)
            && (sithInventory_GetCurrentWeapon(pPlayerThing) != SITHWEAPON_WHIP || !sithWeapon_IsAiming(pPlayerThing)) )
        {
            // Enter look mode
            pActor->flags |= SITH_AF_VIEWCENTRING;
            rdVector_Set3(&pActor->headPYR, 0.0f, 0.0f, 0.0f);
            sithActor_SetHeadPYR(pPlayerThing, &pActor->headPYR);

            sithPlayerControls_bLookMode     = 1;
            sithCamera_g_bExtCameraLookMode = 1;

            rdVector3 offset;
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = pPlayerThing->moveStatus == SITHPLAYERMOVE_CRAWL_STILL ? 0.025f : 0.059999999f;
            sithCamera_SetExtCameraOffset(&offset);

            offset.x = 0.0f;
            if ( pPlayerThing->moveStatus == SITHPLAYERMOVE_CRAWL_STILL )
            {
                offset.y = 0.025f;
                offset.z = 0.025f;
            }
            else
            {
                offset.y = 0.039999999f;
                offset.z = 0.059999999f;
            }

            sithCamera_SetExtCameraLookOffset(&offset);
        }
    }

    sithPlayerControls_ProcessWeaponAim(pPlayerThing, secDeltaTime);
    if ( pPlayerThing->type == SITH_THING_PLAYER && (pPlayerThing->thingInfo.actorInfo.flags & SITH_AF_CONTROLSDISABLED) != 0 )
    {
        return 0;
    }

    if ( sithPlayerControls_bLookMode == 1 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        sithPlayerControls_ProcessLookControls(pPlayerThing, secDeltaTime);
        return 0;
    }

    SithPhysicsFlags physFlags = pPlayerThing->moveInfo.physics.flags;
    if ( (physFlags & SITH_PF_FLY) != 0 && pPlayerThing->moveStatus != SITHPLAYERMOVE_JEWELFLYING )
    {
        sithPlayerControls_ProcessFlyAndUnderwaterMove(pPlayerThing);
    }
    else if ( (physFlags & SITH_PF_JEEP) != 0 )
    {
        sithVehicleControls_ProcessJeepPlayerMove(pPlayerThing, secDeltaTime);
    }
    else if ( (physFlags & SITH_PF_RAFT) != 0 )
    {
        sithVehicleControls_ProcessRaftPlayerMove(pPlayerThing, secDeltaTime);
    }
    else if ( (physFlags & SITH_PF_MINECAR) != 0 )
    {
        sithVehicleControls_ProcessMineCarPlayerMove(pPlayerThing, secDeltaTime);
    }
    else
    {
        if ( (physFlags & (SITH_PF_MINECAR | SITH_PF_RAFT | SITH_PF_JEEP)) == 0 && !pPlayerThing->thingInfo.actorInfo.bForceMovePlay )
        {
            switch ( pPlayerThing->moveStatus )
            {
                case SITHPLAYERMOVE_CRAWL_STILL:
                case SITHPLAYERMOVE_UNKNOWN_4:
                case SITHPLAYERMOVE_UNKNOWN_5:
                case SITHPLAYERMOVE_CLIMBIDLE:
                case SITHPLAYERMOVE_SWIMIDLE:
                case SITHPLAYERMOVE_MOUNTING_WALL:
                case SITHPLAYERMOVE_CLIMBING_UP:
                case SITHPLAYERMOVE_CLIMBING_DOWN:
                case SITHPLAYERMOVE_CLIMBING_LEFT:
                case SITHPLAYERMOVE_CLIMBING_RIGHT:
                case SITHPLAYERMOVE_STAND_TO_CRAWL:
                case SITHPLAYERMOVE_CRAWL_TO_STAND:
                    break;

                default:
                    sithPhysics_SetThingLook(pPlayerThing, &rdroid_g_zVector3, 0.0f);
                    break;
            }
        }

        switch ( pPlayerThing->moveStatus )
        {
            case SITHPLAYERMOVE_STILL:
            case SITHPLAYERMOVE_WALKING:
            case SITHPLAYERMOVE_RUNNING:
            case SITHPLAYERMOVE_CRAWL_STILL:
            case SITHPLAYERMOVE_JUMP_READY:
            case SITHPLAYERMOVE_PUSHPULL_READY:
            case SITHPLAYERMOVE_SLIDEDOWNFORWARD:
            case SITHPLAYERMOVE_SLIDEDOWNBACK:
                if ( (pPlayerThing->pInSector->flags & (SITH_SECTOR_UNDERWATER | SITH_SECTOR_AETHERIUM)) == 0 )
                {
                    sithPlayerControls_ProcessGeneralMove(pPlayerThing, secDeltaTime);
                }
                else
                {
                    pPlayerThing->moveStatus = SITHPLAYERMOVE_SWIMIDLE;
                    sithPlayerControls_secSwimmingTime = 0.0f;
                    sithPlayerControls_ProcessSwimMove(pPlayerThing);
                }
                break;

            case SITHPLAYERMOVE_HANGING:
                sithPlayerControls_ProcessHangMove(pPlayerThing);
                break;

            case SITHPLAYERMOVE_CLIMBIDLE:
                sithPlayerControls_ProcessClimbMove(pPlayerThing);
                break;

            case SITHPLAYERMOVE_WHIPCLIMBIDLE:
            case SITHPLAYERMOVE_WHIP_CLIMB_START:
            case SITHPLAYERMOVE_WHIP_UNK2:
                sithWhip_ProcessWhipClimbMove(pPlayerThing);
                break;

            case SITHPLAYERMOVE_SWIMIDLE:
                sithPlayerControls_ProcessSwimMove(pPlayerThing);
                break;

            case SITHPLAYERMOVE_JUMPFWD:
            case SITHPLAYERMOVE_JUMPUP:
            case SITHPLAYERMOVE_FALLING:
            case SITHPLAYERMOVE_LEAPFWD:
                if ( (pPlayerThing->pInSector->flags & (SITH_SECTOR_UNDERWATER | SITH_SECTOR_AETHERIUM)) == 0 )
                {
                    sithPlayerControls_ProcessJumpForwardMove(pPlayerThing);
                }
                else
                {
                    pPlayerThing->moveStatus = SITHPLAYERMOVE_SWIMIDLE;
                    sithPlayerControls_secSwimmingTime = 0.0f;
                    sithPlayerControls_ProcessSwimMove(pPlayerThing);
                }
                break;

            case SITHPLAYERMOVE_MINECAR_IDLE:
                sithVehicleControls_ProcessMineCarPlayerMove(pPlayerThing, secDeltaTime);
                break;

            case SITHPLAYERMOVE_JEEP_STILL:
                sithVehicleControls_ProcessJeepPlayerMove(pPlayerThing, secDeltaTime);
                break;

            case SITHPLAYERMOVE_RAFT_STILL:
                sithVehicleControls_ProcessRaftPlayerMove(pPlayerThing, secDeltaTime);
                break;

            case SITHPLAYERMOVE_JEWELFLYING:
            case SITHPLAYERMOVE_JEWELFLYING_UNKN1:
            case SITHPLAYERMOVE_JEWELFLYING_UNKN2:
                sithPlayerControls_ProcessJewelFlyMove(pPlayerThing);
                break;

            default:
                break;
        }
    }

    sithPlayerControls_ProcessGeneralControls(pPlayerThing);
    return 0;
}

int J3DAPI sithPlayerControls_ProcessPlayerDebugControls(SithThing* pThing)
{
    int bPressed;
    if ( !sithControl_GetKey(SITHCONTROL_DEBUG, NULL) )
    {
        return 0;
    }

    sithControl_GetKey(SITHCONTROL_ACT2, &bPressed);
    if ( bPressed )
    {
        // Reset player state
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        pThing->collide.movesize = 0.039999999f;
        pThing->thingInfo.actorInfo.bControlsDisabled = 0;
        pThing->thingInfo.actorInfo.bForceMovePlay    = 0;
        sithPuppet_RemoveAllTracks(pThing);

        pThing->renderData.bSkipBuildingJoints = 0;
        rdVector_Copy3(&pThing->orient.dvec, &pThing->pos);
        rdPuppet_BuildJointMatrices(&pThing->renderData, &pThing->orient);
        rdVector_Set3(&pThing->orient.dvec, 0.0f, 0.0f, 0.0f);
        return 1;
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        sithPhysics_ResetThingMovement(pThing);
    }

    for ( int keyId = SITHCONTROL_WSELECT0; keyId < SITHCONTROL_WSELECT10; ++keyId )
    {
        sithControl_GetKey((SithControlFunction)keyId, &bPressed);
        if ( bPressed )
        {
            sithPlayerActions_MoveToPlayerPosition(pThing, keyId - SITHCONTROL_WSELECT0);
            return 1;
        }
    }

    return 0;
}

void J3DAPI sithPlayerControls_ProcessLookControls(SithThing* pThing, float secDeltaTime)
{
    bool bMoved     = false;
    float yawOffset = 0.0f;
    float step;

    if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
    {
        if ( !pThing->thingInfo.actorInfo.bForceMovePlay )
        {
            bMoved = true;
            step   = -0.001f;
        }
        else
        {
            step = 0.0f;
        }
    }
    else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) && !pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        bMoved = true;
        step   = 0.001f;
    }
    else
    {
        step = 0.0f;
    }

    if ( step != 0.0f )
    {
        yawOffset = step * 90.0f * secDeltaTime;
    }

    float pitchOffset = 0.0f;
    if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
    {
        step = 0.001f;
    }
    else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
    {
        step = -0.001f;
    }
    else
    {
        step = 0.0f;
    }

    if ( step != 0.0f )
    {
        bMoved      = true;
        pitchOffset = step * 90.0f * secDeltaTime;
    }

    if ( !bMoved )
    {
        if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 )
        {
            sithPlayerControls_ApplyMouseTurn(pThing, 1.0f);
        }

        return;
    }

    rdVector3 offset;
    offset.x = sithCamera_g_pCurCamera->offset.x;
    offset.y = sithCamera_g_pCurCamera->offset.y;
    offset.z = sithCamera_g_pCurCamera->offset.z + pitchOffset;
    if ( offset.z < -0.07f )
    {
        offset.z = -0.07f;
    }
    else if ( offset.z > 0.15000001f )
    {
        offset.z = 0.15000001f;
    }

    if ( yawOffset != 0.0f )
    {
        offset.x = offset.x + yawOffset;
        if ( offset.x < -0.07f )
        {
            offset.x = -0.07f;
        }
        else if ( offset.x > 0.15000001f )
        {
            offset.x = 0.15000001f;
        }
    }

    sithCamera_SetExtCameraOffset(&offset);
}

static void sithPlayerControls_CheckLevelRoll(SithThing* pThing)
{
    if ( pThing->moveInfo.physics.height != 0.090000004f )
    {
        pThing->moveInfo.physics.height = 0.090000004f;
        pThing->moveInfo.physics.flags  = (pThing->moveInfo.physics.flags & ~SITH_PF_ALIGNSURFACE) | SITH_PF_ALIGNUP;
        sithPhysics_FindFloor(pThing, 1);
        sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_NORMAL);
    }
}

// Returns true when standing on slope surface where jump is not allowed
static bool sithPlayerControls_IsOnSteepSlope(SithThing* pThing)
{
    SithAttachFlag attachFlags = pThing->attach.flags;
    if ( attachFlags && (attachFlags & SITH_ATTACH_SURFACE) != 0 )
    {
        const rdVector3* pNormal = &pThing->attach.pFace->normal;
        float dot = pNormal->x * rdroid_g_zVector3.x + pNormal->y * rdroid_g_zVector3.y + pNormal->z * rdroid_g_zVector3.z;
        if ( dot > 0.69999999f && dot < 0.75f )
        {
            return true;
        }
    }

    return false;
}

static void sithPlayerControls_ProcessJump(SithThing* pThing)
{
    if ( sithPlayerControls_IsOnSteepSlope(pThing) )
    {
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        pThing->thingInfo.actorInfo.bForceMovePlay = 0;
        return;
    }

    sithPlayerControls_curJumpDirection = 0;
    sithPlayerActions_JumpStart(pThing);
}

static void sithPlayerControls_PlayWalk2Stand(SithThing* pThing, rdKeyframe* pKey)
{
    sithPuppet_PlayKey(pThing->renderData.pPuppet, pKey, 1, 2, (rdKeyframeFlags)0x32, sithPlayerControls_PuppetCallback);
    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_STAND, NULL);
    sithPhysics_ResetThingMovement(pThing);
}

static bool sithPlayerControls_CanCrawl(SithThing* pThing)
{
    SithAttachFlag attachFlags = pThing->attach.flags;
    if ( !attachFlags )
    {
        return false;
    }

    if ( (attachFlags & (SITH_ATTACH_THINGFACE | SITH_ATTACH_THING)) != 0 )
    {
        SithThing* pAttachedThing = pThing->attach.attachedToStructure.pThingAttached;
        if ( !pAttachedThing )
        {
            return false;
        }

        bool bCanCrawl;
        switch ( pAttachedThing->moveType )
        {
            case SITH_MT_NONE:
                bCanCrawl = true;
                break;

            case SITH_MT_PHYSICS:
                bCanCrawl = sithPlayerControls_IsZeroVector(&pAttachedThing->moveInfo.physics.velocity);
                break;

            case SITH_MT_PATH:
                bCanCrawl = (pAttachedThing->moveInfo.pathMovement.mode & 1) == 0;
                break;

            default:
                bCanCrawl = false; // Original uses leftover return value of sithPlayerActions_HasActiveWeapon which is 0 at this point
                break;
        }

        if ( bCanCrawl && pThing->attach.pFace )
        {
            rdVector3 normal;
            rdMatrix_TransformVector34(&normal, &pThing->attach.pFace->normal, &pAttachedThing->orient);
            bCanCrawl = fabsf(normal.z * rdroid_g_zVector3.z + normal.y * rdroid_g_zVector3.y + normal.x * rdroid_g_zVector3.x) >= 0.98000002f;
        }

        return bCanCrawl;
    }

    if ( (attachFlags & SITH_ATTACH_SURFACE) != 0 )
    {
        SithSurface* pSurface = pThing->attach.attachedToStructure.pSurfaceAttached;
        if ( !pSurface )
        {
            return false;
        }

        if ( (pSurface->flags & (SITH_SURFACE_WATER | SITH_SURFACE_SHALLOWWATER)) != 0 )
        {
            return false;
        }

        const rdVector3* pNormal = &pThing->attach.pFace->normal;
        return fabsf(pNormal->z * rdroid_g_zVector3.z + pNormal->y * rdroid_g_zVector3.y + pNormal->x * rdroid_g_zVector3.x) >= 0.98000002f;
    }

    return false;
}

// Tries to mount climbable wall in front of thing
static bool sithPlayerControls_TryMountWall(SithThing* pThing)
{
    bool bMounted = false;
    float movesize = pThing->collide.movesize;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &pThing->orient.lvec, movesize + movesize, pThing->collide.movesize, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & (SITHCOLLISION_WORLD | SITHCOLLISION_ADJOINCROSS)) == 0 )
        {
            continue;
        }

        SithSurface* pSurface = pCollision->pSurfaceCollided;
        if ( !pSurface || (pSurface->flags & SITH_SURFACE_CLIMBABLE) == 0 )
        {
            continue;
        }

        rdVector3 wallDir;
        wallDir.x = -pSurface->face.normal.x;
        wallDir.y = -pSurface->face.normal.y;
        wallDir.z = 0.0f;
        rdVector_Normalize3Acc(&wallDir);

        rdVector3 lookDir;
        lookDir.x = pThing->orient.lvec.x;
        lookDir.y = pThing->orient.lvec.y;
        lookDir.z = 0.0f;
        rdVector_Normalize3Acc(&lookDir);

        if ( lookDir.z * wallDir.z + lookDir.y * wallDir.y + lookDir.x * wallDir.x > 0.80000001f )
        {
            bMounted = true;
            pThing->moveStatus = SITHPLAYERMOVE_MOUNTING_WALL;
            pThing->thingInfo.actorInfo.bControlsDisabled = 1;
            sithPhysics_ResetThingMovement(pThing);

            sithPlayerActions_MoveToClimbSector(pThing, pSurface);
            sithThing_AttachThingToClimbSurface(pThing, pSurface);

            sithPuppet_RemoveAllTracks(pThing);
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_MOUNTWALL, sithPlayerControls_PuppetCallback);
            sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)0x88);

            pThing->moveInfo.physics.flags &= ~SITH_PF_FLOORSTICK;
            sithPlayerControls_bJumpActivated = 1;
            sithInventory_SetSwimmingInventory(pThing, 0);
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return bMounted;
}

// Tries to start pushing or pulling movable thing in front of player
static bool sithPlayerControls_TryPushPull(SithThing* pThing)
{
    rdVector3 moveNorm;
    if ( !sithPlayerControls_GetPushPullMoveNorm(&moveNorm, &pThing->orient.lvec, 15.0f)
        || !pThing->attach.flags
        || sithInventory_GetCurrentWeapon(pThing) != SITHWEAPON_NO_WEAPON
        || sithWeapon_IsMountingWeapon(pThing) )
    {
        return false;
    }

    SithThing* pPushThing = NULL;
    float movesize = pThing->collide.movesize;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &moveNorm, movesize, movesize, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_THING) != 0
            && pCollision->pThingCollided != pThing
            && (pCollision->pThingCollided->flags & SITH_TF_MOVABLE) != 0
            && pCollision->distance < 0.0089999996f )
        {
            pPushThing = pCollision->pThingCollided;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !pPushThing )
    {
        return false;
    }

    rdVector3 lookDir;
    lookDir.x = pThing->orient.lvec.x;
    lookDir.y = pThing->orient.lvec.y;
    lookDir.z = 0.0f;
    rdVector_Normalize3Acc(&lookDir);

    rdVector3 thingDir;
    thingDir.x = pThing->pos.x - pPushThing->pos.x;
    thingDir.y = pThing->pos.y - pPushThing->pos.y;
    thingDir.z = 0.0f;
    rdVector_Normalize3Acc(&thingDir);

    if ( fabsf(thingDir.z * lookDir.z + thingDir.y * lookDir.y + thingDir.x * lookDir.x) <= 0.94f )
    {
        return false;
    }

    sithPlayerControls_pCollidedThing = pPushThing;
    sithPhysics_ResetThingMovement(pThing);

    // Align player to the push direction
    rdMatrix34* pOrient = &pThing->orient;
    pOrient->lvec.x = moveNorm.x;
    pOrient->rvec.z = 0.0f;
    pOrient->lvec.y = moveNorm.y;
    pOrient->lvec.z = moveNorm.z;
    pOrient->rvec.x = pOrient->lvec.y - 0.0f;
    pOrient->rvec.y = 0.0f - pOrient->lvec.x;
    rdVector_Normalize3Acc(&pOrient->rvec);

    pOrient->uvec.x = pOrient->lvec.z * pOrient->rvec.y - pOrient->lvec.y * pOrient->rvec.z;
    pOrient->uvec.y = pOrient->rvec.z * pOrient->lvec.x - pOrient->lvec.z * pOrient->rvec.x;
    pOrient->uvec.z = pOrient->rvec.x * pOrient->lvec.y - pOrient->rvec.y * pOrient->lvec.x;

    pThing->thingInfo.actorInfo.bControlsDisabled = 1;
    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_PUSHPULLREADY, sithPlayerControls_PuppetCallback);
    pThing->moveStatus = SITHPLAYERMOVE_PUSHPULL_READY;
    sithPlayerControls_bActionActivated = 1;
    return true;
}

static void sithPlayerControls_ProcessPushMove(SithThing* pThing)
{
    SithThing* pPushThing = sithPlayerControls_pCollidedThing;
    if ( !pPushThing )
    {
        return;
    }

    rdVector3 moveNorm;
    if ( !sithPlayerControls_GetPushPullMoveNorm(&moveNorm, &pThing->orient.lvec, 15.0f) )
    {
        return;
    }

    // Check that the pushed thing can move
    bool bCanMove = true;
    float radius  = pPushThing->collide.movesize < 0.1f ? 0.050000001f : 0.094999999f;
    sithCollision_SearchForCollisions(pPushThing->pInSector, pPushThing, &pPushThing->pos, &moveNorm, 0.19f, radius, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            if ( pCollision->pThingCollided == pThing )
            {
                continue;
            }

            bCanMove = false;
        }

        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            bCanMove = false;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bCanMove )
    {
        sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x6F);
        sithPlayerControls_StopPushPull(pThing);
        return;
    }

    // Check that there's floor at the new position
    rdVector3 checkPos;
    rdVector_Scale3(&checkPos, &moveNorm, 0.2f);
    rdVector_Add3Acc(&checkPos, &pPushThing->pos);

    rdVector3 downDir;
    rdVector_Neg3(&downDir, &rdroid_g_zVector3);

    SithSector* pSector = sithCollision_FindSectorInRadius(pPushThing->pInSector, &pPushThing->pos, &checkPos, 0.0f);
    if ( !pSector )
    {
        return;
    }

    bool bFloor = false;
    sithCollision_SearchForCollisions(pSector, NULL, &checkPos, &downDir, 0.16f, 0.0099999998f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            if ( pCollision->pThingCollided != pPushThing )
            {
                bFloor = true;
                break;
            }
        }
        else if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 && pCollision->pSurfaceCollided )
        {
            const SithSurface* pSurface = pCollision->pSurfaceCollided;
            const rdVector3* pNormal    = &pSurface->face.normal;
            if ( pNormal->y * rdroid_g_zVector3.y + pNormal->z * rdroid_g_zVector3.z + pNormal->x * rdroid_g_zVector3.x > 0.99900001f )
            {
                if ( (pThing->attach.flags & SITH_ATTACH_SURFACE) == 0 )
                {
                    bFloor = true;
                }
                else
                {
                    // Pushed thing has to stay on the same floor level as player
                    float dz = sithPlayerControls_GetWorldVertexZ(pSurface->face.aVertices) - sithPlayerControls_GetWorldVertexZ(pThing->attach.pFace->aVertices);
                    bFloor = fabsf(dz) < 0.0049999999f;
                }

                break;
            }
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bFloor )
    {
        sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x6F);
        sithPlayerControls_StopPushPull(pThing);
        return;
    }

    if ( !strncmp(pPushThing->aName, "pushgear", 8) )
    {
        // Gear can be pushed only along its look direction
        const rdVector3* pLook = &pPushThing->orient.lvec;
        if ( fabsf(pLook->z * moveNorm.z + pLook->y * moveNorm.y + pLook->x * moveNorm.x) < 0.98000002f )
        {
            sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x6F);
            sithPlayerControls_pCollidedThing = NULL;
            return;
        }
    }

    sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_PUSHPULLREADY);
    pThing->thingInfo.actorInfo.bControlsDisabled = 0;
    sithPlayerActions_PushItem(pThing, pPushThing, &moveNorm);
}

// Returns true if no thing (except pIgnoreThing) or world surface blocks the move
static bool sithPlayerControls_CanPullMove(SithSector* pSector, SithThing* pThing, const rdVector3* pPos, const rdVector3* pDir, float distance, float radius, const SithThing* pIgnoreThing)
{
    bool bCanMove = true;
    sithCollision_SearchForCollisions(pSector, pThing, pPos, pDir, distance, radius, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            if ( pCollision->pThingCollided == pIgnoreThing )
            {
                continue;
            }

            bCanMove = false;
        }

        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            bCanMove = false;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return bCanMove;
}

static void sithPlayerControls_ProcessPullMove(SithThing* pThing)
{
    SithThing* pPullThing = sithPlayerControls_pCollidedThing;
    if ( !pPullThing )
    {
        return;
    }

    rdVector3 moveNorm;
    if ( !sithPlayerControls_GetPushPullMoveNorm(&moveNorm, &pThing->orient.lvec, 15.0f) )
    {
        return;
    }

    rdVector3 pullDir;
    rdVector_Neg3(&pullDir, &moveNorm);

    float radius;
    if ( pPullThing->collide.movesize < 0.1f )
    {
        radius = 0.050000001f;
    }
    else
    {
        radius = 0.094999999f;
        if ( pThing->pos.z > pPullThing->pos.z
            && (double)pThing->pos.z - (double)pPullThing->pos.z > 0.02
            && strncmp(pPullThing->aName, "pushgear", 8) )
        {
            // Player is standing too high above the thing
            sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x73);
            sithPlayerControls_StopPushPull(pThing);
            return;
        }
    }

    // Check that both pulled thing and player can move backwards
    float moveDist = pThing->collide.movesize + 0.19f;
    if ( !sithPlayerControls_CanPullMove(pPullThing->pInSector, pPullThing, &pPullThing->pos, &pullDir, moveDist, radius, pThing)
        || !sithPlayerControls_CanPullMove(pThing->pInSector, pThing, &pThing->pos, &pullDir, moveDist, pThing->collide.movesize, pThing) )
    {
        sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x73);
        sithPlayerControls_StopPushPull(pThing);
        return;
    }

    rdVector_Neg3Acc(&moveNorm);

    // Check floor at the new position of pulled thing
    rdVector3 checkPos;
    rdVector_Scale3(&checkPos, &moveNorm, 0.2f);
    rdVector_Add3Acc(&checkPos, &pPullThing->pos);

    rdVector3 downDir;
    rdVector_Neg3(&downDir, &rdroid_g_zVector3);

    SithSector* pSector = sithCollision_FindSectorInRadius(pPullThing->pInSector, &pPullThing->pos, &checkPos, 0.0f);
    if ( !pSector )
    {
        sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x73);
        sithPlayerControls_StopPushPull(pThing);
        return;
    }

    bool bFloor = false;
    sithCollision_SearchForCollisions(pSector, NULL, &checkPos, &downDir, 0.17f, 0.039999999f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            if ( pCollision->pThingCollided != pThing )
            {
                bFloor = true;
                break;
            }
        }
        else if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 && pCollision->pSurfaceCollided )
        {
            const rdVector3* pNormal = &pCollision->pSurfaceCollided->face.normal;
            if ( pNormal->z * rdroid_g_zVector3.z + pNormal->y * rdroid_g_zVector3.y + pNormal->x * rdroid_g_zVector3.x > 0.99900001f )
            {
                bFloor = true;
                break;
            }
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bFloor )
    {
        sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x73);
        sithPlayerControls_StopPushPull(pThing);
        return;
    }

    // Check floor at the new position of player
    rdVector_Scale3(&checkPos, &moveNorm, 0.2f);
    rdVector_Add3Acc(&checkPos, &pThing->pos);
    rdVector_Neg3(&downDir, &rdroid_g_zVector3);

    pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &checkPos, 0.0f);
    if ( !pSector )
    {
        sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x73);
        sithPlayerControls_StopPushPull(pThing);
        return;
    }

    bFloor = false;
    sithCollision_SearchForCollisions(pSector, NULL, &checkPos, &downDir, 0.063000001f, 0.039999999f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            if ( pCollision->pThingCollided == pThing )
            {
                continue;
            }

            bFloor = pCollision->distance >= 0.02f && pCollision->distance <= 0.07f;
            break;
        }

        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 && pCollision->pSurfaceCollided )
        {
            const SithSurface* pSurface = pCollision->pSurfaceCollided;
            if ( pCollision->distance < 0.02f || pCollision->distance > 0.07f )
            {
                bFloor = false;
                break;
            }

            const rdVector3* pNormal = &pSurface->face.normal;
            if ( pNormal->z * rdroid_g_zVector3.z + pNormal->y * rdroid_g_zVector3.y + pNormal->x * rdroid_g_zVector3.x <= 0.99900001f )
            {
                continue;
            }

            if ( (pPullThing->attach.flags & SITH_ATTACH_SURFACE) == 0 )
            {
                bFloor = true;
            }
            else
            {
                float dz = sithPlayerControls_GetWorldVertexZ(pSurface->face.aVertices) - sithPlayerControls_GetWorldVertexZ(pPullThing->attach.pFace->aVertices);
                bFloor = fabsf(dz) < 0.0049999999f;
            }

            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bFloor )
    {
        sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x73);
        sithPlayerControls_StopPushPull(pThing);
        return;
    }

    if ( !strncmp(pPullThing->aName, "pushgear", 8) )
    {
        const rdVector3* pLook = &pPullThing->orient.lvec;
        if ( fabsf(pLook->z * moveNorm.z + pLook->y * moveNorm.y + pLook->x * moveNorm.x) < 0.98000002f )
        {
            sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x73);
            sithPlayerControls_pCollidedThing = NULL;
            return;
        }
    }

    sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_PUSHPULLREADY);
    rdVector_Neg3Acc(&moveNorm);
    pThing->thingInfo.actorInfo.bControlsDisabled = 0;
    sithPlayerActions_PullItem(pThing, pPullThing, &moveNorm);
}

void J3DAPI sithPlayerControls_ProcessGeneralMove(SithThing* pThing, float secDeltaTime)
{
    SithActorInfo* pActor     = &pThing->thingInfo.actorInfo;
    SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;
    bool bMoved               = false;
    rdKeyframe* pKey;
    int bFloorItem;

    sithPlayerActions_g_pCurCollidedModelFace = NULL;
    sithPlayerActions_g_pCurCollidedModel     = NULL;
    sithPlayerActions_g_pCurCollidedSurface   = NULL;

    if ( pActor->bControlsDisabled )
    {
        if ( pThing->moveStatus == SITHPLAYERMOVE_JUMP_READY )
        {
            if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
            {
                sithPlayerControls_curJumpDirection = 1;
            }
            else
            {
                sithPlayerControls_curJumpDirection = sithControl_GetKey(SITHCONTROL_BACK, NULL) ? -1 : 0;
            }

            return;
        }

        if ( pThing->moveStatus != SITHPLAYERMOVE_PUSHPULL_READY )
        {
            return;
        }
    }

    bool bRun   = false;
    float speed = 1.0f;
    if ( (sithControl_g_controlOptions & SITHCONTROL_OPTION_ALWAYSRUN) != 0 || sithControl_GetKey(SITHCONTROL_RUNFWD, NULL) || sithControl_GetKey(SITHCONTROL_ACT1, NULL) )
    {
        bRun  = true;
        speed = 2.5f;
    }

    if ( (sithControl_g_controlOptions & SITHCONTROL_OPTION_ALWAYSRUN) != 0 && sithControl_GetKey(SITHCONTROL_ACT1, NULL) )
    {
        bRun  = false;
        speed = 1.0f;
    }

    SithAttachFlag attachFlags = pThing->attach.flags;
    bool bAttachedToSurface    = (attachFlags & SITH_ATTACH_SURFACE) != 0;
    if ( bAttachedToSurface && (pThing->attach.attachedToStructure.pSurfaceAttached->flags & (SITH_SURFACE_WATER | SITH_SURFACE_SHALLOWWATER | SITH_SURFACE_WEB)) != 0 )
    {
        speed = speed * 0.80000001f;
    }

    switch ( pThing->moveStatus )
    {
        case SITHPLAYERMOVE_STILL:
        {
            if ( (pActor->flags & SITH_AF_IMMOBILE) == 0 )
            {
                sithPlayerControls_CheckLevelRoll(pThing);

                if ( sithControl_GetKey(SITHCONTROL_CAMERAZOOMOUT, NULL) )
                {
                    sithPlayerActions_Jump(pThing, 1.0f, 99);
                    return;
                }

                if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) && !sithPlayerControls_bJumpActivated )
                {
                    if ( sithPlayerActions_CanClimbOn2m(pThing) == 1 && !sithPlayerActions_HasActiveWeapon(pThing) )
                    {
                        sithPlayerActions_ClimbOn2m(pThing);
                        return;
                    }

                    if ( sithPlayerActions_CanClimbOn1m(pThing) == 1 && !sithPlayerActions_HasActiveWeapon(pThing) )
                    {
                        sithPlayerActions_ClimbOn1m(pThing);
                        return;
                    }

                    if ( sithInventory_GetCurrentWeapon(pThing) == SITHWEAPON_NO_WEAPON && !sithWeapon_IsMountingWeapon(pThing) )
                    {
                        if ( sithPlayerControls_TryMountWall(pThing) )
                        {
                            return;
                        }
                    }

                    if ( !pActor->bForceMovePlay )
                    {
                        sithPlayerControls_ProcessJump(pThing);
                        return;
                    }
                }

                if ( sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, NULL) && !pActor->bForceMovePlay && !sithPlayerActions_HasActiveWeapon(pThing) )
                {
                    if ( sithPlayerControls_CanCrawl(pThing) )
                    {
                        sithPlayerActions_Stand2Crawl(pThing);
                        return;
                    }

                    // Can't crawl here
                    sithPlayerControls_PlayComment(pThing, (SithSoundClassMode)0x66);
                }

                if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
                {
                    if ( !pActor->bForceMovePlay )
                    {
                        if ( sithControl_GetKey(SITHCONTROL_ACT3, NULL) )
                        {
                            if ( pThing->attach.flags )
                            {
                                pThing->moveStatus = SITHPLAYERMOVE_JUMPROLLFWD;
                                sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_JUMPROLLFWD, NULL);
                                return;
                            }
                        }
                        else if ( !pActor->bControlsDisabled )
                        {
                            rdVector3 movePos;
                            movePos.x = pThing->collide.movesize * pThing->orient.lvec.x + pThing->pos.x;
                            movePos.y = pThing->collide.movesize * pThing->orient.lvec.y + pThing->pos.y;
                            movePos.z = pThing->collide.movesize * pThing->orient.lvec.z + pThing->pos.z;

                            int unused;
                            if ( !bRun && !sithPlayerActions_CanMoveToPos(pThing, &movePos, &unused) )
                            {
                                return;
                            }

                            pKey = sithPuppet_GetKeyframe("in_stand_bd_walk.key");
                            if ( pKey )
                            {
                                pActor->bControlsDisabled = 1;
                                pThing->moveStatus = bRun ? SITHPLAYERMOVE_STAND2RUN : SITHPLAYERMOVE_STAND2WALK;
                                sithPuppet_PlayKey(pThing->renderData.pPuppet, pKey, 1, 2, (rdKeyframeFlags)0x22, sithPlayerControls_PuppetCallback);
                                sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_STAND);
                                sithPuppet_PlayMode(pThing, bRun ? SITHPUPPETSUBMODE_RUN : SITHPUPPETSUBMODE_WALK, NULL);
                                return;
                            }
                        }
                    }
                }
                else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
                {
                    switch ( sithPlayerActions_SearchClimbSurface(pThing) )
                    {
                        case 1:
                            sithPlayerActions_ClimbToClimb(pThing, 0);
                            return;

                        case 2:
                            sithPlayerActions_ClimbToHang(pThing);
                            return;

                        case 3:
                            sithPlayerActions_ClimbToClimb(pThing, 1);
                            return;

                        default:
                            break;
                    }

                    if ( sithControl_GetKey(SITHCONTROL_ACT3, NULL) )
                    {
                        if ( pThing->attach.flags )
                        {
                            pThing->moveStatus = SITHPLAYERMOVE_JUMPROLLBACK;
                            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_JUMPROLLBACK, NULL);
                            return;
                        }
                    }
                    else
                    {
                        pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                        bMoved = true;
                        pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * -1.0f * 0.5f;
                    }
                }
                else
                {
                    pPhysics->thrust.y = 0.0f;
                }

                if ( sithControl_GetKey(SITHCONTROL_STPLEFT, NULL) )
                {
                    if ( !pActor->bForceMovePlay && sithPlayerControls_CanStrafeMove(pThing, 0) )
                    {
                        sithPlayerActions_StrafeLeft(pThing);
                        sithThing_SyncThing(pThing, SITHTHING_SYNC_MOVEPOS);
                    }

                    return;
                }

                if ( sithControl_GetKey(SITHCONTROL_STPRIGHT, NULL) )
                {
                    if ( !pActor->bForceMovePlay && sithPlayerControls_CanStrafeMove(pThing, 1) )
                    {
                        sithPlayerActions_StrafeRight(pThing);
                        sithThing_SyncThing(pThing, SITHTHING_SYNC_MOVEPOS);
                    }

                    return;
                }

                pPhysics->thrust.x = 0.0f;
                pPhysics->angularVelocity.y = 0.0f;
            }
            else
            {
                speed = 0.1f;
            }

            if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    if ( sithControl_GetKey(SITHCONTROL_ACT3, NULL) && (pActor->flags & SITH_AF_IMMOBILE) == 0 )
                    {
                        if ( pThing->attach.flags )
                        {
                            pThing->moveStatus = SITHPLAYERMOVE_JUMPRIGHT;
                            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_HOPRIGHT, NULL);
                            return;
                        }
                    }
                    else
                    {
                        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, speed);
                        pPhysics->angularVelocity.y = pPhysics->angularVelocity.y * (sithControl_GetKey(SITHCONTROL_ACT1, NULL) ? 2.5f : 0.71428573f);
                        bMoved = true;
                    }
                }
            }
            else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    if ( sithControl_GetKey(SITHCONTROL_ACT3, NULL) && (pActor->flags & SITH_AF_IMMOBILE) == 0 )
                    {
                        if ( pThing->attach.flags )
                        {
                            pThing->moveStatus = SITHPLAYERMOVE_JUMPLEFT;
                            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_HOPLEFT, NULL);
                            return;
                        }
                    }
                    else
                    {
                        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, speed);
                        pPhysics->angularVelocity.y = pPhysics->angularVelocity.y * (sithControl_GetKey(SITHCONTROL_ACT1, NULL) ? 2.5f : 0.71428573f);
                        bMoved = true;
                    }
                }
            }
            else
            {
                pPhysics->angularVelocity.y = 0.0f;
            }

            if ( (pActor->flags & SITH_AF_IMMOBILE) != 0 )
            {
                return;
            }

            if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 )
            {
                sithPlayerControls_ApplyMouseTurn(pThing, sithPlayerControls_MinSpeed(speed));
            }

            if ( !sithControl_GetKey(SITHCONTROL_ACT2, NULL) || sithPlayerControls_bActionActivated )
            {
                return;
            }

            if ( !pActor->bForceMovePlay && !pActor->bControlsDisabled && !sithWeapon_IsMountingWeapon(pThing) )
            {
                SithThing* pActivatedThing = sithCollision_FindActivatedThing(pThing, &bFloorItem);
                if ( pActivatedThing )
                {
                    sithPlayerControls_StartActivate(pThing, pActivatedThing);

                    rdKeyframeFlags keyFlags = (rdKeyframeFlags)2;
                    if ( bFloorItem )
                    {
                        pKey = sithPuppet_GetKeyframeByIndex(0x808A); // pick up
                    }
                    else if ( sithInventory_GetCurrentWeapon(pThing) == SITHWEAPON_ZIPPO )
                    {
                        pKey     = sithPuppet_GetKeyframeByIndex(0x8091);
                        keyFlags = (rdKeyframeFlags)0x20;
                    }
                    else
                    {
                        pKey = sithPuppet_GetKeyframeByIndex(0x808B);
                    }

                    if ( pKey )
                    {
                        sithPuppet_PlayKey(pThing->renderData.pPuppet, pKey, 5, 8, keyFlags, sithPlayerControls_PuppetCallback);
                    }

                    return;
                }

                if ( sithPlayerActions_Activate(pThing) == 1 )
                {
                    return;
                }

                if ( sithPlayerControls_TryPushPull(pThing) )
                {
                    return;
                }

                if ( !sithPlayerControls_bActionActivated && sithPlayerControls_TryBoardVehicle(pThing, 0) )
                {
                    sithPlayerControls_bActionActivated = 1;
                    return;
                }
            }

            if ( !bMoved && !pActor->bControlsDisabled )
            {
                pThing->moveStatus = SITHPLAYERMOVE_STILL;
            }
        } return;

        case SITHPLAYERMOVE_WALKING:
            sithPlayerControls_CheckLevelRoll(pThing);

            if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) && !sithPlayerControls_bJumpActivated && !pActor->bForceMovePlay )
            {
                if ( sithPlayerActions_CanClimbOn2m(pThing) == 1 && !sithPlayerActions_HasActiveWeapon(pThing) )
                {
                    goto climb_on_2m;
                }

                if ( sithPlayerActions_CanClimbOn1m(pThing) == 1 && !sithPlayerActions_HasActiveWeapon(pThing) )
                {
                    goto climb_on_1m;
                }

                sithPlayerControls_ProcessJump(pThing);
                return;
            }

            if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    if ( sithControl_GetKey(SITHCONTROL_ACT3, NULL) && pThing->attach.flags )
                    {
                        sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_WALK);
                        pThing->moveStatus = SITHPLAYERMOVE_JUMPROLLFWD;
                        sithPhysics_ResetThingMovement(pThing);
                        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_JUMPROLLFWD, NULL);
                        return;
                    }

                    pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * 0.89999998f;
                    if ( bRun )
                    {
                        pThing->moveStatus = SITHPLAYERMOVE_RUNNING;
                        if ( sithPuppet_GetModeTrack(pThing, SITHPUPPETSUBMODE_WALK) )
                        {
                            sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_WALK);
                        }

                        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_RUN, NULL);
                    }
                    else
                    {
                        pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                    }

                    if ( speed != 1.0f )
                    {
                        pPhysics->thrust.y = pPhysics->thrust.y * speed;
                    }

                    bMoved = true;
                }
            }
            else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    if ( sithControl_GetKey(SITHCONTROL_ACT3, NULL) && pThing->attach.flags )
                    {
                        sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_WALKBACK);
                        pThing->moveStatus = SITHPLAYERMOVE_JUMPROLLBACK;
                        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_JUMPROLLBACK, NULL);
                        return;
                    }

                    bMoved = true;
                    pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * -1.0f * 0.5f;
                }
            }
            else
            {
                pPhysics->thrust.y = 0.0f;
            }

            if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, speed);
                    bMoved = true;
                }
            }
            else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, speed);
                    bMoved = true;
                }
            }
            else
            {
                pPhysics->angularVelocity.y = 0.0f;
            }

            if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 )
            {
                sithPlayerControls_ApplyMouseTurn(pThing, sithPlayerControls_MinSpeed(speed));
            }

            if ( pActor->bForceMovePlay || bMoved || pActor->bControlsDisabled )
            {
                return;
            }

            pKey = sithPuppet_GetKeyframe("in_walk_bd_stand.key");
            if ( !pKey )
            {
                return;
            }

            pThing->moveStatus = SITHPLAYERMOVE_WALK2STAND;
            pActor->bControlsDisabled = 1;
            if ( sithPuppet_GetModeTrack(pThing, SITHPUPPETSUBMODE_WALK) )
            {
                sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_WALK);
            }
            else if ( sithPuppet_GetModeTrack(pThing, SITHPUPPETSUBMODE_WALKBACK) )
            {
                sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_WALKBACK);
            }

            sithPlayerControls_PlayWalk2Stand(pThing, pKey);
            return;

        case SITHPLAYERMOVE_RUNNING:
            if ( pActor->bForceMovePlay )
            {
                return;
            }

            if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) && !sithPlayerControls_bJumpActivated && !pActor->bForceMovePlay )
            {
                if ( sithPlayerActions_CanClimbOn2m(pThing) == 1 && !sithPlayerActions_HasActiveWeapon(pThing) )
                {
                    goto climb_on_2m;
                }

                if ( sithPlayerActions_CanClimbOn1m(pThing) == 1 && !sithPlayerActions_HasActiveWeapon(pThing) )
                {
                    goto climb_on_1m;
                }

                sithPuppet_g_bPlayerLeapForward = 1;
                return;
            }

            if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
            {
                if ( sithControl_GetKey(SITHCONTROL_ACT3, NULL) && pThing->attach.flags )
                {
                    sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_RUN);
                    sithPhysics_ResetThingMovement(pThing);
                    pThing->moveStatus = SITHPLAYERMOVE_JUMPROLLFWD;
                    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_JUMPROLLFWD, NULL);
                    return;
                }

                pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * 0.89999998f;
                if ( bRun )
                {
                    pThing->moveStatus = SITHPLAYERMOVE_RUNNING;
                }
                else
                {
                    pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                    if ( sithPuppet_GetModeTrack(pThing, SITHPUPPETSUBMODE_RUN) )
                    {
                        sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_RUN);
                    }

                    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_WALK, NULL);
                }

                if ( speed != 1.0f )
                {
                    pPhysics->thrust.y = pPhysics->thrust.y * speed;
                }

                bMoved = true;
            }
            else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
            {
                pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                bMoved = true;
                pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * -1.0f * 0.5f;
            }
            else
            {
                pPhysics->thrust.y = 0.0f;
            }

            if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
            {
                pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, speed);
                bMoved = true;
            }
            else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
            {
                pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, speed);
                bMoved = true;
            }
            else
            {
                pPhysics->angularVelocity.y = 0.0f;
            }

            if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 )
            {
                sithPlayerControls_ApplyMouseTurn(pThing, sithPlayerControls_MinSpeed(speed));
            }

            if ( bMoved || pActor->bControlsDisabled )
            {
                return;
            }

            pKey = sithPuppet_GetKeyframe("in_walk_bd_stand.key");
            if ( !pKey )
            {
                return;
            }

            pThing->moveStatus = SITHPLAYERMOVE_WALK2STAND;
            pActor->bControlsDisabled = 1;
            sithPuppet_ClearMode(pThing, SITHPUPPETSUBMODE_RUN);
            sithPlayerControls_PlayWalk2Stand(pThing, pKey);
            return;

        case SITHPLAYERMOVE_CRAWL_STILL:
            if ( attachFlags && bAttachedToSurface && (pThing->attach.attachedToStructure.pSurfaceAttached->flags & (SITH_SURFACE_WATER | SITH_SURFACE_SHALLOWWATER)) != 0 )
            {
                sithPlayerActions_Crawl2Stand(pThing);
                return;
            }

            if ( sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, NULL) && !pActor->bForceMovePlay )
            {
                sithPlayerActions_Crawl2Stand(pThing);
                return;
            }

            // Note, original adds secDeltaTime to the crawl thrust
            if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    pPhysics->thrust.y = secDeltaTime;
                    pPhysics->thrust.y = pActor->maxThrust * 0.5f + secDeltaTime;
                }
            }
            else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
            {
                if ( !pActor->bForceMovePlay )
                {
                    float backDelta = secDeltaTime * -1.0f;
                    pPhysics->thrust.y = backDelta;
                    pPhysics->thrust.y = pActor->maxThrust * -1.0f * 0.5f + backDelta;
                }
            }
            else
            {
                pPhysics->thrust.y = 0.0f;
            }

            if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
            {
                pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -0.5f, speed);
            }
            else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
            {
                pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 0.5f, speed);
            }
            else
            {
                pPhysics->angularVelocity.y = 0.0f;
            }

            if ( sithControl_GetKey(SITHCONTROL_ACT2, NULL) && !pActor->bForceMovePlay )
            {
                if ( sithPlayerActions_Activate(pThing) == 1 )
                {
                    return;
                }

                SithThing* pActivatedThing = sithCollision_FindActivatedThing(pThing, &bFloorItem);
                if ( pActivatedThing )
                {
                    sithPlayerControls_StartActivate(pThing, pActivatedThing);
                    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_ACTIVATE, sithPlayerControls_PuppetCallback);
                    return;
                }
            }

            if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 )
            {
                sithPlayerControls_ApplyMouseTurn(pThing, sithPlayerControls_MinSpeed(speed));
            }
            return;

        case SITHPLAYERMOVE_PUSHPULL_READY:
            if ( pActor->bForceMovePlay )
            {
                return;
            }

            if ( !sithControl_GetKey(SITHCONTROL_ACT2, NULL) )
            {
                sithPlayerControls_StopPushPull(pThing);
                return;
            }

            if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
            {
                sithPlayerControls_ProcessPushMove(pThing);
            }
            else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
            {
                sithPlayerControls_ProcessPullMove(pThing);
            }
            return;

        case SITHPLAYERMOVE_SLIDEDOWNFORWARD:
            if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) && !sithPlayerControls_bJumpActivated )
            {
                SithPuppetTrack* pTrack = sithPuppet_GetModeTrack(pThing, SITHPUPPETSUBMODE_SLIDEDOWNFWD);
                if ( pTrack )
                {
                    sithPuppet_StopKey(pThing->renderData.pPuppet, pTrack->trackNum, 0.0f);
                    sithPuppet_RemoveTrack(pThing, pTrack);
                }

                sithPhysics_ResetThingMovement(pThing);
                pThing->moveStatus = SITHPLAYERMOVE_LEAPFWD;
                sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_LEAPLEFT, NULL);
                sithPlayerControls_bJumpActivated = 1;
            }
            return;

        default:
            return;
    }

climb_on_2m:
    sithPuppet_RemoveAllTracks(pThing);
    sithPhysics_ResetThingMovement(pThing);
    sithPlayerActions_ClimbOn2m(pThing);
    return;

climb_on_1m:
    sithPuppet_RemoveAllTracks(pThing);
    sithPhysics_ResetThingMovement(pThing);
    sithPlayerActions_ClimbOn1m(pThing);
}

static SithSurface* sithPlayerControls_SearchClimbDir(SithThing* pThing, float x, float z, int climbDir, int* pbHitNonClimbSurface)
{
    rdVector3 moveNorm;
    moveNorm.x = x;
    moveNorm.y = 0.0f;
    moveNorm.z = z;
    rdMatrix_TransformVector34Acc(&moveNorm, &pThing->orient);
    rdVector_Normalize3Acc(&moveNorm);
    return sithPlayerControls_SearchForClimbSurface(pThing, &moveNorm, climbDir, pbHitNonClimbSurface);
}

void J3DAPI sithPlayerControls_ProcessClimbMove(SithThing* pThing)
{
    SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
    if ( pActor->bControlsDisabled || pActor->bForceMovePlay == 1 || (pThing->type != SITH_THING_ACTOR && pThing->type != SITH_THING_PLAYER) )
    {
        return;
    }

    rdVector_Set3(&pThing->moveInfo.physics.velocity, 0.0f, 0.0f, 0.0f);

    SithSurface* pSurf;
    int bHitNonClimbSurface;
    if ( sithControl_GetKey(SITHCONTROL_ACT2, NULL) && !pActor->bForceMovePlay )
    {
        pSurf = sithPlayerControls_SearchClimbDir(pThing, 0.0f, 1.0f, 1, &bHitNonClimbSurface);
        if ( !pSurf && !bHitNonClimbSurface && sithPlayerActions_CanPullUp(pThing) == 1 )
        {
            goto pull_up;
        }
    }

    if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) && !sithPlayerControls_bJumpActivated && !pActor->bForceMovePlay )
    {
        sithPlayerActions_UnmountWall(pThing, sithPlayerActions_climbPupTrackNum);
        sithPlayerControls_bJumpActivated = 1;
        sithPlayerActions_climbPupTrackNum = -1;
        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
    {
        if ( pActor->bForceMovePlay )
        {
            return;
        }

        pSurf = sithPlayerControls_SearchClimbDir(pThing, 0.0f, 1.0f, 1, &bHitNonClimbSurface);
        if ( !pSurf )
        {
            if ( !bHitNonClimbSurface )
            {
                goto pull_up;
            }
        }
        else if ( !bHitNonClimbSurface )
        {
            sithPlayerActions_ClimbMove(pThing, pSurf, 1);
        }

        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
    {
        if ( !pActor->bForceMovePlay )
        {
            pSurf = sithPlayerControls_SearchClimbDir(pThing, 0.0f, -1.0f, 2, &bHitNonClimbSurface);
            if ( pSurf )
            {
                sithPlayerActions_ClimbMove(pThing, pSurf, 2);
            }
        }

        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
    {
        if ( !pActor->bForceMovePlay )
        {
            pSurf = sithPlayerControls_SearchClimbDir(pThing, 1.0f, 0.0f, 3, &bHitNonClimbSurface);
            if ( pSurf )
            {
                sithPlayerActions_ClimbMove(pThing, pSurf, 3);
            }
        }

        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) && !pActor->bForceMovePlay )
    {
        pSurf = sithPlayerControls_SearchClimbDir(pThing, -1.0f, 0.0f, 4, &bHitNonClimbSurface);
        if ( pSurf )
        {
            sithPlayerActions_ClimbMove(pThing, pSurf, 4);
        }
    }

    return;

pull_up:
    sithPuppet_StopKey(pThing->renderData.pPuppet, sithPlayerActions_climbPupTrackNum, 0.0f);
    sithPlayerActions_climbPupTrackNum = -1;
    sithPlayerActions_ClimbPullUp(pThing);
}

// Returns true if nothing blocks the move in pDir direction
static bool sithPlayerControls_CanJewelFlyMove(SithThing* pThing, const rdVector3* pDir, float distance, int searchFlags)
{
    bool bCanMove = true;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, pDir, distance, 0.050000001f, searchFlags);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & (SITHCOLLISION_WORLD | SITHCOLLISION_THING)) != 0
            || ((pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0 && (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_NOPLAYERMOVE) != 0) )
        {
            bCanMove = false;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return bCanMove;
}

static void sithPlayerControls_StopJewelFlyKey(SithThing* pThing)
{
    if ( sithPlayerActions_g_jewelFlyingPuppetTrackNum >= 0 )
    {
        sithPuppet_StopKey(pThing->renderData.pPuppet, sithPlayerActions_g_jewelFlyingPuppetTrackNum, 0.0f);
    }

    sithPlayerActions_g_jewelFlyingPuppetTrackNum = -1;
}

void J3DAPI sithPlayerControls_ProcessJewelFlyMove(SithThing* pThing)
{
    SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;
    int state;

    if ( pThing->thingInfo.actorInfo.bControlsDisabled == 1 )
    {
        return;
    }

    pThing->collide.type = SITH_COLLIDE_SPHERE;
    if ( (pPhysics->flags & SITH_PF_FLY) == 0 || pThing->attach.flags )
    {
        pThing->moveStatus = pThing->attach.flags ? SITHPLAYERMOVE_STILL : SITHPLAYERMOVE_FALLING;
        sithPlayerControls_StopJewelFlyKey(pThing);
        sithPlayerActions_DisableJewelFlying();
        return;
    }

    pPhysics->thrust.y = 0.0f;
    pPhysics->thrust.x = 0.0f;

    bool bCanMoveUp = sithPlayerControls_CanJewelFlyMove(pThing, &rdroid_g_zVector3, 0.12f, 0xA00);

    rdVector3 downDir;
    rdVector_Neg3(&downDir, &rdroid_g_zVector3);
    bool bCanMoveDown = sithPlayerControls_CanJewelFlyMove(pThing, &downDir, 0.1f, 0x200);

    if ( sithControl_GetKey(SITHCONTROL_FORWARD, &state) )
    {
        if ( bCanMoveUp )
        {
            pPhysics->thrust.z = sithTime_g_fps * 0.00019999999f + pPhysics->thrust.z;
            if ( pPhysics->thrust.z > 1.0f )
            {
                pPhysics->thrust.z = 1.0f;
            }
        }
        else
        {
            pPhysics->velocity.z = 0.0f;
            sithPhysics_ResetThingMovement(pThing);
        }
    }
    else if ( sithControl_GetKey(SITHCONTROL_BACK, &state) )
    {
        if ( bCanMoveDown )
        {
            pPhysics->thrust.z = pPhysics->thrust.z - sithTime_g_fps * 0.00019999999f;
            if ( pPhysics->thrust.z < -1.0f )
            {
                pPhysics->thrust.z = -1.0f;
            }
        }
        else
        {
            pPhysics->velocity.z = 0.0f;
            sithPhysics_ResetThingMovement(pThing);
        }
    }
    else
    {
        pPhysics->thrust.z = 0.0f;
        if ( !bCanMoveUp )
        {
            pPhysics->velocity.z = 0.0f;
            sithPhysics_ResetThingMovement(pThing);
        }

        if ( !bCanMoveDown )
        {
            pPhysics->velocity.z = 0.0f;
            sithPhysics_ResetThingMovement(pThing);
        }
    }

    if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, &state) )
    {
        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, 1.0f);
    }
    else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, &state) )
    {
        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, 1.0f);
    }
    else
    {
        pPhysics->angularVelocity.y = 0.0f;
    }

    if ( !sithControl_GetKey(SITHCONTROL_JUMP, &state) )
    {
        return;
    }

    // Stop flying
    rdVector3 vel;
    rdVector_Set3(&vel, 0.0f, 0.5f, 0.0f);
    rdMatrix_TransformVector34Acc(&vel, &pThing->orient);
    rdVector_Copy3(&pPhysics->velocity, &vel);

    pThing->moveStatus = SITHPLAYERMOVE_FALLING;
    sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_FALL, NULL);
    sithPlayerControls_StopJewelFlyKey(pThing);

    pPhysics->flags = (pPhysics->flags & ~SITH_PF_FLY) | SITH_PF_USEGRAVITY;
    sithPlayer_g_impFireType = -1;
    sithWeapon_DeselectWeapon(pThing);
    sithPlayerActions_DisableJewelFlying();
    sithInventory_SetSwimmingInventory(pThing, 1);
}

static void sithPlayerControls_StartMountFromWater(SithThing* pThing)
{
    sithPhysics_ResetThingMovement(pThing);
    float movesize = pThing->collide.movesize * 0.25f;
    pThing->moveStatus = SITHPLAYERMOVE_PULLINGUP;
    pThing->collide.movesize = movesize;
    rdVector_Copy3(&pThing->forceMoveStartPos, &pThing->pos);
    sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_MOUNTFROMWATER, NULL);
}

void J3DAPI sithPlayerControls_ProcessFlyAndUnderwaterMove(SithThing* pThing)
{
    SithActorInfo* pActor     = &pThing->thingInfo.actorInfo;
    SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;
    float speed               = 1.0f;
    int state;

    sithPlayerControls_curJumpDirection = 0;
    SITH_ASSERTREL(sithThing_ValidateThingPointer(sithWorld_g_pCurrentWorld, pThing));

    SithSector* pSector = pThing->pInSector;
    bool bUnderwater    = pSector && (pSector->flags & SITH_SECTOR_UNDERWATER) != 0;
    uint32_t bFly       = pPhysics->flags & SITH_PF_FLY;
    if ( bFly )
    {
        bUnderwater = false;
    }

    if ( bUnderwater )
    {
        if ( !sithControl_GetKey(SITHCONTROL_ACT2, NULL) )
        {
            pPhysics->thrust.y = 0.0f;
        }
        else if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * 0.89999998f;
        }
        else
        {
            int ledge = sithPlayerControls_CheckWaterLedge(pThing);
            if ( ledge == 1 )
            {
                sithPlayerControls_StartMountFromWater(pThing);
                return;
            }

            if ( ledge == 2 )
            {
                sithPlayerActions_ClimbOn1m(pThing);
                return;
            }
        }

        if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            pPhysics->thrust.z = 0.0f;
        }

        if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
        {
            pPhysics->angularVelocity.x = sithTime_g_fps * -1.0f;
        }
        else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
        {
            pPhysics->angularVelocity.x = sithTime_g_fps;
        }
        else
        {
            pPhysics->angularVelocity.x = 0.0f;
        }

        if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
        {
            pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, 1.0f);
        }
        else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
        {
            pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, 1.0f);
        }
        else
        {
            pPhysics->angularVelocity.y = 0.0f;
        }

        // Note, original requires the fly flag here which can't be set in underwater mode
        if ( !sithPlayerControls_IsZeroVector(&pPhysics->thrust) && !sithPlayerControls_IsZeroVector(&pActor->headPYR) && bFly && (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            sithPlayerControls_RotateThrustByHead(pThing);
        }

        return;
    }

    // Note, original stores this flag into the pThing argument slot, which is non-null when neither
    // the fly flag is set nor the thing is underwater, so this condition is always true.
    bool bFlyOrUnderwater = true;

    if ( (sithControl_g_controlOptions & SITHCONTROL_OPTION_ALWAYSRUN) != 0 || sithControl_GetKey(SITHCONTROL_ACT1, NULL) )
    {
        speed = 2.5f;
    }

    if ( pThing->type != SITH_THING_ACTOR && pThing->type != SITH_THING_PLAYER )
    {
        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
    {
        pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * 0.89999998f;
    }
    else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
    {
        pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * -1.0f * 0.5f;
    }
    else
    {
        pPhysics->thrust.y = 0.0f;
    }

    if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
    {
        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, speed);
    }
    else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
    {
        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, speed);
    }
    else
    {
        pPhysics->angularVelocity.y = 0.0f;
    }

    pPhysics->thrust.z = 0.0f;
    if ( (pPhysics->thrust.x != 0.0f || pPhysics->thrust.y != 0.0f)
        && !sithPlayerControls_IsZeroVector(&pActor->headPYR)
        && bFlyOrUnderwater
        && (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
    {
        sithPlayerControls_RotateThrustByHead(pThing);
    }

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 && pPhysics->angularVelocity.y == 0.0f )
    {
        sithPlayerControls_ApplyMouseTurn(pThing, 1.0f);
    }

    if ( bFlyOrUnderwater )
    {
        if ( sithControl_GetKey(SITHCONTROL_JUMP, &state) )
        {
            if ( pThing->attach.flags )
            {
                sithThing_DetachThing(pThing);
                pPhysics->thrust.z = pPhysics->thrust.z - -2.0f;
            }
            else
            {
                pPhysics->thrust.z = pActor->maxThrust * 0.5f + pPhysics->thrust.z;
            }
        }

        if ( sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, &state) )
        {
            pPhysics->thrust.z = pPhysics->thrust.z - pActor->maxThrust * 0.5f;
        }
    }
}

void J3DAPI sithPlayerControls_ProcessGeneralControls(SithThing* pThing)
{
    int state;
    sithControl_GetKey(SITHCONTROL_MAP, &state);
    if ( (state & 1) != 0 )
    {
        sithOverlayMap_ToggleMap();
    }

    if ( sithControl_GetKey(SITHCONTROL_INCREASE, &state) )
    {
        sithOverlayMap_ZoomIn();
    }

    if ( sithControl_GetKey(SITHCONTROL_DECREASE, &state) )
    {
        sithOverlayMap_ZoomOut();
    }

    if ( sithPlayerControls_bHealthActivated || !sithControl_GetKey(SITHCONTROL_HEALTH, &state) )
    {
        return;
    }

    // Use first available health item
    static const size_t aHealthItems[] = { 39, 49, 38, 48 };
    size_t healthItem = 0;
    for ( size_t i = 0; i < STD_ARRAYLEN(aHealthItems); ++i )
    {
        if ( sithInventory_GetInventory(pThing, aHealthItems[i]) != 0.0f && sithInventory_IsInventoryAvailable(pThing, aHealthItems[i]) )
        {
            healthItem = aHealthItems[i];
            break;
        }
    }

    if ( healthItem )
    {
        SithInventoryType* pType = sithInventory_GetType(healthItem);
        if ( pType->pCog )
        {
            sithCog_SendMessage(pType->pCog, SITHCOG_MSG_USER0, SITHCOG_SYM_REF_INT, (int)healthItem, SITHCOG_SYM_REF_THING, pThing->idx, 0);
        }
    }

    sithPlayerControls_bHealthActivated = 1;
}

int J3DAPI sithPlayerControls_ProcessEditorDebugControls(SithThing* pThing, float secDeltaTime)
{
    int bPressed;
    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 && !sithControl_GetKey(SITHCONTROL_DEBUG, NULL) )
    {
        return 0;
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        sithPhysics_ResetThingMovement(pThing);
    }

    for ( int keyId = SITHCONTROL_WSELECT0; keyId < SITHCONTROL_WSELECT10; ++keyId )
    {
        sithControl_GetKey((SithControlFunction)keyId, &bPressed);
        if ( bPressed )
        {
            sithPlayerActions_MoveToPlayerPosition(pThing, keyId - SITHCONTROL_WSELECT0);
            break;
        }
    }

    sithControl_GetKey(SITHCONTROL_JUMP, &bPressed);
    if ( bPressed )
    {
        sithActor_DamageActor(pThing, pThing, 200.0f, SITH_DAMAGE_IMPACT);
        return 1;
    }

    sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, &bPressed);
    if ( bPressed && sithCamera_g_pCurCamera != &sithCamera_g_aCameras[1] )
    {
        sithCamera_SetCurrentCamera(&sithCamera_g_aCameras[1]);
        return 1;
    }

    sithControl_GetKey(SITHCONTROL_ACT2, &bPressed);
    if ( bPressed )
    {
        // Reset player state
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        pThing->collide.movesize = 0.039999999f;
        pThing->thingInfo.actorInfo.bControlsDisabled = 0;
        pThing->thingInfo.actorInfo.bForceMovePlay    = 0;
        pThing->moveInfo.physics.flags  = (pThing->moveInfo.physics.flags & ~SITH_PF_ALIGNSURFACE) | SITH_PF_ALIGNUP;
        pThing->moveInfo.physics.height = 0.090000004f;
        sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_NORMAL);
        return 1;
    }

    sithControl_GetKey(SITHCONTROL_STPRIGHT, &bPressed);
    if ( bPressed && pThing->moveStatus != SITHPLAYERMOVE_WHIPSWINGING )
    {
        pThing->moveStatus = SITHPLAYERMOVE_WHIPSWINGING;
        rdVector_Copy3(&pThing->forceMoveStartPos, &pThing->pos);
        sithThing_DetachThing(pThing);
        sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_WHIPSWING, NULL);
    }

    sithControl_GetKey(SITHCONTROL_ACTIVATE, &bPressed);
    if ( bPressed )
    {
        sithCamera_CycleCamera();
    }

    if ( sithCamera_g_pCurCamera->type == SITHCAMERA_ORBITAL )
    {
        float rotStep = secDeltaTime * 90.0f;
        float yaw;
        if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, &bPressed) )
        {
            yaw = 1.0f;
        }
        else
        {
            yaw = sithControl_GetKey(SITHCONTROL_TURNRIGHT, &bPressed) ? -1.0f : 0.0f;
        }

        rdVector3 pyr;
        float pitch = sithControl_GetKeyAsAxis(SITHCONTROL_PITCH);
        pyr.z = 0.0f;
        pyr.x = rotStep * pitch;
        pyr.y = rotStep * yaw;
        if ( pyr.x != 0.0f || pyr.y != 0.0f || pyr.z != 0.0f )
        {
            rdMatrix34 mat;
            rdMatrix_BuildRotate34(&mat, &pyr);
            rdMatrix_TransformVector34Acc(&sithPlayerControls_vecDebugCamDir, &mat);
            rdVector_Normalize3Acc(&sithPlayerControls_vecDebugCamDir);
        }

        float distStep = secDeltaTime * 0.1f;
        float distDelta;
        if ( sithControl_GetKey(SITHCONTROL_FORWARD, &bPressed) )
        {
            distDelta = distStep * -1.0f;
        }
        else
        {
            distDelta = sithControl_GetKey(SITHCONTROL_BACK, &bPressed) ? distStep : 0.0f;
        }

        if ( distDelta != 0.0f )
        {
            sithPlayerControls_cameraDistance = distDelta + sithPlayerControls_cameraDistance;
            if ( sithPlayerControls_cameraDistance < pThing->collide.size )
            {
                sithPlayerControls_cameraDistance = pThing->collide.size;
            }
            else if ( sithPlayerControls_cameraDistance > 3.0f )
            {
                sithPlayerControls_cameraDistance = 3.0f;
            }

            SITHLOG_STATUS("New camera distance = %f.\n", sithPlayerControls_cameraDistance);
        }

        // Orbit camera looks at player from debug camera direction
        rdMatrix34* pOrient = &sithCamera_g_orbCamOrient;
        rdVector_Neg3(&pOrient->lvec, &sithPlayerControls_vecDebugCamDir);

        pOrient->rvec.x = pOrient->lvec.y * rdroid_g_zVector3.z - pOrient->lvec.z * rdroid_g_zVector3.y;
        pOrient->rvec.y = pOrient->lvec.z * rdroid_g_zVector3.x - pOrient->lvec.x * rdroid_g_zVector3.z;
        pOrient->rvec.z = pOrient->lvec.x * rdroid_g_zVector3.y - pOrient->lvec.y * rdroid_g_zVector3.x;

        pOrient->uvec.x = pOrient->rvec.y * pOrient->lvec.z - pOrient->rvec.z * pOrient->lvec.y;
        pOrient->uvec.y = pOrient->rvec.z * pOrient->lvec.x - pOrient->rvec.x * pOrient->lvec.z;
        pOrient->uvec.z = pOrient->rvec.x * pOrient->lvec.y - pOrient->rvec.y * pOrient->lvec.x;
        rdMatrix_Normalize34(pOrient);

        rdVector_Scale3(&pOrient->dvec, &sithPlayerControls_vecDebugCamDir, sithPlayerControls_cameraDistance);
    }

    sithControl_GetKey(SITHCONTROL_MAP, &bPressed);
    if ( bPressed )
    {
        sithMain_g_sith_mode.mapModeFlags ^= (SITHMAPMODE_SHOWALLSECTORS | SITHMAPMODE_SHOWALLTHINGS);
    }

    return 1;
}

int J3DAPI sithPlayerControls_ProcessDeadPlayer(SithThing* pThing, float secDeltaTime)
{
    (void)pThing;
    (void)secDeltaTime;
    return 1;
}

void J3DAPI sithPlayerControls_ProcessJumpForwardMove(SithThing* pThing)
{
    SithActorInfo* pActor     = &pThing->thingInfo.actorInfo;
    SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;

    if ( pThing->attach.flags && pThing->moveStatus == SITHPLAYERMOVE_FALLING )
    {
        // Landed
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        sithPuppet_RemoveAllTracks(pThing);
        return;
    }

    const rdVector3* pVel = &pPhysics->velocity;
    float speed = sqrtf(pVel->z * pVel->z + pVel->y * pVel->y + pVel->x * pVel->x);
    if ( speed < 1.1f )
    {
        // Try to grab ledge
        SithSurface* pHitSurf  = NULL;
        rdFace* pHitFace       = NULL;
        rdModel3Mesh* pHitMesh = NULL;
        SithThing* pHitThing   = NULL;
        double distance = sithPlayerActions_SearchForCollision(pThing, &pThing->orient.lvec, &pHitSurf, &pHitFace, &pHitMesh, &pHitThing);
        if ( distance >= 0.0 )
        {
            if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) )
            {
                sithPlayerControls_bJumpActivated = 1;
            }

            if ( pActor->bForceMovePlay == 1 )
            {
                sithPuppet_StopForceMove(pThing, 1);
            }

            sithPlayerActions_ActorGrabLedge(pThing, (float)distance, pHitSurf, pHitFace, pHitMesh, pHitThing);
            sithPlayerControls_bJumpActivated = 1;
            sithPhysics_ResetThingMovement(pThing);

            if ( pThing == sithPlayer_g_pLocalPlayerThing && (pPhysics->flags & (SITH_PF_MINECAR | SITH_PF_RAFT | SITH_PF_JEEP | SITH_PF_UNKNOWN_8000000)) == 0 )
            {
                rdKeyframe* pKey = sithPuppet_GetKeyframe("in_jump_up_grab.key");
                if ( pKey && sithPuppet_PlayKey(pThing->renderData.pPuppet, pKey, 3, 5, (rdKeyframeFlags)0x28, NULL) >= 0 )
                {
                    sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)0x52);
                }
            }

            return;
        }
    }

    if ( (pPhysics->flags & SITH_PF_FLY) == 0 )
    {
        rdVector_Set3(&pPhysics->thrust, 0.0f, 0.0f, 0.0f);
        rdVector_Set3(&pPhysics->angularVelocity, 0.0f, 0.0f, 0.0f);
        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
    {
        pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * 0.89999998f;
    }
    else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
    {
        pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * -1.0f * 0.5f;
    }
    else
    {
        pPhysics->thrust.y = 0.0f;
    }

    if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
    {
        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, 1.0f) * 0.5f;
    }
    else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
    {
        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, 1.0f) * 0.5f;
    }
    else
    {
        pPhysics->angularVelocity.y = 0.0f;
    }

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 )
    {
        sithPlayerControls_ApplyMouseTurn(pThing, 1.0f);
    }
    else if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) )
    {
        pPhysics->thrust.z = pActor->maxThrust * 0.5f + pPhysics->thrust.z;
    }
    else
    {
        pPhysics->thrust.z = 0.0f;
    }

    if ( !sithPlayerControls_IsZeroVector(&pPhysics->thrust) && !sithPlayerControls_IsZeroVector(&pActor->headPYR) )
    {
        sithPlayerControls_RotateThrustByHead(pThing);
    }
}

void J3DAPI sithPlayerControls_ProcessSwimMove(SithThing* pThing)
{
    SithActorInfo* pActor     = &pThing->thingInfo.actorInfo;
    SithPhysicsInfo* pPhysics = &pThing->moveInfo.physics;
    int state;
    rdVector3 pyr;

    sithPlayerControls_curJumpDirection = 0;
    if ( sithPlayerControls_secSwimmingTime < 1.0f )
    {
        // Just entered water
        pPhysics->thrust.x = 0.0f;
        rdVector_Set3(&pPhysics->angularVelocity, 0.0f, 0.0f, 0.0f);
        sithPhysics_SetThingLook(pThing, &rdroid_g_zVector3, 0.0f);
        return;
    }

    if ( pThing->pPuppetState->moveMode != SITHPUPPET_MOVEMODE_SWIM && (pThing->pInSector->flags & (SITH_SECTOR_AETHERIUM | SITH_SECTOR_UNDERWATER)) != 0 )
    {
        sithPuppet_RemoveAllTracks(pThing);
        sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_SWIM);
    }

    rdVector_Set3(&pyr, 0.0f, 0.0f, 0.0f);
    if ( (pThing->flags & SITH_TF_SUBMERGED) == 0 && (pThing->pInSector->flags & SITH_SECTOR_AETHERIUM) == 0 && (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
    {
        // Out of water
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
        sithPuppet_SetMoveMode(pThing, SITHPUPPET_MOVEMODE_NORMAL);
        sithPhysics_FindFloor(pThing, 0);
        sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_STAND, NULL);
        return;
    }

    if ( pThing->pInSector && (pThing->pInSector->flags & SITH_SECTOR_AETHERIUM) != 0 && pThing->attach.flags )
    {
        sithThing_DetachThing(pThing);
    }

    if ( sithControl_GetKey(SITHCONTROL_ACT2, NULL) && (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 && !sithWeapon_IsMountingWeapon(pThing) )
    {
        int bFloorItem;
        SithThing* pActivatedThing = sithCollision_FindActivatedThing(pThing, &bFloorItem);
        if ( pActivatedThing )
        {
            sithPlayerControls_StartActivate(pThing, pActivatedThing);
            sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_PICKUP, sithPlayerControls_PuppetCallback);
            return;
        }

        if ( !pActor->bForceMovePlay && sithPlayerActions_Activate(pThing) == 1 )
        {
            return;
        }
    }

    if ( sithControl_GetKey(SITHCONTROL_JUMP, NULL) )
    {
        bool bDive = false;
        if ( sithInventory_GetCurrentWeapon(pThing) != SITHWEAPON_NO_WEAPON || sithWeapon_IsMountingWeapon(pThing) )
        {
            bDive = (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0;
        }
        else if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            bDive = true;
        }
        else if ( !sithPlayerControls_bJumpActivated )
        {
            int ledge = sithPlayerControls_CheckWaterLedge(pThing);
            if ( ledge == 1 )
            {
                sithFX_CreateWaterRipple(pThing);
                sithPlayerControls_StartMountFromWater(pThing);
                sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_EXITWATER);
                sithFX_CreateWaterRipple(pThing);
                return;
            }

            if ( ledge == 2 )
            {
                sithFX_CreateWaterRipple(pThing);
                sithPhysics_ResetThingMovement(pThing);
                float movesize = pThing->collide.movesize * 0.25f;
                pThing->moveStatus = SITHPLAYERMOVE_PULLINGUP;
                pThing->collide.movesize = movesize;
                pThing->pos.z = pThing->pos.z - 0.0049999999f;
                rdVector_Copy3(&pThing->forceMoveStartPos, &pThing->pos);
                sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_MOUNT1MSTEP, NULL);
                sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_EXITWATER);
                sithFX_CreateWaterRipple(pThing);
                return;
            }
        }

        if ( bDive )
        {
            // Swim up
            float thrust = pActor->maxThrust + pActor->extraSpeed;
            pPhysics->thrust.y = (thrust + thrust) * 0.89999998f;
            if ( (pThing->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0 )
            {
                sithPhysics_FindWaterSurface(pThing);
            }

            sithPlayerControls_bJumpActivated = 1;
        }
    }
    else
    {
        pPhysics->thrust.y = 0.0f;
    }

    if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
    {
        pPhysics->thrust.z = 0.0f;
    }
    else
    {
        pyr.z = 1.0f;
        if ( (pThing->pInSector->flags & SITH_SECTOR_AETHERIUM) == 0 )
        {
            sithPhysics_SetThingLook(pThing, &pyr, 0.0f);
        }

        if ( sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, &state) )
        {
            // Dive from water surface
            sithPhysics_ResetThingMovement(pThing);
            sithPuppet_StopMode(pThing, SITHPUPPETSUBMODE_FIRE2, 0.0f);
            sithFX_CreateWaterRipple(pThing);
            rdVector_Copy3(&pThing->forceMoveStartPos, &pThing->pos);
            sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_DIVEFROMSURFACE, NULL);
            sithPlayerControls_PlayComment(pThing, SITHSOUNDCLASS_EXITWATERSLOW);
            sithFX_CreateWaterRipple(pThing);
            return;
        }

        // Note, crawl key was already checked
        if ( sithControl_GetKey(SITHCONTROL_CRAWLTOGGLE, &state) )
        {
            pPhysics->thrust.z = pPhysics->thrust.z - pActor->maxThrust * 0.5f;
        }
    }

    // Note, pyr.z is 0 here when not on water surface; and the functions below return before using it otherwise
    if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) )
    {
        if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            rdVector3 angles;
            rdMatrix_ExtractAngles34(&pThing->orient, &angles);
            pyr.x = sithTime_g_frameTimeFlex * -60.0f;
            if ( fabs(angles.x + pyr.x) <= 75.0 )
            {
                rdMatrix_PreRotate34(&pThing->orient, &pyr);
            }

            return;
        }

        pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * 0.89999998f;
    }
    else if ( sithControl_GetKey(SITHCONTROL_BACK, NULL) )
    {
        if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            rdVector3 angles;
            rdMatrix_ExtractAngles34(&pThing->orient, &angles);
            pyr.x = sithTime_g_frameTimeFlex * 60.0f;
            if ( fabs(angles.x + pyr.x) <= 75.0 )
            {
                rdMatrix_PreRotate34(&pThing->orient, &pyr);
            }

            return;
        }

        pPhysics->thrust.y = (pActor->maxThrust + pActor->extraSpeed) * -1.0f * 0.89999998f;
    }
    else
    {
        // Note, original clears pitch rotation velocity here instead of forward thrust
        pPhysics->angularVelocity.x = 0.0f;
    }

    if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
    {
        if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            pyr.y = sithTime_g_frameTimeFlex * -75.0f;
            rdMatrix_PostRotate34(&pThing->orient, &pyr);
            return;
        }

        pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, -1.0f, 1.0f);
        return;
    }

    if ( !sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
    {
        pPhysics->angularVelocity.y = 0.0f;
        return;
    }

    if ( (pPhysics->flags & SITH_PF_ONWATERSURFACE) == 0 )
    {
        pyr.y = sithTime_g_frameTimeFlex * 75.0f;
        rdMatrix_PostRotate34(&pThing->orient, &pyr);
        return;
    }

    pPhysics->angularVelocity.y = sithPlayerControls_GetTurnVelocity(pThing, 1.0f, 1.0f);
}

static void sithPlayerControls_HangShimmy(SithThing* pThing, float dirX, int searchType, SithPuppetSubMode submode)
{
    rdVector3 moveNorm;
    rdVector_Set3(&moveNorm, dirX, 0.0f, 0.0f);
    rdMatrix_TransformVector34Acc(&moveNorm, &pThing->orient);
    rdVector_Normalize3Acc(&moveNorm);

    SithSurface* pHitSurf  = NULL;
    rdModel3* pHitModel    = NULL;
    rdFace* pHitFace       = NULL;
    rdModel3Mesh* pHitMesh = NULL;
    SithThing* pHitThing   = NULL;
    if ( !sithPlayerControls_SearchForCollisions(pThing, &moveNorm, &pHitSurf, &pHitThing, &pHitModel, &pHitFace, &pHitMesh, searchType) )
    {
        return;
    }

    sithPuppet_RemoveAllTracks(pThing);
    pThing->collide.movesize = 0.0f;
    rdVector_Copy3(&pThing->forceMoveStartPos, &pThing->pos);

    if ( !pHitSurf )
    {
        // Hanging on 3DO thing face
        if ( submode == SITHPUPPETSUBMODE_HANGSHIMLEFT )
        {
            sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBHANDLEFT);
            sithPuppet_PlayForceMoveMode(pThing, submode, NULL);
        }
        else
        {
            sithPuppet_PlayForceMoveMode(pThing, submode, NULL);
            sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBHANDLEFT);
        }

        sithThing_AttachThingToThingFace(pThing, pHitThing, pHitFace, pHitMesh->apVertices, 1);
        sithPlayerActions_g_pCurCollidedModelFace = pHitFace;
        sithPlayerActions_g_pCurCollidedModel     = pHitModel;
        return;
    }

    sithPuppet_PlayForceMoveMode(pThing, submode, NULL);
    sithThing_AttachThingToClimbSurface(pThing, pHitSurf);
    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_CLIMBHANDLEFT);
    sithPlayerActions_g_pCurCollidedSurface = pHitSurf;
}

void J3DAPI sithPlayerControls_ProcessHangMove(SithThing* pThing)
{
    if ( pThing->thingInfo.actorInfo.bForceMovePlay == 1 || pThing->type != SITH_THING_PLAYER )
    {
        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_FORWARD, NULL) && !sithPlayerControls_bJumpActivated )
    {
        if ( !sithPlayerActions_CanPullUp(pThing) )
        {
            return;
        }

        // Pull up on ledge
        pThing->collide.movesize = 0.001f;
        pThing->collide.type     = SITH_COLLIDE_NONE;
        rdVector_Copy3(&pThing->forceMoveStartPos, &pThing->pos);
        pThing->moveStatus = SITHPLAYERMOVE_PULLINGUP;
        pThing->moveInfo.physics.flags = (pThing->moveInfo.physics.flags & ~SITH_PF_ALIGNED) | SITH_PF_FLOORSTICK;

        sithThing_DetachThing(pThing);
        sithPuppet_RemoveAllTracks(pThing);
        sithPuppet_PlayForceMoveMode(pThing, SITHPUPPETSUBMODE_MOUNTLEDGE, NULL);
        sithSoundClass_PlayModeFirst(pThing, (SithSoundClassMode)0x88);
        return;
    }

    if ( (sithControl_GetKey(SITHCONTROL_BACK, NULL) || sithControl_GetKey(SITHCONTROL_JUMP, NULL)) && !sithPlayerControls_bJumpActivated )
    {
        sithPlayerActions_UnmountWall(pThing, -1);
        sithPlayerControls_bJumpActivated = 1;
        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_TURNRIGHT, NULL) )
    {
        sithPlayerControls_HangShimmy(pThing, 1.0f, 3, SITHPUPPETSUBMODE_HANGSHIMRIGHT);
    }
    else if ( sithControl_GetKey(SITHCONTROL_TURNLEFT, NULL) )
    {
        sithPlayerControls_HangShimmy(pThing, -1.0f, 4, SITHPUPPETSUBMODE_HANGSHIMLEFT);
    }
    else
    {
        pThing->moveInfo.physics.thrust.x = 0.0f;
    }
}

static void sithPlayerControls_ResetAim(SithThing* pThing)
{
    if ( pThing->type == SITH_THING_PLAYER )
    {
        sithPlayerControls_pTargetThing = NULL;
    }

    sithPlayerControls_RotateAimJoints(pThing, 0.0f, 0.0f);
    sithWeapon_SendMessageAim(pThing, 0);
}

void J3DAPI sithPlayerControls_ProcessWeaponAim(SithThing* pThing, float secDeltaTime)
{
    SITH_ASSERTREL(pThing);
    if ( (pThing->type != SITH_THING_ACTOR && pThing->type != SITH_THING_PLAYER) || secDeltaTime == 0.0f )
    {
        return;
    }

    SithWeaponId curWeapon = (SithWeaponId)sithInventory_GetCurrentWeapon(pThing);
    const float* aAngles   = sithInventory_GetCurrentWeapon(pThing) == SITHWEAPON_MIRROR ? sithPlayerControls_aMirrorAimAngles : sithPlayerControls_aAimAngles;

    float sinAngle;
    float cosPitchNear, cosPitchFar, cosYawNear, cosYawFar, cosVertical;
    stdMath_SinCos(aAngles[0], &sinAngle, &cosPitchNear);
    stdMath_SinCos(aAngles[1], &sinAngle, &cosPitchFar);
    stdMath_SinCos(aAngles[2], &sinAngle, &cosYawNear);
    stdMath_SinCos(aAngles[3], &sinAngle, &cosYawFar);
    stdMath_SinCos(aAngles[4], &sinAngle, &cosVertical);

    // Note, original leaves these uninitialized for actors without AI control block
    SithInventoryType* pWeaponType = NULL;
    SithThing* pTarget             = NULL;
    if ( pThing->type == SITH_THING_ACTOR && pThing->controlInfo.aiControl.pLocal )
    {
        pWeaponType = sithInventory_GetType(curWeapon);
        pTarget     = pThing->controlInfo.aiControl.pLocal->pTargetThing;
    }
    else if ( pThing->type == SITH_THING_PLAYER )
    {
        pWeaponType = sithInventory_GetInventoryType(pThing, curWeapon);
        pTarget     = sithPlayerControls_pTargetThing;
        if ( sithWeapon_GetMountWait() > sithTime_g_secGameTime )
        {
            sithPlayerControls_ResetAim(pThing);
            return;
        }

        switch ( pThing->moveStatus )
        {
            case SITHPLAYERMOVE_JUMPROLLBACK:
            case SITHPLAYERMOVE_JUMPROLLFWD:
            case SITHPLAYERMOVE_JUMPLEFT:
            case SITHPLAYERMOVE_JUMPRIGHT:
                sithPlayerControls_ResetAim(pThing);
                return;

            default:
                break;
        }
    }

    rdVector3 startPos;
    rdVector_Copy3(&startPos, &pThing->pos);
    startPos.z = pThing->pos.z - -0.050000001f;

    SithSector* pStartSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pStartSector )
    {
        sithPlayerControls_ResetAim(pThing);
        return;
    }

    if ( pWeaponType && (pWeaponType->flags & SITHINVENTORY_TYPE_AUTOAIM) != 0 )
    {
        float maxAimDist = sithWeapon_GetWeaponMaxAimDistance(curWeapon);
        if ( pTarget )
        {
            int bCanAim = 0;
            if ( pTarget->type != SITH_THING_FREE
                && (pTarget->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) == 0
                && (pTarget->thingInfo.actorInfo.flags & SITH_AF_NOTARGET) == 0 )
            {
                float aimDist = maxAimDist - -0.1f;
                float cos1    = cosPitchFar;
                float cos3    = cosYawFar;
                if ( pThing->controlInfo.aiControl.pLocal && !sithWeapon_IsMountingWeapon(pThing) && !sithPuppet_IsModeOnTrack(pThing, SITHPUPPETSUBMODE_AIMWEAPON) )
                {
                    aimDist = maxAimDist;
                    cos1    = cosPitchNear;
                    cos3    = cosYawNear;
                }

                if ( sithPlayerControls_CanAimAtPos(pThing, &startPos, &pTarget->pos, cos1, cosVertical, cos3, aimDist) )
                {
                    if ( pThing->type == SITH_THING_PLAYER )
                    {
                        bCanAim = sithCollision_CanMoveToPos(pStartSector, &startPos, &pTarget->pos, 0.0f);
                    }
                    else if ( pThing->controlInfo.aiControl.pLocal )
                    {
                        bCanAim = pThing->controlInfo.aiControl.pLocal->mode & SITHAI_MODE_TARGETVISIBLE;
                    }
                }
            }

            if ( !bCanAim )
            {
                pTarget = NULL;
                if ( pThing->type == SITH_THING_PLAYER )
                {
                    sithPlayerControls_pTargetThing = NULL;
                }
            }
        }

        if ( !pTarget )
        {
            if ( pThing->type == SITH_THING_PLAYER )
            {
                // Search for new target every other frame
                if ( ((pThing->idx + sithMain_g_frameNumber) & 1) != 0 )
                {
                    return;
                }

                sithCollision_SearchForCollisions(pThing->pInSector, NULL, &pThing->pos, &pThing->orient.lvec, maxAimDist, 1.0f, 0x284);
                for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
                {
                    SithThing* pHitThing = pCollision->pThingCollided;
                    if ( (pCollision->type & SITHCOLLISION_THING) != 0
                        && pHitThing->type == SITH_THING_ACTOR
                        && (pHitThing->flags & (SITH_TF_WHIPSWING | SITH_TF_WHIPCLIMB)) == 0
                        && sithInventory_GetCurrentWeapon(pThing) != SITHWEAPON_WHIP
                        && pHitThing->type != SITH_THING_FREE
                        && (pHitThing->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) == 0
                        && (pHitThing->thingInfo.actorInfo.flags & SITH_AF_NOTARGET) == 0
                        && sithPlayerControls_CanAimAtPos(pThing, &startPos, &pHitThing->pos, cosPitchNear, cosVertical, cosYawNear, maxAimDist)
                        && sithCollision_CanMoveToPos(pStartSector, &startPos, &pHitThing->pos, 0.0f)
                        && (sithInventory_GetCurrentWeapon(pThing) != SITHWEAPON_MIRROR || !stdUtil_StrCmp(pHitThing->aName, "marduk")) )
                    {
                        pTarget = pHitThing;
                        if ( pThing->type == SITH_THING_PLAYER )
                        {
                            sithPlayerControls_pTargetThing = pTarget;
                        }
                        break;
                    }
                }

                sithCollision_DecreaseStackLevel();
            }

            if ( !pTarget )
            {
                sithPlayerControls_ResetAim(pThing);
                return;
            }
        }

        // Aim at target's eye level
        rdVector3 aimPos;
        float eyeZ = pTarget->thingInfo.actorInfo.eyeOffset.z;
        rdVector_Copy3(&aimPos, &pTarget->pos);
        if ( (pTarget->thingInfo.actorInfo.flags & SITH_AF_HUMAN) != 0 )
        {
            eyeZ = eyeZ * 0.25f;
        }

        aimPos.z = pTarget->pos.z + eyeZ;

        rdMatrix34 aimOrient;
        rdMatrix_LookAt(&aimOrient, &startPos, &aimPos, 0.0f);

        rdVector3 thingPYR, aimPYR;
        rdMatrix_ExtractAngles34(&pThing->orient, &thingPYR);
        rdMatrix_ExtractAngles34(&aimOrient, &aimPYR);
        aimPYR.x = stdMath_NormalizeAngleAcute(thingPYR.x - aimPYR.x);
        aimPYR.y = stdMath_NormalizeAngleAcute(thingPYR.y - aimPYR.y);

        if ( aimPYR.x < -80.0f )
        {
            aimPYR.x = -80.0f;
        }
        else if ( aimPYR.x > 80.0f )
        {
            aimPYR.x = 80.0f;
        }

        if ( aimPYR.y < -45.0f )
        {
            aimPYR.y = -45.0f;
        }
        else if ( aimPYR.y > 45.0f )
        {
            aimPYR.y = 45.0f;
        }

        sithPlayerControls_RotateAimJoints(pThing, -aimPYR.x, -aimPYR.y);
        sithWeapon_SendMessageAim(pThing, 1);
        return;
    }

    // Weapon without auto aim
    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 && sithCamera_g_pCurCamera->type != SITHCAMERA_UNKNOWN_100 )
    {
        sithPlayerControls_RotateAimJoints(pThing, 0.0f, 0.0f);
    }

    if ( pThing->type == SITH_THING_PLAYER )
    {
        sithPlayerControls_pTargetThing = NULL;
    }

    SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) != 0 && (pActor->flags & SITH_AF_CANROTATEHEAD) == 0 )
    {
        if ( sithControl_GetKey(SITHCONTROL_CENTER, NULL) )
        {
            sithPhysics_SetThingLook(pThing, &rdroid_g_zVector3, secDeltaTime);
        }

        return;
    }

    rdVector3 headPYR;
    rdVector_Copy3(&headPYR, &pActor->headPYR);
    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        return;
    }

    // Editor head pitch control
    bool bPitchChanged = false;
    float pitchAxis    = sithControl_GetAxis(SITHCONTROL_PITCH);
    if ( pitchAxis != 0.0f )
    {
        headPYR.x     = headPYR.x + pitchAxis;
        bPitchChanged = true;
    }

    pitchAxis = sithControl_GetKeyAsAxis(SITHCONTROL_PITCH);
    if ( pitchAxis != 0.0f )
    {
        bPitchChanged = true;
        headPYR.x     = pitchAxis * 90.0f * secDeltaTime + headPYR.x;
    }

    if ( bPitchChanged )
    {
        if ( headPYR.x < pActor->minHeadPitch )
        {
            headPYR.x = pActor->minHeadPitch;
        }
        else if ( headPYR.x > pActor->maxHeadPitch )
        {
            headPYR.x = pActor->maxHeadPitch;
        }

        sithActor_SetHeadPYR(pThing, &headPYR);
        pActor->flags &= ~SITH_AF_VIEWCENTRING;
        return;
    }

    if ( sithControl_GetKey(SITHCONTROL_CENTER, NULL) || (pActor->flags & SITH_AF_VIEWCENTRING) != 0 )
    {
        // Center the view
        float step = secDeltaTime * 180.0f;
        float delta = -pActor->headPYR.x;
        pActor->flags |= SITH_AF_VIEWCENTRING;
        if ( delta < -step )
        {
            delta = -step;
        }
        else if ( delta > step )
        {
            delta = step;
        }

        if ( fabsf(delta) <= 0.0000099999997f )
        {
            delta = 0.0f;
        }

        if ( delta != 0.0f )
        {
            pActor->headPYR.x = delta + pActor->headPYR.x;
            sithActor_SetHeadPYR(pThing, &pActor->headPYR);
            return;
        }

        pActor->flags = (pActor->flags & ~SITH_AF_VIEWCENTRING) | SITH_AF_VIEWCENTRED;
    }
}

int J3DAPI sithPlayerControls_CanAimAtPos(SithThing* pThing, const rdVector3* pStartPost, const rdVector3* pAimPos, float cos1, float cos2, float cos3, float maxDist)
{
    rdVector3 aimDir;
    aimDir.x = pAimPos->x - pStartPost->x;
    aimDir.y = pAimPos->y - pStartPost->y;
    aimDir.z = pAimPos->z - pStartPost->z;

    float dist = rdVector_Normalize3Acc(&aimDir);
    if ( dist >= maxDist )
    {
        return 0;
    }

    // Interpolate the pitch angle limit for near targets
    if ( dist <= sithPlayerControls_aimNearDistance )
    {
        cos2 = (cos1 - cos2) / sithPlayerControls_aimNearDistance * dist + cos2;
    }
    else
    {
        cos2 = cos1;
    }

    // Project aim direction onto thing's horizontal plane
    const rdVector3* pUp = &pThing->orient.uvec;
    float dot = -((aimDir.x - rdroid_g_zeroVector3.x) * pUp->x + (aimDir.y - rdroid_g_zeroVector3.y) * pUp->y + (aimDir.z - rdroid_g_zeroVector3.z) * pUp->z);

    rdVector3 planeDir;
    planeDir.x = pUp->x * dot + aimDir.x;
    planeDir.y = pUp->y * dot + aimDir.y;
    planeDir.z = pUp->z * dot + aimDir.z;
    rdVector_Normalize3Acc(&planeDir);

    const rdVector3* pLook = &pThing->orient.lvec;
    if ( pLook->x * planeDir.x + pLook->y * planeDir.y + pLook->z * planeDir.z < cos2 )
    {
        return 0;
    }

    if ( aimDir.z * planeDir.z + aimDir.y * planeDir.y + aimDir.x * planeDir.x < cos3 )
    {
        return 0;
    }

    return 1;
}

int J3DAPI sithPlayerControls_GetPushPullMoveNorm(rdVector3* moveNorm, const rdVector3* pLVect, float angle)
{
    float sinAngle, cosAngle;
    stdMath_SinCos(angle, &sinAngle, &cosAngle);

    if ( pLVect->x == 0.0f && pLVect->y == 0.0f )
    {
        return 0;
    }

    // Snap to the nearest world axis
    rdVector3 axis;
    if ( fabsf(pLVect->x) <= fabsf(pLVect->y) )
    {
        rdVector_Set3(&axis, 0.0f, 1.0f, 0.0f);
    }
    else
    {
        rdVector_Set3(&axis, 1.0f, 0.0f, 0.0f);
    }

    float dot = rdVector_Dot3(pLVect, &axis);
    if ( dot > 0.0f )
    {
        if ( dot > cosAngle )
        {
            rdVector_Copy3(moveNorm, &axis);
            return 1;
        }
    }
    else if ( dot < 0.0f && dot < -cosAngle )
    {
        rdVector_Neg3(moveNorm, &axis);
        return 1;
    }

    return 0;
}

int J3DAPI sithPlayerControls_CheckWaterLedge(SithThing* pThing)
{
    int result = 0;
    if ( (pThing->moveInfo.physics.flags & SITH_PF_ONWATERSURFACE) == 0 )
    {
        return 0;
    }

    rdVector3 lookDir, backDir;
    rdVector_Copy3(&lookDir, &pThing->orient.lvec);
    rdVector_Neg3(&backDir, &lookDir);
    if ( lookDir.z != 0.0f )
    {
        return 0;
    }

    rdVector3 startPos;
    startPos.x = pThing->pos.x;
    startPos.y = pThing->pos.y;
    startPos.z = pThing->pos.z - -0.02f;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    // Search for water ledge in front
    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &lookDir, 0.0099999998f, 0.050999999f, 0x801);
    SithCollision* pCollision;
    for ( pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        const SithSurface* pSurface = pCollision->pSurfaceCollided;
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0
            && pSurface
            && (pSurface->flags & SITH_SURFACE_WATERLEDGE) != 0
            && pSurface->face.normal.z * backDir.z + pSurface->face.normal.y * backDir.y + pSurface->face.normal.x * backDir.x > 0.87f )
        {
            break;
        }
    }

    if ( pCollision )
    {
        // Get highest ledge vertex
        const rdFace* pFace = &pCollision->pSurfaceCollided->face;
        float maxZ = sithWorld_g_pCurrentWorld->aVertices[pFace->aVertices[0]].z;
        for ( size_t i = 0; i < pFace->numVertices; ++i )
        {
            float z = sithWorld_g_pCurrentWorld->aVertices[pFace->aVertices[i]].z;
            if ( z > maxZ )
            {
                maxZ = z;
            }
        }

        rdVector3 newPos;
        newPos.x = pThing->pos.x;
        newPos.y = pThing->pos.y;
        newPos.z = maxZ - 0.094999999f;
        SithSector* pNewSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &newPos, 0.0f);
        if ( !pNewSector )
        {
            // Note, original returns here without decreasing the collision stack level
            return 0;
        }

        rdVector_Copy3(&pThing->pos, &newPos);
        if ( pNewSector != pThing->pInSector )
        {
            sithThing_ExitSector(pThing);
            sithThing_EnterSector(pThing, pNewSector, 1, 0);
        }

        result = 1;
    }

    sithCollision_DecreaseStackLevel();
    if ( result )
    {
        return result;
    }

    // Search for 1m step ledge
    startPos.x = pThing->pos.x;
    startPos.y = pThing->pos.y;
    startPos.z = pThing->pos.z - -0.02f;
    pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &lookDir, 0.1f, 0.050000001f, 0x801);
    pCollision = sithCollision_PopStack();
    if ( pCollision )
    {
        for ( ; pCollision; pCollision = sithCollision_PopStack() )
        {
            const SithSurface* pSurface = pCollision->pSurfaceCollided;
            if ( (pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0
                && pSurface
                && (pSurface->flags & SITH_SURFACE_WATERLEDGE) != 0
                && pSurface->face.normal.z * backDir.z + pSurface->face.normal.y * backDir.y + pSurface->face.normal.x * backDir.x > 0.87f )
            {
                break;
            }
        }

        if ( !pCollision )
        {
            sithCollision_DecreaseStackLevel();
            return 0;
        }

        result = 2;
    }

    sithCollision_DecreaseStackLevel();
    return result;
}

int J3DAPI sithPlayerControls_SearchForCollisions(SithThing* pThing, rdVector3* moveNorm, SithSurface** ppHitSurf, SithThing** ppHitThing, rdModel3** ppHitModel, rdFace** ppHitFace, rdModel3Mesh** ppHitMesh, int someType)
{
    float moveDist = (someType == 1 || someType == 2) ? 0.2f : 0.1f;

    rdVector3 startPos;
    startPos.z = (moveNorm->z * moveDist + pThing->pos.z) - -0.12f;
    startPos.x = moveDist * moveNorm->x + pThing->pos.x;
    startPos.y = moveNorm->y * moveDist + pThing->pos.y;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    // Search for ledge surface or 3DO ledge face
    int bFound = 0;
    SithSurface* pHitSurf = NULL;
    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &pThing->orient.lvec, 0.1f, 0.0049999999f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            SithSurface* pSurface = pCollision->pSurfaceCollided;
            if ( pSurface && (pSurface->flags & SITH_SURFACE_LEDGE) != 0 )
            {
                const rdVector3* pAttachedNormal = &pThing->attach.attachedToStructure.pSurfaceAttached->face.normal;
                if ( pAttachedNormal->y * pSurface->face.normal.y + pAttachedNormal->z * pSurface->face.normal.z + pAttachedNormal->x * pSurface->face.normal.x >= 0.89999998f )
                {
                    pHitSurf = pSurface;
                    bFound   = 1;
                    break;
                }
            }
        }
        else if ( (pCollision->type & SITHCOLLISION_THING) != 0
            && (pThing->attach.flags & SITH_ATTACH_THINGFACE) != 0
            && pCollision->pFaceCollided
            && pCollision->pThingCollided == pThing->attach.attachedToStructure.pThingAttached
            && (pCollision->pFaceCollided->flags & RD_FF_3DO_LEDGE) != 0 )
        {
            pHitSurf    = NULL;
            bFound      = 1;
            *ppHitModel = pCollision->pThingCollided->renderData.data.pModel3;
            *ppHitFace  = pCollision->pFaceCollided;
            *ppHitMesh  = pCollision->pMeshCollided;
            *ppHitThing = pCollision->pThingCollided;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( bFound || someType == 2 )
    {
        // Check that nothing blocks the move
        bool bBlocked = false;
        sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, moveNorm, moveDist, pThing->collide.movesize * 0.5f, 0xA00);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                if ( someType == 2 && pCollision->pSurfaceCollided && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0 )
                {
                    sithPlayerActions_UnmountWall(pThing, -1);
                    sithCollision_DecreaseStackLevel();
                    return 0;
                }

                if ( pCollision->pSurfaceCollided != pHitSurf )
                {
                    bBlocked = true;
                }
            }
            else if ( (pCollision->type & SITHCOLLISION_THING) != 0
                && pCollision->pThingCollided
                && ((pThing->attach.flags & SITH_ATTACH_THINGFACE) == 0 || pCollision->pThingCollided != pThing->attach.attachedToStructure.pThingAttached) )
            {
                bBlocked = true;
                break;
            }
        }

        sithCollision_DecreaseStackLevel();
        if ( bBlocked )
        {
            *ppHitSurf  = NULL;
            *ppHitFace  = NULL;
            *ppHitModel = NULL;
            *ppHitMesh  = NULL;
            return 0;
        }
    }

    *ppHitSurf = pHitSurf;
    return bFound;
}

SithSurface* J3DAPI sithPlayerControls_SearchForClimbSurface(SithThing* pThing, const rdVector3* moveNorm, int climbDir, int* pbHitNonClimbSurface)
{
    SithSurface* pClimbSurf = NULL;
    float moveDist = (climbDir == 1 || climbDir == 2) ? 0.2f : 0.1f;
    *pbHitNonClimbSurface = 0;

    rdVector3 startPos;
    startPos.x = moveDist * moveNorm->x + pThing->pos.x;
    startPos.y = moveNorm->y * moveDist + pThing->pos.y;
    startPos.z = moveNorm->z * moveDist + pThing->pos.z;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        return NULL;
    }

    // Search for climbable surface in front
    sithCollision_SearchForCollisions(pSector, pThing, &startPos, &pThing->orient.lvec, pThing->collide.movesize - -0.0099999998f, pThing->collide.movesize, 0x1);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        SithSurface* pSurface = pCollision->pSurfaceCollided;
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            if ( pSurface && (pSurface->flags & SITH_SURFACE_CLIMBABLE) != 0 )
            {
                pClimbSurf = pSurface;
                *pbHitNonClimbSurface = 0;
                break;
            }

            *pbHitNonClimbSurface = 1;
        }
        else if ( (pCollision->type & SITHCOLLISION_ADJOINTOUCH) != 0 )
        {
            if ( !pSurface )
            {
                *pbHitNonClimbSurface = 0;
                continue;
            }

            if ( (pSurface->flags & SITH_SURFACE_CLIMBABLE) != 0 )
            {
                pClimbSurf = pSurface;
                *pbHitNonClimbSurface = 0;
                break;
            }

            if ( !pSurface->pAdjoin )
            {
                *pbHitNonClimbSurface = 0;
            }
            else if ( (pSurface->pAdjoin->flags & SITH_ADJOIN_NOPLAYERMOVE) != 0 )
            {
                *pbHitNonClimbSurface = 1;
            }
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( pClimbSurf || climbDir == 2 )
    {
        // Check that nothing blocks the move
        bool bBlocked = false;
        sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, moveNorm, moveDist, 0.02f, 0xA00);
        for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
        {
            if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
            {
                if ( climbDir == 2 && pCollision->pSurfaceCollided && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0 )
                {
                    // Reached floor while climbing down
                    sithPlayerControls_bJumpActivated = 1;
                    sithPlayerActions_UnmountWall(pThing, -1);
                    sithCollision_DecreaseStackLevel();
                    *pbHitNonClimbSurface = 0;
                    return NULL;
                }

                if ( pCollision->pSurfaceCollided != pClimbSurf )
                {
                    bBlocked = true;
                    *pbHitNonClimbSurface = 1;
                    break;
                }
            }
            else if ( (pCollision->type & (SITHCOLLISION_ADJOINTOUCH | SITHCOLLISION_ADJOINCROSS)) != 0 )
            {
                if ( (pCollision->pSurfaceCollided->flags & (SITH_SURFACE_WATER | SITH_SURFACE_LAVA)) != 0 )
                {
                    *pbHitNonClimbSurface = 1;
                    bBlocked = true;
                    break;
                }
            }
            else if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
            {
                bBlocked = true;
                *pbHitNonClimbSurface = 1;
                break;
            }
        }

        sithCollision_DecreaseStackLevel();
        if ( bBlocked )
        {
            pClimbSurf = NULL;
        }
    }

    return pClimbSurf;
}

// Finds the player thing used for driving the vehicle
static SithThing* sithPlayerControls_FindVehiclePlayer(const char* pName, const char* pAltName, size_t* pPlayerNum)
{
    size_t playerNum = 0;
    for ( ; playerNum < sithPlayer_g_numPlayers; ++playerNum )
    {
        const char* pPlayerName = sithPlayer_g_aPlayers[playerNum].pThing->aName;
        if ( !strcmp(pPlayerName, pName) || (pAltName && !strcmp(pPlayerName, pAltName)) )
        {
            *pPlayerNum = playerNum;
            return sithPlayer_g_aPlayers[playerNum].pThing;
        }
    }

    *pPlayerNum = playerNum;
    return NULL;
}

// Returns distance from pPos to vehicle plane which is perpendicular to vehicle look vector and offset by distance
static inline float sithPlayerControls_GetVehiclePlaneDist(const SithThing* pVehicle, const rdVector3* pPos, float offset)
{
    const rdVector3* pLook = &pVehicle->orient.lvec;
    return (pPos->z - (pLook->z * offset + pVehicle->pos.z)) * pLook->z
        + (pPos->y - (pLook->y * offset + pVehicle->pos.y)) * pLook->y
        + (pPos->x - (pLook->x * offset + pVehicle->pos.x)) * pLook->x;
}

// Transfers player control to vehicle player thing
static void sithPlayerControls_SwitchToPlayerThing(SithThing* pThing, SithThing* pNewThing, size_t playerNum, SithThing* pVehicle, bool bSameSector)
{
    if ( sithPlayer_g_bGuybrush )
    {
        sithPlayer_ToggleGuybrush();
    }

    sithPlayer_HidePlayer(sithPlayer_g_playerNum);
    pVehicle->flags |= SITH_TF_DISABLED;
    sithThing_ExitSector(pNewThing);

    *pNewThing->thingInfo.actorInfo.pPlayer = *pThing->thingInfo.actorInfo.pPlayer;
    pNewThing->thingInfo.actorInfo.pPlayer->pThing = pNewThing;
    pNewThing->thingInfo.actorInfo.health = pThing->thingInfo.actorInfo.health;

    sithPlayer_SetLocalPlayer(playerNum);
    sithPlayer_ShowPlayer(playerNum, 0);
    sithThing_SetPositionAndOrient(pNewThing, &pVehicle->pos, &pVehicle->orient);
    sithThing_EnterSector(pNewThing, pVehicle->pInSector, 1, bSameSector);
}

static void sithPlayerControls_CopyActorFlags(SithThing* pDstThing, const SithThing* pSrcThing)
{
    if ( (pSrcThing->thingInfo.actorInfo.flags & SITH_AF_INVULNERABLE) != 0 )
    {
        pDstThing->thingInfo.actorInfo.flags |= SITH_AF_INVULNERABLE;
    }
    else
    {
        pDstThing->thingInfo.actorInfo.flags &= ~SITH_AF_INVULNERABLE;
    }

    if ( (pSrcThing->thingInfo.actorInfo.flags & SITH_AF_POISONED) != 0 )
    {
        pDstThing->thingInfo.actorInfo.flags |= SITH_AF_POISONED;
    }
    else
    {
        pDstThing->thingInfo.actorInfo.flags &= ~SITH_AF_POISONED;
    }
}

int J3DAPI sithPlayerControls_TryBoardVehicle(SithThing* pThing, int bBoard)
{
    if ( sithInventory_GetCurrentWeapon(pThing) != SITHWEAPON_NO_WEAPON || sithWeapon_IsMountingWeapon(pThing) )
    {
        return 0;
    }

    // Search for vehicle in front
    enum
    {
        VEHICLE_NONE    = 0,
        VEHICLE_JEEP    = 1,
        VEHICLE_MINECAR = 3,
    } vehicleType = VEHICLE_NONE;

    SithThing* pVehicle = NULL;
    rdVector3 lookDir;
    rdVector_Copy3(&lookDir, &pThing->orient.lvec);
    float movesize = pThing->collide.movesize;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &lookDir, movesize, movesize, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_THING) == 0 || pCollision->pThingCollided == pThing )
        {
            continue;
        }

        pVehicle = pCollision->pThingCollided;
        if ( !strcmp(pVehicle->aName, "jeep") || !strcmp(pVehicle->aName, "jeep_pyr") )
        {
            vehicleType = VEHICLE_JEEP;
            break;
        }

        if ( !strcmp(pVehicle->aName, "minecar") )
        {
            vehicleType = VEHICLE_MINECAR;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( vehicleType == VEHICLE_NONE )
    {
        return 0;
    }

    // Note, when player is not positioned correctly at the vehicle side, original returns
    // FPU status word which is always non-zero. Here 1 is returned in such cases.
    size_t playerNum;
    SithThing* pNewThing;
    float sideDot = pThing->orient.lvec.x * pVehicle->orient.rvec.x + pThing->orient.lvec.y * pVehicle->orient.rvec.y + pThing->orient.lvec.z * pVehicle->orient.rvec.z;
    bool bSameSector;
    if ( vehicleType == VEHICLE_JEEP )
    {
        pNewThing = sithPlayerControls_FindVehiclePlayer("jeepplayer", "jeepplayerpyr", &playerNum);

        if ( sideDot < 0.70700002f )
        {
            return 1;
        }

        if ( sithPlayerControls_GetVehiclePlaneDist(pVehicle, &pThing->pos, -0.1f) < 0.0f )
        {
            return 1;
        }

        if ( sithPlayerControls_GetVehiclePlaneDist(pVehicle, &pThing->pos, 0.07f) > 0.0f )
        {
            return 1;
        }

        if ( !pNewThing )
        {
            return 0;
        }

        sithPlayerControls_pVehicleBoardedThing = pVehicle;
        sithCamera_RestoreExtCamera();
        pThing->alpha = 1.0f;

        bSameSector = pThing->pInSector == pVehicle->pInSector;
        if ( !bSameSector )
        {
            sithCog_SectorSendMessage(pThing->pInSector, pThing, SITHCOG_MSG_EXITED);
        }

        sithPlayerControls_SwitchToPlayerThing(pThing, pNewThing, playerNum, pVehicle, bSameSector);

        // Move all attached things to the player thing
        while ( pVehicle->pAttachedThing )
        {
            SithThing* pAttached = pVehicle->pAttachedThing;
            SithAttachFlag attachFlags = pAttached->attach.flags;
            sithThing_DetachThing(pAttached);
            sithThing_AttachThingToThing(pAttached, pNewThing);
            pAttached->attach.flags |= attachFlags & SITH_ATTACH_NOMOVE;
        }

        sithPuppet_SetMoveMode(pNewThing, SITHPUPPET_MOVEMODE_NORMAL);
        sithPhysics_FindFloor(pNewThing, 1);
        sithCamera_SetCameraFocus(&sithCamera_g_aCameras[1], pNewThing, NULL);

        pNewThing->moveStatus = SITHPLAYERMOVE_JEEP_BOARDING;
        pNewThing->alpha = 1.0f;
        sithPuppet_PlayMode(pNewThing, SITHPUPPETSUBMODE_PUSHITEM, sithVehicleControls_PuppetCallback);
        pNewThing->thingInfo.actorInfo.bControlsDisabled = 1;

        sithInventory_SetSwimmingInventory(pNewThing, 0);
        sithCog_ThingSendMessage(pNewThing, pThing, SITHCOG_MSG_BOARDED);
        sithCog_ThingSendMessage(pVehicle, pNewThing, SITHCOG_MSG_BOARDED);

        // Swap jeep body mesh with gas can model
        if ( (int)pVehicle->userval != -1 )
        {
            char aModelName[84];
            STD_FORMAT(aModelName, "gascan_%s", pVehicle->renderData.data.pModel3->aName);
            rdModel3* pModel = sithModel_GetModel(aModelName);
            int meshNum      = sithModel_GetMeshIndex(pModel, "jeep_body");
            pNewThing->userval = (float)(uint32_t)sithThing_AddSwapEntry(pNewThing, sithThing_GetThingMeshIndex(pNewThing, "jeep_body"), pModel, meshNum);
        }

        sithVehicleControls_SendMessageUser0(pNewThing);
    }
    else // VEHICLE_MINECAR
    {
        pNewThing = sithPlayerControls_FindVehiclePlayer("mineplayer", NULL, &playerNum);

        if ( fabsf(sideDot) < 0.70700002f )
        {
            return 1;
        }

        if ( sithPlayerControls_GetVehiclePlaneDist(pVehicle, &pThing->pos, -0.07f) < 0.0f )
        {
            return 1;
        }

        if ( sithPlayerControls_GetVehiclePlaneDist(pVehicle, &pThing->pos, 0.07f) > 0.0f )
        {
            return 1;
        }

        if ( !pNewThing )
        {
            return 0;
        }

        sithPlayerControls_pVehicleBoardedThing = pVehicle;
        sithCamera_RestoreExtCamera();
        pThing->alpha = 1.0f;

        bSameSector = pThing->pInSector == pVehicle->pInSector;
        if ( !bSameSector )
        {
            sithCog_SectorSendMessage(pThing->pInSector, pThing, SITHCOG_MSG_EXITED);
        }

        sithPlayerControls_SwitchToPlayerThing(pThing, pNewThing, playerNum, pVehicle, bSameSector);

        sithPuppet_SetMoveMode(pNewThing, SITHPUPPET_MOVEMODE_NORMAL);
        sithPhysics_FindFloor(pNewThing, 1);
        sithCamera_SetCameraFocus(&sithCamera_g_aCameras[1], pNewThing, NULL);

        pNewThing->moveStatus = SITHPLAYERMOVE_MINECAR_BOARDING;
        pNewThing->alpha = 1.0f;
        if ( bBoard )
        {
            // Board immediately
            pNewThing->moveStatus = SITHPLAYERMOVE_MINECAR_IDLE;
            pNewThing->thingInfo.actorInfo.headLightIntensity.w = 2.0f;
            pNewThing->thingInfo.actorInfo.flags |= SITH_AF_HEADLIGHT;
        }
        else
        {
            // Climb in from the left or right side
            sithPuppet_PlayMode(pNewThing, sideDot <= 0.0f ? SITHPUPPETSUBMODE_HOPRIGHT : SITHPUPPETSUBMODE_HOPLEFT, sithVehicleControls_PuppetCallback);
            pNewThing->thingInfo.actorInfo.bControlsDisabled = 1;
            sithVehicleControls_SendMessageUser0(pNewThing);
        }

        sithInventory_SetSwimmingInventory(pNewThing, 0);
        sithCog_ThingSendMessage(pNewThing, pThing, SITHCOG_MSG_BOARDED);
        sithCog_ThingSendMessage(pVehicle, pNewThing, SITHCOG_MSG_BOARDED);
    }

    sithPlayerControls_CopyActorFlags(pNewThing, pThing);
    return 1;
}

void J3DAPI sithPlayerControls_ExitVehicle(SithThing* pThing)
{
    sithVehicleControls_SendMessageUser1(pThing);
    sithInventory_SetSwimmingInventory(pThing, 1);

    rdVector3 exitPos;
    rdVector_Copy3(&exitPos, &pThing->pos);

    rdMatrix34 exitOrient;
    float vehicleHeightOffset;
    SithThing* pVehicle = sithPlayerControls_pVehicleBoardedThing;
    if ( pThing->moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_LEFT )
    {
        float offset = -sithPlayerControls_exitMineCarOffset;
        exitPos.x = pThing->orient.rvec.x * offset + exitPos.x;
        exitPos.y = pThing->orient.rvec.y * offset + exitPos.y;
        exitPos.z = pThing->orient.rvec.z * offset + exitPos.z;

        // Face left from the car
        rdVector_Neg3(&exitOrient.rvec, &pThing->orient.lvec);
        rdVector_Copy3(&exitOrient.lvec, &pThing->orient.rvec);
        rdVector_Copy3(&exitOrient.uvec, &pThing->orient.uvec);

        rdModel3* pModel = pVehicle->renderData.data.pModel3;
        vehicleHeightOffset = pModel->insertOffset.z - sithPhysics_GetThingHeight(pThing);
        pVehicle->flags |= SITH_TF_EMITLIGHT;
    }
    else if ( pThing->moveStatus == SITHPLAYERMOVE_MINECAR_UNBOARDING_RIGHT )
    {
        float offset = sithPlayerControls_exitMineCarOffset;
        exitPos.x = pThing->orient.rvec.x * offset + exitPos.x;
        exitPos.y = pThing->orient.rvec.y * offset + exitPos.y;
        exitPos.z = pThing->orient.rvec.z * offset + exitPos.z;

        // Face right from the car
        rdVector_Copy3(&exitOrient.rvec, &pThing->orient.lvec);
        rdVector_Neg3(&exitOrient.lvec, &pThing->orient.rvec);
        rdVector_Copy3(&exitOrient.uvec, &pThing->orient.uvec);

        rdModel3* pModel = pVehicle->renderData.data.pModel3;
        vehicleHeightOffset = pModel->insertOffset.z - sithPhysics_GetThingHeight(pThing);
        pVehicle->flags |= SITH_TF_EMITLIGHT;
    }
    else
    {
        // Jeep, exit to the left side
        float offset = -sithPlayerControls_exitJeepOffset;
        exitPos.x = pThing->orient.rvec.x * offset + exitPos.x;
        exitPos.y = pThing->orient.rvec.y * offset + exitPos.y;
        exitPos.z = pThing->orient.rvec.z * offset + exitPos.z;

        rdVector_Copy3(&exitOrient.uvec, &rdroid_g_zVector3);

        const rdVector3* pLook = &pThing->orient.lvec;
        exitOrient.lvec.x = pLook->z * exitOrient.uvec.y - pLook->y * exitOrient.uvec.z;
        exitOrient.lvec.y = pLook->x * exitOrient.uvec.z - pLook->z * exitOrient.uvec.x;
        exitOrient.lvec.z = pLook->y * exitOrient.uvec.x - pLook->x * exitOrient.uvec.y;
        rdVector_Normalize3Acc(&exitOrient.lvec);

        exitOrient.rvec.x = exitOrient.lvec.y * exitOrient.uvec.z - exitOrient.lvec.z * exitOrient.uvec.y;
        exitOrient.rvec.y = exitOrient.lvec.z * exitOrient.uvec.x - exitOrient.lvec.x * exitOrient.uvec.z;
        exitOrient.rvec.z = exitOrient.lvec.x * exitOrient.uvec.y - exitOrient.lvec.y * exitOrient.uvec.x;
        rdVector_Normalize3Acc(&exitOrient.rvec);

        vehicleHeightOffset = 0.0f;

        // Move all attached things back to the vehicle
        while ( pThing->pAttachedThing )
        {
            SithThing* pAttached = pThing->pAttachedThing;
            SithAttachFlag attachFlags = pAttached->attach.flags;
            sithThing_DetachThing(pAttached);
            sithThing_AttachThingToThing(pAttached, sithPlayerControls_pVehicleBoardedThing);
            pAttached->attach.flags |= attachFlags & SITH_ATTACH_NOMOVE;
        }
    }

    SithThing* pPlayerThing = sithPlayer_g_aPlayers[0].pThing;
    pPlayerThing->alpha = 1.0f;

    SithSector* pExitSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &exitPos, 0.0f);
    if ( !pExitSector )
    {
        SITHLOG_ERROR("This is really bad, trying to exit vehicle to non-existent sector..\n");
    }

    SithSector* pVehicleSector = pThing->pInSector;
    bool bSectorChanged        = pExitSector != pVehicleSector;
    if ( bSectorChanged )
    {
        sithCog_SectorSendMessage(pVehicleSector, pThing, SITHCOG_MSG_EXITED);
    }

    // Transfer control back to Indy
    sithPlayer_HidePlayer(sithPlayer_g_playerNum);
    *pPlayerThing->thingInfo.actorInfo.pPlayer = *pThing->thingInfo.actorInfo.pPlayer;
    pPlayerThing->thingInfo.actorInfo.pPlayer->pThing = pPlayerThing;
    pPlayerThing->thingInfo.actorInfo.health = pThing->thingInfo.actorInfo.health;
    sithPlayerControls_CopyActorFlags(pPlayerThing, pThing);

    sithThing_ExitSector(pPlayerThing);
    sithPlayer_SetLocalPlayer(0);
    sithPlayer_ShowPlayer(0, 0);
    sithThing_SetPositionAndOrient(pPlayerThing, &exitPos, &exitOrient);
    sithCog_ThingSendMessage(sithPlayerControls_pVehicleBoardedThing, pThing, SITHCOG_MSG_UNBOARDED);
    sithThing_EnterSector(pPlayerThing, pExitSector, 1, !bSectorChanged);

    pPlayerThing->moveStatus = SITHPLAYERMOVE_STILL;
    sithPuppet_SetMoveMode(pPlayerThing, SITHPUPPET_MOVEMODE_NORMAL);
    sithCamera_SetCameraFocus(&sithCamera_g_aCameras[1], pPlayerThing, NULL);
    sithPhysics_FindFloor(pPlayerThing, 1);
    pPlayerThing->alpha = 1.0f;

    // Place vehicle thing back to the vehicle player position
    sithThing_ExitSector(sithPlayerControls_pVehicleBoardedThing);
    rdVector3 vehiclePos;
    vehiclePos.x = pThing->pos.x;
    vehiclePos.y = pThing->pos.y;
    vehiclePos.z = vehicleHeightOffset + pThing->pos.z;
    sithThing_SetPositionAndOrient(sithPlayerControls_pVehicleBoardedThing, &vehiclePos, &pThing->orient);
    sithThing_EnterSector(sithPlayerControls_pVehicleBoardedThing, pVehicleSector, 1, 1);

    sithPlayerControls_pVehicleBoardedThing->flags &= ~SITH_TF_DISABLED;
    sithPlayerControls_pVehicleBoardedThing = NULL;
}

static inline float sithPlayerControls_ClampAngle(float angle, float max)
{
    if ( angle > max )
    {
        angle = max;
    }

    if ( angle < -max )
    {
        angle = -max;
    }

    return angle;
}

void J3DAPI sithPlayerControls_RotateAimJoints(SithThing* pThing, float pitch, float yaw)
{
    int curWeapon = sithInventory_GetCurrentWeapon(pThing);
    if ( pitch == 0.0f && yaw == 0.0f )
    {
        sithPlayerControls_CenterActorOrientation(pThing);
        return;
    }

    int jointNum;
    switch ( curWeapon )
    {
        case SITHWEAPON_PISTOL:
        case SITHWEAPON_TOKAREV:
        case SITHWEAPON_MAUSER:
        case SITHWEAPON_COMTOKAREV:
        case SITHWEAPON_COMMAUSER:
            // One handed weapon, rotate right arm and torso
            yaw = yaw + sithPlayerControls_rarmAimYawOffset;
            jointNum = sithThing_GetThingJointIndex(pThing, "inrarm");
            if ( jointNum >= 0 )
            {
                pitch = sithPlayerControls_ClampAngle(pitch, 80.0f);
                pThing->renderData.apTweakedAngles[jointNum].y = pitch;
                pThing->renderData.apTweakedAngles[jointNum].z = pitch * -0.5f;
            }

            jointNum = sithThing_GetThingJointIndex(pThing, "intorso");
            if ( jointNum >= 0 )
            {
                yaw = sithPlayerControls_ClampAngle(yaw, 45.0f);
                pThing->renderData.apTweakedAngles[jointNum].y = yaw;
                pThing->renderData.apTweakedAngles[jointNum].x = 0.0f;
            }
            break;

        case SITHWEAPON_SUBMACHINE:
        case SITHWEAPON_SHOTGUN:
        case SITHWEAPON_BAZOOKA:
        case SITHWEAPON_COMSUBMACHINE:
        case SITHWEAPON_COMSHOTGUN:
            yaw = yaw + sithPlayerControls_torsoAimYawOffset;
            // fall through

        case SITHWEAPON_SIMONOV:
        case SITHWEAPON_COMSIMONOV:
        case SITHWEAPON_MIRROR:
            // Two handed weapon, rotate only torso
            jointNum = sithThing_GetThingJointIndex(pThing, "intorso");
            if ( jointNum >= 0 )
            {
                pitch = sithPlayerControls_ClampAngle(pitch, 45.0f);
                pThing->renderData.apTweakedAngles[jointNum].x = pitch;

                jointNum = sithThing_GetThingJointIndex(pThing, "intorso");
                yaw = sithPlayerControls_ClampAngle(yaw, 45.0f);
                pThing->renderData.apTweakedAngles[jointNum].y = yaw;
                pThing->renderData.apTweakedAngles[jointNum].z = 0.0f;
            }
            break;

        default:
            sithPlayerControls_CenterActorOrientation(pThing);
            break;
    }

    if ( pitch != 0.0f || yaw != 0.0f )
    {
        rdVector3 headPYR;
        rdVector_Set3(&headPYR, pitch, 0.0f, 0.0f);
        sithActor_SetHeadPYR(pThing, &headPYR);
    }
}

void J3DAPI sithPlayerControls_CenterActorOrientation(SithThing* pThing)
{
    rdVector3 zero;
    rdVector_Set3(&zero, 0.0f, 0.0f, 0.0f);
    rdVector_Set3(&pThing->thingInfo.actorInfo.headPYR, 0.0f, 0.0f, 0.0f);

    int jointNum = sithThing_GetThingJointIndex(pThing, "inrarm");
    rdVector_Copy3(&pThing->renderData.apTweakedAngles[jointNum], &zero);

    jointNum = sithThing_GetThingJointIndex(pThing, "intorso");
    rdVector_Copy3(&pThing->renderData.apTweakedAngles[jointNum], &zero);

    sithActor_SetHeadPYR(pThing, &zero);
}

SithThing* sithPlayerControls_GetTargetThing(void)
{
    return sithPlayerControls_pTargetThing;
}

SithThing* J3DAPI sithPlayerControls_GetVehicleBoardedThing()
{
    return sithPlayerControls_pVehicleBoardedThing;
}

void J3DAPI sithPlayerControls_SetVehicleBoardedThing(SithThing* pThing)
{
    sithPlayerControls_pVehicleBoardedThing = pThing;
}

signed int J3DAPI sithPlayerControls_CanStrafeMove(SithThing* pThing, int bMoveRight)
{
    rdVector3 moveDir;
    if ( bMoveRight == 0 )
    {
        rdVector_Neg3(&moveDir, &pThing->orient.rvec);
    }
    else if ( bMoveRight == 1 )
    {
        rdVector_Copy3(&moveDir, &pThing->orient.rvec);
    }
    else
    {
        return 0;
    }

    // Check that nothing blocks the strafe
    bool bCanMove = true;
    sithCollision_SearchForCollisions(pThing->pInSector, pThing, &pThing->pos, &moveDir, 0.1f, 0.088999994f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        SithCollisionType type = pCollision->type;
        if ( (type & SITHCOLLISION_WORLD) != 0 )
        {
            bCanMove = false;
            break;
        }

        if ( (type & SITHCOLLISION_ADJOINCROSS) != 0 )
        {
            SithSurfaceAdjoinFlag adjoinFlags = pCollision->pSurfaceCollided->pAdjoin->flags;
            if ( (adjoinFlags & SITH_ADJOIN_NOPLAYERMOVE) != 0 || (adjoinFlags & SITH_ADJOIN_MOVE) == 0 )
            {
                bCanMove = false;
                break;
            }
        }
        else if ( (type & SITHCOLLISION_THING) != 0 )
        {
            bCanMove = false;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !bCanMove )
    {
        return 0;
    }

    // Check that there is floor at probe position
    rdVector3 probePos;
    probePos.x = pThing->orient.lvec.x * 0.1f + pThing->pos.x;
    probePos.y = pThing->pos.y + pThing->orient.lvec.y * 0.1f;
    probePos.z = pThing->pos.z + pThing->orient.lvec.z * 0.1f;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &probePos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    rdVector3 downDir;
    rdVector_Neg3(&downDir, &rdroid_g_zVector3);

    int bFloor = 0;
    sithCollision_SearchForCollisions(pSector, NULL, &probePos, &downDir, 0.11f, 0.0099999998f, 0xA00);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            const SithSurface* pSurface = pCollision->pSurfaceCollided;
            const rdVector3* pVert      = &sithWorld_g_pCurrentWorld->aVertices[pSurface->face.aVertices[0]];
            const rdVector3* pNormal    = &pSurface->face.normal;
            float dist = (probePos.x - pVert->x) * pNormal->x + (probePos.z - pVert->z) * pNormal->z + (probePos.y - pVert->y) * pNormal->y;
            if ( dist < 0.088f || dist > 0.092f
                || pNormal->y * rdroid_g_zVector3.y + pNormal->z * rdroid_g_zVector3.z + pNormal->x * rdroid_g_zVector3.x < 0.97899997f )
            {
                break;
            }

            bFloor = 1;
        }
        else if ( (pCollision->type & SITHCOLLISION_THING) != 0 && pCollision->pFaceCollided )
        {
            const rdFace* pFace   = pCollision->pFaceCollided;
            SithThing* pHitThing  = pCollision->pThingCollided;

            rdVector3 normal;
            rdMatrix_TransformVector34(&normal, &pFace->normal, &pHitThing->orient);
            if ( normal.z * rdroid_g_zVector3.z + normal.x * rdroid_g_zVector3.x + normal.y * rdroid_g_zVector3.y <= 0.97899997f )
            {
                sithCollision_DecreaseStackLevel();
                return 0;
            }

            rdMatrix34 orient = pHitThing->orient;
            rdMatrix_TransformVector34(&normal, &pFace->normal, &orient);

            rdVector3 vert;
            rdMatrix_TransformVector34(&vert, &pCollision->pMeshCollided->apVertices[pFace->aVertices[0]], &orient);
            vert.x = pHitThing->pos.x + vert.x;
            vert.y = pHitThing->pos.y + vert.y;
            vert.z = pHitThing->pos.z + vert.z;

            float dist = (probePos.x - vert.x) * normal.x + (probePos.y - vert.y) * normal.y + (probePos.z - vert.z) * normal.z;
            if ( dist < 0.088f || dist > 0.092f )
            {
                break;
            }

            bFloor = 1;
        }
    }

    sithCollision_DecreaseStackLevel();
    return bFloor;
}
