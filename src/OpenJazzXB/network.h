/**
 * network.h
 * XbJazz -- Network interface matching OJ's ClientGame/ServerGame expectations.
 * Implemented in network.cpp using RXDK sockets via xb_net.cpp.
 */

#ifndef OJ_NETWORK_H
#define OJ_NETWORK_H

#include "OpenJazz.h"

#define MAX_CLIENTS  32
#define NET_ADDRESS  "127.0.0.1"
#define NET_PORT     10052
#define LEVEL_FILE   ""
#define T_TIMEOUT    30000

class Network {
public:
    Network();
    ~Network();

    int  host();
    int  join(const char* address);
    int  accept(int sock);
    void close(int sock);
    int  send(int sock, unsigned char* buffer);
    int  recv(int sock, unsigned char* buffer, int length);
    bool isConnected(int sock);
    int  getError();
};

void XbNet_SetExpectedClients(int n);
EXTERN Network* net;
EXTERN char* netAddress;

#endif