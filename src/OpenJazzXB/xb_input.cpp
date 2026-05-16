/**
 * xb_input.cpp
 * XbJazz -- Xbox controller → SDL event bridge.
 *
 * ISOLATION RULE: <xtl.h> only from Xbox side. OJ SDL types come from
 * xb_sdl_compat.h which has no platform headers.
 *
 * Uses XbTyrian's proven input.h/input.cpp for raw XInput polling.
 * Generates SDL_KEYDOWN / SDL_KEYUP events for OJ's Controls::update().
 *
 * OJ JJ1 default key bindings (controls.cpp):
 *   C_UP     = SDLK_UP       C_DOWN   = SDLK_DOWN
 *   C_LEFT   = SDLK_LEFT     C_RIGHT  = SDLK_RIGHT
 *   C_JUMP   = SDLK_SPACE    C_SWIM   = SDLK_SPACE
 *   C_FIRE   = SDLK_LALT     C_CHANGE = SDLK_RCTRL
 *   C_ENTER  = SDLK_RETURN   C_ESCAPE = SDLK_ESCAPE
 *   C_PAUSE  = SDLK_P        C_STATS  = SDLK_F9
 */

#include <xtl.h>
#include "input.h"      /* XbTyrian XInput layer -- <xtl.h> already included */
#include "xb_sdl_compat.h"
#include "xb_input.h"   /* extern "C" linkage for XbInputInit / XbInputPollEvent */

 /* -----------------------------------------------------------------------
    Button -> SDLKey mapping  (matches OJ JJ1 defaults)
    ----------------------------------------------------------------------- */
#define STICK_THRESHOLD 16000

static const struct { WORD btn; int sdlk; } s_map[] = {
    { BTN_DPAD_UP,    SDLK_UP      },
    { BTN_DPAD_DOWN,  SDLK_DOWN    },
    { BTN_DPAD_LEFT,  SDLK_LEFT    },
    { BTN_DPAD_RIGHT, SDLK_RIGHT   },
    { BTN_A,          SDLK_SPACE   },  /* jump */
    { BTN_B,          SDLK_ESCAPE  },  /* back / escape */
    { BTN_X,          SDLK_LALT    },  /* fire */
    { BTN_Y,          SDLK_RCTRL   },  /* change weapon */
    { BTN_START,      SDLK_RETURN  },  /* enter / confirm */
    { BTN_BACK,       SDLK_ESCAPE  },  /* escape */
    { BTN_RTRIG,      SDLK_SPACE   },  /* jump (alt) */
    { BTN_LTRIG,      SDLK_LALT    },  /* fire (alt) */
    { BTN_BLACK,      SDLK_F9      },  /* stats */
    { BTN_WHITE,      SDLK_p       },  /* pause */
};
#define MAP_COUNT (sizeof(s_map)/sizeof(s_map[0]))

/* -----------------------------------------------------------------------
   Tiny event queue -- holds pending KEYDOWN/KEYUP events
   ----------------------------------------------------------------------- */
#define QUEUE_MAX 32
static SDL_Event s_queue[QUEUE_MAX];
static int s_qHead = 0, s_qTail = 0;

static void enqueue(int type, int sdlk) {
    int next = (s_qTail + 1) % QUEUE_MAX;
    if (next == s_qHead) return;   /* full, drop */
    SDL_Event e;
    e.type = (Uint32)type;
    e.key.type = (Uint32)type;
    e.key.state = (type == SDL_KEYDOWN) ? 1 : 0;
    e.key.keysym.sym = (SDLKey)sdlk;
    e.key.keysym.mod = KMOD_NONE;
    e.key.keysym.scancode = 0;
    s_queue[s_qTail] = e;
    s_qTail = next;
}

/* -----------------------------------------------------------------------
   State
   ----------------------------------------------------------------------- */
static WORD  s_prevButtons = 0;
static bool  s_prevStickUp = false;
static bool  s_prevStickDown = false;
static bool  s_prevStickLeft = false;
static bool  s_prevStickRight = false;
static bool  s_init = false;

/* -----------------------------------------------------------------------
   XbInputInit
   ----------------------------------------------------------------------- */
void XbInputInit(void) {
    InitInput();
    PumpInput();
    s_prevButtons = 0;
    s_qHead = s_qTail = 0;
    s_init = true;
}

/* -----------------------------------------------------------------------
   XbInputPollEvent
   Drain controller state into queue on each call, then return one event.
   ----------------------------------------------------------------------- */
int XbInputPollEvent(SDL_Event* e) {
    if (!s_init) return 0;

    PumpInput();

    WORD buttons = GetButtons();

    /* Left stick -> virtual d-pad */
    int lx = 0, ly = 0, rx = 0, ry = 0;
    GetSticks(lx, ly, rx, ry);

    bool stickUp = (ly > STICK_THRESHOLD);
    bool stickDown = (ly < -STICK_THRESHOLD);
    bool stickLeft = (lx < -STICK_THRESHOLD);
    bool stickRight = (lx > STICK_THRESHOLD);

    if (stickUp)    buttons |= BTN_DPAD_UP;
    if (stickDown)  buttons |= BTN_DPAD_DOWN;
    if (stickLeft)  buttons |= BTN_DPAD_LEFT;
    if (stickRight) buttons |= BTN_DPAD_RIGHT;

    WORD pressed = buttons & ~s_prevButtons;
    WORD released = ~buttons & s_prevButtons;

    /* Generate KEYDOWN events for newly pressed buttons */
    for (unsigned i = 0; i < MAP_COUNT; i++) {
        if (pressed & s_map[i].btn)
            enqueue(SDL_KEYDOWN, s_map[i].sdlk);
        if (released & s_map[i].btn)
            enqueue(SDL_KEYUP, s_map[i].sdlk);
    }

    s_prevButtons = buttons;

    /* Return one event from queue */
    if (s_qHead == s_qTail) return 0;
    *e = s_queue[s_qHead];
    s_qHead = (s_qHead + 1) % QUEUE_MAX;
    return 1;
}

/* -----------------------------------------------------------------------
   XbInputGetButtons -- combined current button + stick state as XB_BTN_*
   Called directly from controls.cpp every frame (no SDL event needed).
   ----------------------------------------------------------------------- */
unsigned short XbInputGetButtons(void) {
    if (!s_init) return 0;

    WORD buttons = GetButtons();

    int lx = 0, ly = 0, rx = 0, ry = 0;
    GetSticks(lx, ly, rx, ry);

    /* Fold stick directions into d-pad bits */
    if (ly > STICK_THRESHOLD) buttons |= BTN_DPAD_UP;
    if (ly < -STICK_THRESHOLD) buttons |= BTN_DPAD_DOWN;
    if (lx < -STICK_THRESHOLD) buttons |= BTN_DPAD_LEFT;
    if (lx > STICK_THRESHOLD) buttons |= BTN_DPAD_RIGHT;

    return (unsigned short)buttons;
}