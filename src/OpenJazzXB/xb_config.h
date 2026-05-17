#pragma once
/**
 * xb_config.h -- Full config header for RXDK-only TUs (xtl.h-first).
 * Includes xb_input.h for XB_BTN_* constants.
 * OJ TUs use xb_config_fwd.h instead.
 */

typedef enum XbVideoMode { XB_VIDEO_AUTO = 0, XB_VIDEO_480P, XB_VIDEO_720P, XB_VIDEO_480I } XbVideoMode;
typedef enum XbAspectMode { XB_ASPECT_4_3 = 0, XB_ASPECT_STRETCH, XB_ASPECT_PIXEL, XB_ASPECT_FILL } XbAspectMode;
typedef enum XbFilterMode { XB_FILTER_SHARP = 0, XB_FILTER_SMOOTH, XB_FILTER_SCALE2X } XbFilterMode;
typedef enum XbScanlines { XB_SCANLINES_OFF = 0, XB_SCANLINES_LIGHT, XB_SCANLINES_MEDIUM, XB_SCANLINES_HEAVY } XbScanlines;

typedef struct XbJazzControls {
    unsigned short btnFire, btnFireAlt, btnJumpAlt, btnChange, btnStats;
} XbJazzControls;

typedef struct XbJazzConfig {
    unsigned int    magic;
    XbVideoMode     videoMode;
    XbAspectMode    aspectMode;
    XbFilterMode    filterMode;
    XbScanlines     scanlines;
    XbJazzControls  controls;
    char            playerName[16];
    char            serverAddr[64];
    unsigned char   reserved[4];
} XbJazzConfig;

#define XBJAZZ_CFG_MAGIC 0x584A4348u

extern XbJazzConfig g_xbConfig;

#include "xb_input.h"

void XbConfigSave(void);
void XbConfigLoad(void);
void XbConfigDefaults(void);
void XbVideoApplyConfig(void);
void XbVideoGetDimensions(XbVideoMode mode, int* w, int* h);
unsigned int XbVideoPresentFlags(XbVideoMode mode);
XbVideoMode XbVideoResolve(XbVideoMode mode);
int XbVideoModeAvailable(XbVideoMode mode);