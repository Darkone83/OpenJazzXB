/**
 * xb_net.cpp -- XbJazz TCP relay network layer.
 *
 * RXDK TU: <xtl.h> first. No OJ headers. No STL.
 *
 * Async connect state machine mirrors XbDiag Update.cpp exactly:
 *   NetEnsure()     = XNetStartup + WSAStartup (no waiting)
 *   XBNET_LINK_WAIT = poll XNetGetTitleXnAddr per frame
 *   XBNET_DNS_WAIT  = poll s_dns iStatus per frame
 *   XBNET_TCP_WAIT  = non-blocking connect, poll select per frame
 *   XBNET_READY     = connected, send HELLO, enter lobby
 *
 * XbNet_ConnectBegin() kicks off the state machine.
 * XbNet_ConnectPoll() advances it -- call every menu frame.
 * XbNet_ConnectPoll() returns:
 *    0  still working
 *    1  connected and HELLO sent
 *   -1  failed (call XbNet_ConnectError() for message)
 */

#include <xtl.h>
#include <winsockx.h>
#include "xb_net.h"
 /* Forward declarations from xb_netlog.h */
extern "C" void XbNetLog_Write(const char* msg);
extern "C" void XbNetLog_Packet(const char* direction, unsigned char type, int length);
extern "C" void XbNetLog_Close(void);

/* ── Connect state ───────────────────────────────────────────────────── */

#define CS_IDLE   0
#define CS_LINK   1
#define CS_DNS    2
#define CS_TCP    3
#define CS_READY  4
#define CS_FAILED 5

static int          s_cs = CS_IDLE;
static SOCKET       s_sock = INVALID_SOCKET;
static int          s_connected = 0;
static XNDNS* s_dns = NULL;
static IN_ADDR        s_serverAddr;
static int          s_relayPort = 10052;
static DWORD         s_stateStart = 0;
static int          s_netUp = 0;

/* Pending host string for DNS (set by XbNet_ConnectBegin) */
char s_relayHost[64] = { 0 };
/* Pending player name for HELLO */
char s_relayPlayerName[16] = { 0 };
/* Error message */
char s_relayError[64] = { 0 };

/* Level file received from OJXB_MAPSEL */
char s_clientLevelFile[16] = { 0 };

/* ── Lobby packet IDs ─────────────────────────────────────────────── */

#define OJXB_HELLO    0xF0u
#define OJXB_ROOMLIST 0xF1u
#define OJXB_JOIN     0xF2u
#define OJXB_ROOMINFO 0xF3u
#define OJXB_MAPSEL   0xF5u

#define CS_TIMEOUT_LINK  5000u
#define CS_TIMEOUT_DNS   5000u
#define CS_TIMEOUT_TCP   5000u

/* ── Internal helpers ─────────────────────────────────────────────── */

static void SetFailed(const char* msg) {
    int i = 0;
    while (msg[i] && i < 63) { s_relayError[i] = msg[i]; i++; }
    s_relayError[i] = '\0';
    s_cs = CS_FAILED;
    if (s_sock != INVALID_SOCKET) { closesocket(s_sock); s_sock = INVALID_SOCKET; }
    if (s_dns) { XNetDnsRelease(s_dns); s_dns = NULL; }
    s_connected = 0;
}

static void NetEnsure(void) {
    /* XNet is ref-counted -- safe to call repeatedly. XbDiag pattern. */
    if (s_netUp) return;
    XNetStartupParams xnsp;
    ZeroMemory(&xnsp, sizeof(xnsp));
    xnsp.cfgSizeOfStruct = sizeof(xnsp);
    xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
    XNetStartup(&xnsp);
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    s_netUp = 1;
}

static int SendAll(const unsigned char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(s_sock, (const char*)(buf + sent), len - sent, 0);
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) { Sleep(1); continue; }
            return -1;
        }
        sent += n;
    }
    if (len >= 2) XbNetLog_Packet("TX", buf[1], len);
    return 0;
}

static int SendPkt(const unsigned char* buf) {
    return SendAll(buf, buf[0]);
}

/* ── XbNet_ConnectBegin ───────────────────────────────────────────── */

