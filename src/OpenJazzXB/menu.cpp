/**
 *
 * @file menu.cpp
 *
 * Part of the OpenJazz project
 *
 * @par History:
 * - 23rd of August 2005: Created menu.c
 * - 3rd of February 2009: Renamed menu.c to menu.cpp
 * - 9th March 2009: Created game.cpp from parts of menu.cpp and level.cpp
 * - 18th July 2009: Created menugame.cpp from parts of menu.cpp
 * - 18th July 2009: Created menuutil.cpp from parts of menu.cpp
 * - 18th July 2009: Created menusetup.cpp from parts of menu.cpp
 * - 19th July 2009: Created menumain.cpp from parts of menu.cpp
 * - 23rd June 2010: Merged menuutil.cpp into menu.cpp
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Provides various generic menus.
 *
 */


#include "menu.h"
#include "xb_textentry.h"
#include "plasma.h"

#include "controls.h"
#include "font.h"
#include "video.h"
#include "sound.h"
#include "loop.h"
#include "util.h"
#include "platforms.h"

#include <string.h>


 /**
  * Show the "b = back" string.
  */
void Menu::showEscString(bool alignLeft) {

	if (alignLeft)
		fontbig->showString(ESCAPE_STRING, 3, canvasH, alignX::Left, alignY::Bottom);
	else
		fontbig->showString(ESCAPE_STRING, canvasW - 3, canvasH, alignX::Right, alignY::Bottom);

}


/**
 * Display a message to the user.
 *
 * @param text The message to display
 *
 * @return Error code
 */
int Menu::message(const char* text) {

	video.setPalette(menuPalette);

	while (true) {

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;

		if (controls.release(C_ENTER) || controls.release(C_ESCAPE) || controls.wasCursorReleased())
			return E_NONE;

		SDL_Delay(T_MENU_FRAME);

		video.clearScreen(15);

		// Draw the message
		fontmn2->showStringCentered(text);

	}

	return E_NONE;

}


/**
 * Let the user select from a menu of the given options.
 *
 * @param title Optional title, can be nullptr
 * @param optionNames Array of option names
 * @param options The number of options (and size of the names array)
 * @param chosen Which option is selected
 *
 * @return Error code
 */
int Menu::generic(const char* title, const char** optionNames, int options, int& chosen) {

	int x, y;
	Plasma plasma;

	if (chosen >= options) chosen = 0;

	// calculate the longest string length for centering
	int xOffset = (canvasW >> 1);
	int maxWidth = 0;
	for (int i = 0; i < options; i++) {
		int w = fontmn2->getStringWidth(optionNames[i]);
		if (w > maxWidth) maxWidth = w;
	}
	xOffset -= maxWidth >> 1;

	while (true) {

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;

		if (controls.release(C_ESCAPE)) return E_RETURN;

		if (controls.release(C_UP)) chosen = (chosen + options - 1) % options;

		if (controls.release(C_DOWN)) chosen = (chosen + 1) % options;

		if (controls.release(C_ENTER)) {

			playConfirmSound();
			return E_NONE;

		}

		if (controls.getCursor(x, y)) {

			if ((x < 100) && (y >= canvasH - 12) && controls.wasCursorReleased()) return E_RETURN;

			x -= canvasW >> 2;
			y -= (canvasH >> 1) - (options << 3);

			if ((x >= 0) && (x < 256) && (y >= 0) && (y < (options << 4))) {

				chosen = y >> 4;

				if (controls.wasCursorReleased()) {

					playConfirmSound();
					return E_NONE;

				}

			}

		}

		SDL_Delay(T_MENU_FRAME);

		plasma.draw();

		if (title)
			fontmn2->showString(title, canvasW >> 1, (canvasH >> 1) - 80, alignX::Center);

		for (int i = 0; i < options; i++) {

			int itemY = (canvasH >> 1) + (i << 4) - (options << 3);

			if (i == chosen)
				video.drawRect(xOffset - 2, itemY - 1,
					maxWidth + 4, 16, 79, false);

			fontmn2->showString(optionNames[i], xOffset, itemY);

		}

		showEscString();

	}

	return E_NONE;

}


/**
 * Let the user edit a text string
 *
 * @param request Description of the text string
 * @param text The text string to be edited
 *
 * @return Error code
 */
int Menu::textInput(const char* request, char*& text, bool ip) {
	/* Xbox: replace PC keyboard input with d-pad character cycling widget */
	char buf[STRING_LENGTH + 1];
	strncpy(buf, text ? text : "", STRING_LENGTH);
	buf[STRING_LENGTH] = '\0';

	int mode = ip ? XB_ENTRY_IP : XB_ENTRY_NAME;
	int ret = xbTextEntry(request, buf, ip ? 15 : STRING_LENGTH, mode);

	if (ret == E_NONE) {
		playConfirmSound();
		delete[] text;
		text = createString(buf);
	}
	return ret;
}