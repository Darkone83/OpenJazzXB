/**
 *
 * @file file.cpp
 *
 * Part of the OpenJazz project
 *
 * @par History:
 * - 23rd August 2005: Created main.c
 * - 22nd July 2008: Created util.c from parts of main.c
 * - 3rd February 2009: Renamed util.c to util.cpp
 * - 3rd February 2009: Created file.cpp from parts of util.cpp
 *
 * @par Licence:
 * Copyright (c) 2005-2017 AJ Thomson
 * Copyright (c) 2015-2026 Carsten Teibes
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Deals with files.
 *
 */


#include "file.h"

#include "video.h"
#include "util.h"
#include "log.h"

#include <string.h>
#include "miniz.h"
#include "unistd.h"
#include "miniz.h"

#ifdef OJ_SDL3
#include <SDL3/SDL.h>
#else
#include "SDL.h"

 /* Implemented in xb_file_compat.cpp (<xtl.h>-only TU) */
extern FILE* xbox_fopen(const char* path, const char* mode);
extern int   xbox_fs_writable(void);
extern void  xbox_detect_fs(void);
#endif

#if !(defined(_WIN32) || defined(WII) || defined(PSP))
#define UPPERCASE_FILENAMES
#define LOWERCASE_FILENAMES
#endif


/**
 * Try opening a file from the available paths.
 *
 * @param name File name
 * @param pathType Kind of directory
 * @param write Whether or not the file can be written to
 */
File::File(const char* name, int pathType, bool write) :
	file(nullptr), filePath(nullptr), forWriting(write) {

	Path* path = gamePaths.paths;

	while (path) {

		// skip other paths
		if (pathType != PATH_TYPE_ANY && (path->pathType & pathType) != pathType) {
			path = path->next;
			continue;
		}

		// only allow certain write paths
		if (!write || (pathType & (PATH_TYPE_CONFIG | PATH_TYPE_TEMP)) > 0) {
			if (open(path->path, name, write)) return;
		}
		else LOG_FATAL("Not allowed to write to %s", name);

		path = path->next;

	}

	LOG_WARN("Could not open file: %s", name);
	/* No throw -- isOpen() returns false; caller must check */


}


/**
 * Delete the file object.
 */
File::~File() {

	if (file) {
		fclose(file);
		LOG_TRACE("Closed file: %s", filePath);
	}

	if (filePath) free(filePath);

}


/**
 * Try opening a file from the given path
 *
 * @param path Directory path
 * @param name File name
 * @param write Whether or not the file can be written to
 */
bool File::open(const char* path, const char* name, bool write) {
	const char* fileMode = write ? "wb" : "rb";

	// Create the file path for the given directory
	filePath = createString(path, name);

	// Open the file from the path
	file = xbox_fopen(filePath, fileMode);

#ifdef UPPERCASE_FILENAMES
	if (!file) {
		uppercaseString(filePath + strlen(path));
		file = xbox_fopen(filePath, fileMode);
	}
#endif

#ifdef LOWERCASE_FILENAMES
	if (!file) {
		lowercaseString(filePath + strlen(path));
		file = xbox_fopen(filePath, fileMode);
	}
#endif

	if (file) {
		LOG_DEBUG("Opened file: %s", filePath);

		return true;
	}

	free(filePath);
	filePath = nullptr;

	return false;
}


/**
 * Get the size of the file.
 *
 * @return The size of the file
 */
int File::getSize() {
	if (!file) return 0;


	int pos, size;

	pos = ftell(file);

	fseek(file, 0, SEEK_END);

	size = ftell(file);

	fseek(file, pos, SEEK_SET);

	return size;

}


/**
 * Get the current read/write location within the file.
 *
 * @return The current location
 */
int File::tell() {
	if (!file) return 0;


	return ftell(file);

}


/**
 * Set the read/write location within the file.
 *
 * @param offset The new offset
 * @param reset Whether to offset from the current location or the start of the file
 */
void File::seek(int offset, bool reset) {
	if (!file) return;


	fseek(file, offset, reset ? SEEK_SET : SEEK_CUR);

}


/**
 * Load an unsigned char from the file.
 *
 * @return The value read
 */
unsigned char File::loadChar() {
	if (!file) return 0;


	return fgetc(file);

}


