/**
 *
 * @file font.cpp
 *
 * Part of the OpenJazz project
 *
 * @par History:
 * - 23rd August 2005: Created font.c
 * - 3rd February 2009: Renamed font.c to font.cpp
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Deals with the loading, displaying and freeing of screen fonts.
 *
 */

#include "file.h"
#include "font.h"
#include "video.h"
#include "log.h"

#include "stb_rect_pack.h"
#include <string.h>

namespace {
	constexpr unsigned int INVALID_FONT_CHAR = -1;

	constexpr int normalPadding = 2;
	constexpr int sceneStringPadding = 1;
}

void Font::commonSetup() {
	isOk = false;
	lineHeight = 0;
	spaceWidth = 0;
	nCharacters = MAX_FONT_CHARS;
	memset(atlasRects, 0, sizeof(atlasRects));
	memset(map, INVALID_FONT_CHAR, sizeof(map));
}

void Font::cleanMapping() {
	// remove empty
	for (int i = 0; i < MAX_FONT_CHARS; i++) {
		if (map[i] >= static_cast<unsigned int>(nCharacters))
			map[i] = INVALID_FONT_CHAR;
	}
}

/**
 * Load a font from the given .0FN file.
 *
 * @param fileName Name of an .0FN file
 */
Font::Font(const char* fileName) {
	commonSetup();

	// Load font from a font file
	File* file = new File(fileName, PATH_TYPE_GAME);
	int fileSize = file->getSize();

	// Checking font file
	char* identifier1 = file->loadString(18);
	char identifier2 = file->loadChar();
	if (strncmp(identifier1, "Digital Dimensions", 18) != 0 || identifier2 != 0x1A) {
		LOG_ERROR("Font not valid!");
		delete[] identifier1;
		return;
	}
	delete[] identifier1;

	spaceWidth = file->loadChar();
	lineHeight = file->loadChar();
	LOG_MAX("spaceWidth: %d, lineHeight: %d", spaceWidth, lineHeight);

	// temporary character data
	SDL_Surface* chars[MAX_FONT_CHARS];
	stbrp_rect rects[MAX_FONT_CHARS];
	memset(chars, 0, sizeof(chars)); /* init to nullptr -- some glyphs may not load */

	// Load characters
	for (int i = 0; i < MAX_FONT_CHARS; i++) {
		if (file->tell() >= fileSize) {
			nCharacters = i;
			LOG_TRACE("Loaded %d characters from font.", nCharacters);

			break;
		}

		int size = file->loadShort();
		int w = 0;
		int h = 0;

		if (size > 4) {
			unsigned char* pixels = file->loadRLE(size);

			int width = pixels[0] | pixels[1] << 8;
			int height = pixels[2] | pixels[3] << 8;

			if (size - 4 >= width * height) {
				chars[i] = video.createSurface(pixels + 4, width, height);

				w = width;
				h = height;
			}

			delete[] pixels;
		}

		// setup sizes for packing
		rects[i] = { i, w, h, 0, 0, 0 };

		// save size
		atlasRects[i].x = -1; atlasRects[i].y = -1;
		atlasRects[i].w = (Uint16)w; atlasRects[i].h = (Uint16)h;
	}

	delete file;

	// Pack all characters in a 128x128 pixels surface
	int aW = 128;
	int aH = 128;
	characterAtlas = video.createSurface(nullptr, aW, aH);
	video.enableColorKey(characterAtlas, 0);

	stbrp_context ctx;
	stbrp_node nodes[256]; /* fixed upper bound: largest atlas is 160px */
	stbrp_init_target(&ctx, aW, aH, nodes, aW);

	bool res = stbrp_pack_rects(&ctx, rects, nCharacters);
	if (res) {
		for (int i = 0; i < nCharacters; i++) {
			// save position
			atlasRects[i].x = rects[i].x;
			atlasRects[i].y = rects[i].y;

			// copy char to atlas
			if (rects[i].w > 0 && rects[i].h > 0)
				video.blitSurface(chars[i], nullptr, characterAtlas, &atlasRects[i]);
		}
	}
	else
		LOG_WARN("Could not pack font atlas!");

	// Delete single char surfaces
	for (int i = 0; i < nCharacters; i++) {
		if (rects[i].w > 0 && rects[i].h > 0)
			video.destroySurface(chars[i]);
	}

	// Create ASCII->font map
	map[33] = 107; // !
	map[34] = 116; // "
	map[36] = 63;  // $
	map[39] = 115; // '
	map[40] = 111; // (
	map[41] = 112; // )
	map[43] = 105; // +
	map[44] = 101; // ,
	map[45] = 104; // -
	map[46] = 102; // .
	map[47] = 108; // /
	for (int i = 48; i < 58; i++)
		map[i] = i + 5;  // Numbers
	map[58] = 114; // :
	map[59] = 113; // ;
	map[61] = 106; // =
	map[63] = 103; // ?
	for (int i = 65; i < 91; i++)
		map[i] = i - 38; // Upper-case letters
	for (int i = 97; i < 123; i++)
		map[i] = i - 96; // Lower-case letters

	cleanMapping();

	isOk = res;
}


