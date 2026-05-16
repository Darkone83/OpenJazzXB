/**
 * controls.cpp -- XbJazz Xbox controller mapping.
 * Remappable actions read from g_xbConfig at runtime.
 *
 * Fixed mappings (not remappable):
 *   D-pad / L-stick -> Move
 *   A               -> Jump / Swim / Confirm
 *   B               -> Back / Escape
 *   Start           -> In-game menu (C_PAUSE)
 *   Back            -> Escape
 *
 * Remappable via setup menu (stored in g_xbConfig.controls):
 *   Fire, FireAlt, JumpAlt, Change, Stats
 */

#include "controls.h"
#include "xb_input.h"
#include "xb_config.h"
#include "loop.h"
#include "OpenJazz.h"
#include <string.h>

Controls::Controls() {
    memset(controlstates, 0, sizeof(controlstates));
    memset(keys, 0, sizeof(keys));
    memset(buttons, 0, sizeof(buttons));
    memset(axes, 0, sizeof(axes));
    memset(hats, 0, sizeof(hats));
    cursorX = cursorY = 0;
    cursorPressed = cursorReleased = false;
    wheelUp = wheelDown = 0;
}

void Controls::init() {}
void Controls::deinit() {}

int Controls::update(SDL_Event* event, LoopType type) {
    (void)event; (void)type;
    return E_NONE;
}

void Controls::loop() {
    unsigned short btns = XbInputGetButtons();
    int m, i;

    for (i = 0; i < CONTROLS; i++) {
        if (controlstates[i].time && globalTicks > controlstates[i].time)
            controlstates[i].time = 0;
    }

    bool active[CONTROLS];
    memset(active, 0, sizeof(active));

    /* Fixed mappings */
    if (btns & XB_BTN_UP)    active[C_UP] = true;
    if (btns & XB_BTN_DOWN)  active[C_DOWN] = true;
    if (btns & XB_BTN_LEFT)  active[C_LEFT] = true;
    if (btns & XB_BTN_RIGHT) active[C_RIGHT] = true;
    if (btns & XB_BTN_A) {
        active[C_JUMP] = true;
        active[C_SWIM] = true;
        active[C_ENTER] = true;
    }
    if (btns & XB_BTN_B) { active[C_ESCAPE] = true; }
    if (btns & XB_BTN_BACK) { active[C_ESCAPE] = true; }
    if (btns & XB_BTN_START) { active[C_PAUSE] = true; }

    /* Remappable mappings from config */
    if (btns & g_xbConfig.controls.btnFire)    active[C_FIRE] = true;
    if (btns & g_xbConfig.controls.btnFireAlt) active[C_FIRE] = true;
    if (btns & g_xbConfig.controls.btnJumpAlt) {
        active[C_JUMP] = true;
        active[C_SWIM] = true;
    }
    if (btns & g_xbConfig.controls.btnChange)  active[C_CHANGE] = true;
    if (btns & g_xbConfig.controls.btnStats)   active[C_STATS] = true;

    for (i = 0; i < CONTROLS; i++) {
        controlstates[i].state = controlstates[i].time ? false : active[i];
    }
}

bool Controls::getState(int control) { return controlstates[control].state; }

bool Controls::release(int control) {
    if (!controlstates[control].state) return false;
    controlstates[control].time = globalTicks + T_KEY;
    controlstates[control].state = false;
    return true;
}

bool Controls::getCursor(int& x, int& y) { x = cursorX; y = cursorY; return false; }
bool Controls::wasCursorReleased() { return false; }

void Controls::setKey(int c, int k) { keys[c].key = k; }
void Controls::setButton(int c, int b) { buttons[c].button = b; }
void Controls::setAxis(int c, int a, bool d) { axes[c].axis = a; axes[c].direction = d; }
void Controls::setHat(int c, int h, int d) { hats[c].hat = h;  hats[c].direction = d; }

int  Controls::getKey(int c) { return keys[c].key; }
int  Controls::getButton(int c) { return buttons[c].button; }
int  Controls::getAxis(int c) { return axes[c].axis; }
int  Controls::getAxisDirection(int c) { return axes[c].direction ? 1 : -1; }
int  Controls::getHat(int c) { return hats[c].hat; }
int  Controls::getHatDirection(int c) { return hats[c].direction; }

void Controls::setCursor(int x, int y, bool p) { cursorX = x; cursorY = y; cursorPressed = p; }