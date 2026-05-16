/**
 * xb_sdl_compat.h
 * Xbox SDL compatibility layer for XbJazz.
 *
 * Provides the SDL1.2 surface/audio/event/timing API surface that
 * OpenJazz requires, without the real SDL library.
 *
 * NOTE: No platform headers pulled in here. OS calls live in xb_sdl_compat.cpp.
 */

#ifndef XB_SDL_COMPAT_H
#define XB_SDL_COMPAT_H

#include "xb_compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef unsigned char  Uint8;
typedef unsigned short Uint16;
typedef unsigned int   Uint32;
typedef signed char    Sint8;
typedef short          Sint16;
typedef int            Sint32;

/* -----------------------------------------------------------------------
   SDL Surface flags
   ----------------------------------------------------------------------- */
#define SDL_HWSURFACE    0x00000001u
#define SDL_SWSURFACE    0x00000000u
#define SDL_SRCCOLORKEY  0x00001000u
#define SDL_SRCALPHA     0x00010000u
#define SDL_DOUBLEBUF    0x00000000u
#define SDL_HWPALETTE    0x00000000u
#define SDL_FULLSCREEN   0x80000000u
#define SDL_RESIZABLE    0x00000000u

#define SDL_MUSTLOCK(s)      (0)
#define SDL_LockSurface(s)   (0)
#define SDL_UnlockSurface(s) ((void)0)

   /* -----------------------------------------------------------------------
      Colour and palette
      ----------------------------------------------------------------------- */
typedef struct SDL_Color {
    Uint8 r, g, b, unused;
} SDL_Color;

typedef struct SDL_Palette {
    int       ncolors;
    SDL_Color colors[256];
} SDL_Palette;

typedef struct SDL_PixelFormat {
    SDL_Palette* palette;
    Uint8        BitsPerPixel;
    Uint8        BytesPerPixel;
    Uint8        Rloss, Gloss, Bloss, Aloss;
    Uint8        Rshift, Gshift, Bshift, Ashift;
    Uint32       Rmask, Gmask, Bmask, Amask;
    Uint32       colorkey;
    Uint8        alpha;
} SDL_PixelFormat;

/* -----------------------------------------------------------------------
   SDL_Surface
   ----------------------------------------------------------------------- */
typedef struct SDL_Surface {
    Uint32           flags;
    SDL_PixelFormat* format;
    int              w, h;
    Uint16           pitch;
    void* pixels;
    SDL_PixelFormat  _fmt;
    SDL_Palette      _pal;
    int              clip_x, clip_y, clip_w, clip_h;
    int              has_clip;
} SDL_Surface;

/* -----------------------------------------------------------------------
   SDL_Rect
   ----------------------------------------------------------------------- */
typedef struct SDL_Rect {
    Sint16 x, y;
    Uint16 w, h;
} SDL_Rect;

/* -----------------------------------------------------------------------
   Keyboard constants (SDLK_*)
   OJ uses these for key-binding menus. On Xbox they're never matched
   against real input; they just need to exist to compile.
   Values match SDL1.2 SDLKey enum.
   ----------------------------------------------------------------------- */
typedef int SDLKey;
typedef int SDLMod;

