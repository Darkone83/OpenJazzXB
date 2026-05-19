#pragma once
/**
 * xb_netlog.h -- XbJazz network debug logger.
 *
 * RXDK TU safe (<xtl.h> not required here -- plain C).
 * Writes to D:\openjazzxb_srv.txt (host) or D:\openjazzxb_client.txt (client).
 * Logging is compiled in but gated by XbNetLog_Enable() at runtime.
 * Zero overhead when disabled -- all calls reduce to a null check.
 */

#ifndef XB_NETLOG_H
#define XB_NETLOG_H

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Call once before any logging. role: 0=client, 1=server/host.
     * Opens the appropriate log file and writes a session header.
     * No-op if logging is already open.
     */
    void XbNetLog_Open(int isHost);

    /**
     * Enable or disable logging at runtime (default: disabled).
     * Can be toggled without closing/reopening the file.
     */
    void XbNetLog_Enable(int enable);

    /**
     * Write a formatted log line with timestamp.
     * No-op if logging is disabled or file not open.
     */
    void XbNetLog_Write(const char* fmt, ...);

    /**
     * Log a packet (direction, type byte, length).
     * direction: "TX" or "RX".
     */
    void XbNetLog_Packet(const char* direction,
        unsigned char type,
        int length);

    /**
     * Flush and close the log file. Call on disconnect/shutdown.
     */
    void XbNetLog_Close(void);

#ifdef __cplusplus
}
#endif

#endif /* XB_NETLOG_H */