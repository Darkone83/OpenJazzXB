/**
 * xb_net.cpp -- OpenJazzXB network layer.
 * Pattern taken directly from XbDiag Update.cpp and FileExplorer.cpp.
 *
 * RXDK rules: <xtl.h> first. No STL. No C++ exceptions.
 */

#include <xtl.h>
#include <winsockx.h>
#include "xb_net.h"

 /* -----------------------------------------------------------------------
    Internal state
    ----------------------------------------------------------------------- */
static XbNetState  s_state = XBNET_IDLE;
static bool        s_netInited = false;
static XNDNS* s_dns = NULL;
static struct in_addr s_resolved;
static DWORD       s_waitStart = 0;
static char        s_localIP[20] = "?.?.?.?";
static char        s_pendingHost[64] = "";

/* -----------------------------------------------------------------------
   XbNet_Init
   Matches XbDiag FileExplorer.cpp / Update.cpp NetEnsure() exactly.
   XNet is ref-counted -- safe to call multiple times.
   ----------------------------------------------------------------------- */
void XbNet_Init(void) {
    if (s_netInited) return;

    XNetStartupParams xnsp;
    ZeroMemory(&xnsp, sizeof(xnsp));
    xnsp.cfgSizeOfStruct = sizeof(xnsp);
    xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
    XNetStartup(&xnsp);

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    s_netInited = true;
}

/* -----------------------------------------------------------------------
   XbNet_BeginResolve
   Kicks off: IDLE -> LINK_WAIT -> DNS_WAIT -> READY/FAIL
   ----------------------------------------------------------------------- */
void XbNet_BeginResolve(const char* hostname) {
    XbNet_Init();
    XbNet_Reset();   /* cancel any previous attempt */

    /* Store hostname for DNS poll */
    int i = 0;
    while (hostname[i] && i < 63) { s_pendingHost[i] = hostname[i]; i++; }
    s_pendingHost[i] = '\0';

    /* Kick off at LINK_WAIT -- Poll() will advance from there */
    s_waitStart = GetTickCount();
    s_state = XBNET_LINK_WAIT;
}

/* -----------------------------------------------------------------------
   XbNet_Poll
   State machine -- call every frame.
   Matches XbDiag Update.cpp BootCheckPoll() exactly.
   ----------------------------------------------------------------------- */

static void PollLinkWait(void) {
    XNADDR xna;
    ZeroMemory(&xna, sizeof(xna));
    DWORD st = XNetGetTitleXnAddr(&xna);

    if (st == XNET_GET_XNADDR_PENDING) {
        /* Still waiting for DHCP */
        if (GetTickCount() - s_waitStart > XBNET_LINK_TIMEOUT_MS) {
            s_state = XBNET_NO_LINK;
        }
        return;
    }

    if ((st & XNET_GET_XNADDR_NONE) || xna.ina.s_addr == 0) {
        s_state = XBNET_NO_LINK;
        return;
    }

    /* Link is up -- store local IP string */
    BYTE* b = (BYTE*)&xna.ina.s_addr;
    char* p = s_localIP;
    char oct[6];
    int oi, di;
    for (oi = 0; oi < 4; oi++) {
        int v = (int)b[oi];
        int len = 0;
        if (v == 0) { oct[len++] = '0'; }
        else { int tmp = v; while (tmp > 0) { oct[len++] = '0' + tmp % 10; tmp /= 10; } }
        /* reverse */
        for (di = 0; di < len / 2; di++) {
            char c = oct[di]; oct[di] = oct[len - 1 - di]; oct[len - 1 - di] = c;
        }
        oct[len] = '\0';
        char* s = oct;
        while (*s) *p++ = *s++;
        if (oi < 3) *p++ = '.';
    }
    *p = '\0';

    /* Kick off DNS */
    int dr = XNetDnsLookup(s_pendingHost, NULL, &s_dns);
    if (dr != 0 || !s_dns) {
        s_state = XBNET_DNS_FAIL;
        return;
    }
    s_waitStart = GetTickCount();
    s_state = XBNET_DNS_WAIT;
}

