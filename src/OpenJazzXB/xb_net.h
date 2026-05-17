/**
 * xb_net.h -- OpenJazzXB network layer.
 *
 * Handles Xbox-specific network init (XNetStartup, WSAStartup),
 * async DNS resolution via XNetDnsLookup, and link status.
 *
 * Pattern taken directly from XbDiag Update.cpp / FileExplorer.cpp.
 * Must be included only in RXDK TUs (<xtl.h> first).
 */
#pragma once
#include <xtl.h>
#include <winsockx.h>

 /* -----------------------------------------------------------------------
    States
    ----------------------------------------------------------------------- */
typedef enum XbNetState {
    XBNET_IDLE = 0,  /* stack not started                 */
    XBNET_LINK_WAIT = 1,  /* waiting for DHCP / link           */
    XBNET_DNS_WAIT = 2,  /* XNetDnsLookup in progress         */
    XBNET_READY = 3,  /* IP resolved, ready to connect     */
    XBNET_NO_LINK = 4,  /* no ethernet link or DHCP failed   */
    XBNET_DNS_FAIL = 5,  /* DNS lookup failed                 */
} XbNetState;

/* -----------------------------------------------------------------------
   Public API
   ----------------------------------------------------------------------- */

   /* Start network stack (safe to call repeatedly -- XNet is ref-counted).
    * Call once before XbNet_BeginResolve. */
void XbNet_Init(void);

/* Begin async DNS lookup for the given hostname.
 * Transitions state: IDLE -> LINK_WAIT -> DNS_WAIT -> READY/FAIL.
 * Call XbNet_Poll() every frame until XbNet_GetState() == XBNET_READY. */
void XbNet_BeginResolve(const char* hostname);

/* Poll the async state machine -- call every frame while resolving.
 * Safe to call in any state (no-op when idle or complete). */
void XbNet_Poll(void);

/* Cancel / tear down. Safe to call in any state. */
void XbNet_Reset(void);

/* Current state */
XbNetState XbNet_GetState(void);

/* Resolved IP as in_addr (valid only when state == XBNET_READY) */
struct in_addr XbNet_GetAddr(void);

/* Console's own IP as dotted string (valid after LINK_WAIT passes) */
void XbNet_GetLocalIP(char* buf, int bufLen);

/* Timeout in ms for link wait (default 5000) */
#define XBNET_LINK_TIMEOUT_MS   5000
#define XBNET_DNS_TIMEOUT_MS    5000

/* -----------------------------------------------------------------------
   TCP lobby connection
   ----------------------------------------------------------------------- */

#define OJXB_HELLO    0xF0
#define OJXB_ROOMLIST 0xF1
#define OJXB_JOIN     0xF2
#define OJXB_ROOMINFO 0xF3
#define OJXB_START    0xF4

#define XBNET_MAX_ROOMS   8
#define XBNET_MAX_PLAYERS 2
#define XBNET_NAME_LEN    16

typedef struct XbNetRoom {
    unsigned char  room_id;
    unsigned char  nPlayers;
    unsigned char  maxPlayers;
    unsigned char  status;       /* 0=waiting, 1=playing */
    char           hostName[XBNET_NAME_LEN + 1];
} XbNetRoom;

typedef struct XbNetLobbyState {
    int        nRooms;
    XbNetRoom  rooms[XBNET_MAX_ROOMS];
    int        mySlot;
    char       players[XBNET_MAX_PLAYERS][XBNET_NAME_LEN + 1];
    int        nPlayers;
} XbNetLobbyState;

/* Connect TCP to resolved server address and send HELLO with player name.
 * Call after XbNet_GetState() == XBNET_READY. */
int  XbNet_Connect(const char* playerName);

/* Non-blocking poll for incoming lobby packet.
 * Returns 1 if roomlist/roominfo was received and state updated,
 * 0 if nothing yet, -1 on error/disconnect. */
int  XbNet_LobbyPoll(XbNetLobbyState* state);

/* Send OJXB_JOIN for a room (0 = create new). */
void XbNet_JoinRoom(unsigned char room_id);

/* Disconnect and clean up socket. */
void XbNet_Disconnect(void);

/* Returns 1 if TCP socket is connected */
int  XbNet_IsConnected(void);

/* Raw send/recv on the relay socket (used by Network class) */
int  XbNet_SendData(const unsigned char* buf, int len);
int  XbNet_RecvData(unsigned char* buf, int len);