#include "sithMulti.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <rdroid/Math/rdVector.h>

#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithComm.h>
#include <sith/Devices/sithConsole.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Dss/sithDSS.h>
#include <sith/Dss/sithDSSThing.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Gameplay/sithEvent.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Main/sithMain.h>
#include <sith/Main/sithString.h>
#include <sith/World/sithModel.h>
#include <sith/World/sithSector.h>
#include <sith/World/sithSurface.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWorld.h>

#include <std/General/std.h>
#include <std/General/stdPlatform.h>
#include <std/General/stdUtil.h>
#include <std/Win95/stdComm.h>

#include <stdlib.h>
#include <string.h>

static size_t sithMulti_tickRate = 70;
static int sithMulti_numUpdatedSurfaces;
static int sithMulti_lastUpdateIdx;
static int sithMulti_playerWelcomeState;
static int sithMulti_numUpdatedSectors;
static int sithMulti_aRemovedStaticThings[256];
static unsigned int sithMulti_curWelcomePlayerNum;
static int sithMulti_numUpdatedThings;
static int sithMulti_quitGameState;
static int sithMulti_checksum;
static int sithMulti_bWelcomingPlayer;
static DPID sithMulti_newPlayerId;
static int sithMulti_bSyncScores;
static SithMultiNewPlayerJoinedCallback sithMulti_pfNewPlayerJoinedCallback;
static unsigned int sithMulti_msecPingStartTime;
static unsigned int sithMulti_msecQuitGameTime;
static int sithMulti_numRemovedStaticThings;
static unsigned int sithMulti_msecLastSyncScoreTime;
static unsigned int sithMulti_msecWelcomeUpdateInterval;
static int sithMulti_dword_17F10EC;

void sithMulti_InstallHooks(void)
{
    // Uncomment only lines for functions that have full definition and doesn't call original function (non-thunk functions)

    J3D_HOOKFUNC(sithMulti_CloseGame);
    J3D_HOOKFUNC(sithMulti_CheckPlayers);
    J3D_HOOKFUNC(sithMulti_ProcessPlayerLost);
    J3D_HOOKFUNC(sithMulti_RemovePlayer);
    J3D_HOOKFUNC(sithMulti_SendWelcome);
    J3D_HOOKFUNC(sithMulti_ProcessWelcome);
    J3D_HOOKFUNC(sithMulti_ProcessPlayerJoin);
    J3D_HOOKFUNC(sithMulti_SyncPlayers);
    J3D_HOOKFUNC(sithMulti_ProcessSyncPlayers);
    J3D_HOOKFUNC(sithMulti_ProcessJoinRequest);
    J3D_HOOKFUNC(sithMulti_FinishJoining);
    J3D_HOOKFUNC(sithMulti_ProcessChat);
    J3D_HOOKFUNC(sithMulti_ProcessPing);
    J3D_HOOKFUNC(sithMulti_ProcessPong);
    J3D_HOOKFUNC(sithMulti_QuitPlayer);
    J3D_HOOKFUNC(sithMulti_ProcessQuit);
    J3D_HOOKFUNC(sithMulti_Update);
    J3D_HOOKFUNC(sithMulti_SyncScores);
    J3D_HOOKFUNC(sithMulti_GetPlayerIndexByID);
    J3D_HOOKFUNC(sithMulti_ProcessKilledPlayer);
    J3D_HOOKFUNC(sithMulti_QuitGame);
    J3D_HOOKFUNC(sithMulti_Respawn);
    J3D_HOOKFUNC(sithMulti_RemoveStaticThing);
    J3D_HOOKFUNC(sithMulti_UpdateSurfaces);
    J3D_HOOKFUNC(sithMulti_UpdateSectors);
    J3D_HOOKFUNC(sithMulti_UpdateThings);
    J3D_HOOKFUNC(sithMulti_UpdateRemovals);
    J3D_HOOKFUNC(sithMulti_StartWelcomingPlayer);
    J3D_HOOKFUNC(sithMulti_StopWelcomingPlayer);
}

DPID sithMulti_g_serverId;
SithMessage sithMulti_g_message;






























// Copies player name as the original wcsncpy(pDest, pSrc, 15), zero padded and not null terminated
static void sithMulti_CopyPlayerName(wchar_t* pDest, const wchar_t* pSrc)
{
    size_t i = 0;
    for ( ; i < 15 && pSrc[i]; ++i )
    {
        pDest[i] = pSrc[i];
    }

    for ( ; i < 15; ++i )
    {
        pDest[i] = 0;
    }
}

void sithMulti_CloseGame()
{
    if ( stdComm_IsGameHost() && sithMulti_bWelcomingPlayer )
    {
        sithMulti_StopWelcomingPlayer(1);
    }

    sithMessage_g_outputstream &= ~SITHMESSAGE_STREAM_NET;
    sithMessage_g_inputstream &= ~SITHMESSAGE_STREAM_NET;
    sithEvent_RegisterTask(2, NULL, 0, SITHEVENT_TASKDISABLED);
    sithMessage_CloseGame();

    sithMulti_g_serverId    = 0;
    sithMulti_dword_17F10EC = 0;
}