static void PollDnsWait(void) {
    if (!s_dns) { s_state = XBNET_DNS_FAIL; return; }

    if (s_dns->iStatus == WSAEINPROGRESS) {
        if (GetTickCount() - s_waitStart > XBNET_DNS_TIMEOUT_MS) {
            XNetDnsRelease(s_dns); s_dns = NULL;
            s_state = XBNET_DNS_FAIL;
        }
        return;
    }

    if (s_dns->iStatus != 0) {
        XNetDnsRelease(s_dns); s_dns = NULL;
        s_state = XBNET_DNS_FAIL;
        return;
    }

    /* DNS resolved -- grab first address */
    s_resolved = s_dns->aina[0];
    XNetDnsRelease(s_dns); s_dns = NULL;
    s_state = XBNET_READY;
}

void XbNet_Poll(void) {
    switch (s_state) {
    case XBNET_LINK_WAIT: PollLinkWait(); break;
    case XBNET_DNS_WAIT:  PollDnsWait();  break;
    default: break;
    }
}

/* -----------------------------------------------------------------------
   XbNet_Reset
   ----------------------------------------------------------------------- */
void XbNet_Reset(void) {
    if (s_dns) { XNetDnsRelease(s_dns); s_dns = NULL; }
    s_state = XBNET_IDLE;
    ZeroMemory(&s_resolved, sizeof(s_resolved));
}

/* -----------------------------------------------------------------------
   Accessors
   ----------------------------------------------------------------------- */
XbNetState    XbNet_GetState(void) { return s_state; }
struct in_addr XbNet_GetAddr(void) { return s_resolved; }
void XbNet_GetLocalIP(char* buf, int len) {
    int i = 0;
    while (s_localIP[i] && i < len - 1) { buf[i] = s_localIP[i]; i++; }
    buf[i] = '\0';
}

/* -----------------------------------------------------------------------
   TCP lobby implementation
   ----------------------------------------------------------------------- */

static SOCKET   s_sock = INVALID_SOCKET;
static int      s_connected = 0;

/* Send one framed packet (buffer[0] = total length) */
static int SendPkt(const unsigned char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(s_sock, (const char*)(buf + sent), len - sent, 0);
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) { Sleep(1); continue; }
            return -1;
        }
        sent += n;
    }
    return sent;   /* return bytes sent, not 0 */
}