/**
 * Create a font from the panel pixel data.
 *
 * @param pixels Panel pixel data
 * @param big Whether to use the small or the big font
 */
Font::Font(unsigned char* pixels, bool big) {
	commonSetup();

	int charWidth = 8;
	spaceWidth = 5;
	if (big) {
		nCharacters = 40;
		lineHeight = 8;
	}
	else {
		nCharacters = 13;
		lineHeight = 7;
	}

	// temporary character data
	SDL_Surface* chars[MAX_FONT_CHARS];
	memset(chars, 0, sizeof(chars));
	stbrp_rect rects[MAX_FONT_CHARS];

	unsigned char* chrPixels = new unsigned char[charWidth * lineHeight];

	for (int i = 0; i < nCharacters; i++) {
		// copy to atlas
		for (int y = 0; y < lineHeight; y++) {
			memcpy(chrPixels + (y * charWidth),
				pixels + (i * charWidth) + (y * SW), charWidth);
		}
		chars[i] = video.createSurface(chrPixels, charWidth, lineHeight);

		// setup sizes for packing
		rects[i] = { i, charWidth, lineHeight, 0, 0, 0 };

		// save size
		atlasRects[i].x = -1; atlasRects[i].y = -1;
		atlasRects[i].w = (Uint16)charWidth; atlasRects[i].h = (Uint16)lineHeight;
	}

	delete[] chrPixels;

	// Pack all characters in a surface
	int aW, aH;
	if (big) {
		// 56x48
		aW = 7 * charWidth;
		aH = 6 * lineHeight;
	}
	else {
		// 32x28
		aW = 4 * charWidth;
		aH = 4 * lineHeight;
	}

	characterAtlas = video.createSurface(nullptr, aW, aH);
	if (big) video.enableColorKey(characterAtlas, 31);

	stbrp_context ctx;
	stbrp_node nodes[256]; /* fixed upper bound: largest atlas is 160px */
	stbrp_init_target(&ctx, aW, aH, nodes, aW);

	bool res = stbrp_pack_rects(&ctx, rects, nCharacters);
	if (res) {
		for (int i = 0; i < nCharacters; i++) {
			// save position
			atlasRects[i].x = rects[i].x;
			atlasRects[i].y = rects[i].y;

			// copy char to atlas
			video.blitSurface(chars[i], nullptr, characterAtlas, &atlasRects[i]);
		}
	}
	else
		LOG_WARN("Could not pack font atlas!");

	// Delete single char surfaces
	for (int i = 0; i < nCharacters; i++)
		video.destroySurface(chars[i]);

	// Create ASCII->font map
	if (big) {
		// Goes " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-:."
		map[45] = 37; // -
		map[46] = 39; // .
		map[58] = 38; // :
		for (int i = 47; i < 58; i++)
			map[i] = i - 47; // Numbers
		for (int i = 64; i < 91; i++) {
			map[i] = i - 54; // Upper-case letters
			map[i + 32] = map[i]; // Lower-case letters (copy)
		}
	}
	else {
		// Goes " 0123456789oo" (where oo = infinity)
		// Use :; to represent the infinity symbol
		for (int i = 47; i < 60; i++)
			map[i] = i - 47; // Numbers and :;
	}

	isOk = res;
}