void File::storeChar(unsigned char val) {
	if (!file) return;


	fputc(val, file);

}


/**
 * Load an unsigned short int from the file.
 *
 * @return The value read
 */
unsigned short int File::loadShort() {
	if (!file) return 0;


	unsigned short int val;

	val = fgetc(file);
	val += fgetc(file) << 8;

	return val;

}


/**
 * Load an unsigned short int with an upper limit from the file.
 *
 * @return The value read
 */
unsigned short int File::loadShort(unsigned short int max) {
	if (!file) return 0;


	unsigned short int val;

	val = loadShort();

	if (val > max) {

		LOG_ERROR("Oversized value %d>%d in file %s", val, max, filePath);

		return max;

	}

	return val;

}


void File::storeShort(unsigned short int val) {
	if (!file) return;


	fputc(val & 255, file);
	fputc(val >> 8, file);

}


/**
 * Load a signed int from the file.
 *
 * @return The value read
 */
signed int File::loadInt() {
	if (!file) return 0;


	unsigned int val;

	val = fgetc(file);
	val += fgetc(file) << 8;
	val += fgetc(file) << 16;
	val += fgetc(file) << 24;

	return *((signed int*)&val);

}


void File::storeInt(signed int val) {
	if (!file) return;


	unsigned int uval;

	uval = *((unsigned int*)&val);

	fputc(uval & 255, file);
	fputc((uval >> 8) & 255, file);
	fputc((uval >> 16) & 255, file);
	fputc(uval >> 24, file);

}


void File::storeData(void* data, int length) {
	if (!file) return;


	if (!forWriting) {
		LOG_ERROR("File %s not opened for writing!", filePath);
		return;
	}

	fwrite(data, length, 1, file);

}


/**
 * Load a block of uncompressed data from the file.
 *
 * @param length The length of the block
 *
 * @return Buffer containing the block of data
 */
unsigned char* File::loadBlock(int length) {
	if (!file) return nullptr;


	unsigned char* buffer; buffer = (unsigned char*)malloc((length) * sizeof(unsigned char));

	int res = fread(buffer, 1, length, file);

	if (res != length)
		LOG_ERROR("Could not read whole block (%d of %d bytes read)", res, length);

	return buffer;

}


/**
 * Load a block of RLE compressed data from the file.
 *
 * @param length The length of the uncompressed block
 * @param checkSize Whether or not the RLE block is terminated
 *
 * @return Buffer containing the uncompressed data
 */
unsigned char* File::loadRLE(int length, bool checkSize) {
	if (!file) return nullptr;

	int size = 0;

	if (checkSize) {
		// Determine the offset that follows the block
		size = loadShort();
	}
	int start = tell(); unsigned char* buffer = (unsigned char*)malloc((length) * sizeof(unsigned char));

	int pos = 0;
	while (pos < length) {
		unsigned char code = loadChar();
		unsigned char amount = code & 127;

		if (code & 128) { // repeat
			unsigned char value = loadChar();

			if (pos + amount >= length) break;

			memset(buffer + pos, value, amount);
			pos += amount;
		}
		else if (amount) { // copy
			if (pos + amount >= length) break;

			fread(buffer + pos, 1, amount, file);
			pos += amount;
		}
		else { // end marker
			buffer[pos++] = loadChar();
			break;
		}
	}

	if (checkSize) {
		if (tell() != start + size)
			LOG_DEBUG("RLE block has incorrect size: %d vs. %d", tell() - start, size);

		seek(start + size, true);
	}
	else {
		LOG_MAX("RLE block was %d bytes long", tell() - start);
	}

	return buffer;

}


/**
 * Skip past a block of RLE compressed data in the file.
 */
void File::skipRLE() {
	if (!file) return;


	int next;

	next = fgetc(file);
	next += fgetc(file) << 8;

	fseek(file, next, SEEK_CUR);

}


/**
 * Load a block of LZ compressed data from the file.
 *
 * @param compressedLength The length of the compressed block
 * @param length The length of the uncompressed block
 *
 * @return Buffer containing the uncompressed data
 */
unsigned char* File::loadLZ(int compressedLength, int length) {
	if (!file) return nullptr;


	unsigned char* compressedBuffer;
	unsigned char* buffer;

	compressedBuffer = loadBlock(compressedLength); buffer = (unsigned char*)malloc((length) * sizeof(unsigned char));

	uncompress(buffer, (unsigned long int*) & length, compressedBuffer, compressedLength);

	free(compressedBuffer);

	return buffer;

}


