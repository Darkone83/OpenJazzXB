/**
 * xb_sdl_compat.cpp
 * Xbox SDL compatibility layer -- surface management and software blitter.
 *
 * SDL_GetTicks -> GetTickCount()  (confirmed XbTyrian pattern)
 * SDL_Delay    -> Sleep()         (confirmed XbTyrian pattern)
 * No std::chrono, no std::thread -- RXDK has no runtime for those.
 */

#include <xtl.h>        /* GetTickCount, Sleep -- must be first */
#include <string.h>
#include <stdlib.h>

#include "xb_sdl_compat.h"
#include "xb_input.h"

 /* -----------------------------------------------------------------------
    Timing
    ----------------------------------------------------------------------- */
Uint32 SDL_GetTicks(void) {
    return (Uint32)GetTickCount();
}

void SDL_Delay(Uint32 ms) {
    Sleep(ms);
}

/* -----------------------------------------------------------------------
   Internal: allocate surface + pixel buffer
   ----------------------------------------------------------------------- */
static SDL_Surface* alloc_surface(int w, int h) {
    SDL_Surface* s = (SDL_Surface*)calloc(1, sizeof(SDL_Surface));
    if (!s) return nullptr;
    s->w = w;
    s->h = h;
    s->pitch = (Uint16)w;
    s->pixels = calloc(w * h, 1);
    if (!s->pixels) { free(s); return nullptr; }
    s->format = &s->_fmt;
    s->format->palette = &s->_pal;
    s->format->BitsPerPixel = 8;
    s->format->BytesPerPixel = 1;
    s->_pal.ncolors = 256;
    s->clip_x = 0; s->clip_y = 0;
    s->clip_w = w; s->clip_h = h;
    return s;
}

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
    Uint32 Rmask, Uint32 Gmask,
    Uint32 Bmask, Uint32 Amask) {
    (void)flags; (void)depth;
    (void)Rmask; (void)Gmask; (void)Bmask; (void)Amask;
    return alloc_surface(w, h);
}

void SDL_FreeSurface(SDL_Surface* s) {
    if (!s) return;
    free(s->pixels);
    free(s);
}

int SDL_SetColors(SDL_Surface* s, SDL_Color* colors, int first, int ncolors) {
    if (!s || !colors || first < 0 || first + ncolors > 256) return 0;
    memcpy(&s->_pal.colors[first], colors, ncolors * sizeof(SDL_Color));
    return 1;
}

int SDL_SetPalette(SDL_Surface* s, int flags, SDL_Color* colors,
    int first, int ncolors) {
    (void)flags;
    return SDL_SetColors(s, colors, first, ncolors);
}

int SDL_SetColorKey(SDL_Surface* s, Uint32 flag, Uint32 key) {
    if (!s) return -1;
    if (flag & SDL_SRCCOLORKEY) {
        s->flags |= SDL_SRCCOLORKEY;
        s->format->colorkey = s->_fmt.colorkey = key;
    }
    else {
        s->flags &= ~SDL_SRCCOLORKEY;
    }
    return 0;
}

int SDL_SetClipRect(SDL_Surface* s, const SDL_Rect* rect) {
    if (!s) return 0;
    if (!rect) {
        s->clip_x = 0; s->clip_y = 0;
        s->clip_w = s->w; s->clip_h = s->h;
        s->has_clip = 0;
    }
    else {
        s->clip_x = rect->x; s->clip_y = rect->y;
        s->clip_w = rect->w; s->clip_h = rect->h;
        s->has_clip = 1;
    }
    return 1;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat* fmt, Uint8 r, Uint8 g, Uint8 b) {
    if (!fmt || !fmt->palette) return 0;
    const SDL_Palette* p = fmt->palette;
    for (int i = 0; i < p->ncolors; i++) {
        if (p->colors[i].r == r && p->colors[i].g == g && p->colors[i].b == b)
            return (Uint32)i;
    }
    return 0;
}

int SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color) {
    if (!dst || !dst->pixels) return -1;
    Uint8 col = (Uint8)(color & 0xFF);
    int x = 0, y = 0, w = dst->w, h = dst->h;
    if (rect) { x = rect->x; y = rect->y; w = rect->w; h = rect->h; }
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dst->w) w = dst->w - x;
    if (y + h > dst->h) h = dst->h - y;
    if (w <= 0 || h <= 0) return 0;
    Uint8* row = (Uint8*)dst->pixels + y * dst->pitch + x;
    for (int i = 0; i < h; i++) { memset(row, col, w); row += dst->pitch; }
    return 0;
}

int SDL_BlitSurface(SDL_Surface* src, SDL_Rect* srcrect,
    SDL_Surface* dst, SDL_Rect* dstrect) {
    if (!src || !dst || !src->pixels || !dst->pixels) return -1;
    int sx = 0, sy = 0, sw = src->w, sh = src->h;
    if (srcrect) { sx = srcrect->x; sy = srcrect->y; sw = srcrect->w; sh = srcrect->h; }
    int dx = 0, dy = 0;
    if (dstrect) { dx = dstrect->x; dy = dstrect->y; }
    int cx = 0, cy = 0, cr = dst->w, cb = dst->h;
    if (dst->has_clip) { cx = dst->clip_x; cy = dst->clip_y; cr = cx + dst->clip_w; cb = cy + dst->clip_h; }
    if (dx < cx) { int d = cx - dx; sx += d; sw -= d; dx = cx; }
    if (dy < cy) { int d = cy - dy; sy += d; sh -= d; dy = cy; }
    if (dx + sw > cr) sw = cr - dx;
    if (dy + sh > cb) sh = cb - dy;
    if (sx < 0) { dx -= sx; sw += sx; sx = 0; }
    if (sy < 0) { dy -= sy; sh += sy; sy = 0; }
    if (sx + sw > src->w) sw = src->w - sx;
    if (sy + sh > src->h) sh = src->h - sy;
    if (sw <= 0 || sh <= 0) return 0;
    if (dstrect) { dstrect->w = (Uint16)sw; dstrect->h = (Uint16)sh; }
    bool ck = (src->flags & SDL_SRCCOLORKEY) != 0;
    Uint8 ckv = ck ? (Uint8)(src->format->colorkey & 0xFF) : 0;
    const Uint8* sr = (const Uint8*)src->pixels + sy * src->pitch + sx;
    Uint8* dr = (Uint8*)dst->pixels + dy * dst->pitch + dx;
    for (int row = 0; row < sh; row++) {
        if (!ck) { memcpy(dr, sr, sw); }
        else { for (int col = 0; col < sw; col++) { if (sr[col] != ckv) dr[col] = sr[col]; } }
        sr += src->pitch; dr += dst->pitch;
    }
    return 0;
}

/* -----------------------------------------------------------------------
   SDL_Init / SDL_PollEvent -- wired to Xbox controller input
   ----------------------------------------------------------------------- */
int SDL_Init(Uint32 flags) {
    (void)flags;
    XbInputInit();
    return 0;
}

int SDL_PollEvent(SDL_Event* e) {
    return XbInputPollEvent(e);
}