/**
 * Load a font from a .000 file.
 *
 * @param bonus whether to use FONTS.000 or BONUS.000
 */
Font::Font(bool bonus) {
	commonSetup();

	// Load font from FONTS.000 or BONUS.000
	File* file = new File(bonus ? "BONUS.000" : "FONTS.000", PATH_TYPE_GAME);

	int fileSize = file->getSize();
	nCharacters = file->loadShort(256);

	if (bonus) {
		int nSprites = file->loadShort();
		nCharacters -= nSprites;

		// Skip sprites
		for (int i = 0; i < nSprites; i++) {
			file->seek(4, false);

			int width = file->loadShort();
			if (width == 0xFFFF) width = 0;

			file->seek((width << 2) + file->loadShort(), false);
		}
	}

	// temporary character data
	SDL_Surface* chars[MAX_FONT_CHARS];
	stbrp_rect rects[MAX_FONT_CHARS];
	memset(chars, 0, sizeof(chars)); /* init to nullptr -- some glyphs may not load */

	// Load characters
	for (int i = 0; i < nCharacters; i++) {
		if (file->tell() >= fileSize) {
			nCharacters = i;
			LOG_TRACE("Loaded %d characters from font.", nCharacters);

			break;
		}

		int width = file->loadShort(SW);
		int height = file->loadShort(SH);

		// adjust
		if (bonus) width = (width + 3) & ~3;
		else width <<= 2;

		file->seek(4, false);

		unsigned char* pixels = file->loadPixels(width * height);

		chars[i] = video.createSurface(pixels, width, height);

		delete[] pixels;

		// setup sizes for packing
		rects[i] = { i, width, height, 0, 0, 0 };

		// save size
		atlasRects[i].x = -1; atlasRects[i].y = -1;
		atlasRects[i].w = (Uint16)width; atlasRects[i].h = (Uint16)height;
	}

	delete file;

	spaceWidth = 5;
	// use "A" as reference
	lineHeight = atlasRects[0].h;
	LOG_MAX("spaceWidth: %d, lineHeight: %d", spaceWidth, lineHeight);

	// Pack all characters in a 160x160 pixels surface
	int aW = 160;
	int aH = 160;
	characterAtlas = video.createSurface(nullptr, aW, aH);
	video.enableColorKey(characterAtlas, 254);

	stbrp_context ctx;
	stbrp_node nodes[256]; /* fixed upper bound: largest atlas is 160px */
	stbrp_init_target(&ctx, aW, aH, nodes, aW);

	bool res = stbrp_pack_rects(&ctx, rects, nCharacters);
	if (res) {
		for (int i = 0; i < nCharacters; i++) {
			// save position
			atlasRects[i].x = rects[i].x;
			atlasRects[i].y = rects[i].y;

			// copy char to atlas
			video.blitSurface(chars[i], nullptr, characterAtlas, &atlasRects[i]);
		}
	}
	else
		LOG_WARN("Could not pack font atlas!");

	// Delete single char surfaces
	for (int i = 0; i < nCharacters; i++)
		video.destroySurface(chars[i]);

	// Create ASCII->font map
	if (bonus) {
		map[42] = 37; // *
		map[46] = 39; // .
		map[47] = 38; // /
		map[58] = 36; // :
	}
	else {
		map[37] = 36; // %
	}
	for (int i = 48; i < 58; i++)
		map[i] = i - 22; // Numbers
	for (int i = 65; i < 91; i++) {
		map[i] = i - 65; // Upper-case letters
		map[i + 32] = map[i]; // Lower-case letters (copy)
	}

	cleanMapping();

	isOk = res;
}


/**
 * Delete the font.
 */
Font::~Font() {
	video.destroySurface(characterAtlas);
}

