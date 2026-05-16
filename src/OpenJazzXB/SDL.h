/**
 * SDL.h  (XbJazz fake — replaces the real SDL.h)
 *
 * Any source file doing #include <SDL.h> or #include "SDL.h" gets this
 * shim, which provides our Xbox-native SDL type definitions.
 *
 * Place this file in the project source directory so the compiler finds
 * it before any system SDL installation.
 */

#ifndef _SDL_H
#define _SDL_H

#include "xb_sdl_compat.h"

#endif /* _SDL_H */