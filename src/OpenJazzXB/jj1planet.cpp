/**
 *
 * @file jj1planet.cpp
 *
 * Part of the OpenJazz project
 *
 * @par History:
 * - 23rd August 2005: Created planet.c
 * - 3rd February 2009: Renamed planet.c to planet.cpp
 * - 1st August 2012: Renamed planet.cpp to jj1planet.cpp
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Deals with the loading, displaying and freeing of the planet landing
 * sequence.
 *
 */


#include "jj1planet.h"

#include "controls.h"
#include "file.h"
#include "font.h"
#include "video.h"
#include "sound.h"
#include "loop.h"
#include "util.h"

#include <string.h>


 /**
  * Create a JJ1 planet approach sequence.
  *
  * @param fileName Name of the file containing the planet data
  * @param previous The ID of the last planet approach sequence
  */
JJ1Planet::JJ1Planet(char* fileName, int previous) {

	File* file;
	unsigned char* pixels;
	int count;

	file = new File(fileName, PATH_TYPE_GAME);

	if (!file->isOpen()) {
		delete file;
		id = -1; /* no throw: caller checks planet->getId() != -1 */
		name = createString("unknown"); return;
	}

	id = file->loadShort();

	if (id == previous) {

		// Not approaching a planet if already there

		delete file;

		/* no throw: id==previous signals "already here", caller skips play() */
		name = createString("here"); return;

	}

	// Load planet name
	name = file->loadTerminatedString();
	if (!name) name = createString("unknown");

	// Lower-case the name
	for (count = 0; name[count]; count++) {

		if ((name[count] >= 65) && (name[count] <= 90)) name[count] += 32;

	}

	// Load the palette
	file->loadPalette(palette, false);

	// Load the planet image
	pixels = file->loadBlock(64 * 55);
	if (pixels) {
		sprite.setPixels(pixels, 64, 55, 0);
		delete[] pixels;
	}


	delete file;

}


/**
 * Delete the JJ1 planet approach sequence.
 */
JJ1Planet::~JJ1Planet() {

	delete[] name;

}


/**
 * Get the ID of the planet approach squence.
 *
 * @return The ID
 */
int JJ1Planet::getId() {

	return id;

}


/**
 * Run the JJ1 planet approach sequence.
 *
 * @return Error code
 */
int JJ1Planet::play() {

	unsigned int tickOffset;

	tickOffset = globalTicks;

	stopMusic();

	video.setPalette(palette);

	/* Warp starfield: stars have normalised x/y in [-1,1] space
	 * and a z depth. Each frame z shrinks, making stars expand outward
	 * from centre -- classic space-travel hyperspace effect. */
	struct WarpStar {
		int nx, ny;        /* normalised position * 4096 */
		int z;             /* depth 1..4096 -- smaller = closer */
		unsigned char bright;
	};
	const int N_STARS = 150;
	WarpStar stars[N_STARS];
	{
		unsigned int seed = (unsigned int)(id + 1) * 2654435761u;
		for (int i = 0; i < N_STARS; i++) {
			seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
			int nx = (int)(seed & 0x1FFF) - 4096; /* -4096..4095 */
			seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
			int ny = (int)(seed & 0x1FFF) - 4096;
			seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
			int z = 256 + (int)(seed & 0xFFF); /* 256..4351 */
			stars[i].nx = nx; stars[i].ny = ny; stars[i].z = z;
			stars[i].bright = (i % 3 == 0) ? 7 : 15;
		}
	}

	while (true) {

		if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;

		if (controls.release(C_ESCAPE) || controls.wasCursorReleased()) return E_NONE;

		SDL_Delay(T_MENU_FRAME);

		video.clearScreen(0);

		/* Animate warp stars: shrink z, project to screen coords */
		if (SDL_LockSurface(canvas) == 0) {
			const int cx = canvas->w >> 1;
			const int cy = canvas->h >> 1;
			for (int i = 0; i < N_STARS; i++) {
				stars[i].z -= 6; /* approach speed */
				if (stars[i].z < 1) stars[i].z = 4096;
				int sx = cx + stars[i].nx * cx / stars[i].z;
				int sy = cy + stars[i].ny * cy / stars[i].z;
				if (sx >= 0 && sx < canvas->w && sy >= 0 && sy < canvas->h)
					((Uint8*)canvas->pixels)[sy * canvas->pitch + sx] = stars[i].bright;
			}
			SDL_UnlockSurface(canvas);
		}

		if (globalTicks - tickOffset < F2)
			sprite.drawScaled(canvasW >> 1, canvasH >> 1, globalTicks - tickOffset);
		else if (globalTicks - tickOffset < F4)
			sprite.drawScaled(canvasW >> 1, canvasH >> 1, F2);
		else if (globalTicks - tickOffset < F4 + FQ)
			sprite.drawScaled(canvasW >> 1, canvasH >> 1, (globalTicks - tickOffset - F4) * 32 + F2);
		else return E_NONE;

		fontmn1->showString("now approaching", (canvasW >> 1), 2, alignX::Center);
		fontmn1->showString(name, (canvasW >> 1), canvasH - 2, alignX::Center, alignY::Bottom);

	}

	return E_NONE;

}