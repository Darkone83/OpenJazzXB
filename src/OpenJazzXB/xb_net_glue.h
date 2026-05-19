#pragma once
/**
 * xb_net_glue.h
 * XbJazz -- Plain-C wrappers around xb_net.cpp.
 *
 * xb_net.cpp is an RXDK TU (<xtl.h> first, no OJ headers).
 * The setup menu is an OJ TU (OJ headers, no <xtl.h>).
 * This glue layer is declared with extern "C" linkage so both sides
 * can include it safely without header conflicts.
 *
 * All structs are plain C -- no STL, no OJ types.
 */

#ifndef XB_NET_GLUE_H
#define XB_NET_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

    /* ── Constants (must match xb_net.h) ────────────────────────────────────── */

#define XBNETG_MAX_ROOMS   16
#define XBNETG_MAX_PLAYERS 2
#define XBNETG_NAME_LEN    16
#define XBNETG_ADDR_LEN    64

/* ── Lobby state ─────────────────────────────────────────────────────────── */

    typedef struct XbNetGlueRoom {
        unsigned char room_id;
        unsigned char nPlayers;
        unsigned char maxPlayers;
        unsigned char status;           /* 0=waiting, 1=playing */
        char          hostName[XBNETG_NAME_LEN];
    } XbNetGlueRoom;

    typedef struct XbNetGlueState {
        XbNetGlueRoom rooms[XBNETG_MAX_ROOMS];
        int           nRooms;
        int           mySlot;           /* 0=host, 1=client, -1=unassigned */
        char          players[XBNETG_MAX_PLAYERS][XBNETG_NAME_LEN];
        int           nPlayers;
    } XbNetGlueState;

    /* ── API ─────────────────────────────────────────────────────────────────── */

    /**
     * Initialise Xbox network stack (XNetStartup + WSAStartup).
     * Call once at startup before any other XbNetG_* function.
     * Returns 0 on success, -1 on failure.
     */
    int  XbNetG_Init(void);

    /**
     * Shut down Xbox network stack. Call on exit.
     */
    void XbNetG_Shutdown(void);

    /**
     * Kick off async connect. Non-blocking -- returns immediately.
     * Call XbNetG_ConnectPoll() every frame until done.
     */
    void XbNetG_ConnectBegin(const char* addrPort, const char* playerName);

    /**
     * Advance connect state machine. Call every frame.
     * Returns 0 (working), 1 (connected), -1 (failed).
     */
    int  XbNetG_ConnectPoll(void);

    /** Get error message after ConnectPoll returns -1. */
    void XbNetG_ConnectError(char* buf, int bufLen);

    /**
     * Non-blocking lobby poll. Call every menu frame.
     * Returns 0 (no change), 1 (state updated), 2 (MAPSEL -- game starting),
     * or -1 (disconnected).
     */
    int  XbNetG_LobbyPoll(XbNetGlueState* state);

    /**
     * Send JOIN to relay. room_id 0 = create new room.
     */
    void XbNetG_JoinRoom(unsigned char room_id);

    /**
     * Host only: send OJXB_MAPSEL to relay.
     * levelFile e.g. "LEVEL1.001", difficulty 0-3.
     */
    void XbNetG_SendMapsel(const char* levelFile, unsigned char difficulty);

    /**
     * After LobbyPoll returns 2, client calls this to get the level filename.
     */
    void XbNetG_GetClientLevelFile(char* buf, int bufLen);

    /**
     * Prime the Network::accept() counter before creating ServerGame.
     */
    void XbNetG_SetExpectedClients(int n);

    /**
     * Disconnect from relay.
     */
    void XbNetG_Disconnect(void);

    /**
     * Returns non-zero if connected.
     */
    int  XbNetG_IsConnected(void);

#ifdef __cplusplus
}
#endif

#endif /* XB_NET_GLUE_H */