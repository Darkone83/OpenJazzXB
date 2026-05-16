/**
 * @file setupmenu.cpp
 * XbJazz -- Setup menus (Xbox port). Replaces original OJ setupmenu.cpp.
 * Stubs out PC-only menus (keyboard, joystick, video resize).
 * Adds Xbox-specific controls remapping and video mode selection.
 */

#include "menu.h"
#include <string.h>
#include "plasma.h"
#include "controls.h"
#include "font.h"
#include "video.h"
#include "sound.h"
#include "loop.h"
#include "setup.h"
#include "util.h"
#include "xb_config.h"
#include "xb_input.h"

extern void XbVideoApplyConfig(void);

/* -----------------------------------------------------------------------
   Stubs for OJ class methods we don't use on Xbox
   ----------------------------------------------------------------------- */
#ifndef NO_KEYBOARD_CFG
int SetupMenu::setupKeyboard() { return message("NOT AVAILABLE ON XBOX"); }
#endif
int SetupMenu::setupJoystick() { return message("NOT AVAILABLE ON XBOX"); }
int SetupMenu::setupVideo() { return message("NOT AVAILABLE ON XBOX"); }

/* -----------------------------------------------------------------------
   Button name helper
   ----------------------------------------------------------------------- */
static const char* btnName(unsigned short btn) {
	switch (btn) {
	case XB_BTN_A:     return "A";
	case XB_BTN_B:     return "B";
	case XB_BTN_X:     return "X";
	case XB_BTN_Y:     return "Y";
	case XB_BTN_LTRIG: return "L-Trigger";
	case XB_BTN_RTRIG: return "R-Trigger";
	case XB_BTN_WHITE: return "White";
	case XB_BTN_BLACK: return "Black";
	case XB_BTN_BACK:  return "Back";
	default:           return "?";
	}
}

static const unsigned short MAPPABLE_BTNS[] = {
	XB_BTN_A, XB_BTN_X, XB_BTN_Y,
	XB_BTN_LTRIG, XB_BTN_RTRIG,
	XB_BTN_WHITE, XB_BTN_BLACK
};
#define MAPPABLE_COUNT 7

/* -----------------------------------------------------------------------
   xbSetupControls -- remap gameplay buttons
   ----------------------------------------------------------------------- */
