/**
 * xb_config.cpp -- XbJazz config defaults. No file I/O (in xb_file_compat.cpp).
 */

#include "xb_config.h"
#include "xb_input.h"
#include <string.h>

XbJazzConfig g_xbConfig;

void XbConfigDefaults(void) {
	int i;
	unsigned char* p = (unsigned char*)&g_xbConfig;
	for (i = 0; i < (int)sizeof(g_xbConfig); i++) p[i] = 0;
	g_xbConfig.magic = XBJAZZ_CFG_MAGIC;
	g_xbConfig.videoMode = XB_VIDEO_AUTO;
	g_xbConfig.aspectMode = XB_ASPECT_4_3;
	g_xbConfig.filterMode = XB_FILTER_SHARP;
	g_xbConfig.scanlines = XB_SCANLINES_OFF;
	g_xbConfig.controls.btnFire = XB_BTN_LTRIG;
	g_xbConfig.controls.btnFireAlt = XB_BTN_X;
	g_xbConfig.controls.btnJumpAlt = XB_BTN_RTRIG;
	g_xbConfig.controls.btnChange = XB_BTN_Y;
	g_xbConfig.controls.btnStats = XB_BTN_BLACK;
	strncpy(g_xbConfig.playerName, "PLAYER", sizeof(g_xbConfig.playerName) - 1);
	strncpy(g_xbConfig.serverAddr, "darkone83.myddns.me", sizeof(g_xbConfig.serverAddr) - 1);
}