void XbNet_ConnectBegin(const char* addrPort, const char* playerName) {
    /* Clean up previous */
    XbNet_Disconnect();
    if (s_dns) { XNetDnsRelease(s_dns); s_dns = NULL; }

    /* Parse host:port */
    s_relayPort = XBNET_DEFAULT_PORT;
    int i = 0;
    int colonPos = -1;
    while (addrPort[i]) { if (addrPort[i] == ':') colonPos = i; i++; }

    i = 0;
    if (colonPos >= 0) {
        int hi = 0;
        while (i < colonPos && hi < 63) s_relayHost[hi++] = addrPort[i++];
        s_relayHost[hi] = '\0';
        i++; /* skip : */
        int p = 0;
        while (addrPort[i] >= '0' && addrPort[i] <= '9') {
            p = p * 10 + (addrPort[i] - '0'); i++;
        }
        if (p > 0) s_relayPort = p;
    }
    else {
        int hi = 0;
        while (addrPort[i] && hi < 63) s_relayHost[hi++] = addrPort[i++];
        s_relayHost[hi] = '\0';
    }

    /* Copy player name */
    i = 0;
    while (playerName[i] && i < 15) { s_relayPlayerName[i] = playerName[i]; i++; }
    s_relayPlayerName[i] = '\0';

    s_relayError[0] = '\0';
    s_clientLevelFile[0] = '\0';
    /* s_relayHost already set above */

    /* Kick off -- NetEnsure first, then link wait */
    NetEnsure();
    s_stateStart = GetTickCount();
    s_cs = CS_LINK;
}

/* ── XbNet_ConnectPoll ────────────────────────────────────────────── */

int XbNet_ConnectPoll(void) {

    switch (s_cs) {

    case CS_IDLE:
    case CS_READY:
    case CS_FAILED:
        return (s_cs == CS_READY) ? 1 : (s_cs == CS_FAILED) ? -1 : 0;

        /* ── Link wait (XbDiag BootCheckPoll UPST_NET_INIT) ──────────── */
    case CS_LINK: {
        XNADDR xna;
        ZeroMemory(&xna, sizeof(xna));
        DWORD st = XNetGetTitleXnAddr(&xna);
        if (st == XNET_GET_XNADDR_PENDING) {
            if (GetTickCount() - s_stateStart > CS_TIMEOUT_LINK)
                SetFailed("No network link");
            return 0;
        }
        if ((st & XNET_GET_XNADDR_NONE) || xna.ina.s_addr == 0) {
            SetFailed("No network link"); return -1;
        }
        /* Link up -- start DNS */
        int dr = XNetDnsLookup(s_relayHost, NULL, &s_dns);
        if (dr != 0 || !s_dns) { SetFailed("DNS start failed"); return -1; }
        s_stateStart = GetTickCount();
        s_cs = CS_DNS;
        return 0;
    }

                /* ── DNS wait (XbDiag BootCheckPoll UPST_DNS) ────────────────── */
    case CS_DNS: {
        if (!s_dns) { SetFailed("DNS handle null"); return -1; }
        if (s_dns->iStatus == WSAEINPROGRESS) {
            if (GetTickCount() - s_stateStart > CS_TIMEOUT_DNS) {
                XNetDnsRelease(s_dns); s_dns = NULL;
                SetFailed("DNS timeout"); return -1;
            }
            return 0;
        }
        if (s_dns->iStatus != 0) {
            XNetDnsRelease(s_dns); s_dns = NULL;
            SetFailed("DNS failed"); return -1;
        }
        s_serverAddr = s_dns->aina[0];
        XNetDnsRelease(s_dns); s_dns = NULL;

        /* Begin non-blocking TCP connect (XbDiag BeginConnect) */
        s_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s_sock == INVALID_SOCKET) { SetFailed("socket() failed"); return -1; }
        unsigned long nb = 1;
        ioctlsocket(s_sock, FIONBIO, &nb);
        struct sockaddr_in sa;
        ZeroMemory(&sa, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons((unsigned short)s_relayPort);
        sa.sin_addr = s_serverAddr;
        int r = connect(s_sock, (struct sockaddr*)&sa, sizeof(sa));
        if (r != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
            SetFailed("connect() failed"); return -1;
        }
        s_stateStart = GetTickCount();
        s_cs = CS_TCP;
        return 0;
    }

               /* ── TCP connect wait (XbDiag PollConnect) ───────────────────── */
    case CS_TCP: {
        if (GetTickCount() - s_stateStart > CS_TIMEOUT_TCP) {
            SetFailed("Connect timeout"); return -1;
        }
        fd_set wfds, efds;
        FD_ZERO(&wfds); FD_SET(s_sock, &wfds);
        FD_ZERO(&efds); FD_SET(s_sock, &efds);
        TIMEVAL tv = { 0, 0 };
        int r = select(0, NULL, &wfds, &efds, &tv);
        if (r == SOCKET_ERROR) { SetFailed("select() failed"); return -1; }
        if (FD_ISSET(s_sock, &efds)) { SetFailed("Connect refused"); return -1; }
        if (!FD_ISSET(s_sock, &wfds)) return 0; /* still waiting */

        /* Connected -- send HELLO */
        s_connected = 1;
        int nameLen = 0;
        while (s_relayPlayerName[nameLen] && nameLen < 15) nameLen++;
        unsigned char pkt[2 + 16];
        pkt[0] = (unsigned char)(2 + nameLen + 1);
        pkt[1] = OJXB_HELLO;
        int pi;
        for (pi = 0; pi < nameLen; pi++) pkt[2 + pi] = (unsigned char)s_relayPlayerName[pi];
        pkt[2 + nameLen] = '\0';
        if (SendPkt(pkt) < 0) { SetFailed("HELLO send failed"); return -1; }

        XbNetLog_Write("Connected and HELLO sent");
        s_cs = CS_READY;
        return 1;
    }

    default: return 0;
    }
}

