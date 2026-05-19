#pragma once
/**
 * xb_net.h -- XbJazz TCP relay network layer.
 * RXDK TU -- no OJ headers, no STL.
 *
 * Async connect state machine matches XbDiag Update.cpp exactly:
 *   XbNet_ConnectBegin()  -- kick off (non-blocking)
 *   XbNet_ConnectPoll()   -- call every frame: 0=working, 1=done, -1=fail
 *   XbNet_ConnectError()  -- get error string on failure
 */

#ifndef XB_NET_H
#define XB_NET_H

#ifdef __cplusplus
extern "C" {
#endif

#define XBNET_DEFAULT_PORT  10052
#define XBNET_MAX_ROOMS     16
#define XBNET_MAX_PLAYERS   2
#define XBNET_NAME_LEN      16
#define XBNET_ADDR_LEN      64

    typedef struct XbNetRoom {
        unsigned char room_id;
        unsigned char nPlayers;
        unsigned char maxPlayers;
        unsigned char status;
        char          hostName[XBNET_NAME_LEN];
    } XbNetRoom;

    typedef struct XbNetLobbyState {
        XbNetRoom rooms[XBNET_MAX_ROOMS];
        int       nRooms;
        int       mySlot;
        char      players[XBNET_MAX_PLAYERS][XBNET_NAME_LEN];
        int       nPlayers;
    } XbNetLobbyState;

    /* ── Async connect API ───────────────────────────────────────────── */

    /** Kick off async connect. Non-blocking -- returns immediately. */
    void XbNet_ConnectBegin(const char* addrPort, const char* playerName);

    /**
     * Advance the connect state machine. Call every menu frame.
     * Returns 0 (working), 1 (connected + HELLO sent), -1 (failed).
     */
    int  XbNet_ConnectPoll(void);

    /** Get error string after ConnectPoll returns -1. */
    void XbNet_ConnectError(char* buf, int bufLen);

    /* ── Lobby / game API ────────────────────────────────────────────── */

    int  XbNet_LobbyPoll(XbNetLobbyState* state);
    void XbNet_JoinRoom(unsigned char room_id);
    void XbNet_SendMapsel(const char* levelFile, unsigned char difficulty);
    void XbNet_GetClientLevelFile(char* buf, int bufLen);
    int  XbNet_SendData(const unsigned char* buf, int len);
    int  XbNet_RecvData(unsigned char* buf, int len);
    void XbNet_Disconnect(void);
    int  XbNet_IsConnected(void);
    int  XbNet_ParseAddrPort(const char* addrPort, char* hostBuf, int hostBufLen);

#ifdef __cplusplus
}
#endif

#endif /* XB_NET_H */