/**
 * xb_localrelay.cpp -- OpenJazzXB local LAN lobby/control shim.
 *
 * RXDK TU: <xtl.h> first. No OJ headers, no STL.
 *
 * This is a cooperative single-thread state machine. It is intended to be
 * polled from setupmenu.cpp while the LAN host/client waits in menus.
 *
 * It does not forward gameplay. After START is sent/received, setupmenu.cpp
 * should stop this control layer and launch ServerGame / ClientGame.
 */

#include <xtl.h>
#include <winsockx.h>
#include "xb_localrelay.h"

#include <string.h>

#define XBLR_MAGIC0 'O'
#define XBLR_MAGIC1 'J'
#define XBLR_MAGIC2 'X'
#define XBLR_MAGIC3 'L'

#define XBLR_PKT_HELLO 0x20
#define XBLR_PKT_START 0x21
#define XBLR_PKT_CANCEL 0x22

#define XBLR_CONNECT_TIMEOUT 5000u

 /* OpenJazz game packet constants used by the local bridge. */
#define MT_G_PROPS   0x00
#define MT_G_PJOIN   0x01
#define MT_G_LEVEL   0x03
#define MT_G_LTYPE   0x06
#define MTL_G_PROPS  8
#define MTL_G_LEVEL  4
#define MTL_G_PJOIN  10
#define M_COOP       1
#define MAX_PLAYERS_LAN 2

#pragma pack(push, 1)
typedef struct XbLocalRelayHello {
    unsigned char magic[4];
    unsigned char type;
    unsigned char version;
    unsigned char reserved0;
    unsigned char reserved1;
    char          name[XBLOCALRELAY_NAME_LEN];
} XbLocalRelayHello;

typedef struct XbLocalRelayStart {
    unsigned char magic[4];
    unsigned char type;
    unsigned char version;
    unsigned char difficulty;
    unsigned char reserved0;
    char          levelFile[XBLOCALRELAY_LEVEL_LEN];
} XbLocalRelayStart;
#pragma pack(pop)

static int    s_netInited = 0;
static char   s_error[64];

static SOCKET s_listenSock = INVALID_SOCKET;
static SOCKET s_hostClientSock = INVALID_SOCKET;
static int    s_hostState = XBLOCALRELAY_IDLE;
static char   s_hostClientName[XBLOCALRELAY_NAME_LEN];

static SOCKET s_clientSock = INVALID_SOCKET;
static int    s_clientState = XBLOCALRELAY_IDLE;
static DWORD  s_clientConnectStart = 0;
static char   s_clientLevel[XBLOCALRELAY_LEVEL_LEN];
static unsigned char s_clientDifficulty = 0;

/* LAN client launch handoff.
 * This state is intentionally independent of the live client control socket.
 */
static char   s_launchLevel[XBLOCALRELAY_LEVEL_LEN];
static unsigned char s_launchDifficulty = 0;
static int    s_launchLevelValid = 0;

static int    s_gameRole = XBLOCALRELAY_GAME_NONE;
static int    s_gameAcceptPending = 0;
static char   s_hostName[XBLOCALRELAY_NAME_LEN];

/* Local bridge helper prototypes.
 * Keep these above XbLocalRelay_HostSendStart().
 */
static int IsHostSuppressedPacket(const unsigned char* buffer, int len);
static int SendPacketToClientSocket(const unsigned char* data, int len);
static int BuildPjoin(unsigned char* out, int outMax,
    const char* name, unsigned char slot);
static int HostSendClientHandshake(unsigned char difficulty);