int J3DAPI sithMulti_CheckPlayers(int msecTime, SithEventParams* pParam)
{
    (void)msecTime;
    (void)pParam;

    if ( sithWorld_g_pCurrentWorld && sithPlayer_g_pLocalPlayerThing && (sithMain_g_sith_mode.subModeFlags & 8) == 0 )
    {
        sithDSSThing_Pos(sithPlayer_g_pLocalPlayerThing, SITHMESSAGE_SENDTOJOINEDPLAYERS, 0);
    }

    if ( !stdComm_IsGameHost() )
    {
        if ( sithPlayer_g_aPlayers[0].msecLastCommTime + 45000 < sithTime_g_msecGameTime )
        {
            sithPlayer_g_aPlayers[0].msecLastCommTime = sithTime_g_msecGameTime;
            SITHLOG_ERROR("Haven't heard from server, exiting the game.\n");
            sithMulti_RemovePlayer(0);
        }

        return 1;
    }

    for ( size_t playerNum = 0; playerNum < sithPlayer_g_numPlayers; ++playerNum )
    {
        const SithPlayer* pPlayer = &sithPlayer_g_aPlayers[playerNum];
        if ( pPlayer->playerNetId != sithMessage_g_localPlayerId && (pPlayer->flags & 1) != 0 && pPlayer->msecLastCommTime + 45000 < sithTime_g_msecGameTime )
        {
            SITHLOG_ERROR("Haven't heard from player %d, kicking them out of the game.\n", playerNum);
            sithMulti_QuitPlayer(pPlayer->playerNetId);
            sithMulti_RemovePlayer(playerNum);
        }
    }

    if ( sithMulti_msecLastSyncScoreTime + 10000 < sithTime_g_clockTime )
    {
        sithMulti_msecLastSyncScoreTime = sithTime_g_clockTime;
        sithMulti_SyncScores();
    }

    return 1;
}

void J3DAPI sithMulti_ProcessPlayerLost(DPID idPlayer)
{
    if ( sithMulti_bWelcomingPlayer && idPlayer == sithMulti_newPlayerId )
    {
        sithMulti_StopWelcomingPlayer(1);
    }

    int playerNum = sithMulti_GetPlayerIndexByID(idPlayer);
    SITHLOG_ERROR("Player Lost: id=%x, playerNum=%d.\n", idPlayer, playerNum);
    if ( playerNum >= 0 )
    {
        sithMulti_RemovePlayer(playerNum);
    }
}

void J3DAPI sithMulti_RemovePlayer(unsigned int playerNum)
{
    SITH_ASSERTREL(playerNum < sithPlayer_g_numPlayers);
    SITHLOG_ERROR("Removing player %d.\n", playerNum);

    wchar_t awText[128];
    STD_WFORMAT(awText, sithString_GetString("%s_HAS_LEFT_THE_GAME"), sithPlayer_g_aPlayers[playerNum].awName);
    sithConsole_PrintWString(awText);

    if ( sithPlayer_g_aPlayers[playerNum].playerNetId == sithMulti_g_serverId )
    {
        sithConsole_PrintWString(sithString_GetString("SERVER_LEFT_GAME"));
        sithMulti_g_serverId = 0;
    }

    sithSoundMixer_StopSoundThing(sithPlayer_g_aPlayers[playerNum].pThing, 0);
    sithPlayer_HidePlayer(playerNum);

    if ( stdComm_IsGameHost() )
    {
        sithCog_BroadcastMessage(SITHCOG_MSG_LEAVE, SITHCOG_SYM_REF_THING, sithPlayer_g_aPlayers[playerNum].pThing->idx, SITHCOG_SYM_REF_NONE, playerNum);
    }
}

int J3DAPI sithMulti_SendWelcome(DPID idPlayer, int playerNum, DPID idTo)
{
    SITHLOG_STATUS("Sending game welcome of player %d to playerID %x.\n", playerNum, idTo);

    SITHDSS_STARTOUT(SITHDSS_WELCOME);
    SITHDSS_PUSHINT32(playerNum);
    SITHDSS_PUSHUINT32(idPlayer);

    wchar_t awName[16];
    sithMulti_CopyPlayerName(awName, sithPlayer_g_aPlayers[playerNum].awName);
    awName[15] = 0;
    SITHDSS_PUSHWSTRING(awName, STD_ARRAYLEN(awName));

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, SITHMESSAGE_STREAM_NET, 1u);
}