static int xbSetupControls() {

	const char* actionNames[5] = {
		"primary fire",
		"secondary fire",
		"alt jump / swim",
		"change weapon",
		"stats overlay"
	};
	unsigned short* actionBtns[5] = {
		&g_xbConfig.controls.btnFire,
		&g_xbConfig.controls.btnFireAlt,
		&g_xbConfig.controls.btnJumpAlt,
		&g_xbConfig.controls.btnChange,
		&g_xbConfig.controls.btnStats
	};
	int chosen = 0;
	bool waiting = false;
	Plasma plasma;

	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	while (true) {

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;

		if (!waiting) {
			if (controls.release(C_ESCAPE)) { XbConfigSave(); return E_NONE; }
			if (controls.release(C_UP))     chosen = (chosen + 4) % 5;
			if (controls.release(C_DOWN))   chosen = (chosen + 1) % 5;
			if (controls.release(C_ENTER))  waiting = true;
		}
		else {
			unsigned short btns = XbInputGetButtons();
			int bi;
			for (bi = 0; bi < MAPPABLE_COUNT; bi++) {
				if (btns & MAPPABLE_BTNS[bi]) {
					*actionBtns[chosen] = MAPPABLE_BTNS[bi];
					waiting = false;
					playConfirmSound();
					break;
				}
			}
			if (!XbInputGetButtons()) {
				/* allow B to cancel only when no buttons held */
				if (controls.release(C_ESCAPE)) waiting = false;
			}
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();

		/* Title */
		fontmn2->showString("CONTROLS",
			canvasW >> 1, 8, alignX::Center);

		/* 2-column table: action | button */
		const int labelX = 12;
		const int valueX = (canvasW >> 1) + 20;
		const int rowH = 18;
		const int startY = 36;
		int i;

		for (i = 0; i < 5; i++) {
			int itemY = startY + i * rowH;
			if (i == chosen)
				video.drawRect(labelX - 2, itemY - 1,
					canvasW - (labelX - 2) * 2, rowH - 1, 79, false);
			fontmn2->showString(actionNames[i], labelX, itemY);
			if (waiting && i == chosen)
				fontmn2->showString("press button...", valueX, itemY);
			else
				fontmn2->showString(btnName(*actionBtns[i]), valueX, itemY);
		}

		/* Footer hints */
		fontbig->showString("a=jump  start=pause",
			3, canvasH, alignX::Left, alignY::Bottom);
		fontbig->showString("a=remap  b=back",
			canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}

	return E_NONE;
}

/* -----------------------------------------------------------------------
   xbSetupVideoXbox -- resolution, aspect, filter, scanlines
   ----------------------------------------------------------------------- */
static int xbSetupVideoXbox() {

	const char* modeNames[4] = { "auto",    "480p",    "720p",    "480i" };
	const char* aspectNames[4] = { "4:3",     "stretch", "pixel",   "fill" };
	const char* filterNames[3] = { "sharp",   "smooth",  "scale2x" };
	const char* scanNames[4] = { "off",     "light",   "medium",  "heavy" };

	int chosen = 0;
	Plasma plasma;

	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	while (true) {

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (controls.release(C_ESCAPE)) { XbConfigSave(); return E_NONE; }
		if (controls.release(C_UP))   chosen = (chosen + 3) % 4;
		if (controls.release(C_DOWN)) chosen = (chosen + 1) % 4;

		if (controls.release(C_LEFT)) {
			switch (chosen) {
			case 0: g_xbConfig.videoMode = (XbVideoMode)(((int)g_xbConfig.videoMode + 3) % 4); break;
			case 1: g_xbConfig.aspectMode = (XbAspectMode)(((int)g_xbConfig.aspectMode + 3) % 4); break;
			case 2: g_xbConfig.filterMode = (XbFilterMode)(((int)g_xbConfig.filterMode + 2) % 3); break;
			case 3: g_xbConfig.scanlines = (XbScanlines)(((int)g_xbConfig.scanlines + 3) % 4); break;
			}
			XbVideoApplyConfig(); playConfirmSound();
		}
		if (controls.release(C_RIGHT)) {
			switch (chosen) {
			case 0: g_xbConfig.videoMode = (XbVideoMode)(((int)g_xbConfig.videoMode + 1) % 4); break;
			case 1: g_xbConfig.aspectMode = (XbAspectMode)(((int)g_xbConfig.aspectMode + 1) % 4); break;
			case 2: g_xbConfig.filterMode = (XbFilterMode)(((int)g_xbConfig.filterMode + 1) % 3); break;
			case 3: g_xbConfig.scanlines = (XbScanlines)(((int)g_xbConfig.scanlines + 1) % 4); break;
			}
			XbVideoApplyConfig(); playConfirmSound();
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();

		fontmn2->showString("VIDEO OPTIONS",
			canvasW >> 1, (canvasH >> 1) - 56, alignX::Center);

		int labelX = (canvasW >> 1) - 88;
		int valueX = (canvasW >> 1) + 64;

		const char* rowLabels[4] = {
			"resolution < >", "aspect     < >",
			"filter     < >", "scanlines  < >"
		};
		const char* rowValues[4] = {
			modeNames[(int)g_xbConfig.videoMode],
			aspectNames[(int)g_xbConfig.aspectMode],
			filterNames[(int)g_xbConfig.filterMode],
			scanNames[(int)g_xbConfig.scanlines]
		};

		int i;
		for (i = 0; i < 4; i++) {
			int itemY = (canvasH >> 1) - 28 + i * 18;
			if (i == chosen)
				video.drawRect(labelX - 2, itemY - 1, 180, 16, 79, false);
			fontmn2->showString(rowLabels[i], labelX, itemY);
			fontmn2->showString(rowValues[i], valueX, itemY);
		}

		fontmn2->showString("resolution change needs restart",
			canvasW >> 1, (canvasH >> 1) + 44, alignX::Center);
		fontbig->showString("b = back",
			canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}

	return E_NONE;
}

/* -----------------------------------------------------------------------
   setupAudio
   ----------------------------------------------------------------------- */
int SetupMenu::setupAudio() {

	int x, y;
	bool soundActive = false;
	Plasma plasma;

	video.setPalette(menuPalette);

	while (true) {

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (controls.release(C_ESCAPE)) return E_NONE;
		if (controls.release(C_ENTER))  return E_NONE;

		if (controls.getCursor(x, y)) {
			if ((x < 100) && (y >= canvasH - 12) && controls.wasCursorReleased()) return E_NONE;
			x -= (canvasW >> 1) + 48;
			y -= canvasH >> 1;
			if ((x >= 0) && (x < (MAX_VOLUME >> 1)) && (y >= 0) && (y < 11)) setMusicVolume(x << 1);
			if ((x >= 0) && (x < (MAX_VOLUME >> 1)) && (y >= 16) && (y < 27)) setSoundVolume(x << 1);
			if (controls.wasCursorReleased()) playConfirmSound();
		}

		if (controls.release(C_UP))   soundActive = !soundActive;
		if (controls.release(C_DOWN)) soundActive = !soundActive;

		if (controls.release(C_LEFT)) {
			if (soundActive) setSoundVolume(getSoundVolume() - 4);
			else             setMusicVolume(getMusicVolume() - 4);
			playConfirmSound();
		}
		if (controls.release(C_RIGHT)) {
			if (soundActive) setSoundVolume(getSoundVolume() + 4);
			else             setMusicVolume(getMusicVolume() + 4);
			playConfirmSound();
		}

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();

		fontmn2->showString("AUDIO OPTIONS", canvasW >> 1, (canvasH >> 1) - 80, alignX::Center);

		if (!soundActive)
			video.drawRect((canvasW >> 1) - 90, (canvasH >> 1) - 1, 92, 14, 79, false);
		fontmn2->showString("music volume", (canvasW >> 1) - 88, canvasH >> 1);
		video.drawRect((canvasW >> 1) + 48, canvasH >> 1, getMusicVolume() >> 1, 11, 175);

		if (soundActive)
			video.drawRect((canvasW >> 1) - 90, (canvasH >> 1) + 15, 92, 14, 79, false);
		fontmn2->showString("effect volume", (canvasW >> 1) - 88, (canvasH >> 1) + 16);
		video.drawRect((canvasW >> 1) + 48, (canvasH >> 1) + 16, getSoundVolume() >> 1, 11, 175);

		showEscString();
	}

	return E_NONE;
}

/* -----------------------------------------------------------------------
   setupMain -- top-level setup menu
   ----------------------------------------------------------------------- */
int SetupMenu::setupMain() {

	const char* options[4] = { "controls", "video", "audio", "gameplay" };
	const char* setupModsOff[4] = {
		"slow motion: off", "extra items: take", "bird limit: one", "hud style: classic"
	};
	const char* setupModsOn[4] = {
		"slow motion: on",  "extra items: leave","bird limit: no",   "hud style: old fps"
	};
	const char* setupMods[4];
	int ret, option, suboption;

	option = 0;

	setupMods[0] = setup.slowMotion ? setupModsOn[0] : setupModsOff[0];
	setupMods[1] = setup.leaveUnneeded ? setupModsOn[1] : setupModsOff[1];
	setupMods[2] = setup.manyBirds ? setupModsOn[2] : setupModsOff[2];
	setupMods[3] = (setup.hudStyle == hudType::FPS) ? setupModsOn[3] : setupModsOff[3];

	/* Restore plasma colours from main menu into menuPalette */
	{
		int pi;
		for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi];
	}
	video.setPalette(menuPalette);

	while (true) {

		ret = generic("SETUP OPTIONS", options, 4, option);

		if (ret == E_RETURN) return E_NONE;
		if (ret < 0) return ret;

		switch (option) {

		case 0:
			if (xbSetupControls() == E_QUIT) return E_QUIT;
			break;

		case 1:
			if (xbSetupVideoXbox() == E_QUIT) return E_QUIT;
			break;

		case 2:
			if (setupAudio() == E_QUIT) return E_QUIT;
			break;

		case 3:
			suboption = 0;
			while (true) {
				ret = generic("GAME OPTIONS", setupMods, 4, suboption);
				if (ret == E_QUIT) return E_QUIT;
				if (ret < 0) break;
				setupMods[suboption] = (setupMods[suboption] == setupModsOff[suboption])
					? setupModsOn[suboption] : setupModsOff[suboption];
				setup.slowMotion = (setupMods[0] == setupModsOn[0]);
				setup.leaveUnneeded = (setupMods[1] == setupModsOn[1]);
				setup.manyBirds = (setupMods[2] == setupModsOn[2]);
				setup.hudStyle = (setupMods[3] == setupModsOn[3]) ? hudType::FPS : hudType::Classic;
			}
			break;
		}
	}

	return E_NONE;
}