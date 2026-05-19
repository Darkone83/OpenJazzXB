/**
 * xb_net_glue.cpp
 * XbJazz -- Plain-C wrappers around xb_net.cpp.
 *
 * RXDK TU: <xtl.h> must be first.
 * Implements the extern "C" interface declared in xb_net_glue.h.
 * Translates between XbNetGlueState (glue-visible) and XbNetLobbyState
 * (xb_net-internal) so the setup menu never needs to include xb_net.h.
 */

#include <xtl.h>
#include <winsockx.h>
#include "xb_net_glue.h"
#include "xb_net.h"
#include "network.h"

 /* ── XNetStartup config ─────────────────────────────────────────────────── */

static XNetStartupParams s_xnsp;
static bool s_netInited = false;




/* ── Lobby state translation ────────────────────────────────────────────── */

static void CopyState(const XbNetLobbyState* src, XbNetGlueState* dst) {
    int i, j;

    dst->nRooms = src->nRooms;
    dst->mySlot = src->mySlot;
    dst->nPlayers = src->nPlayers;

    for (i = 0; i < src->nRooms && i < XBNETG_MAX_ROOMS; i++) {
        dst->rooms[i].room_id = src->rooms[i].room_id;
        dst->rooms[i].nPlayers = src->rooms[i].nPlayers;
        dst->rooms[i].maxPlayers = src->rooms[i].maxPlayers;
        dst->rooms[i].status = src->rooms[i].status;
        for (j = 0; j < XBNETG_NAME_LEN; j++)
            dst->rooms[i].hostName[j] = src->rooms[i].hostName[j];
    }

    for (i = 0; i < XBNETG_MAX_PLAYERS; i++)
        for (j = 0; j < XBNETG_NAME_LEN; j++)
            dst->players[i][j] = src->players[i][j];
}

/* ── XbNetG_ConnectBegin / Poll / Error ───────────────────────────────── */

void XbNetG_ConnectBegin(const char* addrPort, const char* playerName) {
    XbNet_ConnectBegin(addrPort, playerName);
}

int XbNetG_ConnectPoll(void) {
    return XbNet_ConnectPoll();
}

void XbNetG_ConnectError(char* buf, int bufLen) {
    XbNet_ConnectError(buf, bufLen);
}

/* ── XbNetG_LobbyPoll ───────────────────────────────────────────────────── */

int XbNetG_LobbyPoll(XbNetGlueState* state) {
    XbNetLobbyState inner;
    memset(&inner, 0, sizeof(inner));

    int ret = XbNet_LobbyPoll(&inner);

    if (ret != 0 && state)
        CopyState(&inner, state);

    return ret;
}

/* ── XbNetG_JoinRoom ────────────────────────────────────────────────────── */

void XbNetG_JoinRoom(unsigned char room_id) {
    XbNet_JoinRoom(room_id);
}

/* ── XbNetG_SendMapsel ──────────────────────────────────────────────────── */

void XbNetG_SendMapsel(const char* levelFile, unsigned char difficulty) {
    XbNet_SendMapsel(levelFile, difficulty);
}

/* ── XbNetG_GetClientLevelFile ──────────────────────────────────────────── */

void XbNetG_GetClientLevelFile(char* buf, int bufLen) {
    XbNet_GetClientLevelFile(buf, bufLen);
}

/* ── XbNetG_SetExpectedClients ──────────────────────────────────────────── */

void XbNetG_SetExpectedClients(int n) {
    XbNet_SetExpectedClients(n);
}

/* ── XbNetG_Disconnect ──────────────────────────────────────────────────── */

void XbNetG_Disconnect(void) {
    XbNet_Disconnect();
}

/* ── XbNetG_IsConnected ─────────────────────────────────────────────────── */

int XbNetG_IsConnected(void) {
    return XbNet_IsConnected();
}