/**
 * Load a terminated string from the file.
 *
 * @param maxSize maximum length of field in the file
 *
 * @return The new string
 */
char* File::loadTerminatedString(int maxSize) {
	if (!file) return nullptr;

	char* string;
	int length = loadChar();

	if (maxSize > 0 && length > maxSize) {
		LOG_WARN("Trimming oversized terminated string (%d > %d)", length, maxSize);
		length = maxSize;
	}

	if (length) {
		string = loadString(length);
	}
	else {
		string = (char*)malloc((1) * sizeof(char));
		string[0] = '\0';
	}

	// Skip until end of field
	if (maxSize > length) {
		seek(maxSize - length);
	}

	return string;
}


/**
 * Load a 8.3 file name from the file.
 *
 * @return The new string
 */
char* File::loadFileName() {
	if (!file) return nullptr;

	int length = 0; char* string = (char*)malloc((13) * sizeof(char));

	for (int i = 0; i < 9; i++) {
		string[i] = loadChar();

		if (string[i] == '.') {
			string[++i] = loadChar();
			string[++i] = loadChar();
			string[++i] = loadChar();
			i++;

			break;
		}

		length = i;
	}

	string[length] = 0;

	return string;
}



/**
 * Load a string with given length from the file.
 *
 * @return The new string
 */
char* File::loadString(int length) {
	if (!file) return nullptr; char* string = (char*)malloc((length + 1) * sizeof(char));
	int res = fread(string, 1, length, file);

	if (res != length)
		LOG_ERROR("Could not read whole string (%d of %d bytes read)", res, length);

	string[length] = '\0';

	return string;
}


/**
 * Load RLE compressed graphical data from the file.
 *
 * @param width The width of the image to load
 * @param height The height of the image to load
 * @param checkSize Whether or not the RLE block is terminated
 *
 * @return SDL surface containing the loaded image
 */
SDL_Surface* File::loadSurface(int width, int height, bool checkSize) {
	if (!file) return nullptr;


	SDL_Surface* surface;
	unsigned char* pixels;

	pixels = loadRLE(width * height, checkSize);

	surface = video.createSurface(pixels, width, height);

	free(pixels);

	return surface;

}


/**
 * Load a block of scrambled pixel data from the file.
 *
 * @param length The length of the block
 *
 * @return Buffer containing the de-scrambled data
 */
unsigned char* File::loadPixels(int length) {
	if (!file) return nullptr;


	unsigned char* pixels;
	unsigned char* sorted;
	int count; sorted = (unsigned char*)malloc((length) * sizeof(unsigned char));

	pixels = loadBlock(length);

	// Rearrange pixels in correct order
	for (count = 0; count < length; count++) {

		sorted[count] = pixels[(count >> 2) + ((count & 3) * (length >> 2))];

	}

	free(pixels);

	return sorted;

}


/**
 * Load a block of scrambled and masked pixel data from the file.
 *
 * @param length The length of the block
 * @param key The transparent pixel value
 *
 * @return Buffer containing the de-scrambled data
 */
unsigned char* File::loadPixels(int length, int key) {
	if (!file) return nullptr;


	unsigned char* pixels;
	unsigned char* sorted;
	unsigned char mask = 0;
	int count; sorted = (unsigned char*)malloc((length) * sizeof(unsigned char)); pixels = (unsigned char*)malloc((length) * sizeof(unsigned char));


	// Read the mask
	// Each mask pixel is either 0 or 1
	// Four pixels are packed into the lower end of each byte
	for (count = 0; count < length; count++) {

		if (!(count & 3)) mask = fgetc(file);
		pixels[count] = (mask >> (count & 3)) & 1;

	}

	// Pixels are loaded if the corresponding mask pixel is 1, otherwise
	// the transparent index is used. Pixels are scrambled, so the mask
	// has to be scrambled the same way.
	for (count = 0; count < length; count++) {

		sorted[(count >> 2) + ((count & 3) * (length >> 2))] = pixels[count];

	}

	// Read pixels according to the scrambled mask
	for (count = 0; count < length; count++) {

		// Use the transparent pixel
		pixels[count] = key;

		if (sorted[count] == 1) {

			// The unmasked portions are transparent, so no masked
			// portion should be transparent.
			while (pixels[count] == key) pixels[count] = fgetc(file);

		}

	}

	// Rearrange pixels in correct order
	for (count = 0; count < length; count++) {

		sorted[count] = pixels[(count >> 2) + ((count & 3) * (length >> 2))];

	}

	free(pixels);

	return sorted;

}


