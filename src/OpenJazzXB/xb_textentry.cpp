/**
 * xb_textentry.cpp
 * Xbox d-pad character cycling text entry widget.
 * Ported from XbTyrian mainint.cpp JE_xboxCycleTextChar.
 *
 * Up/Down    = cycle char at cursor
 * Left/Right = move cursor
 * A          = confirm char, advance
 * B          = backspace
 * Start      = accept
 */

#include <string.h>
#include "xb_textentry.h"
#include "xb_config.h"
#include "video.h"
#include "font.h"
#include "controls.h"
#include "loop.h"
#include "plasma.h"
#include "menu.h"
#include "util.h"
#include "sound.h"

extern Font* fontmn2;
extern Font* fontbig;

static const char CHARSET_NAME[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
static const char CHARSET_IP[] = "0123456789.";

static char cycleChar(char c, int dir, int mode) {
	const char* set = (mode == XB_ENTRY_IP) ? CHARSET_IP : CHARSET_NAME;
	int count = 0;
	while (set[count]) count++;
	int idx = 0;
	for (int i = 0; i < count; i++) if (set[i] == c) { idx = i; break; }
	return set[(idx + dir + count) % count];
}

int xbTextEntry(const char* title, char* buf, int maxLen, int mode) {

	/* Pad buffer with spaces */
	int len = (int)strlen(buf);
	for (int i = len; i < maxLen; i++) buf[i] = ' ';
	buf[maxLen] = '\0';

	/* Clamp cursor to first non-space */
	int cursor = 0;
	for (int i = 0; i < maxLen; i++) if (buf[i] != ' ') { cursor = i; break; }

	/* Validate existing chars against charset */
	const char* set = (mode == XB_ENTRY_IP) ? CHARSET_IP : CHARSET_NAME;
	for (int i = 0; i < maxLen; i++) {
		bool ok = false;
		for (int j = 0; set[j]; j++) if (set[j] == buf[i]) { ok = true; break; }
		if (!ok) buf[i] = set[0];
	}

	Plasma plasma;
	{ int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
	video.setPalette(menuPalette);

	bool upWas = false, downWas = false, leftWas = false, rightWas = false;
	bool aWas = false, bWas = false, startWas = false;

	while (true) {
		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;

		bool up = controls.getState(C_UP);
		bool down = controls.getState(C_DOWN);
		bool left = controls.getState(C_LEFT);
		bool right = controls.getState(C_RIGHT);
		bool aBtn = controls.getState(C_ENTER);
		bool bBtn = controls.getState(C_ESCAPE);
		bool stBtn = controls.getState(C_PAUSE);

		if (up && !upWas) { buf[cursor] = cycleChar(buf[cursor], -1, mode); playConfirmSound(); }
		if (down && !downWas) { buf[cursor] = cycleChar(buf[cursor], 1, mode); playConfirmSound(); }
		if (left && !leftWas) { if (cursor > 0) cursor--; playConfirmSound(); }
		if ((right && !rightWas) || (aBtn && !aWas)) {
			if (cursor < maxLen - 1) cursor++;
			playConfirmSound();
		}
		if (bBtn && !bWas) {
			buf[cursor] = ' ';
			if (cursor > 0) cursor--;
			playConfirmSound();
		}
		if (stBtn && !startWas) {
			/* Trim trailing spaces */
			int end = maxLen - 1;
			while (end > 0 && buf[end] == ' ') end--;
			buf[end + 1] = '\0';
			return E_NONE;
		}

		upWas = up; downWas = down; leftWas = left; rightWas = right;
		aWas = aBtn; bWas = bBtn; startWas = stBtn;

		SDL_Delay(T_MENU_FRAME);
		plasma.draw();

		fontmn2->showString(title, canvasW >> 1, 8, alignX::Center);

		/* Field: 11px slots with fontmn2 for legibility */
		const int slotW = 11;
		const int fieldW = maxLen * slotW;
		const int fieldX = (canvasW - fieldW) >> 1;
		const int fieldY = (canvasH >> 1) - 7;

		/* Field background and border */
		video.drawRect(fieldX - 3, fieldY - 3, fieldW + 6, 18, 0, true);
		video.drawRect(fieldX - 3, fieldY - 3, fieldW + 6, 18, 79, false);

		/* Characters and cursor underline */
		for (int i = 0; i < maxLen; i++) {
			char ch[2] = { buf[i], '\0' };
			int cx = fieldX + i * slotW;
			fontmn2->showString(ch, cx, fieldY);
			if (i == cursor)
				video.drawRect(cx, fieldY + 11, slotW - 1, 2, 79, true);
		}

		/* Footer split left/right -- avoids center-clip */
		fontbig->showString("up/dn=char  lt/rt=move",
			3, canvasH, alignX::Left, alignY::Bottom);
		fontbig->showString("start=done  b=del",
			canvasW - 3, canvasH, alignX::Right, alignY::Bottom);
	}
}