Point Font::showString(const char* string, int x, int y,
	alignX xAlign, alignY yAlign) {

	if (!isOk) return Point(x, y);

	int xOffsetBase, xOffset, yOffset;
	switch (xAlign) {
	default:
	case alignX::Left:   xOffset = x; break;
	case alignX::Center: xOffset = x - (getStringWidth(string) >> 1); break;
	case alignX::Right:  xOffset = x - getStringWidth(string); break;
	}
	xOffsetBase = xOffset;

	switch (yAlign) {
	default:
	case alignY::Top:    yOffset = y; break;
	case alignY::Center: yOffset = y - (getStringHeight(string) >> 1); break;
	case alignY::Bottom: yOffset = y - getStringHeight(string); break;
	}

	for (int i = 0; string[i]; i++) {
		if (string[i] == '\n') {
			xOffset = xOffsetBase;
			yOffset += lineHeight;
		}
		else {
			unsigned int c = map[int(string[i])];
			if (c == INVALID_FONT_CHAR) { xOffset += spaceWidth; continue; }
			SDL_Rect dst = { (Sint16)xOffset, (Sint16)yOffset, 0, 0 };
			video.blitSurface(characterAtlas, &atlasRects[c], canvas, &dst);
			xOffset += atlasRects[c].w + normalPadding;
		}
	}
	return Point(xOffset, yOffset);
}

Point Font::showStringCentered(const char* s) {
	return showString(s, (canvasW >> 1), (canvasH >> 1), alignX::Center, alignY::Center);
}

int Font::showSceneString(const unsigned char* string, int x, int y) {
	if (!isOk) return x;
	int offset = x;
	for (int i = 0; string[i]; i++) {
		SDL_Rect dst = { (Sint16)offset, (Sint16)y, 0, 0 };
		if (string[i] >= nCharacters) { offset += spaceWidth; continue; }
		int c = string[i];
		video.blitSurface(characterAtlas, &atlasRects[c], canvas, &dst);
		offset += atlasRects[c].w + sceneStringPadding;
	}
	return offset;
}

int Font::showSceneStringShadow(const unsigned char* string, int x, int y) {
	if (!isOk) return x;
	/* Write palette index 0 (black) directly into the canvas for every
	 * non-transparent glyph pixel.  Fonts loaded from .0FN files use
	 * colorkey = 0, so any atlas pixel != 0 is a real glyph pixel.
	 * We bypass SDL_BlitSurface entirely so the shadow index lands in
	 * the canvas independently of whatever palette remapping is active. */
	if (SDL_LockSurface(characterAtlas) != 0) return x;
	if (SDL_LockSurface(canvas) != 0) {
		SDL_UnlockSurface(characterAtlas);
		return x;
	}
	const Uint8* src = (const Uint8*)characterAtlas->pixels;
	Uint8* dst = (Uint8*)canvas->pixels;
	const int    srcP = characterAtlas->pitch;
	const int    dstP = canvas->pitch;
	const int    cW = canvas->w;
	const int    cH = canvas->h;

	int offset = x;
	for (int i = 0; string[i]; i++) {
		if (string[i] >= (unsigned char)nCharacters) {
			offset += spaceWidth;
			continue;
		}
		const SDL_Rect& r = atlasRects[string[i]];
		for (int row = 0; row < r.h; row++) {
			for (int col = 0; col < r.w; col++) {
				Uint8 pixel = src[(r.y + row) * srcP + (r.x + col)];
				if (pixel == 0) continue;   /* colorkey = 0 = transparent */
				int dx = offset + col;
				int dy = y + row;
				if (dx >= 0 && dx < cW && dy >= 0 && dy < cH)
					dst[dy * dstP + dx] = 0; /* black shadow */
			}
		}
		offset += r.w + sceneStringPadding;
	}
	SDL_UnlockSurface(canvas);
	SDL_UnlockSurface(characterAtlas);
	return offset;
}

void Font::showNumber(int n, int x, int y) {
	if (!isOk) return;
	SDL_Rect dst;
	if (!n) {
		unsigned int c = map[int('0')];
		dst.y = (Sint16)y; dst.x = (Sint16)(x - atlasRects[c].w);
		video.blitSurface(characterAtlas, &atlasRects[c], canvas, &dst);
		return;
	}
	int count = (n > 0) ? n : -n;
	int offset = x;
	while (count) {
		unsigned int c = map[int('0' + (count % 10))];
		offset -= atlasRects[c].w;
		dst.y = (Sint16)y; dst.x = (Sint16)offset;
		video.blitSurface(characterAtlas, &atlasRects[c], canvas, &dst);
		count /= 10;
	}
	if (n < 0) {
		unsigned int c = map[int('-')];
		dst.y = (Sint16)y; dst.x = (Sint16)(offset - atlasRects[c].w);
		video.blitSurface(characterAtlas, &atlasRects[c], canvas, &dst);
	}
}

