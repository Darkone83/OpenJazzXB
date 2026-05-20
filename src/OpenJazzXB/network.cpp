/**
 * network.cpp
 * XbJazz -- Network class shim implementation.
 *
 * RXDK TU: <xtl.h> must be first. Wraps xb_net.cpp via its plain-C interface.
 * The OJ engine calls net->host/join/accept/send/recv/close/isConnected
 * and this shim routes everything through the single relay TCP socket.
 */

#include <xtl.h>
#include <winsockx.h>
#include "network.h"
#include "xb_net.h"
#include "xb_localrelay.h"

 /* ── accept() pre-seed counter ─────────────────────────────────────────── */

static int s_acceptsRemaining = 0;

void XbNet_SetExpectedClients(int n) {
    s_acceptsRemaining = n;
}

/* ── Network::host ──────────────────────────────────────────────────────── */

int Network::host() {
    return XBNET_SOCK_SERVER;
}

/* ── Network::join ──────────────────────────────────────────────────────── */

int Network::join(char* /*address*/) {
    if (XbLocalRelay_GameRole() == XBLOCALRELAY_GAME_CLIENT &&
        XbLocalRelay_GameIsConnected())
        return XBNET_SOCK_CLIENT;

    if (!XbNet_IsConnected()) return -1;
    return XBNET_SOCK_CLIENT;
}

/* ── Network::accept ────────────────────────────────────────────────────── */

int Network::accept(int /*sock*/) {
    if (XbLocalRelay_GameRole() == XBLOCALRELAY_GAME_HOST) {
        if (XbLocalRelay_GameAcceptPending())
            return XBNET_SOCK_ACCEPT;
        return -1;
    }

    if (s_acceptsRemaining > 0) {
        s_acceptsRemaining--;
        return XBNET_SOCK_ACCEPT;
    }
    return -1;
}

/* ── Network::close ─────────────────────────────────────────────────────── */

void Network::close(int sock) {
    if (XbLocalRelay_GameRole() != XBLOCALRELAY_GAME_NONE) {
        (void)sock;
        return;
    }

    if (sock == XBNET_SOCK_SERVER) {
        XbNet_Disconnect();
    }
}

/* ── Network::send ──────────────────────────────────────────────────────── */

int Network::send(int sock, unsigned char* buffer) {
    int len = (int)(unsigned char)buffer[0];

    if (XbLocalRelay_GameRole() == XBLOCALRELAY_GAME_HOST)
        return XbLocalRelay_GameHostSend(buffer, len);

    if (XbLocalRelay_GameRole() == XBLOCALRELAY_GAME_CLIENT)
        return XbLocalRelay_GameClientSend(buffer, len);

    (void)sock;
    if (!XbNet_IsConnected()) return -1;
    return XbNet_SendData(buffer, len);
}

/* ── Network::recv ──────────────────────────────────────────────────────── */

int Network::recv(int sock, unsigned char* buffer, int length) {
    if (XbLocalRelay_GameRole() == XBLOCALRELAY_GAME_HOST)
        return XbLocalRelay_GameHostRecv(buffer, length);

    if (XbLocalRelay_GameRole() == XBLOCALRELAY_GAME_CLIENT)
        return XbLocalRelay_GameClientRecv(buffer, length);

    (void)sock;
    if (!XbNet_IsConnected()) return -1;
    return XbNet_RecvData(buffer, length);
}

/* ── Network::isConnected ───────────────────────────────────────────────── */

bool Network::isConnected(int /*sock*/) {
    if (XbLocalRelay_GameRole() != XBLOCALRELAY_GAME_NONE)
        return XbLocalRelay_GameIsConnected() != 0;

    return XbNet_IsConnected() != 0;
}

/* ── Network::getError ──────────────────────────────────────────────────── */

int Network::getError() {
    return WSAGetLastError();
}