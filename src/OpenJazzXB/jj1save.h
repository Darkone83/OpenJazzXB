/**
 *
 * @file jj1save.h
 *
 * Part of the OpenJazz project
 *
 * @par Licence:
 * Copyright (c) 2015-2024 Carsten Teibes
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 */


#ifndef OJ_JJ1SAVE_H
#define OJ_JJ1SAVE_H

#include "file.h"
#include "types.h"

class JJ1Save {

private:
	JJ1Save(const JJ1Save&); // non construction-copyable
	JJ1Save& operator=(const JJ1Save&); // non copyable

public:
	explicit JJ1Save(const char* fileName);
	~JJ1Save();

	bool valid;
	char* name;

	static void write(int slot, int worldNum, int levelNum,
		difficultyType diff, const char* playerName);

	int planet;
	int level;
	difficultyType difficulty;
	int unknown[6];

	static bool write(const char* fileName, const char* playerName,
		int levelNum, int worldNum, difficultyType diff);

};

#endif