/**
 * Load a palette from the file.
 *
 * @param palette The palette to be filled with loaded colours
 * @param rle Whether or not the palette data is RLE-encoded
 */
void File::loadPalette(SDL_Color* palette, bool rle) {
	if (!file) return;


	unsigned char* buffer;
	int count;

	if (rle) buffer = loadRLE(MAX_PALETTE_COLORS * 3);
	else buffer = loadBlock(MAX_PALETTE_COLORS * 3);

	for (count = 0; count < MAX_PALETTE_COLORS; count++) {

		// Palette entries are 6-bit
		// Shift them upwards to 8-bit, and fill in the lower 2 bits
		palette[count].r = (buffer[count * 3] << 2) + (buffer[count * 3] >> 4);
		palette[count].g = (buffer[(count * 3) + 1] << 2) + (buffer[(count * 3) + 1] >> 4);
		palette[count].b = (buffer[(count * 3) + 2] << 2) + (buffer[(count * 3) + 2] >> 4);

	}

	free(buffer);

}


PathMgr::PathMgr() :
	paths(NULL), has_config(false), has_temp(false) {

};

PathMgr::~PathMgr() {
	delete paths;
};

bool PathMgr::add(char* newPath, int newPathType) {

	// Check for CWD
	/* Xbox: no getcwd -- CWD is always the XBE root (D:\) */
	if (!strlen(newPath)) {
		free(newPath);
		newPath = createString("D:\\");
	}

	// Append a directory separator if necessary
	if (newPath[strlen(newPath) - 1] != OJ_DIR_SEP) {
		char* tmp = createString(newPath, OJ_DIR_SEP_STR);
		free(newPath);
		newPath = tmp;
	}

	/* Xbox: skip _access() R_OK check -- just trust provided paths */

	// ignore, if already present
	if (has_config) newPathType &= ~PATH_TYPE_CONFIG;
	if (has_temp) newPathType &= ~PATH_TYPE_TEMP;

	/* Xbox: skip _access() W_OK check.
	   D:\ is read-only (DVD); T:\ and E:\ are writable.
	   Strip CONFIG/TEMP from D:\ based on drive letter instead. */
	if ((newPathType & (PATH_TYPE_CONFIG | PATH_TYPE_TEMP)) > 0) {
		if (newPath[0] == 'D' || newPath[0] == 'd') {
			newPathType &= ~PATH_TYPE_CONFIG;
			newPathType &= ~PATH_TYPE_TEMP;
		}
	}

	if (newPathType == PATH_TYPE_INVALID) {
		free(newPath);
		return false;
	}

	// we only need one directory for these
	if (newPathType & PATH_TYPE_CONFIG) has_config = true;
	if (newPathType & PATH_TYPE_TEMP) has_temp = true;

	// Finally add
	paths = new Path(paths, newPath, newPathType);

	return true;
}


/**
 * Create a new directory path object.
 *
 * @param newNext Next path
 * @param newPath The new path
 */
Path::Path(Path* newNext, char* newPath, int newPathType) {

	char pathInfo[10] = {};
	if (newPathType & PATH_TYPE_SYSTEM) strcat(pathInfo, "S");
	if (newPathType & PATH_TYPE_CONFIG) strcat(pathInfo, "C");
	if (newPathType & PATH_TYPE_GAME) strcat(pathInfo, "G");
	if (newPathType & PATH_TYPE_TEMP) strcat(pathInfo, "T");
	if (newPathType & PATH_TYPE_ANY) strcat(pathInfo, "A");

	LOG_DEBUG("Adding '%s' to the path list [%s]", newPath, pathInfo);

	next = newNext;
	path = newPath;
	pathType = newPathType;

}


/**
 * Delete the directory path object.
 */
Path::~Path() {

	if (next) delete next;
	free(path);

}

bool File::isOpen() const {
	return file != nullptr;
}