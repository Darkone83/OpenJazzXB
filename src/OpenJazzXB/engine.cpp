/**
 * engine.cpp
 * XbJazz -- OpenJazz engine lifecycle functions.
 *
 * Contains startUp(), shutDown(), and play() extracted from the original
 * OpenJazz main.cpp, with SDL_GetTicks() / SDL_Delay() resolving to
 * our Xbox shims.
 *
 * DO NOT put #define EXTERN here -- that lives in main.cpp.
 *
 * Original copyright:
 * Copyright (c) 2005-2017 AJ Thomson
 * Copyright (c) 2015-2026 Carsten Teibes
 */

#include <excpt.h>    /* EXCEPTION_EXECUTE_HANDLER */
#include "game.h"
#include "controls.h"
#include "file.h"
#include "font.h"
#include "video.h"
#include "network.h"
#include "sound.h"
#include "jj1level.h"
#include "menu.h"
#include "player.h"
#include "jj1scene.h"
#include "loop.h"
#include "setup.h"
#include "util.h"
#include "log.h"
#include "platforms.h"
#include "version.h"

#define PI 3.141592f

 /**
  * startUp
  *
  * Establishes game paths, loads config, creates the canvas, loads fonts.
  * argv0 and paths are NULL / 0 on Xbox -- paths come from AddGamePaths().
  */
void startUp(const char* argv0, int pathCount, char* paths[]) {

    File* file;
    unsigned char* pixels = nullptr;
    SetupOptions   config;

    /* Determine paths */
    platform->AddGamePaths();

    for (int i = 0; i < pathCount; i++)
        gamePaths.add(createString(paths[i]), PATH_TYPE_GAME);

    if (argv0) {
        int i = (int)strlen(argv0) - 1;
        while ((argv0[i] != OJ_DIR_SEP) && (i > 0)) i--;
        if (i > 0) {
            char* dir = createString(argv0);
            dir[i + 1] = '\0';
            gamePaths.add(dir, PATH_TYPE_SYSTEM | PATH_TYPE_GAME);
        }
    }

    /* Xbox: CWD-based path not needed -- D:\ already added by AddGamePaths() */

    netAddress = createString(NET_ADDRESS);

    /* Xbox: skip setup.load() -- always use defaults in RAM.
     * openjazz.cfg does not exist on first boot; bypassing avoids
     * file I/O during startup entirely. save() is also a no-op. */
    config.valid = true;
    config.videoWidth = DEFAULT_SCREEN_WIDTH;
    config.videoHeight = DEFAULT_SCREEN_HEIGHT;
    config.videoScale = MIN_SCALE;
    config.fullScreen = true;
    config.scaleMethod = scalerType::None;

    canvas = nullptr;
    if (!video.init(config)) {
        LOG_FATAL("video.init() failed");
        for (;;) { SDL_Delay(500); }
    }

    controls.init();

    openAudio();

    file = new File("PANEL.000", PATH_TYPE_GAME);
    if (!file->isOpen()) {
        LOG_FATAL("PANEL.000 not found -- check game data at D:\\");
        for (;;) { SDL_Delay(500); }
    }

    pixels = file->loadRLE(46272);
    delete file;

    panelBigFont = nullptr;
    panelSmallFont = nullptr;
    font2 = nullptr;
    fontbig = nullptr;
    fontiny = nullptr;
    fontmn1 = nullptr;
    fontmn2 = nullptr;

    panelBigFont = new Font(pixels + (40 * 320), true);
    panelSmallFont = new Font(pixels + (48 * 320), false);
    font2 = new Font("FONT2.0FN");
    fontbig = new Font("FONTBIG.0FN");
    fontiny = new Font("FONTINY.0FN");
    fontmn1 = new Font("FONTMN1.0FN");
    fontmn2 = new Font("FONTMN2.0FN");

    free(pixels);

    globalTicks = SDL_GetTicks() - 20;

    for (int i = 0; i < 1024; i++) {
        /* Inline x87 FPU conversion -- avoids __ftol2_sse which is not
           in the RXDK runtime.  Each __asm on its own line per RXDK convention. */
        float fval = sinf(2 * PI * float(i) / 1024.0f) * 1024.0f;
        int   ival = 0;
        __asm fld  fval
        __asm fistp ival
        sinLut[i] = (fixed)ival;
    }

    net = new Network();
    level = nullptr;
}


/**
 * shutDown
 */
void shutDown() {
    delete net;
    delete panelBigFont;
    delete panelSmallFont;
    delete font2;
    delete fontbig;
    delete fontiny;
    delete fontmn1;
    delete fontmn2;
    closeAudio();
    controls.deinit();
    video.deinit();
    /* Xbox: skip setup.save() -- no writable config path */
}


/**
 * play
 */
int play() {

    MainMenu* mainMenu = nullptr;
    JJ1Scene* scene = nullptr;

    playMusic("MENUSNG.PSM");

    scene = new JJ1Scene("STARTUP.0SC");

    if (scene->play() == E_QUIT) {
        delete scene;
        return E_NONE;
    }
    delete scene;

    mainMenu = new MainMenu();

    int menuRet = mainMenu->main();
    delete mainMenu;

    if (menuRet == E_QUIT) return E_NONE;

    /* E_RETURN = EXIT GAME selected -- play ending scene if present,
     * then return. On Xbox this leads to the exit handler. */
    scene = new JJ1Scene("END.0SC");

    scene->play();
    delete scene;

    return E_NONE;
}