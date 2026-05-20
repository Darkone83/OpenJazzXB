#pragma once
#pragma once
/**
 * xb_localnet.h -- OpenJazzXB LAN discovery layer.
 *
 * Purpose:
 *   - Advertise a local LAN host over UDP broadcast.
 *   - Scan for local LAN hosts.
 *   - Maintain a small discovered-host list.
 *
 * This module is intentionally discovery-only.
 * It does NOT own gameplay packets, ClientGame, ServerGame, or level state.
 */

#ifndef XB_LOCALNET_H
#define XB_LOCALNET_H

#ifdef __cplusplus
extern "C" {
#endif

#define XBLOCALNET_VERSION        1
#define XBLOCALNET_BCAST_PORT     10053
#define XBLOCALNET_DEFAULT_CTRL   10054
#define XBLOCALNET_DEFAULT_GAME   10052
#define XBLOCALNET_MAX_HOSTS      8
#define XBLOCALNET_NAME_LEN       16
#define XBLOCALNET_IP_LEN         32

#define XBLOCALNET_STATUS_WAITING 0
#define XBLOCALNET_STATUS_BUSY    1

    typedef struct XbLocalNetHost {
        char           name[XBLOCALNET_NAME_LEN];
        char           ip[XBLOCALNET_IP_LEN];
        unsigned short controlPort;
        unsigned short gamePort;
        unsigned int   version;
        unsigned int   lastSeenTicks;
        unsigned char  status;
        unsigned char  compatible;
    } XbLocalNetHost;

    /* Global init/shutdown.
     * Safe to call more than once.
     */
    int  XbLocalNet_Init(void);
    void XbLocalNet_Shutdown(void);

    /* Host advertisement.
     * Call StartAdvertise once, then PollAdvertise every menu frame.
     */
    int  XbLocalNet_StartAdvertise(const char* playerName,
        unsigned short controlPort,
        unsigned short gamePort,
        unsigned char status);
    void XbLocalNet_StopAdvertise(void);
    void XbLocalNet_PollAdvertise(void);
    void XbLocalNet_SetAdvertiseStatus(unsigned char status);

    /* Client scan.
     * Call StartScan once, then PollScan every menu frame.
     */
    int  XbLocalNet_StartScan(void);
    void XbLocalNet_StopScan(void);
    void XbLocalNet_PollScan(void);
    void XbLocalNet_ClearHosts(void);

    int  XbLocalNet_GetHostCount(void);
    const XbLocalNetHost* XbLocalNet_GetHost(int index);

#ifdef __cplusplus
}
#endif

#endif /* XB_LOCALNET_H */