static void SafeCopy(char* dst, int dstLen, const char* src) {
    int i = 0;
    if (!dst || dstLen <= 0) return;
    if (!src) src = "";
    while (src[i] && i < dstLen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void SetError(const char* msg) {
    SafeCopy(s_error, sizeof(s_error), msg);
}

static void SetNonBlocking(SOCKET s) {
    unsigned long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
}

static int NetEnsure(void) {
    if (s_netInited) return 0;

    XNetStartupParams xnsp;
    ZeroMemory(&xnsp, sizeof(xnsp));
    xnsp.cfgSizeOfStruct = sizeof(xnsp);
    xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;

    if (XNetStartup(&xnsp) != 0) {
        SetError("XNetStartup failed");
        return -1;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        SetError("WSAStartup failed");
        return -1;
    }

    s_netInited = 1;
    return 0;
}

static void FillMagic(unsigned char* magic) {
    magic[0] = XBLR_MAGIC0;
    magic[1] = XBLR_MAGIC1;
    magic[2] = XBLR_MAGIC2;
    magic[3] = XBLR_MAGIC3;
}

static int MagicOK(const unsigned char* magic) {
    if (!magic) return 0;
    if (magic[0] != XBLR_MAGIC0) return 0;
    if (magic[1] != XBLR_MAGIC1) return 0;
    if (magic[2] != XBLR_MAGIC2) return 0;
    if (magic[3] != XBLR_MAGIC3) return 0;
    return 1;
}

static int SendAll(SOCKET s, const unsigned char* data, int len) {
    int sent = 0;

    while (sent < len) {
        int r = send(s, (const char*)(data + sent), len - sent, 0);
        if (r == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                Sleep(1);
                continue;
            }
            return -1;
        }
        if (r <= 0)
            return -1;
        sent += r;
    }

    return 0;
}

int XbLocalRelay_Init(void) {
    s_error[0] = '\0';
    return NetEnsure();
}

void XbLocalRelay_Shutdown(void) {
    XbLocalRelay_HostStop();
    XbLocalRelay_ClientStop();
}

/* ── Host side ─────────────────────────────────────────────────────────── */

int XbLocalRelay_HostStart(unsigned short controlPort) {
    struct sockaddr_in sa;
    int yes = 1;

    if (NetEnsure() < 0)
        return -1;

    XbLocalRelay_HostStop();

    if (!controlPort)
        controlPort = XBLOCALRELAY_DEFAULT_PORT;

    s_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_listenSock == INVALID_SOCKET) {
        SetError("host socket failed");
        s_hostState = XBLOCALRELAY_ERROR;
        return -1;
    }

    setsockopt(s_listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    SetNonBlocking(s_listenSock);

    ZeroMemory(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(controlPort);
    sa.sin_addr.s_addr = INADDR_ANY;

    if (bind(s_listenSock, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) {
        SetError("host bind failed");
        closesocket(s_listenSock);
        s_listenSock = INVALID_SOCKET;
        s_hostState = XBLOCALRELAY_ERROR;
        return -1;
    }

    if (listen(s_listenSock, 1) == SOCKET_ERROR) {
        SetError("host listen failed");
        closesocket(s_listenSock);
        s_listenSock = INVALID_SOCKET;
        s_hostState = XBLOCALRELAY_ERROR;
        return -1;
    }

    s_hostClientName[0] = '\0';
    s_hostState = XBLOCALRELAY_LISTENING;
    return 0;
}

void XbLocalRelay_HostStop(void) {
    if (s_gameRole == XBLOCALRELAY_GAME_HOST) XbLocalRelay_GameEnd();
    if (s_hostClientSock != INVALID_SOCKET) {
        closesocket(s_hostClientSock);
        s_hostClientSock = INVALID_SOCKET;
    }

    if (s_listenSock != INVALID_SOCKET) {
        closesocket(s_listenSock);
        s_listenSock = INVALID_SOCKET;
    }

    s_hostState = XBLOCALRELAY_IDLE;
    s_hostClientName[0] = '\0';
}

void XbLocalRelay_HostPoll(void) {
    if (s_hostState == XBLOCALRELAY_LISTENING) {
        struct sockaddr_in from;
        int fromLen = sizeof(from);
        SOCKET s = accept(s_listenSock, (struct sockaddr*)&from, &fromLen);

        if (s != INVALID_SOCKET) {
            SetNonBlocking(s);
            s_hostClientSock = s;
            s_hostState = XBLOCALRELAY_CLIENT_CONNECTED;
        }
        else {
            int e = WSAGetLastError();
            if (e != WSAEWOULDBLOCK) {
                SetError("host accept failed");
                s_hostState = XBLOCALRELAY_ERROR;
            }
        }
    }

    if (s_hostState == XBLOCALRELAY_CLIENT_CONNECTED &&
        s_hostClientSock != INVALID_SOCKET) {
        XbLocalRelayHello h;
        int r = recv(s_hostClientSock, (char*)&h, sizeof(h), 0);

        if (r == sizeof(h) &&
            MagicOK(h.magic) &&
            h.type == XBLR_PKT_HELLO &&
            h.version == XBLOCALRELAY_VERSION) {
            SafeCopy(s_hostClientName, XBLOCALRELAY_NAME_LEN, h.name);
        }
        else if (r == 0) {
            SetError("client disconnected");
            XbLocalRelay_HostStop();
            s_hostState = XBLOCALRELAY_ERROR;
        }
        else if (r == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e != WSAEWOULDBLOCK) {
                SetError("host recv failed");
                XbLocalRelay_HostStop();
                s_hostState = XBLOCALRELAY_ERROR;
            }
        }
    }
}

int XbLocalRelay_HostState(void) {
    return s_hostState;
}

int XbLocalRelay_HostHasClient(void) {
    return (s_hostState == XBLOCALRELAY_CLIENT_CONNECTED &&
        s_hostClientSock != INVALID_SOCKET) ? 1 : 0;
}

void XbLocalRelay_HostGetClientName(char* outName, int outSize) {
    SafeCopy(outName, outSize, s_hostClientName);
}

int XbLocalRelay_HostSendStart(const char* levelFile, unsigned char difficulty) {
    XbLocalRelayStart p;

    if (!XbLocalRelay_HostHasClient()) {
        SetError("no client");
        return -1;
    }

    ZeroMemory(&p, sizeof(p));
    FillMagic(p.magic);
    p.type = XBLR_PKT_START;
    p.version = XBLOCALRELAY_VERSION;
    p.difficulty = difficulty;
    SafeCopy(p.levelFile, XBLOCALRELAY_LEVEL_LEN, levelFile);

    if (SendAll(s_hostClientSock, (const unsigned char*)&p, sizeof(p)) < 0) {
        SetError("send start failed");
        return -1;
    }

    /* Local equivalent of Python relay handshake:
     * START is consumed by setupmenu, then ClientGame reads these buffered
     * OpenJazz game packets.
     */
    if (HostSendClientHandshake(difficulty) < 0) {
        SetError("send handshake failed");
        return -1;
    }

    return 0;
}

/* ── Client side ───────────────────────────────────────────────────────── */

int XbLocalRelay_ClientConnect(const char* hostIp,
    unsigned short controlPort,
    const char* playerName) {
    struct sockaddr_in sa;
    int r;

    if (NetEnsure() < 0)
        return -1;

    XbLocalRelay_ClientStop();

    if (!controlPort)
        controlPort = XBLOCALRELAY_DEFAULT_PORT;

    s_clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_clientSock == INVALID_SOCKET) {
        SetError("client socket failed");
        s_clientState = XBLOCALRELAY_ERROR;
        return -1;
    }

    SetNonBlocking(s_clientSock);

    ZeroMemory(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(controlPort);
    sa.sin_addr.s_addr = inet_addr(hostIp);

    if (sa.sin_addr.s_addr == INADDR_NONE) {
        SetError("bad host ip");
        closesocket(s_clientSock);
        s_clientSock = INVALID_SOCKET;
        s_clientState = XBLOCALRELAY_ERROR;
        return -1;
    }

    r = connect(s_clientSock, (struct sockaddr*)&sa, sizeof(sa));
    if (r != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
        SetError("client connect failed");
        closesocket(s_clientSock);
        s_clientSock = INVALID_SOCKET;
        s_clientState = XBLOCALRELAY_ERROR;
        return -1;
    }

    s_clientLevel[0] = '\0';
    s_clientDifficulty = 0;
    s_clientConnectStart = GetTickCount();
    s_clientState = XBLOCALRELAY_CONNECTING;

    /* Store playerName by sending HELLO once connect completes in ClientPoll. */
    SafeCopy(s_hostClientName, XBLOCALRELAY_NAME_LEN, playerName);

    return 0;
}

void XbLocalRelay_ClientStop(void) {
    if (s_gameRole == XBLOCALRELAY_GAME_CLIENT) XbLocalRelay_GameEnd();
    if (s_clientSock != INVALID_SOCKET) {
        closesocket(s_clientSock);
        s_clientSock = INVALID_SOCKET;
    }

    s_clientState = XBLOCALRELAY_IDLE;
    s_clientLevel[0] = '\0';
    s_clientDifficulty = 0;
}

void XbLocalRelay_ClientPoll(void) {
    if (s_clientState == XBLOCALRELAY_CONNECTING) {
        fd_set wfds, efds;
        struct timeval tv;
        int r;

        if (GetTickCount() - s_clientConnectStart > XBLR_CONNECT_TIMEOUT) {
            SetError("local connect timeout");
            XbLocalRelay_ClientStop();
            s_clientState = XBLOCALRELAY_ERROR;
            return;
        }

        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        FD_SET(s_clientSock, &wfds);
        FD_SET(s_clientSock, &efds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        r = select(0, 0, &wfds, &efds, &tv);
        if (r > 0 && FD_ISSET(s_clientSock, &wfds)) {
            XbLocalRelayHello h;
            ZeroMemory(&h, sizeof(h));
            FillMagic(h.magic);
            h.type = XBLR_PKT_HELLO;
            h.version = XBLOCALRELAY_VERSION;
            SafeCopy(h.name, XBLOCALRELAY_NAME_LEN, s_hostClientName);

            if (SendAll(s_clientSock, (const unsigned char*)&h, sizeof(h)) < 0) {
                SetError("local hello failed");
                XbLocalRelay_ClientStop();
                s_clientState = XBLOCALRELAY_ERROR;
                return;
            }

            s_clientState = XBLOCALRELAY_WAITING_START;
        }
        else if (r > 0 && FD_ISSET(s_clientSock, &efds)) {
            SetError("local connect failed");
            XbLocalRelay_ClientStop();
            s_clientState = XBLOCALRELAY_ERROR;
        }

        return;
    }

    if (s_clientState == XBLOCALRELAY_WAITING_START) {
        XbLocalRelayStart p;
        int r = recv(s_clientSock, (char*)&p, sizeof(p), 0);

        if (r == sizeof(p) &&
            MagicOK(p.magic) &&
            p.type == XBLR_PKT_START &&
            p.version == XBLOCALRELAY_VERSION) {
            SafeCopy(s_clientLevel, XBLOCALRELAY_LEVEL_LEN, p.levelFile);
            s_clientDifficulty = p.difficulty;
            s_clientState = XBLOCALRELAY_START_RECEIVED;
        }
        else if (r == 0) {
            SetError("host closed local control");
            XbLocalRelay_ClientStop();
            s_clientState = XBLOCALRELAY_ERROR;
        }
        else if (r == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e != WSAEWOULDBLOCK) {
                SetError("local start recv failed");
                XbLocalRelay_ClientStop();
                s_clientState = XBLOCALRELAY_ERROR;
            }
        }
    }
}

int XbLocalRelay_ClientState(void) {
    return s_clientState;
}

int XbLocalRelay_ClientHasStart(void) {
    return (s_clientState == XBLOCALRELAY_START_RECEIVED) ? 1 : 0;
}

void XbLocalRelay_ClientGetLevel(char* outLevel, int outSize) {
    SafeCopy(outLevel, outSize, s_clientLevel);
}

unsigned char XbLocalRelay_ClientGetDifficulty(void) {
    return s_clientDifficulty;
}


static int IsHostSuppressedPacket(const unsigned char* buffer, int len) {
    if (!buffer || len < 2)
        return 0;

    if (buffer[1] == MT_G_PROPS) return 1;
    if (buffer[1] == MT_G_PJOIN) return 1;
    if (buffer[1] == MT_G_LEVEL) return 1;
    if (buffer[1] == MT_G_LTYPE) return 1;

    return 0;
}

static int SendPacketToClientSocket(const unsigned char* data, int len) {
    if (s_hostClientSock == INVALID_SOCKET)
        return -1;

    return SendAll(s_hostClientSock, data, len);
}

static int BuildPjoin(unsigned char* out, int outMax,
    const char* name, unsigned char slot) {
    int ni = 0;
    int pos;
    int total;

    if (!out || outMax < MTL_G_PJOIN + 1)
        return 0;

    if (!name)
        name = "PLAYER";

    total = MTL_G_PJOIN;
    while (name[ni] && ni < 15 && total < outMax - 1) {
        total++;
        ni++;
    }
    total++; /* NUL */

    if (total > outMax)
        return 0;

    out[0] = (unsigned char)total;
    out[1] = MT_G_PJOIN;
    out[2] = slot;       /* clientID */
    out[3] = slot;       /* playerSlot */
    out[4] = slot % 2;   /* team */
    out[5] = 0;
    out[6] = 0;
    out[7] = 0;
    out[8] = 0;

    pos = 9;
    ni = 0;
    while (name[ni] && ni < 15 && pos < total - 1)
        out[pos++] = (unsigned char)name[ni++];

    out[pos++] = 0;

    return total;
}

static int HostSendClientHandshake(unsigned char difficulty) {
    unsigned char pkt[64];
    int len;

    if (s_hostClientSock == INVALID_SOCKET)
        return -1;

    pkt[0] = MTL_G_PROPS;
    pkt[1] = MT_G_PROPS;
    pkt[2] = 1;              /* OJ version */
    pkt[3] = M_COOP;
    pkt[4] = difficulty;
    pkt[5] = MAX_PLAYERS_LAN;
    pkt[6] = 2;              /* host + client */
    pkt[7] = 1;              /* this client is slot/clientID 1 */

    if (SendPacketToClientSocket(pkt, MTL_G_PROPS) < 0)
        return -1;

    len = BuildPjoin(pkt, sizeof(pkt), s_hostName[0] ? s_hostName : "HOST", 0);
    if (len <= 0 || SendPacketToClientSocket(pkt, len) < 0)
        return -1;

    len = BuildPjoin(pkt, sizeof(pkt),
        s_hostClientName[0] ? s_hostClientName : "CLIENT", 1);
    if (len <= 0 || SendPacketToClientSocket(pkt, len) < 0)
        return -1;

    /* ClientGame::setLevel() waits for MT_G_LEVEL to begin and then waits
     * for a final empty MT_G_LEVEL packet to finish.
     *
     * In LAN bridge mode, the Xbox already has the local level files and
     * ClientGame drains level packets without writing them. The host's real
     * MT_G_LEVEL packets are suppressed below, so provide a tiny synthetic
     * level-transfer pair:
     *
     *   1) non-empty MT_G_LEVEL: makes client leave "WAITING FOR SERVER"
     *   2) empty MT_G_LEVEL:     makes client leave "RECEIVING LEVEL"
     */
    pkt[0] = MTL_G_LEVEL + 1;
    pkt[1] = MT_G_LEVEL;
    pkt[2] = 0;
    pkt[3] = 0;
    pkt[4] = 0;
    if (SendPacketToClientSocket(pkt, MTL_G_LEVEL + 1) < 0)
        return -1;

    pkt[0] = MTL_G_LEVEL;
    pkt[1] = MT_G_LEVEL;
    pkt[2] = 0;
    pkt[3] = 0;
    if (SendPacketToClientSocket(pkt, MTL_G_LEVEL) < 0)
        return -1;

    return 0;
}

void XbLocalRelay_SetLaunchLevel(const char* levelFile, unsigned char difficulty) {
    SafeCopy(s_launchLevel, XBLOCALRELAY_LEVEL_LEN, levelFile);
    s_launchDifficulty = difficulty;
    s_launchLevelValid = s_launchLevel[0] ? 1 : 0;
}

int XbLocalRelay_HasLaunchLevel(void) {
    return s_launchLevelValid;
}

void XbLocalRelay_GetLaunchLevel(char* outLevel, int outSize) {
    SafeCopy(outLevel, outSize, s_launchLevel);
}

unsigned char XbLocalRelay_GetLaunchDifficulty(void) {
    return s_launchDifficulty;
}

void XbLocalRelay_ClearLaunchLevel(void) {
    s_launchLevel[0] = '\0';
    s_launchDifficulty = 0;
    s_launchLevelValid = 0;
}



static void CloseLanHostGameSocket(void) {
    if (s_hostClientSock != INVALID_SOCKET) {
        closesocket(s_hostClientSock);
        s_hostClientSock = INVALID_SOCKET;
    }

    if (s_gameRole == XBLOCALRELAY_GAME_HOST)
        s_gameRole = XBLOCALRELAY_GAME_NONE;
}

static void CloseLanClientGameSocket(void) {
    if (s_clientSock != INVALID_SOCKET) {
        closesocket(s_clientSock);
        s_clientSock = INVALID_SOCKET;
    }

    if (s_gameRole == XBLOCALRELAY_GAME_CLIENT)
        s_gameRole = XBLOCALRELAY_GAME_NONE;
}

void XbLocalRelay_HostSetName(const char* hostName) {
    SafeCopy(s_hostName, XBLOCALRELAY_NAME_LEN, hostName);
}

int XbLocalRelay_HostBeginGame(void) {
    if (s_hostClientSock == INVALID_SOCKET) {
        SetError("no lan client socket");
        return -1;
    }

    if (s_listenSock != INVALID_SOCKET) {
        closesocket(s_listenSock);
        s_listenSock = INVALID_SOCKET;
    }

    s_gameRole = XBLOCALRELAY_GAME_HOST;
    s_gameAcceptPending = 1;
    return 0;
}

int XbLocalRelay_ClientBeginGame(void) {
    if (s_clientSock == INVALID_SOCKET) {
        SetError("no lan host socket");
        return -1;
    }

    s_gameRole = XBLOCALRELAY_GAME_CLIENT;
    return 0;
}

void XbLocalRelay_GameEnd(void) {
    s_gameRole = XBLOCALRELAY_GAME_NONE;
    s_gameAcceptPending = 0;
}

int XbLocalRelay_GameRole(void) {
    return s_gameRole;
}

int XbLocalRelay_GameAcceptPending(void) {
    if (s_gameRole == XBLOCALRELAY_GAME_HOST && s_gameAcceptPending > 0) {
        s_gameAcceptPending = 0;
        return 1;
    }

    return 0;
}

int XbLocalRelay_GameHostSend(const unsigned char* buffer, int len) {
    if (!buffer || len <= 0)
        return -1;

    if (s_hostClientSock == INVALID_SOCKET)
        return -1;

    if (IsHostSuppressedPacket(buffer, len))
        return len;

    if (SendAll(s_hostClientSock, buffer, len) < 0) {
        SetError("lan host send failed");
        CloseLanHostGameSocket();
        return -1;
    }

    return len;
}

int XbLocalRelay_GameHostRecv(unsigned char* buffer, int len) {
    int r;

    if (!buffer || len <= 0)
        return -1;

    if (s_hostClientSock == INVALID_SOCKET)
        return -1;

    r = recv(s_hostClientSock, (char*)buffer, len, 0);
    if (r == SOCKET_ERROR) {
        int e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK)
            return 0;
        SetError("lan host recv failed");
        CloseLanHostGameSocket();
        return -1;
    }

    if (r == 0) {
        SetError("lan client disconnected");
        CloseLanHostGameSocket();
        return -1;
    }

    return r;
}

int XbLocalRelay_GameClientSend(const unsigned char* buffer, int len) {
    if (!buffer || len <= 0)
        return -1;

    if (s_clientSock == INVALID_SOCKET)
        return -1;

    if (SendAll(s_clientSock, buffer, len) < 0) {
        SetError("lan client send failed");
        CloseLanClientGameSocket();
        return -1;
    }

    return len;
}

int XbLocalRelay_GameClientRecv(unsigned char* buffer, int len) {
    int r;

    if (!buffer || len <= 0)
        return -1;

    if (s_clientSock == INVALID_SOCKET)
        return -1;

    r = recv(s_clientSock, (char*)buffer, len, 0);
    if (r == SOCKET_ERROR) {
        int e = WSAGetLastError();
        if (e == WSAEWOULDBLOCK)
            return 0;
        SetError("lan client recv failed");
        CloseLanClientGameSocket();
        return -1;
    }

    if (r == 0) {
        SetError("lan host disconnected");
        CloseLanClientGameSocket();
        return -1;
    }

    return r;
}

int XbLocalRelay_GameIsConnected(void) {
    if (s_gameRole == XBLOCALRELAY_GAME_HOST)
        return s_hostClientSock != INVALID_SOCKET;

    if (s_gameRole == XBLOCALRELAY_GAME_CLIENT)
        return s_clientSock != INVALID_SOCKET;

    return 0;
}

void XbLocalRelay_GetLastError(char* outError, int outSize) {
    SafeCopy(outError, outSize, s_error);
}