#define SDLK_UNKNOWN     0
#define SDLK_BACKSPACE   8
#define SDLK_TAB         9
#define SDLK_CLEAR       12
#define SDLK_RETURN      13
#define SDLK_PAUSE       19
#define SDLK_ESCAPE      27
#define SDLK_SPACE       32
#define SDLK_EXCLAIM     33
#define SDLK_QUOTEDBL    34
#define SDLK_HASH        35
#define SDLK_DOLLAR      36
#define SDLK_AMPERSAND   38
#define SDLK_QUOTE       39
#define SDLK_LEFTPAREN   40
#define SDLK_RIGHTPAREN  41
#define SDLK_ASTERISK    42
#define SDLK_PLUS        43
#define SDLK_COMMA       44
#define SDLK_MINUS       45
#define SDLK_PERIOD      46
#define SDLK_SLASH       47
#define SDLK_0           48
#define SDLK_1           49
#define SDLK_2           50
#define SDLK_3           51
#define SDLK_4           52
#define SDLK_5           53
#define SDLK_6           54
#define SDLK_7           55
#define SDLK_8           56
#define SDLK_9           57
#define SDLK_COLON       58
#define SDLK_SEMICOLON   59
#define SDLK_LESS        60
#define SDLK_EQUALS      61
#define SDLK_GREATER     62
#define SDLK_QUESTION    63
#define SDLK_AT          64
#define SDLK_LEFTBRACKET  91
#define SDLK_BACKSLASH    92
#define SDLK_RIGHTBRACKET 93
#define SDLK_CARET        94
#define SDLK_UNDERSCORE   95
#define SDLK_BACKQUOTE    96
#define SDLK_a           97
#define SDLK_b           98
#define SDLK_c           99
#define SDLK_d           100
#define SDLK_e           101
#define SDLK_f           102
#define SDLK_g           103
#define SDLK_h           104
#define SDLK_i           105
#define SDLK_j           106
#define SDLK_k           107
#define SDLK_l           108
#define SDLK_m           109
#define SDLK_n           110
#define SDLK_o           111
#define SDLK_p           112
#define SDLK_q           113
#define SDLK_r           114
#define SDLK_s           115
#define SDLK_t           116
#define SDLK_u           117
#define SDLK_v           118
#define SDLK_w           119
#define SDLK_x           120
#define SDLK_y           121
#define SDLK_z           122
#define SDLK_DELETE      127
#define SDLK_KP0         256
#define SDLK_KP1         257
#define SDLK_KP2         258
#define SDLK_KP3         259
#define SDLK_KP4         260
#define SDLK_KP5         261
#define SDLK_KP6         262
#define SDLK_KP7         263
#define SDLK_KP8         264
#define SDLK_KP9         265
#define SDLK_KP_PERIOD   266
#define SDLK_KP_DIVIDE   267
#define SDLK_KP_MULTIPLY 268
#define SDLK_KP_MINUS    269
#define SDLK_KP_PLUS     270
#define SDLK_KP_ENTER    271
#define SDLK_KP_EQUALS   272
#define SDLK_UP          273
#define SDLK_DOWN        274
#define SDLK_RIGHT       275
#define SDLK_LEFT        276
#define SDLK_INSERT      277
#define SDLK_HOME        278
#define SDLK_END         279
#define SDLK_PAGEUP      280
#define SDLK_PAGEDOWN    281
#define SDLK_F1          282
#define SDLK_F2          283
#define SDLK_F3          284
#define SDLK_F4          285
#define SDLK_F5          286
#define SDLK_F6          287
#define SDLK_F7          288
#define SDLK_F8          289
#define SDLK_F9          290
#define SDLK_F10         291
#define SDLK_F11         292
#define SDLK_F12         293
#define SDLK_RSHIFT      303
#define SDLK_LSHIFT      304
#define SDLK_RCTRL       305
#define SDLK_LCTRL       306
#define SDLK_RALT        307
#define SDLK_LALT        308
#define SDLK_LAST        323

#define KMOD_NONE        0x0000
#define KMOD_LSHIFT      0x0001
#define KMOD_RSHIFT      0x0002
#define KMOD_LCTRL       0x0040
#define KMOD_RCTRL       0x0080
#define KMOD_LALT        0x0100
#define KMOD_RALT        0x0200

/* -----------------------------------------------------------------------
   Joystick hat constants
   ----------------------------------------------------------------------- */
#define SDL_HAT_CENTERED  0x00
#define SDL_HAT_UP        0x01
#define SDL_HAT_RIGHT     0x02
#define SDL_HAT_DOWN      0x04
#define SDL_HAT_LEFT      0x08
#define SDL_HAT_RIGHTUP   (SDL_HAT_RIGHT | SDL_HAT_UP)
#define SDL_HAT_RIGHTDOWN (SDL_HAT_RIGHT | SDL_HAT_DOWN)
#define SDL_HAT_LEFTUP    (SDL_HAT_LEFT  | SDL_HAT_UP)
#define SDL_HAT_LEFTDOWN  (SDL_HAT_LEFT  | SDL_HAT_DOWN)

   /* -----------------------------------------------------------------------
      SDL_Event
      ----------------------------------------------------------------------- */
#define SDL_QUIT            0x100
#define SDL_KEYDOWN         0x300
#define SDL_KEYUP           0x301
#define SDL_JOYBUTTONDOWN   0x600
#define SDL_JOYBUTTONUP     0x601
#define SDL_JOYAXISMOTION   0x700
#define SDL_JOYHATMOTION    0x900
#define SDL_MOUSEBUTTONDOWN 0x401
#define SDL_MOUSEBUTTONUP   0x402
#define SDL_MOUSEMOTION     0x400
#define SDL_MOUSEWHEEL      0x403
#define SDL_WINDOWEVENT     0x200

typedef struct SDL_Keysym {
    int    scancode;
    SDLKey sym;
    SDLMod mod;
} SDL_Keysym;