/* ── XbNet_ConnectError ───────────────────────────────────────────── */

void XbNet_ConnectError(char* buf, int bufLen) {
    int i = 0;
    while (s_relayError[i] && i < bufLen - 1) { buf[i] = s_relayError[i]; i++; }
    buf[i] = '\0';
}

/* ── RecvExact (blocking with timeout) ───────────────────────────── */

static int RecvExact(unsigned char* buf, int n, int timeout_ms) {
    int got = 0;
    DWORD deadline = GetTickCount() + (DWORD)timeout_ms;
    while (got < n) {
        if ((int)(GetTickCount() - deadline) > 0) return -1;
        int r = recv(s_sock, (char*)(buf + got), n - got, 0);
        if (r == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) { Sleep(1); continue; }
            return -1;
        }
        if (r == 0) return -1;
        got += r;
    }
    return 0;
}

/* ── XbNet_LobbyPoll ──────────────────────────────────────────────── */

int XbNet_LobbyPoll(XbNetLobbyState* state) {
    if (!s_connected || s_sock == INVALID_SOCKET) return -1;

    unsigned char lenBuf[1];
    int n = recv(s_sock, (char*)lenBuf, 1, 0);
    if (n == 0) { XbNet_Disconnect(); return -1; }
    if (n == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
        XbNet_Disconnect(); return -1;
    }

    int pktLen = lenBuf[0];
    if (pktLen < 2) return 0;

    unsigned char pkt[256];
    pkt[0] = lenBuf[0];
    if (RecvExact(pkt + 1, pktLen - 1, 2000) < 0) {
        XbNet_Disconnect(); return -1;
    }

    unsigned char type = pkt[1];

    if (type == OJXB_ROOMLIST) {
        if (pktLen < 3 || !state) return 1;
        int nRooms = pkt[2];
        if (nRooms > XBNET_MAX_ROOMS) nRooms = XBNET_MAX_ROOMS;
        state->nRooms = nRooms;
        int offset = 3, i, j;
        for (i = 0; i < nRooms; i++) {
            if (offset + 20 > pktLen) break;
            state->rooms[i].room_id = pkt[offset + 0];
            state->rooms[i].nPlayers = pkt[offset + 1];
            state->rooms[i].maxPlayers = pkt[offset + 2];
            state->rooms[i].status = pkt[offset + 3];
            for (j = 0; j < XBNET_NAME_LEN - 1; j++)
                state->rooms[i].hostName[j] = (char)pkt[offset + 4 + j];
            state->rooms[i].hostName[XBNET_NAME_LEN - 1] = '\0';
            offset += 20;
        }
        return 1;
    }

    if (type == OJXB_ROOMINFO) {
        if (pktLen < 4 || !state) return 1;
        int nPlayers = pkt[3];
        if (nPlayers > XBNET_MAX_PLAYERS) nPlayers = XBNET_MAX_PLAYERS;
        state->nPlayers = nPlayers;
        int offset = 4, i, j;
        for (i = 0; i < nPlayers; i++) {
            if (offset + 17 > pktLen) break;
            int slot = pkt[offset];
            if (slot < XBNET_MAX_PLAYERS) {
                /* Copy name for this slot */
                for (j = 0; j < XBNET_NAME_LEN - 1; j++)
                    state->players[slot][j] = (char)pkt[offset + 1 + j];
                state->players[slot][XBNET_NAME_LEN - 1] = '\0';
                /* Determine mySlot by matching our own player name */
                {
                    int match = 1, k;
                    for (k = 0; k < XBNET_NAME_LEN - 1; k++) {
                        char a = state->players[slot][k];
                        char b = s_relayPlayerName[k];
                        if (a != b) { match = 0; break; }
                        if (a == '\0') break;
                    }
                    if (match) state->mySlot = slot;
                }
            }
            offset += 17;
        }
        return 1;
    }

    if (type == OJXB_MAPSEL) {
        int fi = 0, offset = 3;
        while (fi < XBNET_NAME_LEN - 1 && offset + fi < pktLen) {
            char c = (char)pkt[offset + fi];
            if (c == '\0') break;
            s_clientLevelFile[fi] = c; fi++;
        }
        s_clientLevelFile[fi] = '\0';
        return 2;
    }

    return 0;
}

