/**
 * main.cpp
 * XbJazz -- RXDK entry point.
 *
 * NO <xtl.h> here (STL conflict). D3D lives in xb_d3d_init.cpp.
 * D3D device created in xb_d3d_init.cpp (<xtl.h>-only TU).
 *
 * COLOUR MAP (watch xemu screen):
 *   BLUE   -- D3D init done
 *   RED    -- entering startUp()
 *   ORANGE -- after setup.load() / video.init() returned true
 *   YELLOW -- after openAudio()
 *   GREEN  -- after PANEL.000 + fonts
 *   MAGENTA-- startUp() returned, entering play()
 *   If colour stays solid the NEXT step is where it hangs.
 */

extern "C" {
    __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
    typedef unsigned long DWORD;
}

extern void XbConfigLoad(void);
extern void XbD3DInit(void);
extern "C" void XbInputInit(void);   /* input.h -- <xtl.h>-only TU */
#define EXTERN

#include "xb_platform.h"
#include "controls.h"
#include "video.h"
#include "sound.h"
#include "network.h"
#include "file.h"
#include "font.h"
#include "player.h"
#include "jj1level.h"
#include "loop.h"
#include "setup.h"
#include "util.h"
#include "log.h"
#include "platforms.h"
#include "version.h"
#include "xb_exit.h"
#include "xb_splash.h"
#include "paletteeffects.h"

void startUp(const char* argv0, int pathCount, char* paths[]);
void shutDown();
int  play();

/* -----------------------------------------------------------------------
   loop
   ----------------------------------------------------------------------- */
int loop(LoopType type, PaletteEffect* paletteEffects, bool effectsStopped) {
    unsigned int prevTicks = globalTicks;
    globalTicks = SDL_GetTicks();
    if (globalTicks - prevTicks < 4) {
        SDL_Delay(4 + prevTicks - globalTicks);
        globalTicks = SDL_GetTicks();
    }
    video.flip(globalTicks - prevTicks, paletteEffects, effectsStopped);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return E_QUIT;
        int ret = controls.update(&event, type);
        if (ret != E_NONE) return ret;
        video.update(&event);
    }
    controls.loop();
    if (platform->WantsExit()) return E_QUIT;
    return E_NONE;
}

/* -----------------------------------------------------------------------
   main
   ----------------------------------------------------------------------- */
void __cdecl main() {
    XbConfigLoad();  /* must run before XbD3DInit so video mode is known */
    XbD3DInit();                        /* BLUE  flash on success */
    XbJazzShowSplashes();               /* RXDK + Darkone83 splash */
    XbInputInit();   /* open Xbox controllers */

    platform = new XboxPlatform();
    LOG_INFO("XbJazz: platform ready");

    startUp(nullptr, 0, nullptr);

    LOG_INFO("XbJazz: entering play()");
    play();

    shutDown();
    delete platform;
    platform = nullptr;
    XbReturnToDashboard();
}