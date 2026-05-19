/**
 * @file setupmenu.cpp
 * XbJazz -- Setup menus (Xbox port). Replaces original OJ setupmenu.cpp.
 * Stubs out PC-only menus (keyboard, joystick, video resize).
 * Adds Xbox-specific controls remapping and video mode selection.
 */

#include "menu.h"
#include "plasma.h"
#include "xb_config.h"
#include "xb_textentry.h"
#include "xb_net_glue.h"
#include "xb_netlog.h"
#include "game.h"
#include "gamemode.h"
#include "util.h"
#include <string.h>
#include "controls.h"
#include "font.h"
#include "video.h"
#include "sound.h"
#include "loop.h"
#include "setup.h"
#include "util.h"
#include "xb_config.h"
#include "xb_input.h"

extern void XbVideoApplyConfig(void);

/* -----------------------------------------------------------------------
   Stubs for OJ class methods we don't use on Xbox
   ----------------------------------------------------------------------- */
#ifndef NO_KEYBOARD_CFG
int SetupMenu::setupKeyboard() { return message("NOT AVAILABLE ON XBOX"); }
#endif
int SetupMenu::setupJoystick() { return message("NOT AVAILABLE ON XBOX"); }
int SetupMenu::setupVideo() { return message("NOT AVAILABLE ON XBOX"); }

/* -----------------------------------------------------------------------
   Button name helper
   ----------------------------------------------------------------------- */
static const char* btnName(unsigned short btn) {
    switch (btn) {
    case XB_BTN_A:     return "A";
    case XB_BTN_B:     return "B";
    case XB_BTN_X:     return "X";
    case XB_BTN_Y:     return "Y";
    case XB_BTN_LTRIG: return "L-Trigger";
    case XB_BTN_RTRIG: return "R-Trigger";
    case XB_BTN_WHITE: return "White";
    case XB_BTN_BLACK: return "Black";
    case XB_BTN_BACK:  return "Back";
    default:           return "?";
    }
}

static void xbRestoreMenuPalette() {
    int pi;
    for (pi = 0; pi < 16; pi++)
        menuPalette[pi] = plasmaMenuPalette[pi];
    video.setPalette(menuPalette);
}

/* Safe text baselines for 320x200 canvas.
 * The Xbox font draws taller than the old PC menu assumptions, so using
 * canvasH-10/canvasH with bottom alignment clips on real hardware.
 */
static const int XB_FOOTER_Y1 = 166;
static const int XB_FOOTER_Y2 = 182;

/* Compact text helper for the 320x200 Xbox setup screens.
 * Keeps long room/host strings from running into the edge/count column.
 */
static void xbMakeShortText(char* dst, int dstLen, const char* src, int maxChars, bool tail) {
    int len = 0;
    int si = 0;

    if (!dst || dstLen <= 0) return;
    dst[0] = '\0';
    if (!src) return;

    while (src[len]) len++;

    if (maxChars > dstLen - 1) maxChars = dstLen - 1;
    if (maxChars < 1) return;

    if (len <= maxChars) {
        while (src[si] && si < maxChars) { dst[si] = src[si]; si++; }
        dst[si] = '\0';
        return;
    }

    if (maxChars <= 3) {
        for (si = 0; si < maxChars; si++) dst[si] = src[si];
        dst[si] = '\0';
        return;
    }

    if (tail) {
        dst[0] = '.'; dst[1] = '.'; dst[2] = '.';
        si = len - (maxChars - 3);
        int di = 3;
        while (src[si] && di < maxChars) dst[di++] = src[si++];
        dst[di] = '\0';
    }
    else {
        int di = 0;
        while (di < maxChars - 3) { dst[di] = src[di]; di++; }
        dst[di++] = '.'; dst[di++] = '.'; dst[di++] = '.';
        dst[di] = '\0';
    }
}

static const unsigned short MAPPABLE_BTNS[] = {
    XB_BTN_A, XB_BTN_X, XB_BTN_Y,
    XB_BTN_LTRIG, XB_BTN_RTRIG,
    XB_BTN_WHITE, XB_BTN_BLACK
};
#define MAPPABLE_COUNT 7

/* -----------------------------------------------------------------------
   xbSetupControls -- remap gameplay buttons
   ----------------------------------------------------------------------- */
