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

 /* ── accept() pre-seed counter ─────────────────────────────────────────── */

static int s_acceptsRemaining = 0;

void XbNet_SetExpectedClients(int n) {
    s_acceptsRemaining = n;
}

/* ── Network::host ──────────────────────────────────────────────────────── */

int Network::host() {
    /* Relay already has both players -- no real listen needed.
     * Return a fake server socket handle so ServerGame stores something. */
    return XBNET_SOCK_SERVER;
}

/* ── Network::join ──────────────────────────────────────────────────────── */

int Network::join(char* /*address*/) {
    /* XbNet_Connect() was called from the setup menu before ClientGame
     * was constructed. The TCP socket to the relay is already open.
     * Return a fake client socket handle. */
    if (!XbNet_IsConnected()) return -1;
    return XBNET_SOCK_CLIENT;
}

/* ── Network::accept ────────────────────────────────────────────────────── */

int Network::accept(int /*sock*/) {
    /* ServerGame calls this each step until it returns -1.
     * We pre-seed s_acceptsRemaining via XbNet_SetExpectedClients(1)
     * before ServerGame is created. Returns XBNET_SOCK_ACCEPT for the
     * one relay-connected client, then -1 forever. */
    if (s_acceptsRemaining > 0) {
        s_acceptsRemaining--;
        return XBNET_SOCK_ACCEPT;
    }
    return -1;
}

/* ── Network::close ─────────────────────────────────────────────────────── */

void Network::close(int sock) {
    /* Only disconnect on the server socket -- fake client/accept handles
     * are not real sockets and should not trigger a disconnect. */
    if (sock == XBNET_SOCK_SERVER) {
        XbNet_Disconnect();
    }
    /* XBNET_SOCK_CLIENT and XBNET_SOCK_ACCEPT: no-op.
     * ClientGame calls close(sock) on error/quit -- let the destructor
     * or explicit XbNet_Disconnect() call handle teardown. */
}

/* ── Network::send ──────────────────────────────────────────────────────── */

int Network::send(int /*sock*/, unsigned char* buffer) {
    /* buffer[0] = total packet length (OJ framing convention).
     * sock is ignored -- all traffic routes through the single relay socket. */
    if (!XbNet_IsConnected()) return -1;
    int len = (int)(unsigned char)buffer[0];
    return XbNet_SendData(buffer, len);
}

/* ── Network::recv ──────────────────────────────────────────────────────── */

int Network::recv(int /*sock*/, unsigned char* buffer, int length) {
    /* Non-blocking. Returns bytes read, 0 if nothing available, -1 on error.
     * sock is ignored -- single relay socket. */
    if (!XbNet_IsConnected()) return -1;
    return XbNet_RecvData(buffer, length);
}

/* ── Network::isConnected ───────────────────────────────────────────────── */

bool Network::isConnected(int /*sock*/) {
    return XbNet_IsConnected() != 0;
}

/* ── Network::getError ──────────────────────────────────────────────────── */

int Network::getError() {
    return WSAGetLastError();
}