/**
 *
 * @file jj1demolevel.cpp
 *
 * Part of the OpenJazz project
 *
 * @par History:
 * - 23rd August 2005: Created level.c
 * - 22nd July 2008: Created levelload.c from parts of level.c
 * - 3rd February 2009: Renamed level.c to level.cpp and levelload.c to
 *                    levelload.cpp
 * - 18th July 2009: Created demolevel.cpp from parts of level.cpp and
 *                 levelload.cpp
 * - 1st August 2012: Renamed demolevel.cpp to jj1demolevel.cpp
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Deals with the loading and playing of demo levels.
 *
 */


#include "jj1level.h"
#include "jj1levelplayer.h"

#include "game.h"
#include "gamemode.h"
#include "controls.h"
#include "file.h"
#include "font.h"
#include "video.h"
#include "sound.h"
#include "loop.h"
#include "util.h"


 /**
  * Create a JJ1 demo level.
  *
  * @param owner The current game
  * @param fileName Name of the file containing the macro data.
  */
JJ1DemoLevel::JJ1DemoLevel(Game* owner, const char* fileName) : JJ1Level(owner) {

	File* file;
	char* levelFile;
	int lNum, wNum, ret;

	multiplayer = false;

	file = new File(fileName, PATH_TYPE_GAME);

	// Check this is a normal level
	if (file->loadShort() == 0) {

		delete file;
		/* Xbox: no throw -- signal via macro=null, play() guards */
		macro = nullptr; return;

	}

	// Level file to load
	lNum = file->loadShort(9);
	wNum = file->loadShort(999);
	levelFile = createFileName("LEVEL", lNum, wNum);

	// Difficulty
	file->loadShort();

	macro = file->loadBlock(1024);

	delete file;

	// Load level data

	ret = load(levelFile, false);

	free(levelFile);

	if (ret < 0) { free(macro); macro = nullptr; }  /* no throw -- play() checks macro */

}


/**
 * Delete the JJ1 demo level.
 */
JJ1DemoLevel::~JJ1DemoLevel() {

	free(macro);

}


/**
 * Play the demo.
 *
 * @return Error code
 */
int JJ1DemoLevel::play() {

	/* Guard: macro=null means constructor failed (bad demo file or load error) */
	if (!macro) return E_NONE;

	tickOffset = globalTicks;
	ticks = 17;
	steps = 0;

	video.setPalette(palette);

	playMusic(musicFile);

	while (true) {

		// Do general processing
		if (::loop(NORMAL_LOOP, paletteEffects) == E_QUIT) return E_QUIT;

		if (controls.release(C_ESCAPE)) return E_NONE;

		if (controls.release(C_STATS)) stats ^= S_SCREEN;


		timeCalcs();



		// Use macro
		unsigned char macroPoint = macro[(ticks / 76) & 1023];

		if (macroPoint & 128) return E_NONE;

		if (macroPoint & 1) {

			localPlayer->setControl(C_LEFT, false);
			localPlayer->setControl(C_RIGHT, false);
			localPlayer->setControl(C_UP, !(macroPoint & 4));

		}
		else {

			localPlayer->setControl(C_LEFT, !(macroPoint & 2));
			localPlayer->setControl(C_RIGHT, macroPoint & 2);
			localPlayer->setControl(C_UP, false);

		}

		localPlayer->setControl(C_DOWN, macroPoint & 8);
		localPlayer->setControl(C_FIRE, macroPoint & 16);
		localPlayer->setControl(C_CHANGE, macroPoint & 32);
		localPlayer->setControl(C_JUMP, macroPoint & 64);
		localPlayer->setControl(C_SWIM, macroPoint & 64);



		// Check if level has been won
		if (getStage() == LS_END) return WON;


		// Process frame-by-frame activity

		// Process step
		while (getTimeChange() >= T_STEP) {

			int ret = step();
			steps++;

			if (ret < 0) return ret;

		}


		// Handle player reactions
		if (localPlayer->getJJ1LevelPlayer()->reacted(ticks) == PR_KILLED) return LOST;


		// Draw the graphics

		draw();
		drawOverlay(LEVEL_BLACK, false, 0, 0, 0, 0);


		font->showString("demo", canvasW >> 1, 32, alignX::Center);


	}

	return E_NONE;

}