typedef struct SDL_KeyboardEvent {
    Uint32     type;
    Uint8      state;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_JoyButtonEvent {
    Uint32 type;
    Uint8  button, state;
} SDL_JoyButtonEvent;

typedef struct SDL_JoyAxisEvent {
    Uint32 type;
    Uint8  axis;
    Sint16 value;
} SDL_JoyAxisEvent;

typedef struct SDL_JoyHatEvent {
    Uint32 type;
    Uint8  hat, value;
} SDL_JoyHatEvent;

typedef struct SDL_MouseMotionEvent {
    Uint32 type;
    Sint16 x, y;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent {
    Uint32 type;
    Uint8  button, state;
    Sint16 x, y;
} SDL_MouseButtonEvent;

typedef struct SDL_MouseWheelEvent {
    Uint32 type;
    Sint32 x, y;
} SDL_MouseWheelEvent;

typedef union SDL_Event {
    Uint32               type;
    SDL_KeyboardEvent    key;
    SDL_JoyButtonEvent   jbutton;
    SDL_JoyAxisEvent     jaxis;
    SDL_JoyHatEvent      jhat;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent  wheel;
    Uint8                _pad[56];
} SDL_Event;

/* SDL_PollEvent: implemented in xb_sdl_compat.cpp via Xbox controller */
int SDL_PollEvent(SDL_Event* e);

/* -----------------------------------------------------------------------
   Timing -- declared here, implemented in xb_sdl_compat.cpp
   ----------------------------------------------------------------------- */
Uint32 SDL_GetTicks(void);
void   SDL_Delay(Uint32 ms);

/* -----------------------------------------------------------------------
   Surface functions
   ----------------------------------------------------------------------- */
SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
    Uint32 Rmask, Uint32 Gmask,
    Uint32 Bmask, Uint32 Amask);
void   SDL_FreeSurface(SDL_Surface* s);
int    SDL_SetColors(SDL_Surface* s, SDL_Color* colors, int first, int ncolors);
int    SDL_SetPalette(SDL_Surface* s, int flags, SDL_Color* colors, int first, int ncolors);
int    SDL_SetColorKey(SDL_Surface* s, Uint32 flag, Uint32 key);
int    SDL_SetClipRect(SDL_Surface* s, const SDL_Rect* rect);
int    SDL_BlitSurface(SDL_Surface* src, SDL_Rect* srcrect,
    SDL_Surface* dst, SDL_Rect* dstrect);
int    SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color);
Uint32 SDL_MapRGB(const SDL_PixelFormat* fmt, Uint8 r, Uint8 g, Uint8 b);

/* -----------------------------------------------------------------------
   Audio stubs
   ----------------------------------------------------------------------- */
#define SDL_AUDIO_S16SYS  0x8010
#define SDL_AUDIO_S16LSB  0x8010
#define SDL_AUDIO_U8      0x0008
#define SDL_MIX_MAXVOLUME 128

typedef struct SDL_AudioSpec {
    int    freq;
    Uint16 format;
    Uint8  channels, silence;
    Uint16 samples;
    Uint32 size;
    void (*callback)(void* userdata, Uint8* stream, int len);
    void* userdata;
} SDL_AudioSpec;

typedef Uint32 SDL_AudioDeviceID;

static inline void SDL_MixAudio(Uint8* d, const Uint8* s, Uint32 n, int v) {
    (void)d; (void)s; (void)n; (void)v;
}

/* -----------------------------------------------------------------------
   Joystick stubs
   ----------------------------------------------------------------------- */
typedef void SDL_Joystick;
static inline int           SDL_NumJoysticks(void) { return 0; }
static inline SDL_Joystick* SDL_JoystickOpen(int i) { (void)i; return nullptr; }
static inline void          SDL_JoystickClose(SDL_Joystick*) {}
static inline const char* SDL_JoystickName(int) { return "Xbox Controller"; }
static inline int           SDL_JoystickNumAxes(SDL_Joystick*) { return 0; }
static inline int           SDL_JoystickNumButtons(SDL_Joystick*) { return 0; }
static inline int           SDL_JoystickNumHats(SDL_Joystick*) { return 0; }

/* -----------------------------------------------------------------------
   Misc stubs
   ----------------------------------------------------------------------- */
   /* SDL_Init: calls XbInputInit() -- implemented in xb_sdl_compat.cpp */
int SDL_Init(Uint32 f);
static inline void        SDL_Quit(void) {}
static inline void        SDL_ShowCursor(int e) { (void)e; }
static inline const char* SDL_GetError(void) { return ""; }
static inline void SDL_WM_SetCaption(const char* t, const char* i) {
    (void)t; (void)i;
}

typedef struct { Uint8 major, minor, patch; } SDL_version;
#define SDL_VERSION(v) do { (v)->major=1; (v)->minor=2; (v)->patch=15; } while(0)
static inline const SDL_version* SDL_Linked_Version(void) {
    static SDL_version v = { 1,2,15 };
    return &v;
}

#endif /* XB_SDL_COMPAT_H */