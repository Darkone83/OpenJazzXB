/**
 * xb_input.h
 * XbJazz -- Xbox controller input bridge.
 * No <xtl.h> -- safe to include from controls.cpp, xb_sdl_compat.cpp, etc.
 */
#ifndef XB_INPUT_H
#define XB_INPUT_H

#include "xb_sdl_compat.h"   /* SDL_Event */

 /* BTN_* masks -- match input.h values but defined here so callers
	don't need <xtl.h> / input.h.                                    */
#define XB_BTN_UP     0x0001   /* DPAD UP      */
#define XB_BTN_DOWN   0x0002   /* DPAD DOWN    */
#define XB_BTN_LEFT   0x0004   /* DPAD LEFT    */
#define XB_BTN_RIGHT  0x0008   /* DPAD RIGHT   */
#define XB_BTN_START  0x0010   /* START        */
#define XB_BTN_BACK   0x0020   /* BACK         */
#define XB_BTN_LTHUMB 0x0040   /* L-THUMB      */
#define XB_BTN_RTHUMB 0x0080   /* R-THUMB      */
#define XB_BTN_BLACK  0x0100   /* BLACK        */
#define XB_BTN_WHITE  0x0200   /* WHITE        */
#define XB_BTN_LTRIG  0x0400   /* L-TRIGGER    */
#define XB_BTN_RTRIG  0x0800   /* R-TRIGGER    */
#define XB_BTN_A      0x1000   /* A            */
#define XB_BTN_B      0x2000   /* B            */
#define XB_BTN_X      0x4000   /* X            */
#define XB_BTN_Y      0x8000   /* Y            */

	/* Stick virtual d-pad bits added to XbInputGetButtons() result */
#define XB_BTN_STICK_UP    0x0100   /* reuses BLACK slot -- only one set */
#define XB_BTN_STICK_DOWN  0x0200
#define XB_BTN_STICK_LEFT  0x0400
#define XB_BTN_STICK_RIGHT 0x0800

#ifdef __cplusplus
extern "C" {
#endif

	void           XbInputInit(void);
	unsigned short XbInputGetButtons(void);     /* combined d-pad + face + sticks */
	int            XbInputPollEvent(SDL_Event* e); /* for SDL_PollEvent compat     */

#ifdef __cplusplus
}
#endif

#endif /* XB_INPUT_H */