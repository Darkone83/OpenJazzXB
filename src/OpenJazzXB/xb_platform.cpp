/**
 * xb_platform.cpp
 * XbJazz -- Xbox platform implementation.
 *
 * Game data is always at the XBE root (D:\). One path, that's it.
 */

extern "C" {
    __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);
    __declspec(dllimport) void __stdcall Sleep(unsigned long);
}

#include "xb_platform.h"
#include "file.h"
#include "util.h"
#include "log.h"

extern void xbox_detect_fs(void);

void XboxPlatform::AddGamePaths() {
    xbox_detect_fs();
    gamePaths.add(createString("D:\\"), PATH_TYPE_GAME | PATH_TYPE_SYSTEM
        | PATH_TYPE_CONFIG | PATH_TYPE_TEMP);
}

void XboxPlatform::ErrorNoDatafiles() {
    OutputDebugStringA("XbJazz: FATAL -- game data missing from D:\\\n");
    for (;;) { Sleep(1000); }
}

bool XboxPlatform::WantsExit() { return false; }