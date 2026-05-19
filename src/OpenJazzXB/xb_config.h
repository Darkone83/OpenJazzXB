#pragma once
/**
 * xb_config.h -- XbJazz persistent configuration.
 * Video mode architecture mirrors XbTyrian exactly:
 *   - videoMode set at D3D init time (requires restart to change)
 *   - aspectMode, filter, scanlines applied via build_quad/render states
 */

 /* Video output resolution -- detected via XGetVideoFlags() */
typedef enum XbVideoMode {
    XB_VIDEO_AUTO = 0,  /* pick best available    */
    XB_VIDEO_480P = 1,  /* 640x480  progressive   */
    XB_VIDEO_720P = 2,  /* 1280x720               */
    XB_VIDEO_480I = 3,  /* 640x480  interlaced SD */
} XbVideoMode;

/* Aspect / scale mode -- applied to build_quad, no restart needed */
typedef enum XbAspectMode {
    XB_ASPECT_4_3 = 0,  /* 4:3 corrected, letterbox/pillarbox */
    XB_ASPECT_STRETCH = 1,  /* fill entire screen                 */
    XB_ASPECT_PIXEL = 2,  /* integer scale centred (contain)    */
    XB_ASPECT_FILL = 3,  /* integer scale fill (cover/crop)    */
} XbAspectMode;

/* Texture filter */
typedef enum XbFilterMode {
    XB_FILTER_SHARP = 0,  /* D3DTEXF_POINT  */
    XB_FILTER_SMOOTH = 1,  /* D3DTEXF_LINEAR */
    XB_FILTER_SCALE2X = 2,  /* CPU Scale2x    */
} XbFilterMode;

/* Scanline overlay */
typedef enum XbScanlines {
    XB_SCANLINES_OFF = 0,
    XB_SCANLINES_LIGHT = 1,
    XB_SCANLINES_MEDIUM = 2,
    XB_SCANLINES_HEAVY = 3,
} XbScanlines;

/* Controller remapping */
typedef struct XbJazzControls {
    unsigned short btnFire;
    unsigned short btnFireAlt;
    unsigned short btnJumpAlt;
    unsigned short btnChange;
    unsigned short btnStats;
} XbJazzControls;

typedef struct XbJazzConfig {
    unsigned int    magic;
    XbVideoMode     videoMode;
    XbAspectMode    aspectMode;
    XbFilterMode    filterMode;
    XbScanlines     scanlines;
    XbJazzControls  controls;
    char            playerName[16];    /* multiplayer display name  */
    char            serverAddr[64];    /* relay host[:port]         */
} XbJazzConfig;

#define XBJAZZ_CFG_MAGIC 0x584A4348u  /* v3 -- added playerName/serverAddr */

extern XbJazzConfig g_xbConfig;

/* Backbuffer dimensions for a resolved mode */
void XbVideoGetDimensions(XbVideoMode mode, int* w, int* h);

/* D3D present flags for a resolved mode */
unsigned int XbVideoPresentFlags(XbVideoMode mode);

/* Resolve AUTO -> best available mode using XGetVideoFlags() */
XbVideoMode XbVideoResolve(XbVideoMode mode);

/* Returns non-zero if the mode is available on this hardware */
int XbVideoModeAvailable(XbVideoMode mode);

void XbConfigDefaults(void);
void XbConfigLoad(void);
void XbConfigSave(void);
void XbVideoApplyConfig(void);