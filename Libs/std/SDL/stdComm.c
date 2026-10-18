// stdComm for non-Windows builds: DirectPlay multiplayer isn't available, the game runs single player only.
// Multiplayer is dormant in Indy3D, sessions can't be created or joined.
#include <std/Win95/stdComm.h>

#include <string.h>

void stdComm_InstallHooks(void)
{
}

int32_t J3DAPI stdComm_Send(DPID idFrom, DPID idTo, const void* pData, uint32_t size, uint32_t flags)
{
    J3D_UNUSED(idFrom);
    J3D_UNUSED(idTo);
    J3D_UNUSED(pData);
    J3D_UNUSED(size);
    J3D_UNUSED(flags);
    return 0;
}

int32_t J3DAPI stdComm_Receive(DPID* pSender, void* pData, size_t* pLength)
{
    J3D_UNUSED(pSender);
    J3D_UNUSED(pData);
    if ( pLength ) {
        *pLength = 0;
    }
    return -1; // nothing received
}

int J3DAPI stdComm_GetGame(size_t gameNum, StdCommGame* pGame)
{
    J3D_UNUSED(gameNum);
    J3D_UNUSED(pGame);
    return 1;
}

HRESULT J3DAPI stdComm_CreateGame(const StdCommGame* pSettings)
{
    J3D_UNUSED(pSettings);
    return E_FAIL;
}

HRESULT J3DAPI stdComm_SetGameParams(StdCommGame* pSettings)
{
    J3D_UNUSED(pSettings);
    return E_FAIL;
}

int J3DAPI stdComm_JoinGame(size_t gameNum, const wchar_t* pPassword)
{
    J3D_UNUSED(gameNum);
    J3D_UNUSED(pPassword);
    return 1;
}

void stdComm_CloseGame(void)
{
}

int J3DAPI stdComm_RejoinSession(DPID* pPlayerId, const wchar_t* pPlayerName)
{
    J3D_UNUSED(pPlayerId);
    J3D_UNUSED(pPlayerName);
    return 1;
}

int J3DAPI stdComm_GetSessionSettings(StdCommGame* pSettings)
{
    if ( pSettings ) {
        memset(pSettings, 0, sizeof(*pSettings));
    }
    return 1;
}

int J3DAPI stdComm_IsGameActive(void)
{
    return 0;
}

int J3DAPI stdComm_IsGameHost(void)
{
    return 0;
}

int J3DAPI stdComm_UpdatePlayers(size_t gameNum)
{
    J3D_UNUSED(gameNum);
    return 1;
}

DPID J3DAPI stdComm_CreatePlayer(const wchar_t* pPlayerName)
{
    J3D_UNUSED(pPlayerName);
    return 0;
}

void J3DAPI stdComm_DestroyPlayer(DPID playerId)
{
    J3D_UNUSED(playerId);
}

size_t stdComm_GetNumPlayers(void)
{
    return 0;
}

int J3DAPI stdComm_VerifyPlayer(DPID id)
{
    J3D_UNUSED(id);
    return 0;
}

DPID J3DAPI stdComm_GetPlayerID(size_t playerNum)
{
    J3D_UNUSED(playerNum);
    return 0;
}
