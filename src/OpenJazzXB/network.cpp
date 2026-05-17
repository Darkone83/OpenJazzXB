/**
 * network.cpp
 * XbJazz -- Network implementation for relay-based multiplayer.
 * All traffic routes through s_sock (our one relay TCP connection).
 * sock parameters are ignored -- they're fake handles for OJ's benefit.
 *
 * RXDK TU: <xtl.h> first.
 */

#include <xtl.h>
#include <winsockx.h>
#include "xb_net.h"
#include "network.h"

 /* How many fake client accepts to issue (set before ServerGame launches) */
static int s_acceptsRemaining = 0;
static int s_nextHandle = 2;   /* 1 = our main sock, 2+ = fake clients */

void XbNet_SetExpectedClients(int n) {
    s_acceptsRemaining = n;
    s_nextHandle = 2;
}

Network::Network() {}
Network::~Network() {}

int Network::host() {
    return XbNet_IsConnected() ? 1 : -1;
}

int Network::join(const char* /*address*/) {
    return XbNet_IsConnected() ? 1 : -1;
}

int Network::accept(int /*sock*/) {
    /* Return a fake handle for each expected client, then -1 */
    if (s_acceptsRemaining > 0) {
        s_acceptsRemaining--;
        return s_nextHandle++;
    }
    return -1;
}

void Network::close(int /*sock*/) {
    /* Only disconnect relay on main socket close */
    XbNet_Disconnect();
}

int Network::send(int /*sock*/, unsigned char* buffer) {
    return XbNet_SendData(buffer, buffer[0]);
}

int Network::recv(int /*sock*/, unsigned char* buffer, int length) {
    return XbNet_RecvData(buffer, length);
}

bool Network::isConnected(int /*sock*/) {
    return XbNet_IsConnected() != 0;
}

int Network::getError() {
    return WSAGetLastError();
}