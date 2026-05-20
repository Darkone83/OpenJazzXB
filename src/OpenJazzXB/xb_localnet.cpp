/**
 * xb_localnet.cpp -- OpenJazzXB LAN discovery layer.
 *
 * RXDK TU: <xtl.h> first. No OJ headers, no STL.
 *
 * This module is intentionally separate from the existing relay netplay path.
 * It only handles LAN advertisement and discovery. Actual gameplay handoff
 * should remain in setupmenu.cpp / xb_localrelay.cpp / ClientGame / ServerGame.
 */

#include <xtl.h>
#include <winsockx.h>
#include "xb_localnet.h"

#include <string.h>

#define XBLOCALNET_MAGIC0 'O'
#define XBLOCALNET_MAGIC1 'J'
#define XBLOCALNET_MAGIC2 'X'
#define XBLOCALNET_MAGIC3 'B'

#define XBLOCALNET_PKT_ADVERTISE 0x10
#define XBLOCALNET_AD_INTERVAL   1000u
#define XBLOCALNET_HOST_EXPIRE   5000u

#pragma pack(push, 1)
typedef struct XbLocalNetAdvertPacket {
    unsigned char  magic[4];
    unsigned char  type;
    unsigned char  version;
    unsigned char  status;
    unsigned char  reserved;
    unsigned short controlPort;
    unsigned short gamePort;
    char           name[XBLOCALNET_NAME_LEN];
} XbLocalNetAdvertPacket;
#pragma pack(pop)

static int      s_netInited = 0;

static SOCKET   s_adSock = INVALID_SOCKET;
static DWORD    s_lastAdvert = 0;
static char     s_adName[XBLOCALNET_NAME_LEN];
static unsigned short s_adControlPort = XBLOCALNET_DEFAULT_CTRL;
static unsigned short s_adGamePort = XBLOCALNET_DEFAULT_GAME;
static unsigned char  s_adStatus = XBLOCALNET_STATUS_WAITING;

static SOCKET   s_scanSock = INVALID_SOCKET;
static XbLocalNetHost s_hosts[XBLOCALNET_MAX_HOSTS];
static int      s_hostCount = 0;

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

static void AppendDecByte(char* out, int outLen, int* pos, unsigned int v) {
    char tmp[4];
    int n = 0;
    int i;

    if (!out || !pos || outLen <= 0)
        return;

    if (v > 255)
        v = 255;

    if (v >= 100) {
        tmp[n++] = (char)('0' + (v / 100));
        v %= 100;
        tmp[n++] = (char)('0' + (v / 10));
        tmp[n++] = (char)('0' + (v % 10));
    }
    else if (v >= 10) {
        tmp[n++] = (char)('0' + (v / 10));
        tmp[n++] = (char)('0' + (v % 10));
    }
    else {
        tmp[n++] = (char)('0' + v);
    }

    for (i = 0; i < n && *pos < outLen - 1; i++)
        out[(*pos)++] = tmp[i];

    out[*pos] = '\0';
}

static void FormatIp(char* out, int outLen, unsigned long addrNetOrder) {
    int pos = 0;
    unsigned long addr = ntohl(addrNetOrder);

    if (!out || outLen <= 0)
        return;

    out[0] = '\0';

    AppendDecByte(out, outLen, &pos, (unsigned int)((addr >> 24) & 0xFF));
    if (pos < outLen - 1) out[pos++] = '.';
    out[pos] = '\0';

    AppendDecByte(out, outLen, &pos, (unsigned int)((addr >> 16) & 0xFF));
    if (pos < outLen - 1) out[pos++] = '.';
    out[pos] = '\0';

    AppendDecByte(out, outLen, &pos, (unsigned int)((addr >> 8) & 0xFF));
    if (pos < outLen - 1) out[pos++] = '.';
    out[pos] = '\0';

    AppendDecByte(out, outLen, &pos, (unsigned int)(addr & 0xFF));
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

    if (XNetStartup(&xnsp) != 0)
        return -1;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return -1;

    s_netInited = 1;
    return 0;
}

static int IsAdvertPacketValid(const XbLocalNetAdvertPacket* p) {
    if (!p) return 0;
    if (p->magic[0] != XBLOCALNET_MAGIC0) return 0;
    if (p->magic[1] != XBLOCALNET_MAGIC1) return 0;
    if (p->magic[2] != XBLOCALNET_MAGIC2) return 0;
    if (p->magic[3] != XBLOCALNET_MAGIC3) return 0;
    if (p->type != XBLOCALNET_PKT_ADVERTISE) return 0;
    return 1;
}

static void UpsertHost(const char* ip, const XbLocalNetAdvertPacket* p) {
    int i;
    DWORD now = GetTickCount();

    if (!ip || !p) return;

    for (i = 0; i < s_hostCount; i++) {
        if (strcmp(s_hosts[i].ip, ip) == 0 &&
            s_hosts[i].controlPort == ntohs(p->controlPort)) {
            SafeCopy(s_hosts[i].name, XBLOCALNET_NAME_LEN, p->name);
            s_hosts[i].gamePort = ntohs(p->gamePort);
            s_hosts[i].version = p->version;
            s_hosts[i].status = p->status;
            s_hosts[i].compatible = (p->version == XBLOCALNET_VERSION) ? 1 : 0;
            s_hosts[i].lastSeenTicks = now;
            return;
        }
    }

    if (s_hostCount >= XBLOCALNET_MAX_HOSTS)
        return;

    SafeCopy(s_hosts[s_hostCount].ip, XBLOCALNET_IP_LEN, ip);
    SafeCopy(s_hosts[s_hostCount].name, XBLOCALNET_NAME_LEN, p->name);
    s_hosts[s_hostCount].controlPort = ntohs(p->controlPort);
    s_hosts[s_hostCount].gamePort = ntohs(p->gamePort);
    s_hosts[s_hostCount].version = p->version;
    s_hosts[s_hostCount].status = p->status;
    s_hosts[s_hostCount].compatible = (p->version == XBLOCALNET_VERSION) ? 1 : 0;
    s_hosts[s_hostCount].lastSeenTicks = now;
    s_hostCount++;
}

