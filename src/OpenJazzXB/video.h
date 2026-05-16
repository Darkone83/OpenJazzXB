/**
 * video.h
 * XbJazz port — SDL headers replaced with Xbox compat layer.
 * Public interface is identical to the original OpenJazz video.h.
 *
 * Original copyright:
 * Copyright (c) 2005-2017 AJ Thomson
 * Copyright (c) 2015-2026 Carsten Teibes
 */

#ifndef OJ_VIDEO_H
#define OJ_VIDEO_H

#include "setup.h"
#include "paletteeffects.h"
#include "xb_sdl_compat.h"   /* SDL_Surface, SDL_Color, SDL_Rect */

 /* -----------------------------------------------------------------------
    Screen geometry constants (unchanged from original)
    ----------------------------------------------------------------------- */

    /* Original game canvas dimensions */
#define SW 320
#define SH 200

#define MIN_SCALE  1
#define MAX_SCALE  1    /* No runtime scaling on Xbox; D3D8 handles upscale */

#define MAX_SCREEN_WIDTH  (32 * 256 * MAX_SCALE)
#define MAX_SCREEN_HEIGHT (32 * 64  * MAX_SCALE)

#ifndef DEFAULT_SCREEN_WIDTH
#define DEFAULT_SCREEN_WIDTH  SW
#endif
#ifndef DEFAULT_SCREEN_HEIGHT
#define DEFAULT_SCREEN_HEIGHT SH
#endif

/* Timing */
#define T_MENU_FRAME 20

/* -----------------------------------------------------------------------
   Video class
   ----------------------------------------------------------------------- */
class Video {
public:
    Video();

    bool        init(SetupOptions cfg);
    void        deinit();
    bool        reset(int width, int height);

    void        setPalette(SDL_Color* palette);
    SDL_Color* getPalette() const;
    void        changePalette(SDL_Color* palette,
        unsigned char first,
        unsigned int  amount);

    /* Surface management */
    SDL_Surface* createSurface(const unsigned char* pixels,
        int width, int height);
    void         destroySurface(SDL_Surface* surface);
    void         setSurfacePalette(SDL_Surface* surface,
        SDL_Color* palette,
        int start, int length);
    void         restoreSurfacePalette(SDL_Surface* surface);
    void         enableColorKey(SDL_Surface* surface,
        unsigned int index);
    unsigned int getColorKey(SDL_Surface* surface);
    void         blitSurface(SDL_Surface* src, SDL_Rect* srcRect,
        SDL_Surface* dst, SDL_Rect* dstRect);
    void         setClipRect(SDL_Surface* surface,
        const SDL_Rect* rect);

    void         drawRect(int x, int y, int width, int height,
        int index, bool fill = true);

    int          getMinWidth() const;
    int          getMaxWidth() const;
    int          getMinHeight() const;
    int          getMaxHeight() const;
    int          getWidth() const;
    int          getHeight() const;
    void         setTitle(const char* title);
    int          getScaleFactor() const;
    scalerType   getScaleMethod() const;
    void         setScaling(int newScaleFactor,
        scalerType newScaleMethod);
    bool         isFullscreen() const;

    void         moviePlayback(bool status);

    void         update(SDL_Event* event);
    void         flip(int mspf,
        PaletteEffect* paletteEffects = NULL,
        bool effectsStopped = false);
    void         clearScreen(int index);

private:
    void         findResolutions();
    void         expose();
    void         commonDeinit();

    SDL_Surface* screen;         /* the 8bpp canvas surface (= ::canvas) */
    SDL_Color    currentPalette[256];
    SDL_Color    logicalPalette[256]; /* greyscale — for OJ palette effects */

    int          minW, maxW, screenW;
    int          minH, maxH, screenH;
    int          scaleFactor;
    scalerType   scaleMethod;
    bool         fullscreen;
    bool         isPlayingMovie;

    /* Phase 2: D3D8 device/texture live in xb_video.cpp, not here */
};

/* -----------------------------------------------------------------------
   Inline accessors (must match original signatures exactly)
   ----------------------------------------------------------------------- */
inline SDL_Color* Video::getPalette() const { return (SDL_Color*)currentPalette; }
inline int        Video::getMinWidth() const { return minW; }
inline int        Video::getMaxWidth() const { return maxW; }
inline int        Video::getMinHeight() const { return minH; }
inline int        Video::getMaxHeight() const { return maxH; }
inline int        Video::getWidth() const { return screenW; }
inline int        Video::getHeight() const { return screenH; }
inline int        Video::getScaleFactor() const { return scaleFactor; }
inline scalerType Video::getScaleMethod() const { return scaleMethod; }
inline bool       Video::isFullscreen() const { return fullscreen; }

/* -----------------------------------------------------------------------
   Global canvas surface and Video instance
   ----------------------------------------------------------------------- */
EXTERN SDL_Surface* canvas;
EXTERN int          canvasW;
EXTERN int          canvasH;
EXTERN Video        video;

#endif /* OJ_VIDEO_H */