/* Blocking receive of exactly n bytes */
static int RecvExact(unsigned char* buf, int n) {
    int got = 0;
    while (got < n) {
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

int XbNet_Connect(const char* playerName) {
    XbNet_Disconnect();

    s_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_sock == INVALID_SOCKET) return -1;

    /* Connect to resolved address on NET_PORT */
    struct sockaddr_in sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(20052);
    sa.sin_addr = s_resolved;

    if (connect(s_sock, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) {
        closesocket(s_sock); s_sock = INVALID_SOCKET; return -1;
    }

    /* Switch to non-blocking after connect */
    u_long nb = 1;
    ioctlsocket(s_sock, FIONBIO, &nb);
    s_connected = 1;

    /* Send OJXB_HELLO: [length][0xF0][name bytes] */
    unsigned char hello[20];
    int nameLen = 0;
    while (playerName[nameLen] && nameLen < 16) nameLen++;
    hello[0] = (unsigned char)(3 + nameLen);
    hello[1] = 0xF0;   /* OJXB_HELLO */
    int i;
    for (i = 0; i < nameLen; i++) hello[2 + i] = (unsigned char)playerName[i];
    hello[2 + nameLen] = '\0';
    if (SendPkt(hello, 3 + nameLen) < 0) {
        XbNet_Disconnect(); return -1;
    }
    return 0;
}

/* Parse OJXB_ROOMLIST into state */
static void ParseRoomList(const unsigned char* data, int len, XbNetLobbyState* state) {
    if (len < 1) return;
    int nRooms = data[0];
    if (nRooms > XBNET_MAX_ROOMS) nRooms = XBNET_MAX_ROOMS;
    state->nRooms = nRooms;
    int offset = 1;
    int ri;
    for (ri = 0; ri < nRooms && offset + 20 <= len; ri++) {
        state->rooms[ri].room_id = data[offset + 0];
        state->rooms[ri].nPlayers = data[offset + 1];
        state->rooms[ri].maxPlayers = data[offset + 2];
        state->rooms[ri].status = data[offset + 3];
        int ci;
        for (ci = 0; ci < 16; ci++)
            state->rooms[ri].hostName[ci] = (char)data[offset + 4 + ci];
        state->rooms[ri].hostName[16] = '\0';
        offset += 20;
    }
}

/* Parse OJXB_ROOMINFO into state */
static void ParseRoomInfo(const unsigned char* data, int len, XbNetLobbyState* state) {
    /* [room_id][nPlayers][slot, name x16 per player] */
    if (len < 2) return;
    int nPlayers = data[1];
    if (nPlayers > XBNET_MAX_PLAYERS) nPlayers = XBNET_MAX_PLAYERS;
    state->nPlayers = nPlayers;
    int offset = 2, pi;
    for (pi = 0; pi < nPlayers && offset + 17 <= len; pi++) {
        int slot = data[offset];
        int ci;
        for (ci = 0; ci < 16; ci++)
            state->players[slot < XBNET_MAX_PLAYERS ? slot : 0][ci] = (char)data[offset + 1 + ci];
        state->players[slot < XBNET_MAX_PLAYERS ? slot : 0][16] = '\0';
        offset += 17;
    }
}

int XbNet_LobbyPoll(XbNetLobbyState* state) {
    if (s_sock == INVALID_SOCKET || !s_connected) return -1;

    /* Try to read packet header (1 byte) non-blocking */
    unsigned char lenByte;
    int r = recv(s_sock, (char*)&lenByte, 1, 0);
    if (r == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
        XbNet_Disconnect(); return -1;
    }
    if (r == 0) { XbNet_Disconnect(); return -1; }

    int pktLen = (int)lenByte;
    if (pktLen < 2) return 0;

    unsigned char buf[256];
    buf[0] = lenByte;
    if (RecvExact(buf + 1, pktLen - 1) < 0) { XbNet_Disconnect(); return -1; }

    unsigned char type = buf[1];
    if (type == 0xF1) {      /* OJXB_ROOMLIST */
        ParseRoomList(buf + 2, pktLen - 2, state);
        return 1;
    }
    if (type == 0xF3) {      /* OJXB_ROOMINFO */
        ParseRoomInfo(buf + 2, pktLen - 2, state);
        return 1;
    }
    if (type == 0xF5) {      /* OJXB_MAPSEL -- server chosen level, start game */
        return 2;
    }
    return 0;
}

void XbNet_JoinRoom(unsigned char room_id) {
    if (s_sock == INVALID_SOCKET || !s_connected) return;
    unsigned char pkt[3] = { 3, 0xF2, room_id };
    SendPkt(pkt, 3);
}

void XbNet_Disconnect(void) {
    if (s_sock != INVALID_SOCKET) { closesocket(s_sock); s_sock = INVALID_SOCKET; }
    s_connected = 0;
}

int XbNet_IsConnected(void) { return s_connected; }

int XbNet_SendData(const unsigned char* buf, int len) {
    if (s_sock == INVALID_SOCKET || !s_connected) return -1;
    return SendPkt(buf, len);
}

int XbNet_RecvData(unsigned char* buf, int len) {
    if (s_sock == INVALID_SOCKET || !s_connected) return 0;
    int r = recv(s_sock, (char*)buf, len, 0);
    if (r == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
        XbNet_Disconnect(); return -1;
    }
    if (r == 0) { XbNet_Disconnect(); return -1; }
    return r;
}