#pragma once
/**
 * xb_platform.h
 * XbJazz — Xbox platform implementation.
 */

#ifndef XB_PLATFORM_H
#define XB_PLATFORM_H

#include "platform_interface.h"

class XboxPlatform : public IPlatform {
public:
    XboxPlatform() {}
    ~XboxPlatform() {}

    void AddGamePaths();
    void ErrorNoDatafiles();
    bool WantsExit();

    /* Network stubs — no Xbox XLink Kai support planned */
    void NetInit() {}
    void NetExit() {}
    bool NetHasConsole() { return false; }

    /* Text input — no keyboard on Xbox; return false to signal no input */
    bool InputIP(char*&, char*&) { return false; }
    bool InputString(const char*, char*&, char*&) { return false; }
};

#endif /* XB_PLATFORM_H */