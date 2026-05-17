/**
 * xb_net_glue.cpp
 * Plain C wrappers around xb_net.cpp so OJ TUs can call network
 * functions without including xb_net.h (which requires <xtl.h> first).
 *
 * RXDK TU: <xtl.h> must be first.
 */
#include <xtl.h>
#include <winsockx.h>
#include "xb_net.h"

extern "C" {

    int XbNet_GetStateC(void) {
        return (int)XbNet_GetState();
    }

    void XbNet_BeginResolveC(const char* host) {
        XbNet_BeginResolve(host);
    }

    void XbNet_PollC(void) {
        XbNet_Poll();
    }

    void XbNet_ResetC(void) {
        XbNet_Reset();
    }

    void XbNet_GetLocalIPC(char* buf, int len) {
        XbNet_GetLocalIP(buf, len);
    }

} /* extern "C" */

extern "C" {

    int XbNet_ConnectC(const char* name) {
        return XbNet_Connect(name);
    }

    int XbNet_LobbyPollC(void* state) {
        return XbNet_LobbyPoll((XbNetLobbyState*)state);
    }

    void XbNet_JoinRoomC(unsigned char room_id) {
        XbNet_JoinRoom(room_id);
    }

    void XbNet_DisconnectC(void) {
        XbNet_Disconnect();
    }

    int XbNet_IsConnectedC(void) {
        return XbNet_IsConnected();
    }

} /* extern "C" */

extern "C" {

    void XbNet_SendMapselectC(const char* levelFile, unsigned char difficulty) {
        if (!XbNet_IsConnected()) return;
        /* OJXB_MAPSEL: [len][0xF5][difficulty][levelfile...] */
        int nameLen = 0;
        while (levelFile[nameLen]) nameLen++;
        unsigned char pkt[64];
        pkt[0] = (unsigned char)(3 + nameLen + 1);
        pkt[1] = 0xF5;   /* OJXB_MAPSEL */
        pkt[2] = difficulty;
        int i;
        for (i = 0; i <= nameLen; i++) pkt[3 + i] = (unsigned char)levelFile[i];
        XbNet_SendData(pkt, pkt[0]);
    }

} /* extern "C" */