static int xbSetupControls() {

    const char* actionNames[5] = {
        "primary fire",
        "secondary fire",
        "alt jump / swim",
        "change weapon",
        "stats overlay"
    };
    unsigned short* actionBtns[5] = {
        &g_xbConfig.controls.btnFire,
        &g_xbConfig.controls.btnFireAlt,
        &g_xbConfig.controls.btnJumpAlt,
        &g_xbConfig.controls.btnChange,
        &g_xbConfig.controls.btnStats
    };
    int chosen = 0;
    bool waiting = false;
    Plasma plasma;

    { int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
    video.setPalette(menuPalette);

    while (true) {

        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;

        if (!waiting) {
            if (controls.release(C_ESCAPE)) { XbConfigSave(); return E_NONE; }
            if (controls.release(C_UP))     chosen = (chosen + 4) % 5;
            if (controls.release(C_DOWN))   chosen = (chosen + 1) % 5;
            if (controls.release(C_ENTER))  waiting = true;
        }
        else {
            unsigned short btns = XbInputGetButtons();
            int bi;
            for (bi = 0; bi < MAPPABLE_COUNT; bi++) {
                if (btns & MAPPABLE_BTNS[bi]) {
                    *actionBtns[chosen] = MAPPABLE_BTNS[bi];
                    waiting = false;
                    playConfirmSound();
                    break;
                }
            }
            if (!XbInputGetButtons()) {
                /* allow B to cancel only when no buttons held */
                if (controls.release(C_ESCAPE)) waiting = false;
            }
        }

        SDL_Delay(T_MENU_FRAME);
        plasma.draw();

        /* Title */
        fontmn2->showString("CONTROLS",
            canvasW >> 1, 8, alignX::Center);

        /* 2-column table: action | button */
        const int labelX = 12;
        const int valueX = (canvasW >> 1) + 20;
        const int rowH = 19;
        const int startY = 34;
        int i;

        for (i = 0; i < 5; i++) {
            int itemY = startY + i * rowH;
            if (i == chosen)
                video.drawRect(labelX - 2, itemY - 3,
                    canvasW - (labelX - 2) * 2, rowH + 2, 79, false);
            fontmn2->showString(actionNames[i], labelX, itemY);
            if (waiting && i == chosen)
                fontmn2->showString("press button...", valueX, itemY);
            else
                fontmn2->showString(btnName(*actionBtns[i]), valueX, itemY);
        }

        /* Footer hints -- keep clear of overscan/bottom clipping. */
        fontmn2->showString("a=jump  start=pause",
            3, XB_FOOTER_Y2, alignX::Left);
        fontmn2->showString("a=remap  b=back",
            canvasW - 3, XB_FOOTER_Y2, alignX::Right);
    }

    return E_NONE;
}

/* -----------------------------------------------------------------------
   xbSetupVideoXbox -- resolution, aspect, filter, scanlines
   ----------------------------------------------------------------------- */
static int xbSetupVideoXbox() {

    const char* modeNames[4] = { "auto",    "480p",    "720p",    "480i" };
    const char* aspectNames[4] = { "4:3",     "stretch", "pixel",   "fill" };
    const char* filterNames[3] = { "sharp",   "smooth",  "scale2x" };
    const char* scanNames[4] = { "off",     "light",   "medium",  "heavy" };

    int chosen = 0;
    Plasma plasma;

    { int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
    video.setPalette(menuPalette);

    while (true) {

        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
        if (controls.release(C_ESCAPE)) { XbConfigSave(); return E_NONE; }
        if (controls.release(C_UP))   chosen = (chosen + 3) % 4;
        if (controls.release(C_DOWN)) chosen = (chosen + 1) % 4;

        if (controls.release(C_LEFT)) {
            switch (chosen) {
            case 0: g_xbConfig.videoMode = (XbVideoMode)(((int)g_xbConfig.videoMode + 3) % 4); break;
            case 1: g_xbConfig.aspectMode = (XbAspectMode)(((int)g_xbConfig.aspectMode + 3) % 4); break;
            case 2: g_xbConfig.filterMode = (XbFilterMode)(((int)g_xbConfig.filterMode + 2) % 3); break;
            case 3: g_xbConfig.scanlines = (XbScanlines)(((int)g_xbConfig.scanlines + 3) % 4); break;
            }
            XbVideoApplyConfig(); playConfirmSound();
        }
        if (controls.release(C_RIGHT)) {
            switch (chosen) {
            case 0: g_xbConfig.videoMode = (XbVideoMode)(((int)g_xbConfig.videoMode + 1) % 4); break;
            case 1: g_xbConfig.aspectMode = (XbAspectMode)(((int)g_xbConfig.aspectMode + 1) % 4); break;
            case 2: g_xbConfig.filterMode = (XbFilterMode)(((int)g_xbConfig.filterMode + 1) % 3); break;
            case 3: g_xbConfig.scanlines = (XbScanlines)(((int)g_xbConfig.scanlines + 1) % 4); break;
            }
            XbVideoApplyConfig(); playConfirmSound();
        }

        SDL_Delay(T_MENU_FRAME);
        plasma.draw();

        fontmn2->showString("VIDEO OPTIONS",
            canvasW >> 1, (canvasH >> 1) - 56, alignX::Center);

        int labelX = (canvasW >> 1) - 88;
        int valueX = (canvasW >> 1) + 64;

        const char* rowLabels[4] = {
            "resolution < >", "aspect     < >",
            "filter     < >", "scanlines  < >"
        };
        const char* rowValues[4] = {
            modeNames[(int)g_xbConfig.videoMode],
            aspectNames[(int)g_xbConfig.aspectMode],
            filterNames[(int)g_xbConfig.filterMode],
            scanNames[(int)g_xbConfig.scanlines]
        };

        int i;
        for (i = 0; i < 4; i++) {
            int itemY = (canvasH >> 1) - 28 + i * 18;
            if (i == chosen)
                video.drawRect(24, itemY - 4, canvasW - 48, 20, 79, false);
            fontmn2->showString(rowLabels[i], labelX, itemY);
            fontmn2->showString(rowValues[i], valueX, itemY);
        }

        fontmn2->showString("resolution change needs restart",
            canvasW >> 1, (canvasH >> 1) + 44, alignX::Center);
        fontmn2->showString("b=back",
            canvasW - 3, XB_FOOTER_Y2, alignX::Right);
    }

    return E_NONE;
}

/* -----------------------------------------------------------------------
   setupAudio
   ----------------------------------------------------------------------- */
int SetupMenu::setupAudio() {

    int x, y;
    bool soundActive = false;
    Plasma plasma;

    video.setPalette(menuPalette);

    while (true) {

        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
        if (controls.release(C_ESCAPE)) return E_NONE;
        if (controls.release(C_ENTER))  return E_NONE;

        if (controls.getCursor(x, y)) {
            if ((x < 100) && (y >= canvasH - 12) && controls.wasCursorReleased()) return E_NONE;
            x -= (canvasW >> 1) + 48;
            y -= canvasH >> 1;
            if ((x >= 0) && (x < (MAX_VOLUME >> 1)) && (y >= 0) && (y < 11)) setMusicVolume(x << 1);
            if ((x >= 0) && (x < (MAX_VOLUME >> 1)) && (y >= 16) && (y < 27)) setSoundVolume(x << 1);
            if (controls.wasCursorReleased()) playConfirmSound();
        }

        if (controls.release(C_UP))   soundActive = !soundActive;
        if (controls.release(C_DOWN)) soundActive = !soundActive;

        if (controls.release(C_LEFT)) {
            if (soundActive) setSoundVolume(getSoundVolume() - 4);
            else             setMusicVolume(getMusicVolume() - 4);
            playConfirmSound();
        }
        if (controls.release(C_RIGHT)) {
            if (soundActive) setSoundVolume(getSoundVolume() + 4);
            else             setMusicVolume(getMusicVolume() + 4);
            playConfirmSound();
        }

        SDL_Delay(T_MENU_FRAME);
        plasma.draw();

        fontmn2->showString("AUDIO OPTIONS", canvasW >> 1, (canvasH >> 1) - 80, alignX::Center);

        if (!soundActive)
            video.drawRect(24, (canvasH >> 1) - 4, canvasW - 48, 18, 79, false);
        fontmn2->showString("music volume", (canvasW >> 1) - 88, canvasH >> 1);
        video.drawRect((canvasW >> 1) + 48, canvasH >> 1, getMusicVolume() >> 1, 11, 175);

        if (soundActive)
            video.drawRect(24, (canvasH >> 1) + 12, canvasW - 48, 18, 79, false);
        fontmn2->showString("effect volume", (canvasW >> 1) - 88, (canvasH >> 1) + 16);
        video.drawRect((canvasW >> 1) + 48, (canvasH >> 1) + 16, getSoundVolume() >> 1, 11, 175);

        showEscString();
    }

    return E_NONE;
}


/* -----------------------------------------------------------------------
   xbSetupNetwork -- player name and server address configuration
   ----------------------------------------------------------------------- */
static int xbSetupNetwork() {

    /* Split stored serverAddr into host and port strings */
    char nameBuf[16];
    char hostBuf[48];
    char portBuf[8];
    int  option = 0;
    int  ret;
    int  i;

    for (i = 0; i < 15; i++) nameBuf[i] = g_xbConfig.playerName[i];
    nameBuf[15] = '\0';
    if (nameBuf[0] == '\0') {
        nameBuf[0] = 'P'; nameBuf[1] = 'L'; nameBuf[2] = 'A';
        nameBuf[3] = 'Y'; nameBuf[4] = 'E'; nameBuf[5] = 'R'; nameBuf[6] = '\0';
    }

    /* Parse host:port from serverAddr */
    {
        int si = 0, hi = 0, pi = 0;
        int colonPos = -1;
        while (g_xbConfig.serverAddr[si] && si < 63) {
            if (g_xbConfig.serverAddr[si] == ':') colonPos = si;
            si++;
        }
        si = 0;
        if (colonPos >= 0) {
            while (si < colonPos && hi < 47) { hostBuf[hi++] = g_xbConfig.serverAddr[si++]; }
            si++; /* skip colon */
            while (g_xbConfig.serverAddr[si] && pi < 7) { portBuf[pi++] = g_xbConfig.serverAddr[si++]; }
        }
        else {
            while (g_xbConfig.serverAddr[si] && hi < 47) { hostBuf[hi++] = g_xbConfig.serverAddr[si++]; }
        }
        hostBuf[hi] = '\0';
        portBuf[pi] = '\0';
        if (portBuf[0] == '\0') { portBuf[0] = '1'; portBuf[1] = '0'; portBuf[2] = '0'; portBuf[3] = '5'; portBuf[4] = '2'; portBuf[5] = '\0'; }
    }

    Plasma plasma;
    { int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
    video.setPalette(menuPalette);

    while (true) {

        if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
        if (controls.release(C_ESCAPE)) break;

        SDL_Delay(T_MENU_FRAME);
        plasma.draw();

        fontmn2->showString("NETWORK SETUP", canvasW >> 1, 8, alignX::Center);

        /* Three larger rows: label over value.
         * Keep fontmn2 for both labels and values. The Xbox font already has
         * mixed-width glyphs, so changing only the value font makes names like
         * DARKONE83 look uneven.
         */
        const int rowH = 40;
        const int baseY = 31;
        const char* labels[3] = { "player name", "server host", "server port" };
        const char* vals[3] = { nameBuf, hostBuf, portBuf };

        for (i = 0; i < 3; i++) {
            char shown[36];
            int labelY = baseY + i * rowH;
            int valueY = labelY + 15;

            xbMakeShortText(shown, 36, vals[i][0] ? vals[i] : "---",
                (i == 1) ? 30 : 20, (i == 1));

            if (i == option)
                video.drawRect(8, labelY - 5, canvasW - 16, 36, 79, false);

            fontmn2->showString(labels[i], 14, labelY);
            fontmn2->showString(shown, 18, valueY);
        }

        fontmn2->showString("a=edit", 3, XB_FOOTER_Y1, alignX::Left);
        fontmn2->showString("up/dn=select", canvasW - 3, XB_FOOTER_Y1, alignX::Right);
        fontmn2->showString("start=save", 3, XB_FOOTER_Y2, alignX::Left);
        fontmn2->showString("b=back", canvasW - 3, XB_FOOTER_Y2, alignX::Right);

        if (controls.release(C_UP))   option = (option + 3 - 1) % 3;
        if (controls.release(C_DOWN)) option = (option + 1) % 3;

        if (controls.release(C_ENTER)) {
            if (option == 0)      ret = xbTextEntry("PLAYER NAME", nameBuf, 15, XB_ENTRY_NAME);
            else if (option == 1) ret = xbTextEntry("SERVER HOST", hostBuf, 47, XB_ENTRY_IP);
            else                  ret = xbTextEntry("SERVER PORT", portBuf, 7, XB_ENTRY_IP);
            if (ret == E_QUIT) return E_QUIT;
            video.setPalette(menuPalette);
        }

        if (controls.release(C_PAUSE)) {
            /* Rebuild host:port into serverAddr */
            int di = 0, si = 0;
            while (hostBuf[si] && di < 62) g_xbConfig.serverAddr[di++] = hostBuf[si++];
            if (portBuf[0]) {
                g_xbConfig.serverAddr[di++] = ':';
                si = 0;
                while (portBuf[si] && di < 63) g_xbConfig.serverAddr[di++] = portBuf[si++];
            }
            g_xbConfig.serverAddr[di] = '\0';
            for (i = 0; i < 15; i++) g_xbConfig.playerName[i] = nameBuf[i];
            g_xbConfig.playerName[15] = '\0';
            XbConfigSave();
            break;
        }
    }

    return E_NONE;
}


/* -----------------------------------------------------------------------
   xbNetLobby -- connect to relay, browse rooms, host or join, launch game
   ----------------------------------------------------------------------- */
static int xbNetLobby() {

    Plasma plasma;
    { int pi; for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi]; }
    video.setPalette(menuPalette);

    /* ── Connect (async, polled per frame -- XbDiag pattern) ─────────── */
    {
        const char* addr = g_xbConfig.serverAddr[0]
            ? g_xbConfig.serverAddr : "localhost";
        XbNetG_ConnectBegin(addr, g_xbConfig.playerName);

        bool done = false;
        while (!done) {
            if (loop(NORMAL_LOOP) == E_QUIT) { XbNetG_Disconnect(); return E_QUIT; }
            if (controls.release(C_ESCAPE)) { XbNetG_Disconnect(); return E_NONE; }
            SDL_Delay(T_MENU_FRAME);
            plasma.draw();
            fontmn2->showString("CONNECTING...", canvasW >> 1, (canvasH >> 1) - 8, alignX::Center);
            fontmn2->showString("b=cancel", canvasW - 3, canvasH - 8, alignX::Right);

            int cr = XbNetG_ConnectPoll();
            if (cr == 1) {
                done = true;
            }
            else if (cr == -1) {
                char errbuf[64]; errbuf[0] = 0;
                XbNetG_ConnectError(errbuf, 64);
                int et;
                for (et = 0; et < 90; et++) {
                    if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
                    SDL_Delay(T_MENU_FRAME);
                    plasma.draw();
                    fontmn2->showString("CONNECT FAILED", canvasW >> 1, (canvasH >> 1) - 8, alignX::Center);
                    fontmn2->showString(errbuf[0] ? errbuf : "check server addr",
                        canvasW >> 1, canvasH >> 1, alignX::Center);
                }
                return E_NONE;
            }
        }
    }

    /* ── Lobby room list ──────────────────────────────────────────────── */
    XbNetGlueState lobbyState;
    int i;
    for (i = 0; i < (int)sizeof(lobbyState); i++) ((char*)&lobbyState)[i] = 0;
    lobbyState.mySlot = -1;

    int chosen = 0;
    bool gotList = false;

    while (true) {

        if (loop(NORMAL_LOOP) == E_QUIT) { XbNetG_Disconnect(); return E_QUIT; }
        if (controls.release(C_ESCAPE)) { XbNetG_Disconnect(); return E_NONE; }

        int pr = XbNetG_LobbyPoll(&lobbyState);
        if (pr < 0) {
            /* Disconnected */
            int t;
            for (t = 0; t < 60; t++) {
                if (loop(NORMAL_LOOP) == E_QUIT) return E_QUIT;
                SDL_Delay(T_MENU_FRAME);
                plasma.draw();
                fontmn2->showString("DISCONNECTED", canvasW >> 1, canvasH >> 1, alignX::Center);
            }
            return E_NONE;
        }
        if (pr > 0) gotList = true;

        /* Up/Down navigate room list */
        int totalOpts = lobbyState.nRooms + 1;  /* rooms + NEW ROOM */
        if (controls.release(C_UP))   chosen = (chosen + totalOpts - 1) % totalOpts;
        if (controls.release(C_DOWN)) chosen = (chosen + 1) % totalOpts;

        /* A = join/create */
        if (controls.release(C_ENTER) && gotList) {
            if (chosen < lobbyState.nRooms) {
                /* Join existing room */
                XbNetG_JoinRoom(lobbyState.rooms[chosen].room_id);
            }
            else {
                /* Create new room */
                XbNetG_JoinRoom(0);
            }
            /* Fall through to room screen below */
            break;
        }

        SDL_Delay(T_MENU_FRAME);
        plasma.draw();
        fontmn2->showString("NETWORK LOBBY", canvasW >> 1, 8, alignX::Center);

        if (!gotList) {
            fontmn2->showString("searching...", canvasW >> 1, canvasH >> 1, alignX::Center);
        }
        else {
            const int rowH2 = 18;
            const int baseY2 = 30;
            const int maxRows = 7;
            int startRoom = 0;

            if (chosen >= maxRows)
                startRoom = chosen - maxRows + 1;
            if (startRoom > lobbyState.nRooms)
                startRoom = lobbyState.nRooms;

            for (i = startRoom; i < lobbyState.nRooms && i < startRoom + maxRows; i++) {
                char shown[28];
                int iy = baseY2 + (i - startRoom) * rowH2;

                xbMakeShortText(shown, 28, lobbyState.rooms[i].hostName, 22, false);

                if (i == chosen)
                    video.drawRect(8, iy - 3, canvasW - 16, rowH2 + 1, 79, false);

                fontmn2->showString(shown, 12, iy);

                char cnt[4] = {
                    (char)('0' + lobbyState.rooms[i].nPlayers), '/',
                    (char)('0' + lobbyState.rooms[i].maxPlayers), '\0'
                };
                fontmn2->showString(cnt, canvasW - 12, iy, alignX::Right);
            }

            int createY = baseY2 + (lobbyState.nRooms - startRoom) * rowH2;
            if (createY > baseY2 + maxRows * rowH2)
                createY = baseY2 + maxRows * rowH2;

            if (chosen == lobbyState.nRooms)
                video.drawRect(8, createY - 3, canvasW - 16, rowH2 + 1, 79, false);
            fontmn2->showString("+ new room", 12, createY);
        }

        fontmn2->showString("a=join", 3, XB_FOOTER_Y1, alignX::Left);
        fontmn2->showString("up/dn=select", canvasW - 3, XB_FOOTER_Y1, alignX::Right);
        fontmn2->showString("b=back", canvasW - 3, XB_FOOTER_Y2, alignX::Right);
    }

    /* ── Room screen ──────────────────────────────────────────────────── */
    for (i = 0; i < (int)sizeof(lobbyState); i++) ((char*)&lobbyState)[i] = 0;

    int episode = 0;
    int levelNum = 0;
    int diffChoice = 1;
    int pickRow = 0;
    const char* diffNames[4] = { "easy", "normal", "hard", "turbo" };
    bool isHost = false;

    /* Poll once to get room info and find out our slot */
    {
        int t;
        for (t = 0; t < 100; t++) {
            if (loop(NORMAL_LOOP) == E_QUIT) { XbNetG_Disconnect(); return E_QUIT; }
            SDL_Delay(T_MENU_FRAME);
            int rr = XbNetG_LobbyPoll(&lobbyState);
            if (rr > 0 && lobbyState.mySlot >= 0) break;
        }
        isHost = (lobbyState.mySlot == 0);
    }

    while (true) {

        if (loop(NORMAL_LOOP) == E_QUIT) { XbNetG_Disconnect(); return E_QUIT; }
        if (controls.release(C_ESCAPE)) { XbNetG_Disconnect(); return E_NONE; }

        int rr = XbNetG_LobbyPoll(&lobbyState);
        if (rr < 0) { XbNetG_Disconnect(); xbRestoreMenuPalette(); return E_NONE; }

        if (rr == 2) {
            /* MAPSEL received -- client launches game */
            char lvlFile[16];
            XbNetG_GetClientLevelFile(lvlFile, 16);

            /* Copy player name into OJ setup */
            char* cn = setup.characterName;
            if (cn) {
                int ni;
                for (ni = 0; ni < 15 && g_xbConfig.playerName[ni]; ni++)
                    cn[ni] = g_xbConfig.playerName[ni];
                cn[ni] = '\0';
            }

            XbNetLog_Open(0);
            XbNetLog_Enable(1);
            XbNetLog_Write("Client launching ClientGame");

            int gameRet = E_NONE;

            try {
                ClientGame* g = new ClientGame(netAddress);
                gameRet = g->play();
                delete g;
            }
            catch (int e) {
                gameRet = e;
            }

            XbNetG_Disconnect();
            xbRestoreMenuPalette();

            if (gameRet == E_QUIT)
                return E_QUIT;

            if (gameRet == E_N_DISCONNECT) {
                int dt;
                for (dt = 0; dt < 90; dt++) {
                    if (loop(NORMAL_LOOP) == E_QUIT)
                        return E_QUIT;

                    SDL_Delay(T_MENU_FRAME);
                    plasma.draw();
                    fontmn2->showString("HOST DISCONNECTED",
                        canvasW >> 1,
                        canvasH >> 1,
                        alignX::Center);
                }

                return E_NONE;
            }

            return E_NONE;
        }

        if (isHost) {

            /* Host: episode/level/difficulty picker */
            if (controls.release(C_UP))   pickRow = (pickRow + 2) % 3;
            if (controls.release(C_DOWN)) pickRow = (pickRow + 1) % 3;
            if (controls.release(C_LEFT)) {
                if (pickRow == 0) episode = (episode + 9) % 10;
                if (pickRow == 1) levelNum = (levelNum + 9) % 10;
                if (pickRow == 2) diffChoice = (diffChoice + 3) % 4;
            }
            if (controls.release(C_RIGHT)) {
                if (pickRow == 0) episode = (episode + 1) % 10;
                if (pickRow == 1) levelNum = (levelNum + 1) % 10;
                if (pickRow == 2) diffChoice = (diffChoice + 1) % 4;
            }

            /* A = start game */
            if (controls.release(C_ENTER) && lobbyState.nPlayers == 2) {
                int world = episode + 1;
                char lvlFile[12];
                lvlFile[0] = 'L'; lvlFile[1] = 'E'; lvlFile[2] = 'V';
                lvlFile[3] = 'E'; lvlFile[4] = 'L';
                lvlFile[5] = '0' + levelNum;
                lvlFile[6] = '.';
                lvlFile[7] = '0' + (world / 100) % 10;
                lvlFile[8] = '0' + (world / 10) % 10;
                lvlFile[9] = '0' + world % 10;
                lvlFile[10] = '\0';

                XbNetG_SendMapsel(lvlFile, (unsigned char)diffChoice);
                XbNetG_SetExpectedClients(1);
                XbNetLog_Open(1);
                XbNetLog_Enable(1);
                XbNetLog_Write("Host launching ServerGame");

                /* Copy player name into OJ setup */
                char* cn = setup.characterName;
                if (cn) {
                    int ni;
                    for (ni = 0; ni < 15 && g_xbConfig.playerName[ni]; ni++)
                        cn[ni] = g_xbConfig.playerName[ni];
                    cn[ni] = '\0';
                }

                try {
                    char* lvlCopy = createString(lvlFile);
                    ServerGame* sg = new ServerGame(
                        M_COOP, lvlCopy,
                        (difficultyType)diffChoice);
                    int gret = static_cast<Game*>(sg)->play();
                    delete sg;
                    delete[] lvlCopy;
                    if (gret == E_QUIT) { XbNetG_Disconnect(); return E_QUIT; }
                }
                catch (int) {}

                XbNetG_Disconnect();
                xbRestoreMenuPalette();
                return E_NONE;
            }
        }

        SDL_Delay(T_MENU_FRAME);
        plasma.draw();
        fontmn2->showString("ROOM", canvasW >> 1, 6, alignX::Center);

        /* Player list -- compact rows with room count kept away from names. */
        for (i = 0; i < lobbyState.nPlayers && i < 2; i++) {
            char shown[24];
            int py = 32 + i * 17;
            const char* role = (i == 0) ? "host" : "client";

            xbMakeShortText(shown, 24, lobbyState.players[i], 18, false);

            fontmn2->showString(role, 12, py);
            fontmn2->showString(shown, 58, py);
        }
        if (lobbyState.nPlayers < 2)
            fontmn2->showString("waiting...", 58, 32 + lobbyState.nPlayers * 17);

        {
            char cnt[4] = {
                (char)('0' + lobbyState.nPlayers), '/', '2', '\0'
            };
            fontmn2->showString(cnt, canvasW - 12, 32, alignX::Right);
        }

        if (isHost) {
            /* Picker rows */
            const int pickY = 78;
            const int pRowH = 21;
            const char* pickLabels[3] = { "episode", "level", "difficulty" };
            char epStr[4] = { (char)('0' + episode),    '\0' };
            char lvStr[4] = { (char)('0' + levelNum),   '\0' };
            for (i = 0; i < 3; i++) {
                int ry = pickY + i * pRowH;
                if (i == pickRow)
                    video.drawRect(8, ry - 4, canvasW - 16, pRowH + 1, 79, false);
                fontmn2->showString(pickLabels[i], 16, ry);
                const char* val = (i == 0) ? epStr : (i == 1) ? lvStr : diffNames[diffChoice];
                fontmn2->showString(val, canvasW - 16, ry, alignX::Right);
            }
            /* Footer */
            if (lobbyState.nPlayers == 2) {
                fontmn2->showString("a=start", 3, XB_FOOTER_Y1, alignX::Left);
            }
            else {
                fontmn2->showString("waiting for player", 3, XB_FOOTER_Y1, alignX::Left);
            }
            fontmn2->showString("lt/rt=change", canvasW - 3, XB_FOOTER_Y1, alignX::Right);
        }
        else {
            fontmn2->showString("waiting for host", canvasW >> 1, canvasH >> 1, alignX::Center);
        }
        fontmn2->showString("b=leave", canvasW - 3, XB_FOOTER_Y2, alignX::Right);
    }
}

/* -----------------------------------------------------------------------
   setupMain -- top-level setup menu
   ----------------------------------------------------------------------- */
int SetupMenu::setupMain() {

    const char* options[5] = { "controls", "video", "audio", "gameplay", "network" };
    const char* setupModsOff[4] = {
        "slow motion: off", "extra items: take", "bird limit: one", "hud style: classic"
    };
    const char* setupModsOn[4] = {
        "slow motion: on",  "extra items: leave","bird limit: no",   "hud style: old fps"
    };
    const char* setupMods[4];
    int ret, option, suboption;

    option = 0;

    setupMods[0] = setup.slowMotion ? setupModsOn[0] : setupModsOff[0];
    setupMods[1] = setup.leaveUnneeded ? setupModsOn[1] : setupModsOff[1];
    setupMods[2] = setup.manyBirds ? setupModsOn[2] : setupModsOff[2];
    setupMods[3] = (setup.hudStyle == hudType::FPS) ? setupModsOn[3] : setupModsOff[3];

    /* Restore plasma colours from main menu into menuPalette */
    {
        int pi;
        for (pi = 0; pi < 16; pi++) menuPalette[pi] = plasmaMenuPalette[pi];
    }
    video.setPalette(menuPalette);

    while (true) {

        ret = generic("SETUP OPTIONS", options, 5, option);

        if (ret == E_RETURN) return E_NONE;
        if (ret < 0) return ret;

        switch (option) {

        case 0:
            if (xbSetupControls() == E_QUIT) return E_QUIT;
            break;

        case 1:
            if (xbSetupVideoXbox() == E_QUIT) return E_QUIT;
            break;

        case 2:
            if (setupAudio() == E_QUIT) return E_QUIT;
            break;

        case 3:
            suboption = 0;
            while (true) {
                ret = generic("GAME OPTIONS", setupMods, 4, suboption);
                if (ret == E_QUIT) return E_QUIT;
                if (ret < 0) break;
                setupMods[suboption] = (setupMods[suboption] == setupModsOff[suboption])
                    ? setupModsOn[suboption] : setupModsOff[suboption];
                setup.slowMotion = (setupMods[0] == setupModsOn[0]);
                setup.leaveUnneeded = (setupMods[1] == setupModsOn[1]);
                setup.manyBirds = (setupMods[2] == setupModsOn[2]);
                setup.hudStyle = (setupMods[3] == setupModsOn[3]) ? hudType::FPS : hudType::Classic;
            }
            break;

        case 4: {
            const char* netOpts[2] = { "setup", "join game" };
            int netSub = 0;
            while (true) {
                ret = generic("NETWORK", netOpts, 2, netSub);
                if (ret == E_QUIT) return E_QUIT;
                if (ret < 0) break;
                if (netSub == 0) {
                    if (xbSetupNetwork() == E_QUIT) return E_QUIT;
                }
                else {
                    if (xbNetLobby() == E_QUIT) return E_QUIT;
                }
            }
            break;
        }
        }
    }

    return E_NONE;
}