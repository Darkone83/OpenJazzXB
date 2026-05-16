/**
 * @file jj1save.cpp
 * XbJazz save format -- mirrors XbTyrian file write pattern exactly.
 */

#include "jj1save.h"
#include "util.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define XBJAZZ_SAVE_MAGIC 0x584A4A31u

 /* Same signature as XbTyrian's xbox_fopen_createfile, declared in xb_file_compat.cpp */
extern FILE* xbox_fopen(const char* path, const char* mode);

JJ1Save::JJ1Save(const char* fileName) :
	valid(false), name(nullptr), planet(0), level(0),
	difficulty(difficultyType::Normal) {

	char path[20];
	unsigned int buf[4];
	char nameBuf[17];
	FILE* f;

	memset(unknown, 0, sizeof(unknown));
	memset(nameBuf, 0, sizeof(nameBuf));

	/* Build D:\SAVE.x */
	path[0] = 'D'; path[1] = ':'; path[2] = '\\';
	path[3] = 'S'; path[4] = 'A'; path[5] = 'V'; path[6] = 'E'; path[7] = '.';
	path[8] = fileName[5]; path[9] = 0;

	f = xbox_fopen(path, "rb");
	if (!f) { name = createString("empty"); return; }

	if (fread(buf, sizeof(unsigned int), 4, f) != 4) {
		fclose(f); name = createString("empty"); return;
	}
	fread(nameBuf, 1, 16, f);
	fclose(f);

	if (buf[3] != XBJAZZ_SAVE_MAGIC) {
		name = createString("invalid"); return;
	}

	level = (int)buf[0];
	planet = (int)buf[1];
	difficulty = static_cast<difficultyType>((int)buf[2]);
	name = createString(nameBuf[0] ? nameBuf : "save");
	valid = true;
}

JJ1Save::~JJ1Save() {
	free(name);
}

void JJ1Save::write(int slot, int worldNum, int levelNum,
	difficultyType diff, const char* playerName) {

	char path[20];
	unsigned int buf[4];
	char nameBuf[16];
	int n;
	FILE* f;

	/* Build D:\SAVE.x */
	path[0] = 'D'; path[1] = ':'; path[2] = '\\';
	path[3] = 'S'; path[4] = 'A'; path[5] = 'V'; path[6] = 'E'; path[7] = '.';
	path[8] = (char)('0' + slot); path[9] = 0;

	f = xbox_fopen(path, "wb");
	if (!f) return;

	buf[0] = (unsigned int)levelNum;
	buf[1] = (unsigned int)worldNum;
	buf[2] = (unsigned int)(int)diff;
	buf[3] = XBJAZZ_SAVE_MAGIC;
	fwrite(buf, sizeof(unsigned int), 4, f);

	memset(nameBuf, 0, 16);
	if (playerName)
		for (n = 0; n < 15 && playerName[n]; n++)
			nameBuf[n] = playerName[n];
	fwrite(nameBuf, 1, 16, f);

	fclose(f);
}