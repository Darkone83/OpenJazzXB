/**
 * controls.h
 * XbJazz port -- interface identical to original.
 * xb_sdl_compat.h included directly to guarantee SDL_Event is always defined,
 * regardless of what paletteeffects.h or the include chain does.
 *
 * Original copyright:
 * Copyright (c) 2005-2012 AJ Thomson
 * Copyright (c) 2015-2026 Carsten Teibes
 */

#ifndef OJ_CONTROLS_H
#define OJ_CONTROLS_H

#include "xb_sdl_compat.h"   /* SDL_Event, SDL_GetTicks -- must come first */
#include "loop.h"
#include "OpenJazz.h"

 /* -----------------------------------------------------------------------
    Control indices (unchanged from original)
    ----------------------------------------------------------------------- */
#define C_UP       0
#define C_DOWN     1
#define C_LEFT     2
#define C_RIGHT    3
#define C_JUMP     4
#define C_SWIM     5
#define C_FIRE     6
#define C_CHANGE   7
#define C_ENTER    8
#define C_ESCAPE   9
#define C_BLASTER 10
#define C_TOASTER 11
#define C_MISSILE 12
#define C_BOUNCER 13
#define C_TNT     14
#define C_STATS   15
#define C_PAUSE   16
#define C_YES     17
#define C_NO      18
#define CONTROLS  19

#define T_KEY     200

    /* -----------------------------------------------------------------------
       Controls class
       ----------------------------------------------------------------------- */
class Controls {

private:
    typedef struct {
        int  key;
        bool pressed;
    } Keys;

    typedef struct {
        int  button;
        bool pressed;
    } Buttons;

    typedef struct {
        int  axis;
        bool direction;
        bool pressed;
    } Axes;

    typedef struct {
        int  hat;
        int  direction;
        bool pressed;
    } Hats;

    typedef struct {
        unsigned int time;
        bool         state;
    } ControlState;

    Keys         keys[CONTROLS];
    Buttons      buttons[CONTROLS];
    Axes         axes[CONTROLS];
    Hats         hats[CONTROLS];
    ControlState controlstates[CONTROLS];

    int  cursorX, cursorY;
    bool cursorPressed, cursorReleased;
    int  wheelUp, wheelDown;

    void setCursor(int x, int y, bool pressed);

public:
    Controls();

    void init();
    void deinit();

    void setKey(int control, int key);
    void setButton(int control, int button);
    void setAxis(int control, int axis, bool direction);
    void setHat(int control, int hat, int direction);
    int  getKey(int control);
    int  getButton(int control);
    int  getAxis(int control);
    int  getAxisDirection(int control);
    int  getHat(int control);
    int  getHatDirection(int control);

    int  update(SDL_Event* event, LoopType type);
    void loop();

    bool getState(int control);
    bool release(int control);
    bool getCursor(int& x, int& y);
    bool wasCursorReleased();
};

EXTERN Controls controls;

#endif /* OJ_CONTROLS_H */