int J3DAPI sithMulti_ProcessWelcome(const SithMessage* pMsg)
{
    SITHDSS_STARTIN(pMsg);

    int playerNum = SITHDSS_POPINT32();
    DPID idPlayer = SITHDSS_POPUINT32();
    SITHLOG_STATUS("Processing welcome from player %d (id=%x).\n", playerNum, idPlayer);

    SithPlayer* pPlayer = &sithPlayer_g_aPlayers[playerNum];
    sithMulti_CopyPlayerName(pPlayer->awName, (const wchar_t*)pCurDSSMsgData_);
    pPlayer->awName[15] = 0;

    if ( idPlayer != sithMessage_g_localPlayerId )
    {
        if ( (pPlayer->flags & 1) != 0 )
        {
            SITHLOG_STATUS("Received duplicate welcome packet from player %d (id=%x).\n", playerNum, idPlayer);
            return 1;
        }

        sithPlayer_ShowPlayer(playerNum, idPlayer);
        sithMulti_ProcessPlayerJoin(playerNum);
        if ( stdComm_IsGameHost() )
        {
            sithMulti_SyncScores();
        }

        return 1;
    }

    SITHLOG_STATUS("DSS::welcome packet addressed to me!.\n");
    if ( (sithMain_g_sith_mode.subModeFlags & 8) == 0 )
    {
        SITHLOG_STATUS("Got duplicate Welcome packet from host, ignored.\n");
        return 1;
    }

    SITHLOG_STATUS("Joining the game loop.\n");
    for ( size_t i = 0; i < sithPlayer_g_numPlayers; ++i )
    {
        sithPlayer_g_aPlayers[i].msecLastCommTime = sithTime_g_msecGameTime;
    }

    sithMain_g_sith_mode.subModeFlags &= ~8;
    sithThing_LoadPostProcess(sithWorld_g_pCurrentWorld);
    sithPlayer_ShowPlayer(playerNum, idPlayer);
    sithPlayer_SetLocalPlayer(playerNum);

    SITH_ASSERTREL(sithMulti_tickRate);
    sithEvent_RegisterTask(2, sithMulti_CheckPlayers, sithMulti_tickRate, SITHEVENT_TASKINTERVAL);
    sithMessage_StopProcessMessages();

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithMulti_ProcessPlayerJoin(int playerNum)
{
    wchar_t awText[128];
    STD_WFORMAT(awText, sithString_GetString("%s_HAS_JOINED_THE_GAME"), sithPlayer_g_aPlayers[playerNum].awName);
    sithConsole_PrintWString(awText);

    sithPlayer_g_aPlayers[playerNum].msecLastCommTime = sithTime_g_msecGameTime;
    if ( stdComm_IsGameHost() )
    {
        sithCog_BroadcastMessage(SITHCOG_MSG_JOIN, SITHCOG_SYM_REF_THING, sithPlayer_g_aPlayers[playerNum].pThing->idx, SITHCOG_SYM_REF_NONE, playerNum);
    }

    if ( sithMulti_pfNewPlayerJoinedCallback )
    {
        sithMulti_pfNewPlayerJoinedCallback();
    }

    return sithDSSThing_UpdateState(sithPlayer_g_pLocalPlayerThing, SITHMESSAGE_SENDTOJOINEDPLAYERS, SITHMESSAGE_STREAM_ALL);
}

// Sets model of remote player thing
static void sithMulti_SetRemotePlayerModel(SithThing* pThing)
{
    rdModel3* pModel = sithModel_Load("gen_vi.3do", 1);
    if ( !pModel )
    {
        SITHLOG_ERROR("Model %s not found, using default.\n", "gen_vi.3do");
    }

    sithThing_SetThingModel(pThing, pModel);
}

void J3DAPI sithMulti_SyncPlayers(DPID idTo, uint32_t dpFlags)
{
    SITHDSS_STARTOUT(SITHDSS_SYNCPLAYERS);

    SITHDSS_PUSHUINT32(sithMulti_g_serverId);
    SITHDSS_PUSHUINT16(sithPlayer_g_numPlayers);

    for ( size_t i = 0; i < sithPlayer_g_numPlayers; ++i )
    {
        const SithPlayer* pPlayer = &sithPlayer_g_aPlayers[i];
        SITHDSS_PUSHUINT32(pPlayer->flags);
        if ( (pPlayer->flags & 4) != 0 )
        {
            SITHDSS_PUSHUINT32(pPlayer->playerNetId);

            char aName[16];
            stdUtil_ToAStringEx(aName, pPlayer->awName, 15);
            aName[15] = 0;

            // Note, copied as strncpy(dst, aName, 15) + null termination
            char aOutName[16] = { 0 };
            memcpy(aOutName, aName, strnlen(aName, 15));
            SITHDSS_PUSHSTRING(aOutName, sizeof(aOutName));
        }
    }

    SITHDSS_ENDOUT;
    sithMessage_SendMessage(&sithMulti_g_message, idTo, SITHMESSAGE_STREAM_NET, dpFlags);

    for ( size_t i = 0; i < sithPlayer_g_numPlayers; ++i )
    {
        DPID netId = sithPlayer_g_aPlayers[i].playerNetId;
        if ( netId && sithMessage_g_localPlayerId && netId != sithMessage_g_localPlayerId )
        {
            sithMulti_SetRemotePlayerModel(sithPlayer_g_aPlayers[i].pThing);
        }
    }
}

int J3DAPI sithMulti_ProcessSyncPlayers(const SithMessage* pMsg)
{
    sithMulti_g_serverId    = 0;
    sithMulti_dword_17F10EC = 0;
    sithMulti_dword_17F10EC = stdPlatform_GetTimeMsec();

    SITHDSS_STARTIN(pMsg);

    sithMulti_g_serverId    = SITHDSS_POPUINT32();
    sithPlayer_g_numPlayers = SITHDSS_POPUINT16();

    for ( size_t playerNum = 0; playerNum < sithPlayer_g_numPlayers; ++playerNum )
    {
        SithPlayer* pPlayer = &sithPlayer_g_aPlayers[playerNum];

        SithPlayerFlag prevFlags = pPlayer->flags;
        pPlayer->flags = SITHDSS_POPUINT32();
        if ( (pPlayer->flags & 4) != 0 )
        {
            pPlayer->playerNetId = SITHDSS_POPUINT32();
            if ( (pPlayer->flags & 1) != 0 && (prevFlags & 1) == 0 && (sithMain_g_sith_mode.subModeFlags & 8) == 0 )
            {
                SITHLOG_STATUS("Got a new player but we missed the welcome packet, apparently.\n");
                sithMulti_ProcessPlayerJoin(playerNum);
            }
            else if ( pPlayer->playerNetId == 0 && (prevFlags & 1) != 0 && (sithMain_g_sith_mode.subModeFlags & 8) == 0 )
            {
                SITHLOG_STATUS("Noticed that player %d was removed by the server.  Removing locally.\n", playerNum);
                sithMulti_RemovePlayer(playerNum);
            }

            char aName[16];
            SITHDSS_POPSTRING(aName, sizeof(aName));
            aName[15] = 0;

            stdUtil_ToWStringEx(pPlayer->awName, aName, 15);
            pPlayer->awName[15] = 0;
        }

        if ( pPlayer->playerNetId && sithMessage_g_localPlayerId && pPlayer->playerNetId != sithMessage_g_localPlayerId )
        {
            rdModel3* pModel = sithModel_Load("gen_vi.3do", 1);
            if ( !pModel )
            {
                SITHLOG_ERROR("Model %s not found, using default.\n", "gen_vi.3do");
                return 1;
            }

            sithThing_SetThingModel(pPlayer->pThing, pModel);
        }
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithMulti_ProcessJoinRequest(const SithMessage* pMsg)
{
    DPID idSender = sithMessage_GetSenderID();
    if ( !stdComm_IsGameHost() )
    {
        SITHLOG_ERROR("Ignoring JoinRequest from player %d, ignoring.\n", idSender);
        return 1;
    }

    char aWorldName[64];
    memcpy(aWorldName, pMsg->data, 63);
    aWorldName[63] = 0;
    if ( _strcmpi(aWorldName, sithWorld_g_pCurrentWorld->aName) != 0 )
    {
        SITHLOG_ERROR("Got join request but its for the wrong level %s.\n", aWorldName);
        sithMulti_FinishJoining(SITHMULTI_JOIN_WRONGLEVEL, 0.0f, idSender);
        return 1;
    }

    size_t playerNum = 0;
    for ( ; playerNum < sithPlayer_g_numPlayers; ++playerNum )
    {
        if ( sithPlayer_g_aPlayers[playerNum].playerNetId == idSender )
        {
            break;
        }
    }

    if ( playerNum < sithPlayer_g_numPlayers )
    {
        SITHLOG_STATUS("Received knock from player %d, already in game.\n", idSender);
        sithMulti_SendWelcome(idSender, playerNum, idSender);
        return 1;
    }

    sithMessage_CheckMembershipList(idSender);
    if ( sithMulti_quitGameState )
    {
        sithMulti_FinishJoining(SITHMULTI_JOIN_CANCEL, 0.0f, idSender);
        return 1;
    }

    if ( sithMulti_bWelcomingPlayer )
    {
        if ( sithMulti_newPlayerId == idSender )
        {
            sithMulti_FinishJoining(SITHMULTI_JOIN_OK, 0.5f, idSender);
            return 1;
        }

        SITHLOG_STATUS("Deferring join request from player %x, busy already.\n", idSender);
        sithMulti_FinishJoining(SITHMULTI_JOIN_BUSY, 0.0f, idSender);
        return 1;
    }

    // Find free player slot
    playerNum = 0;
    for ( ; playerNum < sithPlayer_g_numPlayers; ++playerNum )
    {
        if ( (sithPlayer_g_aPlayers[playerNum].flags & 2) != 0 && sithPlayer_g_aPlayers[playerNum].playerNetId == 0 )
        {
            break;
        }
    }

    if ( playerNum == sithPlayer_g_numPlayers )
    {
        SITHLOG_STATUS("Received join request from player %d, but the game is full.. sucks for him.\n", idSender);
        sithMulti_FinishJoining(SITHMULTI_JOIN_GAMEFULL, 0.0f, idSender);
        return 1;
    }

    sithMulti_curWelcomePlayerNum = playerNum;
    if ( stdComm_UpdatePlayers(0) )
    {
        SITHLOG_WARNING("Unable to UpdatePlayers List.\n");
        return 1;
    }

    if ( stdComm_VerifyPlayer(idSender) )
    {
        SITHLOG_ERROR("Got a Knock message, but the dude isn't enumerating.\n");
        return 1;
    }

    sithPlayer_Reset(sithMulti_curWelcomePlayerNum);
    SithPlayer* pPlayer = &sithPlayer_g_aPlayers[sithMulti_curWelcomePlayerNum];
    sithMulti_CopyPlayerName(pPlayer->awName, (const wchar_t*)&pMsg->data[64]);
    pPlayer->awName[15] = 0;

    int checksum = *(const int*)&pMsg->data[96];
    SITHLOG_STATUS("My checksum=%x, client=%x.\n", sithMulti_checksum, checksum);
    if ( sithMulti_checksum != checksum )
    {
        SITHLOG_ERROR("Got join request with wrong checksum, %x != %x.\n", sithMulti_checksum, checksum);
        sithMulti_FinishJoining(SITHMULTI_JOIN_WRONGCHECKSUM, 0.0f, idSender);
        return 1;
    }

    sithMulti_FinishJoining(SITHMULTI_JOIN_OK, 0.25f, idSender);
    sithMulti_SyncPlayers(idSender, 0);
    sithMulti_StartWelcomingPlayer(idSender);
    SITHLOG_ERROR("Starting welcome of player %d.\n", idSender);
    return 1;
}

int J3DAPI sithMulti_FinishJoining(SithMultiJoinStatus code, float arg4, int playerId)
{
    SITHDSS_STARTOUT(SITHDSS_UNKNOWN_36);
    SITHDSS_PUSHUINT32(code);
    SITHDSS_PUSHFLOAT(arg4);
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, playerId, SITHMESSAGE_STREAM_NET, 0);
}

int J3DAPI sithMulti_ProcessChat(const SithMessage* pMsg)
{
    int playerNum = *(const int*)&pMsg->data[4];
    size_t len    = *(const uint32_t*)&pMsg->data[8];
    if ( len > 127 )
    {
        len = 128;
    }

    // Added: Guard against 0 length which would make strncpy copy huge amount of data
    if ( len == 0 )
    {
        len = 1;
    }

    // Note, original copies with strncpy(aText, text, len - 1) and zeroes aText[len - 1] and aText[len + 1]
    char aText[132];
    const char* pText = (const char*)&pMsg->data[12];
    size_t textLen    = strnlen(pText, len - 1);
    memcpy(aText, pText, textLen);
    memset(&aText[textLen], 0, (len - 1) - textLen);
    aText[len - 1] = 0;
    aText[len + 1] = 0;

    if ( playerNum < 0 )
    {
        STD_FORMAT(std_g_genBuffer, "%s", aText);
    }
    else
    {
        STD_FORMAT(std_g_genBuffer, "%S says '%s'", sithPlayer_g_aPlayers[playerNum].awName, aText);
    }

    sithConsole_PrintString(std_g_genBuffer);
    return 1;
}

int J3DAPI sithMulti_ProcessPing(const SithMessage* pMsg)
{
    // Note, the type of received message is changed to pong but the global message is sent back
    ((SithMessage*)pMsg)->type = SITHDSS_PONG;
    sithMessage_SendMessage(&sithMulti_g_message, sithMessage_GetSenderID(), SITHMESSAGE_STREAM_NET, 0);
    return 1;
}

int J3DAPI sithMulti_ProcessPong(const SithMessage* pMsg)
{
    DPID idSender = sithMessage_GetSenderID();
    if ( *(const uint32_t*)pMsg->data != sithMulti_msecPingStartTime )
    {
        return 1;
    }

    for ( size_t playerNum = 0; playerNum < sithPlayer_g_numPlayers; ++playerNum )
    {
        if ( sithPlayer_g_aPlayers[playerNum].playerNetId == idSender )
        {
            int msecPing = sithTime_g_msecGameTime - sithMulti_msecPingStartTime;
            STD_FORMAT(std_g_genBuffer, "Ping time to %S is %d msec", sithPlayer_g_aPlayers[playerNum].awName, msecPing);
            sithConsole_PrintString(std_g_genBuffer);
            SITHLOG_STATUS("Ping reply from player %d - %d msec.\n", playerNum, msecPing);
            return 1;
        }
    }

    return 1;
}

int J3DAPI sithMulti_QuitPlayer(DPID id)
{
    if ( !stdComm_IsGameHost() )
    {
        return 0;
    }

    SITHDSS_STARTOUT(SITHDSS_QUIT);
    SITHDSS_PUSHUINT32(id);
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, id, SITHMESSAGE_STREAM_NET, 1u);
}

int J3DAPI sithMulti_ProcessQuit(const SithMessage* pMsg)
{
    if ( sithMessage_GetSenderID() != sithMulti_g_serverId )
    {
        SITHLOG_ERROR("Got QUIT message from non-server, ignored.\n");
        return 0;
    }

    DPID id = *(const DPID*)pMsg->data;
    if ( id == sithMessage_g_localPlayerId )
    {
        if ( sithMulti_quitGameState != 2 )
        {
            sithConsole_PrintWString(sithString_GetString("MULTI_EJECTED"));
            sithMulti_QuitGame(sithTime_g_msecGameTime + 5000, 2);
        }
    }
    else
    {
        int playerNum = sithPlayer_GetPlayerNum(id);
        if ( playerNum >= 0 )
        {
            sithMulti_RemovePlayer(playerNum);
        }
    }

    return 1;
}

void J3DAPI sithMulti_Update(int msecDeltaTime)
{
    if ( !stdComm_IsGameActive() )
    {
        return;
    }

    sithThing_SyncThings();
    sithSurface_SyncSurfaces();
    sithSector_SyncSectors();

    if ( sithMulti_quitGameState && sithTime_g_msecGameTime > sithMulti_msecQuitGameTime )
    {
        if ( sithMulti_quitGameState == 1 || sithMulti_quitGameState == 2 )
        {
            sithMulti_quitGameState = 0;
        }

        return;
    }

    if ( !stdComm_IsGameHost() )
    {
        return;
    }

    if ( sithMulti_bSyncScores )
    {
        sithMulti_bSyncScores = 0;
        sithMulti_SyncPlayers(SITHMESSAGE_SENDTOJOINEDPLAYERS, 0);
    }

    if ( !sithMulti_bWelcomingPlayer )
    {
        return;
    }

    if ( sithMulti_quitGameState )
    {
        sithMulti_StopWelcomingPlayer(1);
        return;
    }

    // Send one welcome update every 60 msec
    size_t numUpdates = (msecDeltaTime + sithMulti_msecWelcomeUpdateInterval) / 60;
    sithMulti_msecWelcomeUpdateInterval = (msecDeltaTime + sithMulti_msecWelcomeUpdateInterval) % 60;
    for ( size_t i = 0; i < numUpdates; ++i )
    {
        switch ( sithMulti_playerWelcomeState )
        {
            case 1:
                sithMulti_UpdateSectors();
                ++sithMulti_numUpdatedSectors;
                break;

            case 2:
                sithMulti_UpdateSurfaces();
                ++sithMulti_numUpdatedSurfaces;
                break;

            case 3:
                sithMulti_UpdateThings();
                ++sithMulti_numUpdatedThings;
                break;

            case 4:
                sithMulti_UpdateRemovals();
                break;

            default:
                return;
        }
    }
}

void J3DAPI sithMulti_SyncScores()
{
    sithMulti_bSyncScores = 1;
}

void J3DAPI sithMulti_ProcessKilledPlayer(const SithPlayer* pPlayer, const SithThing* pPlayerThing, const SithThing* pKiller)
{
    wchar_t awText[128];
    if ( !pKiller || pKiller->type != SITH_THING_PLAYER )
    {
        STD_WFORMAT(awText, sithString_GetString("%s_DIED"), pPlayer->awName);
        sithConsole_PrintWString(awText);
        return;
    }

    SITH_ASSERTREL(pKiller->thingInfo.actorInfo.pPlayer);
    if ( pKiller != pPlayerThing )
    {
        STD_WFORMAT(awText, sithString_GetString("%s_WAS_KILLED_BY_%s"), pPlayer->awName, pKiller->thingInfo.actorInfo.pPlayer->awName);
        sithConsole_PrintWString(awText);
        return;
    }

    // Suicide, pick random message
    double rnd = (double)rand() * (double)(1.0f / RAND_MAX) * 4.0;
    const char* pMsgKey;
    if ( rnd < 1.0 )
    {
        pMsgKey = "%s_COMMITTED_SUICIDE0";
    }
    else if ( rnd < 2.0 )
    {
        pMsgKey = "%s_COMMITTED_SUICIDE1";
    }
    else if ( rnd < 3.0 )
    {
        pMsgKey = "%s_COMMITTED_SUICIDE2";
    }
    else
    {
        pMsgKey = "%s_COMMITTED_SUICIDE3";
    }

    STD_WFORMAT(awText, sithString_GetString(pMsgKey), pPlayer->awName);
    sithConsole_PrintWString(awText);
}

int J3DAPI sithMulti_QuitGame(unsigned int msecTime, int state)
{
    if ( sithMulti_quitGameState != state || msecTime < sithMulti_msecQuitGameTime )
    {
        sithMulti_quitGameState   = state;
        sithMulti_msecQuitGameTime = msecTime;
    }

    return state;
}

size_t J3DAPI sithMulti_Respawn(SithThing* pPlayer)
{
    SITH_ASSERTREL(pPlayer && pPlayer->type == SITH_THING_PLAYER);

    // Collect respawn points not masked out for player
    size_t aRespawnNums[SITHPLAYER_MAX_PLAYERS];
    size_t numRespawns = 0;
    for ( size_t i = 0; i < sithPlayer_g_numPlayers; ++i )
    {
        if ( (pPlayer->thingInfo.actorInfo.pPlayer->respawnMask & (1 << (i & 0x1F))) == 0 )
        {
            aRespawnNums[numRespawns++] = i;
        }
    }

    if ( !numRespawns )
    {
        SITHLOG_ERROR("No respawn points unmasked for thing %s, using respawn 0.\n", pPlayer->aName);
        return 0;
    }

    if ( numRespawns == 1 )
    {
        return aRespawnNums[0];
    }

    size_t idx = (size_t)((double)rand() * (double)(1.0f / RAND_MAX) * (double)numRespawns);
    if ( idx > numRespawns - 1 )
    {
        idx = numRespawns - 1;
    }

    // Find respawn point not occupied by another player
    size_t numTries = 0;
    for ( ;; )
    {
        size_t respawnNum = aRespawnNums[idx];
        const SithPlayer* pRespawn = &sithPlayer_g_aPlayers[respawnNum];
        sithCollision_SearchForCollisions(pRespawn->pInSector, NULL, &pRespawn->orient.dvec, &rdroid_g_zeroVector3, 0.0f, pPlayer->collide.movesize, 0x482);

        SithCollision* pCollision = sithCollision_PopStack();
        while ( pCollision
            && ((pCollision->type & SITHCOLLISION_THING) == 0
                || pCollision->pThingCollided->type != SITH_THING_PLAYER
                || (pCollision->pThingCollided->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) != 0) )
        {
            pCollision = sithCollision_PopStack();
        }

        sithCollision_DecreaseStackLevel();
        if ( !pCollision )
        {
            return respawnNum;
        }

        SITHLOG_STATUS("Tried to use respawn %d but was occupied by %s.\n", respawnNum, pCollision->pThingCollided->aName);
        if ( numTries >= numRespawns )
        {
            SITHLOG_STATUS("All available respawn positions occupied, using %d anyway.\n", respawnNum);
            return respawnNum;
        }

        ++numTries;
        idx = (idx + 1) % numRespawns;
    }
}

void J3DAPI sithMulti_RemoveStaticThing(int guid)
{
    if ( sithMulti_numRemovedStaticThings >= (int)STD_ARRAYLEN(sithMulti_aRemovedStaticThings) )
    {
        SITHLOG_ERROR("Limit %d on # of removed static things exceeded!", STD_ARRAYLEN(sithMulti_aRemovedStaticThings));
        return;
    }

    sithMulti_aRemovedStaticThings[sithMulti_numRemovedStaticThings++] = guid;
}

void J3DAPI sithMulti_UpdateSurfaces()
{
    while ( (size_t)sithMulti_lastUpdateIdx < sithWorld_g_pCurrentWorld->numSurfaces )
    {
        SithSurface* pSurf = &sithWorld_g_pCurrentWorld->aSurfaces[sithMulti_lastUpdateIdx++];
        if ( (pSurf->flags & SITH_SURFACE_SYNC) != 0 )
        {
            sithDSS_SurfaceStatus(pSurf, sithMulti_newPlayerId, SITHMESSAGE_STREAM_NET);
            break;
        }
    }

    if ( (size_t)sithMulti_lastUpdateIdx < sithWorld_g_pCurrentWorld->numSurfaces )
    {
        return;
    }

    SITHLOG_ERROR("Welcome of player %d switching to SECTORS.\n", sithMulti_newPlayerId);
    sithMulti_playerWelcomeState = 1;
    sithMulti_lastUpdateIdx      = 0;
}

void J3DAPI sithMulti_UpdateSectors()
{
    while ( (size_t)sithMulti_lastUpdateIdx < sithWorld_g_pCurrentWorld->numSectors )
    {
        SithSector* pSector = &sithWorld_g_pCurrentWorld->aSectors[sithMulti_lastUpdateIdx++];
        if ( (pSector->flags & SITH_SECTOR_SYNC) != 0 )
        {
            sithDSS_SectorStatus(pSector, sithMulti_newPlayerId, SITHMESSAGE_STREAM_NET);
            break;
        }

        if ( (pSector->flags & SITH_SECTOR_ADJOINSOFF) != 0 )
        {
            sithDSS_SectorFlags(pSector, sithMulti_newPlayerId, SITHMESSAGE_STREAM_NET);
            break;
        }
    }

    if ( (size_t)sithMulti_lastUpdateIdx < sithWorld_g_pCurrentWorld->numSectors )
    {
        return;
    }

    SITHLOG_ERROR("Welcome of player %d switching to THINGS.\n", sithMulti_newPlayerId);
    sithMulti_playerWelcomeState = 3;
    sithMulti_lastUpdateIdx      = 0;
}

void J3DAPI sithMulti_UpdateThings()
{
    while ( sithMulti_lastUpdateIdx <= sithWorld_g_pCurrentWorld->lastThingIdx )
    {
        SithThing* pThing = &sithWorld_g_pCurrentWorld->aThings[sithMulti_lastUpdateIdx++];
        if ( sithThing_CanSync(pThing) && pThing->type != SITH_THING_WEAPON && pThing->type != SITH_THING_EXPLOSION )
        {
            if ( (pThing->guid & 0xFFFF0000) == 0 )
            {
                sithDSSThing_UpdateState(pThing, sithMulti_newPlayerId, SITHMESSAGE_STREAM_NET);
            }
            else
            {
                sithDSSThing_FullDescription(pThing, sithMulti_newPlayerId, SITHMESSAGE_STREAM_NET);
            }

            sithDSSThing_Pos(pThing, sithMulti_newPlayerId, 0);
            break;
        }
    }

    if ( sithMulti_lastUpdateIdx <= sithWorld_g_pCurrentWorld->lastThingIdx )
    {
        return;
    }

    SITHLOG_ERROR("Welcome of player %d switching to REMOVALS.\n", sithMulti_newPlayerId);
    sithMulti_playerWelcomeState = 4;
    sithMulti_lastUpdateIdx      = 0;
}

void J3DAPI sithMulti_UpdateRemovals()
{
    if ( sithMulti_lastUpdateIdx < sithMulti_numRemovedStaticThings )
    {
        sithDSSThing_DestroyThing(sithMulti_aRemovedStaticThings[sithMulti_lastUpdateIdx], sithMulti_newPlayerId);
        if ( ++sithMulti_lastUpdateIdx < sithMulti_numRemovedStaticThings )
        {
            return;
        }
    }

    SITHLOG_ERROR("Finished welcoming player %d, sending the welcome packet.\n", sithMulti_newPlayerId);
    SITHLOG_ERROR("Update totals: %d surfaces, %d sectors, %d things.\n", sithMulti_numUpdatedSurfaces, sithMulti_numUpdatedSectors, sithMulti_numUpdatedThings);

    DPID idTo = sithMulti_newPlayerId;
    sithPlayer_g_aPlayers[sithMulti_curWelcomePlayerNum].playerNetId = sithMulti_newPlayerId;
    sithMulti_SyncPlayers(idTo, 1);
    sithMulti_SendWelcome(sithMulti_newPlayerId, sithMulti_curWelcomePlayerNum, sithMulti_newPlayerId);
    sithMulti_StopWelcomingPlayer(0);
    sithMulti_SyncScores();
}

void J3DAPI sithMulti_StartWelcomingPlayer(DPID playerId)
{
    SITH_ASSERTREL(!sithMulti_bWelcomingPlayer);
    SITH_ASSERTREL(playerId);
    SITHLOG_ERROR("Starting to welcome player %d.\n", playerId);

    sithMulti_lastUpdateIdx      = 0;
    sithMulti_numUpdatedSurfaces = 0;
    sithMulti_numUpdatedSectors  = 0;
    sithMulti_numUpdatedThings   = 0;
    sithMulti_newPlayerId        = playerId;
    sithMulti_bWelcomingPlayer   = 1;
    sithMulti_playerWelcomeState = 2;
}

void J3DAPI sithMulti_StopWelcomingPlayer(int bError)
{
    SITH_ASSERTREL(sithMulti_bWelcomingPlayer);
    SITHLOG_ERROR("Stopped welcoming player %d.\n", sithMulti_newPlayerId);

    if ( bError && sithMulti_newPlayerId )
    {
        sithMulti_FinishJoining(SITHMULTI_JOIN_CANCEL, 0.0f, sithMulti_newPlayerId);
    }

    sithMulti_bWelcomingPlayer   = 0;
    sithMulti_newPlayerId        = 0;
    sithMulti_playerWelcomeState = 2;
    sithMulti_lastUpdateIdx      = 0;
}

void sithMulti_OpenGame(void)
{
    // TODO: Add implementation
    SITH_ASSERT(0);
}

size_t J3DAPI sithMulti_GetTickRate(void)
{
    return sithMulti_tickRate;
}

void J3DAPI sithMulti_SetTickRate(size_t tickRate)
{
    sithMulti_tickRate = tickRate;
}

int J3DAPI sithMulti_Ping(DPID idTo)
{
    sithMulti_msecPingStartTime = sithTime_g_msecGameTime;

    SITHDSS_STARTOUT(SITHDSS_PING);
    SITHDSS_PUSHUINT32(sithTime_g_msecGameTime);
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, 1u, 0);
}

int J3DAPI sithMulti_GetPlayerIndexByID(DPID playerID)
{
    for ( size_t i = 0; i < sithPlayer_g_numPlayers; ++i )
    {
        if ( playerID == sithPlayer_g_aPlayers[i].playerNetId )
        {
            return i;
        }
    }

    return -1;
}