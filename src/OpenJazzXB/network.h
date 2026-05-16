/**
 * network.h
 * XbJazz port -- full stub. JJ1 singleplayer never enters network code.
 */

#ifndef OJ_NETWORK_H
#define OJ_NETWORK_H

#include "OpenJazz.h"

 /* MAX_CLIENTS referenced by game.h */
#define MAX_CLIENTS 32

/* NET_ADDRESS default */
#define NET_ADDRESS "127.0.0.1"

/* LEVEL_FILE -- used by jj1levelload.cpp as a strcmp target to identify
   network-provided level data.  Defined as an empty string so that any
   strcmp against a real filename returns non-zero, safely skipping the
   network code path in singleplayer. */
#define LEVEL_FILE ""

class Network {
public:
    Network() {}
    ~Network() {}
    int  host(int) { return E_N_OTHER; }
    int  join(const char*) { return E_N_OTHER; }
    int  send(unsigned char*, int) { return E_N_OTHER; }
    int  recv(unsigned char*, int) { return E_N_OTHER; }
    void disconnect() {}
};

EXTERN Network* net;
EXTERN char* netAddress;

#endif /* OJ_NETWORK_H */