/* ── XbNet_JoinRoom ───────────────────────────────────────────────── */

void XbNet_JoinRoom(unsigned char room_id) {
    if (!s_connected) return;
    unsigned char pkt[3] = { 3, OJXB_JOIN, room_id };
    SendPkt(pkt);
}

/* ── XbNet_SendMapsel ─────────────────────────────────────────────── */

void XbNet_SendMapsel(const char* levelFile, unsigned char difficulty) {
    if (!s_connected) return;
    int nameLen = 0;
    while (levelFile[nameLen] && nameLen < XBNET_NAME_LEN - 1) nameLen++;
    unsigned char pkt[3 + XBNET_NAME_LEN];
    pkt[0] = (unsigned char)(3 + nameLen + 1);
    pkt[1] = OJXB_MAPSEL;
    pkt[2] = difficulty;
    int i;
    for (i = 0; i < nameLen; i++) pkt[3 + i] = (unsigned char)levelFile[i];
    pkt[3 + nameLen] = '\0';
    SendPkt(pkt);
}

/* ── XbNet_GetClientLevelFile ─────────────────────────────────────── */

void XbNet_GetClientLevelFile(char* buf, int bufLen) {
    int i = 0;
    while (s_clientLevelFile[i] && i < bufLen - 1) { buf[i] = s_clientLevelFile[i]; i++; }
    buf[i] = '\0';
}

/* ── XbNet_SendData / XbNet_RecvData ──────────────────────────────── */

int XbNet_SendData(const unsigned char* buf, int len) {
    if (!s_connected || s_sock == INVALID_SOCKET) return -1;
    return SendAll(buf, len);
}

int XbNet_RecvData(unsigned char* buf, int len) {
    if (!s_connected || s_sock == INVALID_SOCKET) return -1;
    int n = recv(s_sock, (char*)buf, len, 0);
    if (n == 0) { XbNet_Disconnect(); return -1; }
    if (n == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
        XbNet_Disconnect(); return -1;
    }
    if (n >= 2) XbNetLog_Packet("RX", buf[1], n);
    return n;
}

/* ── XbNet_Disconnect ─────────────────────────────────────────────── */

void XbNet_Disconnect(void) {
    if (s_sock != INVALID_SOCKET) {
        XbNetLog_Write("Disconnecting from relay");
        closesocket(s_sock);
        s_sock = INVALID_SOCKET;
    }
    s_connected = 0;
    s_cs = CS_IDLE;
    XbNetLog_Close();
}

/* ── XbNet_IsConnected ────────────────────────────────────────────── */

int XbNet_IsConnected(void) { return s_connected; }

/* ── XbNet_ParseAddrPort (kept for compat) ────────────────────────── */

int XbNet_ParseAddrPort(const char* addrPort, char* hostBuf, int hostBufLen) {
    int port = XBNET_DEFAULT_PORT, i = 0;
    while (addrPort[i] && addrPort[i] != ':' && i < hostBufLen - 1) {
        hostBuf[i] = addrPort[i]; i++;
    }
    hostBuf[i] = '\0';
    if (addrPort[i] == ':') {
        port = 0; i++;
        while (addrPort[i] >= '0' && addrPort[i] <= '9') {
            port = port * 10 + (addrPort[i] - '0'); i++;
        }
        if (port == 0) port = XBNET_DEFAULT_PORT;
    }
    return port;
}