/**
 * network.h
 * XbJazz -- Network class shim wrapping xb_net.cpp.
 *
 * Interface matches the original OJ Network class exactly so ServerGame
 * and ClientGame compile and run unchanged.
 *
 * Design:
 *   - One TCP socket to the relay throughout (managed by xb_net.cpp).
 *   - host()   : no-op, relay handles connections. Returns fake server sock 1.
 *   - join()   : no-op, already connected via XbNet_Connect(). Returns fake sock 2.
 *   - accept() : returns fake client sock 3 once (pre-seeded), then -1.
 *   - send()   : routes to XbNet_SendData() -- sock param ignored.
 *   - recv()   : routes to XbNet_RecvData() -- sock param ignored.
 *   - close()  : XbNet_Disconnect() only on the main server sock.
 *   - isConnected() : routes to XbNet_IsConnected().
 *   - getError()    : returns last WSA error.
 */

#ifndef OJ_NETWORK_H
#define OJ_NETWORK_H

#include "OpenJazz.h"

 /* MAX_CLIENTS referenced by game.h */
#define MAX_CLIENTS 32

/* NET_ADDRESS default -- overridden at runtime from config */
#define NET_ADDRESS "127.0.0.1"

/* Connection timeout in ms -- matches OJ network.h */
#define T_TIMEOUT 30000

/* LEVEL_FILE -- client loads from its own D:\ copy, no transfer needed.
 * Must not match any real level filename so OJ's strcmp skips the net path. */
#define LEVEL_FILE ""

 /* Fake socket handles -- the relay uses one real TCP socket.
  * These values are returned by host()/join()/accept() so OJ's game code
  * has valid-looking handles to pass back to send()/recv()/close(). */
#define XBNET_SOCK_SERVER  1   /* returned by host()   */
#define XBNET_SOCK_CLIENT  2   /* returned by join()   */
#define XBNET_SOCK_ACCEPT  3   /* returned by accept() */

  /* Call before creating ServerGame to prime accept() for n clients. */
void XbNet_SetExpectedClients(int n);

class Network {
public:
    Network() {}
    ~Network() {}

    /* ServerGame: start listening. No-op -- relay handles it. */
    int  host();

    /* ClientGame: connect to server. No-op -- XbNet_Connect() already done. */
    int  join(char* address);

    /* ServerGame: accept a client. Returns XBNET_SOCK_ACCEPT once, then -1. */
    int  accept(int sock);

    /* Close a socket. Disconnects relay only for XBNET_SOCK_SERVER. */
    void close(int sock);

    /* Send buffer[0] bytes to relay (sock ignored). */
    int  send(int sock, unsigned char* buffer);

    /* Receive up to length bytes from relay (sock ignored). */
    int  recv(int sock, unsigned char* buffer, int length);

    /* Returns true if relay connection is still alive. */
    bool isConnected(int sock);

    /* Returns last Winsock error code. */
    int  getError();
};

EXTERN Network* net;
EXTERN char* netAddress;

#endif /* OJ_NETWORK_H */