#pragma once
/**
 * xb_localrelay.h -- OpenJazzXB local LAN lobby/control shim.
 *
 * Purpose:
 *   - Coordinate LAN host/client start without an external relay.
 *   - Carry the selected level/difficulty from host to client.
 *   - Then hand off to the existing ClientGame / ServerGame path.
 *
 * This module is not intended to be a permanent gameplay relay.
 */

#ifndef XB_LOCALRELAY_H
#define XB_LOCALRELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#define XBLOCALRELAY_VERSION      1
#define XBLOCALRELAY_DEFAULT_PORT 10054
#define XBLOCALRELAY_NAME_LEN     16
#define XBLOCALRELAY_LEVEL_LEN    16

#define XBLOCALRELAY_IDLE              0
#define XBLOCALRELAY_LISTENING         1
#define XBLOCALRELAY_CLIENT_CONNECTED  2
#define XBLOCALRELAY_CONNECTING        3
#define XBLOCALRELAY_WAITING_START     4
#define XBLOCALRELAY_START_RECEIVED    5
#define XBLOCALRELAY_ERROR            -1

int  XbLocalRelay_Init(void);
void XbLocalRelay_Shutdown(void);

/* Host side */
int  XbLocalRelay_HostStart(unsigned short controlPort);
void XbLocalRelay_HostStop(void);
void XbLocalRelay_HostPoll(void);
int  XbLocalRelay_HostState(void);
int  XbLocalRelay_HostHasClient(void);
void XbLocalRelay_HostGetClientName(char* outName, int outSize);
int  XbLocalRelay_HostSendStart(const char* levelFile, unsigned char difficulty);

/* Client side */
int  XbLocalRelay_ClientConnect(const char* hostIp,
                                unsigned short controlPort,
                                const char* playerName);
void XbLocalRelay_ClientStop(void);
void XbLocalRelay_ClientPoll(void);
int  XbLocalRelay_ClientState(void);
int  XbLocalRelay_ClientHasStart(void);
void XbLocalRelay_ClientGetLevel(char* outLevel, int outSize);
unsigned char XbLocalRelay_ClientGetDifficulty(void);

/* LAN client launch-level handoff.
 * This is separate from the control socket state so setupmenu.cpp can stop
 * the LAN control layer before launching ClientGame, while ClientGame can
 * still read the selected level without touching xb_net / xb_net_glue.
 */
void XbLocalRelay_SetLaunchLevel(const char* levelFile, unsigned char difficulty);
int  XbLocalRelay_HasLaunchLevel(void);
void XbLocalRelay_GetLaunchLevel(char* outLevel, int outSize);
unsigned char XbLocalRelay_GetLaunchDifficulty(void);
void XbLocalRelay_ClearLaunchLevel(void);

/* LAN game bridge mode. */
#define XBLOCALRELAY_GAME_NONE   0
#define XBLOCALRELAY_GAME_HOST   1
#define XBLOCALRELAY_GAME_CLIENT 2

void XbLocalRelay_HostSetName(const char* hostName);
int  XbLocalRelay_HostBeginGame(void);
int  XbLocalRelay_ClientBeginGame(void);
void XbLocalRelay_GameEnd(void);

int  XbLocalRelay_GameRole(void);
int  XbLocalRelay_GameAcceptPending(void);

int  XbLocalRelay_GameHostSend(const unsigned char* buffer, int len);
int  XbLocalRelay_GameHostRecv(unsigned char* buffer, int len);
int  XbLocalRelay_GameClientSend(const unsigned char* buffer, int len);
int  XbLocalRelay_GameClientRecv(unsigned char* buffer, int len);
int  XbLocalRelay_GameIsConnected(void);

void XbLocalRelay_GetLastError(char* outError, int outSize);

#ifdef __cplusplus
}
#endif

#endif /* XB_LOCALRELAY_H */