static void ExpireHosts(void) {
    DWORD now = GetTickCount();
    int i = 0;

    while (i < s_hostCount) {
        if (now - s_hosts[i].lastSeenTicks > XBLOCALNET_HOST_EXPIRE) {
            int j;
            for (j = i; j < s_hostCount - 1; j++)
                s_hosts[j] = s_hosts[j + 1];
            s_hostCount--;
            continue;
        }
        i++;
    }
}

int XbLocalNet_Init(void) {
    return NetEnsure();
}

void XbLocalNet_Shutdown(void) {
    XbLocalNet_StopAdvertise();
    XbLocalNet_StopScan();
    XbLocalNet_ClearHosts();
}

int XbLocalNet_StartAdvertise(const char* playerName,
                              unsigned short controlPort,
                              unsigned short gamePort,
                              unsigned char status) {
    int yes = 1;

    if (NetEnsure() < 0)
        return -1;

    XbLocalNet_StopAdvertise();

    s_adSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_adSock == INVALID_SOCKET)
        return -1;

    setsockopt(s_adSock, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof(yes));
    SetNonBlocking(s_adSock);

    SafeCopy(s_adName, XBLOCALNET_NAME_LEN, playerName);
    s_adControlPort = controlPort ? controlPort : XBLOCALNET_DEFAULT_CTRL;
    s_adGamePort = gamePort ? gamePort : XBLOCALNET_DEFAULT_GAME;
    s_adStatus = status;
    s_lastAdvert = 0;

    return 0;
}

void XbLocalNet_StopAdvertise(void) {
    if (s_adSock != INVALID_SOCKET) {
        closesocket(s_adSock);
        s_adSock = INVALID_SOCKET;
    }
}

void XbLocalNet_SetAdvertiseStatus(unsigned char status) {
    s_adStatus = status;
}

void XbLocalNet_PollAdvertise(void) {
    DWORD now;
    XbLocalNetAdvertPacket p;
    struct sockaddr_in to;

    if (s_adSock == INVALID_SOCKET)
        return;

    now = GetTickCount();
    if (s_lastAdvert && (now - s_lastAdvert < XBLOCALNET_AD_INTERVAL))
        return;

    ZeroMemory(&p, sizeof(p));
    p.magic[0] = XBLOCALNET_MAGIC0;
    p.magic[1] = XBLOCALNET_MAGIC1;
    p.magic[2] = XBLOCALNET_MAGIC2;
    p.magic[3] = XBLOCALNET_MAGIC3;
    p.type = XBLOCALNET_PKT_ADVERTISE;
    p.version = XBLOCALNET_VERSION;
    p.status = s_adStatus;
    p.controlPort = htons(s_adControlPort);
    p.gamePort = htons(s_adGamePort);
    SafeCopy(p.name, XBLOCALNET_NAME_LEN, s_adName);

    ZeroMemory(&to, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(XBLOCALNET_BCAST_PORT);
    to.sin_addr.s_addr = INADDR_BROADCAST;

    sendto(s_adSock, (const char*)&p, sizeof(p), 0,
           (struct sockaddr*)&to, sizeof(to));

    s_lastAdvert = now;
}

int XbLocalNet_StartScan(void) {
    struct sockaddr_in sa;

    if (NetEnsure() < 0)
        return -1;

    XbLocalNet_StopScan();
    XbLocalNet_ClearHosts();

    s_scanSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_scanSock == INVALID_SOCKET)
        return -1;

    SetNonBlocking(s_scanSock);

    ZeroMemory(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(XBLOCALNET_BCAST_PORT);
    sa.sin_addr.s_addr = INADDR_ANY;

    if (bind(s_scanSock, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) {
        closesocket(s_scanSock);
        s_scanSock = INVALID_SOCKET;
        return -1;
    }

    return 0;
}

void XbLocalNet_StopScan(void) {
    if (s_scanSock != INVALID_SOCKET) {
        closesocket(s_scanSock);
        s_scanSock = INVALID_SOCKET;
    }
}

void XbLocalNet_PollScan(void) {
    for (;;) {
        XbLocalNetAdvertPacket p;
        struct sockaddr_in from;
        int fromLen = sizeof(from);
        int r;

        if (s_scanSock == INVALID_SOCKET)
            break;

        r = recvfrom(s_scanSock, (char*)&p, sizeof(p), 0,
                     (struct sockaddr*)&from, &fromLen);

        if (r == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK)
                break;
            break;
        }

        if (r == sizeof(p) && IsAdvertPacketValid(&p)) {
            char ip[XBLOCALNET_IP_LEN];

            FormatIp(ip, sizeof(ip), from.sin_addr.s_addr);

            UpsertHost(ip, &p);
        }
    }

    ExpireHosts();
}

void XbLocalNet_ClearHosts(void) {
    ZeroMemory(s_hosts, sizeof(s_hosts));
    s_hostCount = 0;
}

int XbLocalNet_GetHostCount(void) {
    ExpireHosts();
    return s_hostCount;
}

const XbLocalNetHost* XbLocalNet_GetHost(int index) {
    ExpireHosts();
    if (index < 0 || index >= s_hostCount)
        return 0;
    return &s_hosts[index];
}