/* Saved atlas palette for mapPalette/restorePalette.
 * We remap only the characterAtlas surface palette -- NEVER currentPalette.
 * currentPalette is used by flip() for game rendering; touching it corrupts
 * game colours (e.g. turns orange carrots grey). */
static SDL_Color fontSavedPal[MAX_PALETTE_COLORS];
static int fontSavedStart = -1;
static int fontSavedLength = 0;

void Font::mapPalette(int start, int length, int newStart, int newLength) {
	if (!isOk || !characterAtlas) return;
	if (!characterAtlas->format || !characterAtlas->format->palette ||
		!characterAtlas->format->palette->colors) return;
	SDL_Color* cur = video.getPalette();  /* read-only source for remap values */
	SDL_Color snap[MAX_PALETTE_COLORS];
	int i;
	memcpy(snap, cur, MAX_PALETTE_COLORS * sizeof(SDL_Color));
	/* Save current atlas palette then remap it -- read directly from surface */
	memcpy(fontSavedPal, characterAtlas->format->palette->colors,
		MAX_PALETTE_COLORS * sizeof(SDL_Color));
	fontSavedStart = start;
	fontSavedLength = length;
	/* Build remapped copy and apply to atlas surface only */
	SDL_Color remapped[MAX_PALETTE_COLORS];
	memcpy(remapped, characterAtlas->format->palette->colors,
		MAX_PALETTE_COLORS * sizeof(SDL_Color));
	for (i = 0; i < length && (start + i) < MAX_PALETTE_COLORS; i++) {
		int srcIdx = (i * newLength / length) + newStart;
		if (srcIdx >= 0 && srcIdx < MAX_PALETTE_COLORS)
			remapped[start + i] = snap[srcIdx];
	}
	SDL_SetColors(characterAtlas, remapped, 0, MAX_PALETTE_COLORS);
}

void Font::restorePalette() {
	if (!isOk || !characterAtlas || fontSavedStart < 0) return;
	if (!characterAtlas->format || !characterAtlas->format->palette) return;
	/* Restore atlas surface palette -- currentPalette is untouched */
	SDL_SetColors(characterAtlas, fontSavedPal, 0, MAX_PALETTE_COLORS);
	fontSavedStart = -1;
}

int Font::getStringWidth(const char* string) {
	if (!isOk) return 0;
	int stringWidth = 0, tmpWidth = 0;
	for (int i = 0; string[i]; i++) {
		if (string[i] == '\n') {
			if (tmpWidth > stringWidth) stringWidth = tmpWidth;
			tmpWidth = 0;
		}
		unsigned int c = map[int(string[i])];
		if (c == INVALID_FONT_CHAR) { tmpWidth += spaceWidth; continue; }
		tmpWidth += atlasRects[c].w + normalPadding;
	}
	if (tmpWidth > stringWidth) return tmpWidth;
	return stringWidth;
}

int Font::getStringHeight(const char* string) {
	if (!isOk) return 0;
	int stringHeight = lineHeight;
	for (int i = 0; string[i]; i++)
		if (string[i] == '\n') stringHeight += lineHeight;
	return stringHeight;
}

int Font::getSceneStringWidth(const unsigned char* string) {
	if (!isOk) return 0;
	int stringWidth = 0;
	for (int i = 0; string[i]; i++) {
		if (string[i] >= nCharacters) { stringWidth += spaceWidth; continue; }
		stringWidth += atlasRects[string[i]].w + sceneStringPadding;
	}
	return stringWidth;
}

#ifdef DEBUG_FONTS
void Font::saveAtlasAsBMP(const char* fileName) {
	if (!isOk) { LOG_WARN("Not saving empty font atlas!"); return; }
	video.restoreSurfacePalette(characterAtlas);
	SDL_SaveBMP(characterAtlas, fileName);
}
#endif