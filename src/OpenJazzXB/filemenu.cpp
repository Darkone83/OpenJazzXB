/**
 * @file filemenu.cpp
 * Part of the OpenJazz project -- Save/Load file selection menu.
 */

#include "menu.h"
#include <string.h>
#include "plasma.h"
#include "controls.h"
#include "font.h"
#include "video.h"
#include "sound.h"
#include "jj1save.h"
#include "loop.h"

int FileMenu::main(bool forSaving, bool showCustom) {

	int i, chosen, options;
	Plasma plasma;
	const char* names[5];
	bool slotValid[4];

	chosen = 0;
	options = showCustom ? 5 : 4;

	/* Check which slots have valid saves */
	JJ1Save save0("SAVE.0");
	JJ1Save save1("SAVE.1");
	JJ1Save save2("SAVE.2");
	JJ1Save save3("SAVE.3");

	slotValid[0] = save0.valid;
	slotValid[1] = save1.valid;
	slotValid[2] = save2.valid;
	slotValid[3] = save3.valid;

	names[0] = save0.valid ? save0.name : "empty";
	names[1] = save1.valid ? save1.name : "empty";
	names[2] = save2.valid ? save2.name : "empty";
	names[3] = save3.valid ? save3.name : "empty";
	if (showCustom) names[4] = "custom";

	/* Restore plasma colours into menuPalette regardless of current palette */
	{
		int pi;
		for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi];
	}
	video.setPalette(menuPalette);

	while (true) {

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
		if (controls.release(C_ESCAPE))  return E_RETURN;

		if (controls.release(C_UP))   chosen = (chosen + options - 1) % options;
		if (controls.release(C_DOWN)) chosen = (chosen + 1) % options;

		if (controls.release(C_ENTER)) {
			if (chosen < 4 && !forSaving && !slotValid[chosen]) {
				playSound(SE::WAIT);
			}
			else {
				playConfirmSound();
				return chosen;
			}
		}

		SDL_Delay(T_MENU_FRAME);

		plasma.draw();

		fontmn2->showString(forSaving ? "SAVE GAME" : "LOAD GAME",
			canvasW >> 1, (canvasH >> 1) - (options << 3) - 32, alignX::Center);

		for (i = 0; i < options; i++) {
			int itemY = (canvasH >> 1) + (i << 4) - (options << 3);
			if (i == chosen)
				video.drawRect((canvasW >> 1) - 82, itemY - 1, 164, 16, 79, false);
			fontmn2->showString(names[i], (canvasW >> 1) - 80, itemY);
		}

		showEscString();
	}

	return E_RETURN;
}