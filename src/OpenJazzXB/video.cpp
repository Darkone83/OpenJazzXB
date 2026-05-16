/**
 * video.cpp
 * XbJazz port -- D3D8 canvas presentation.
 *
 * Based directly on the working XbTyrian video.cpp.
 *
 * KEY FACTS (learned from XbTyrian source):
 *   - D3DFMT_LIN_X8R8G8B8 uses TEXEL-SPACE UVs (0..width, 0..height),
 *     NOT normalized 0..1.  Using 1.0f as max UV = 1x1 texel on screen.
 *   - setup_render_states() and D3DDevice_Clear() are called EVERY FRAME.
 *   - D3DDevice_BeginScene / EndScene are no-ops in the header but kept.
 *   - LockRect result is checked; bail if it fails.
 *   - Device is created by main() + Sleep(250); init() only makes the texture.
 *
 * RXDK rules: <xtl.h> first. No float-to-int casts. No STL.
 */

#include <xtl.h>
#include "video.h"
#include "paletteeffects.h"
#include "setup.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include "xb_config.h"

 /* D3DFMT_LIN_X8R8G8B8 guard -- XbTyrian defines this in case it's missing */
#ifndef D3DFMT_LIN_X8R8G8B8
#define D3DFMT_LIN_X8R8G8B8 ((D3DFORMAT)0x0000001e)
#endif

/* -----------------------------------------------------------------------
   State
   ----------------------------------------------------------------------- */
static D3DTexture* s_pFrameTex = nullptr;  /* 320x200 normal */
static D3DTexture* s_pScale2xTex = nullptr;  /* 640x400 scale2x */
static unsigned char s_scale2xBuf[640 * 400];

static const int VGA_W = SW;   /* 320 */
static const int VGA_H = SH;   /* 200 */
static const int XB_W = 640;
static const int XB_H = 480;

/* FVF for pre-transformed textured vertices */
#define BLIT_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)
struct BlitVertex { float x, y, z, rhw, u, v; };

/* Fullscreen quad -- UV in TEXEL SPACE (0..320, 0..200) per XbTyrian */
static BlitVertex s_quad[4];

static int s_textureFilter = 0;   /* 0=point, 1=linear */

extern int g_bbWidth;
extern int g_bbHeight;

/* -----------------------------------------------------------------------
   build_quad -- 4:3 letterbox centered in 640x480
   UVs are texel-space: u runs 0..VGA_W, v runs 0..VGA_H
   ----------------------------------------------------------------------- */

   /* -----------------------------------------------------------------------
      Scale2x -- EPX/Scale2x pixel art upscale, 320x200 -> 640x400
      ----------------------------------------------------------------------- */
static void doScale2x(const unsigned char* src, unsigned char* dst) {
    int x, y;
    for (y = 0; y < VGA_H; y++) {
        for (x = 0; x < VGA_W; x++) {
            unsigned char P = src[y * VGA_W + x];
            unsigned char A = y > 0 ? src[(y - 1) * VGA_W + x] : P;
            unsigned char B = x < VGA_W - 1 ? src[y * VGA_W + x + 1] : P;
            unsigned char C = x > 0 ? src[y * VGA_W + x - 1] : P;
            unsigned char D = y < VGA_H - 1 ? src[(y + 1) * VGA_W + x] : P;
            int ox = x * 2, oy = y * 2, dw = VGA_W * 2;
            dst[oy * dw + ox] = (C == A && A != D && C != B) ? A : P;
            dst[oy * dw + ox + 1] = (A == B && A != C && B != D) ? B : P;
            dst[(oy + 1) * dw + ox] = (C == D && D != A && C != B) ? C : P;
            dst[(oy + 1) * dw + ox + 1] = (B == D && B != A && D != C) ? D : P;
        }
    }
}

static void build_quad(void) {
    float bbW = (float)g_bbWidth;
    float bbH = (float)g_bbHeight;
    float qx0, qy0, qx1, qy1;

    switch (g_xbConfig.aspectMode) {
    case XB_ASPECT_STRETCH:
        qx0 = 0; qy0 = 0; qx1 = bbW; qy1 = bbH;
        break;
    case XB_ASPECT_PIXEL: {
        /* Integer scale, contain */
        int scaleX = g_bbWidth / VGA_W;
        int scaleY = g_bbHeight / VGA_H;
        int scale = (scaleX < scaleY) ? scaleX : scaleY;
        if (scale < 1) scale = 1;
        float tw2 = (float)(VGA_W * scale);
        float th2 = (float)(VGA_H * scale);
        qx0 = (bbW - tw2) * 0.5f; qy0 = (bbH - th2) * 0.5f;
        qx1 = qx0 + tw2;        qy1 = qy0 + th2;
        break;
    }
    case XB_ASPECT_FILL: {
        /* Integer scale, cover */
        int scaleX = g_bbWidth / VGA_W;
        int scaleY = g_bbHeight / VGA_H;
        int scale = (scaleX > scaleY) ? scaleX : scaleY;
        if (scale < 1) scale = 1;
        float tw2 = (float)(VGA_W * scale);
        float th2 = (float)(VGA_H * scale);
        qx0 = (bbW - tw2) * 0.5f; qy0 = (bbH - th2) * 0.5f;
        qx1 = qx0 + tw2;        qy1 = qy0 + th2;
        break;
    }
    default: /* XB_ASPECT_4_3 -- corrected 4:3, letterbox/pillarbox */
    {
        float tH = bbH;
        float tW = tH * 4.0f / 3.0f;
        if (tW > bbW) { tW = bbW; tH = tW * 3.0f / 4.0f; }
        qx0 = (bbW - tW) * 0.5f; qy0 = (bbH - tH) * 0.5f;
        qx1 = qx0 + tW;        qy1 = qy0 + tH;
    }
    break;
    }

    /* UV range: texel-space. Scale2x uses 640x400 texture. */
    float tw = (g_xbConfig.filterMode == XB_FILTER_SCALE2X && s_pScale2xTex)
        ? (float)(VGA_W * 2) : (float)VGA_W;
    float th = (g_xbConfig.filterMode == XB_FILTER_SCALE2X && s_pScale2xTex)
        ? (float)(VGA_H * 2) : (float)VGA_H;
    s_quad[0] = { qx0, qy0, 0.0f, 1.0f,  0.0f, 0.0f };
    s_quad[1] = { qx1, qy0, 0.0f, 1.0f,  tw,   0.0f };
    s_quad[2] = { qx0, qy1, 0.0f, 1.0f,  0.0f, th };
    s_quad[3] = { qx1, qy1, 0.0f, 1.0f,  tw,   th };
}

/* -----------------------------------------------------------------------
   setup_render_states -- called every frame (per XbTyrian)
   ----------------------------------------------------------------------- */
static void setup_render_states(void) {
    D3DDevice_SetRenderState(D3DRS_ZENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_LIGHTING, FALSE);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    DWORD filter = (s_textureFilter != 0) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, filter);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, filter);
    D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

    D3DDevice_SetVertexShader(BLIT_FVF);
}

/* -----------------------------------------------------------------------
   Video constructor
   ----------------------------------------------------------------------- */
Video::Video()
    : screen(nullptr), scaleFactor(MIN_SCALE),
    fullscreen(true), isPlayingMovie(false) {
    minW = maxW = screenW = DEFAULT_SCREEN_WIDTH;
    minH = maxH = screenH = DEFAULT_SCREEN_HEIGHT;
    scaleMethod = scalerType::None;
    for (int i = 0; i < 256; i++) {
        logicalPalette[i].r = logicalPalette[i].g =
            logicalPalette[i].b = (unsigned char)i;
        logicalPalette[i].unused = 0;
    }
    memset(currentPalette, 0, sizeof(currentPalette));
}

/* -----------------------------------------------------------------------
   Video::init
   Device already exists (created + Sleep(250) in main).
   Only creates the texture and sets state -- same as XbTyrian init_video().
   ----------------------------------------------------------------------- */
bool Video::init(SetupOptions cfg) {
    (void)cfg;
    screenW = canvasW = VGA_W;
    screenH = canvasH = VGA_H;
    fullscreen = true;
    scaleFactor = MIN_SCALE;
    scaleMethod = scalerType::None;

    if (s_pFrameTex) return true;   /* already init'd */

    /* Create 320x200 linear XRGB texture -- CPU writes every frame */
    s_pFrameTex = D3DDevice_CreateTexture2(
        VGA_W, VGA_H,
        1,                    /* depth (1 for 2D)  */
        1,                    /* mip levels        */
        0,                    /* usage flags       */
        D3DFMT_LIN_X8R8G8B8,
        D3DRTYPE_TEXTURE
    );
    if (!s_pFrameTex) {
        LOG_FATAL("Video::init: D3DDevice_CreateTexture2 failed");
        return false;
    }

    /* Try to create 640x400 scale2x texture -- optional, falls back gracefully */
    s_pScale2xTex = D3DDevice_CreateTexture2(
        VGA_W * 2, VGA_H * 2, 1, 1, 0, D3DFMT_LIN_X8R8G8B8, D3DRTYPE_TEXTURE);
    /* s_pScale2xTex may be null on some hardware -- scale2x falls back to smooth */

    /* Apply filter from config at init */
    s_textureFilter = (g_xbConfig.filterMode == XB_FILTER_SMOOTH) ? 1 : 0;
    build_quad();
    setup_render_states();

    /* 8bpp software canvas for all game drawing */
    canvas = SDL_CreateRGBSurface(SDL_SWSURFACE, VGA_W, VGA_H, 8, 0, 0, 0, 0);
    if (!canvas) {
        LOG_FATAL("Video::init: canvas alloc failed");
        s_pFrameTex->Release(); s_pFrameTex = nullptr;
        return false;
    }
    screen = canvas;
    SDL_SetColors(canvas, logicalPalette, 0, 256);

    LOG_DEBUG("Video::init ok: LIN_X8R8G8B8 %dx%d -> %dx%d",
        VGA_W, VGA_H, XB_W, XB_H);
    return true;
}

/* -----------------------------------------------------------------------
   Video::deinit
   ----------------------------------------------------------------------- */
void Video::deinit() {
    if (canvas) { SDL_FreeSurface(canvas); canvas = nullptr; }
    screen = nullptr;
    if (s_pFrameTex) { s_pFrameTex->Release();   s_pFrameTex = nullptr; }
    if (s_pScale2xTex) { s_pScale2xTex->Release(); s_pScale2xTex = nullptr; }
}

void Video::commonDeinit() { deinit(); }
bool Video::reset(int w, int h) { (void)w; (void)h; return true; }

/* -----------------------------------------------------------------------
   Palette
   ----------------------------------------------------------------------- */
   /* Saved palette entries for mapPalette/restorePalette font color remapping.
    * File-scope so both setPalette and setSurfacePalette can access them. */
static SDL_Color s_savedPalEntries[256];
static int       s_savedStart = -1;
static int       s_savedLength = 0;

void Video::setPalette(SDL_Color* palette) {
    if (!palette) return;
    memcpy(currentPalette, palette, 256 * sizeof(SDL_Color));
    if (canvas) SDL_SetColors(canvas, currentPalette, 0, 256);
    s_savedStart = -1;  /* invalidate any stale mapPalette save */
}

void Video::changePalette(SDL_Color* palette,
    unsigned char first, unsigned int amount) {
    if (!palette) return;
    memcpy(&currentPalette[first], palette, amount * sizeof(SDL_Color));
    if (canvas) SDL_SetColors(canvas, &currentPalette[first],
        (int)first, (int)amount);
}

/* -----------------------------------------------------------------------
   Video::flip  (hot path -- matches JE_showVGA from XbTyrian exactly)
   ----------------------------------------------------------------------- */
void Video::flip(int mspf, PaletteEffect* paletteEffects, bool effectsStopped) {
    if (!s_pFrameTex || !canvas) return;

    /* Apply palette effects to a local copy */
    SDL_Color framePal[256];
    memcpy(framePal, currentPalette, 256 * sizeof(SDL_Color));
    if (paletteEffects)
        /* Xbox: pass direct=false -- effects modify framePal only.
         * On PC SDL, direct=true updates the hardware 8bpp canvas palette.
         * Here we do our own palette expansion; direct=true would corrupt
         * currentPalette (feedback loop) making WhiteInEffect permanent. */
        paletteEffects->apply(framePal, false, mspf, effectsStopped);

    /* Lock -- bail if it fails */
    bool useScale2x = (g_xbConfig.filterMode == XB_FILTER_SCALE2X && s_pScale2xTex);
    D3DTexture* activeTex = useScale2x ? s_pScale2xTex : s_pFrameTex;

    D3DLOCKED_RECT lr;
    if (FAILED(activeTex->LockRect(0, &lr, nullptr, 0))) return;

    DWORD* dst = (DWORD*)lr.pBits;
    int    dpitch = lr.Pitch / 4;

    if (useScale2x) {
        doScale2x((const unsigned char*)canvas->pixels, s_scale2xBuf);
        int sw2 = VGA_W * 2;
        for (int y = 0; y < VGA_H * 2; y++) {
            const unsigned char* srow = s_scale2xBuf + y * sw2;
            DWORD* drow = dst + y * dpitch;
            for (int x = 0; x < sw2; x++) {
                SDL_Color c = framePal[srow[x]];
                drow[x] = ((DWORD)c.r << 16) | ((DWORD)c.g << 8) | (DWORD)c.b;
            }
        }
    }
    else {
        const unsigned char* src = (const unsigned char*)canvas->pixels;
        for (int y = 0; y < VGA_H; y++) {
            const unsigned char* srow = src + y * canvas->pitch;
            DWORD* drow = dst + y * dpitch;
            for (int x = 0; x < VGA_W; x++) {
                SDL_Color c = framePal[srow[x]];
                drow[x] = ((DWORD)c.r << 16) | ((DWORD)c.g << 8) | (DWORD)c.b;
            }
        }
    }
    activeTex->UnlockRect(0);

    /* --- Render pass --- */
    D3DDevice_BeginScene();
    D3DDevice_Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
    setup_render_states();
    D3DDevice_SetTexture(0, activeTex);
    D3DDevice_DrawVerticesUP(D3DPT_TRIANGLESTRIP, 4, s_quad, sizeof(BlitVertex));

    /* Scanlines -- darken alternate rows */
    if (g_xbConfig.scanlines != XB_SCANLINES_OFF) {
        int scanH = g_bbHeight / VGA_H;
        if (scanH < 1) scanH = 1;
        int gapH = 1;
        if (g_xbConfig.scanlines == XB_SCANLINES_MEDIUM)
            gapH = (scanH > 1) ? ((scanH + 1) / 2) : 1;
        else if (g_xbConfig.scanlines == XB_SCANLINES_HEAVY)
            gapH = scanH;
        int row;
        for (row = scanH; row < g_bbHeight; row += scanH * 2) {
            D3DRECT r;
            r.x1 = 0; r.x2 = g_bbWidth;
            r.y1 = row; r.y2 = row + gapH;
            if (r.y2 > g_bbHeight) r.y2 = g_bbHeight;
            D3DDevice_Clear(1, &r, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
        }
    }

    D3DDevice_EndScene();
    D3DDevice_Swap(0);
}

/* -----------------------------------------------------------------------
   Surface management
   ----------------------------------------------------------------------- */
SDL_Surface* Video::createSurface(const unsigned char* pixels,
    int width, int height) {
    SDL_Surface* s = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
        8, 0, 0, 0, 0);
    if (!s) return nullptr;
    restoreSurfacePalette(s);
    if (pixels)
        for (int y = 0; y < height; y++)
            memcpy((unsigned char*)s->pixels + s->pitch * y,
                pixels + width * y, width);
    return s;
}

void Video::destroySurface(SDL_Surface* s) { SDL_FreeSurface(s); }

void Video::restoreSurfacePalette(SDL_Surface* s) {
    if (s) SDL_SetColors(s, currentPalette, 0, 256);
    /* Restore currentPalette entries modified by setSurfacePalette */
    if (s_savedStart >= 0 && s_savedLength > 0) {
        memcpy(currentPalette + s_savedStart, s_savedPalEntries + s_savedStart,
            s_savedLength * sizeof(SDL_Color));
        s_savedStart = -1;
    }
}

void Video::setSurfacePalette(SDL_Surface* s, SDL_Color* pal,
    int start, int length) {
    if (s && pal) SDL_SetColors(s, pal, start, length);
    /* Also update currentPalette so flip() uses the remapped colors.
     * Save previous values so restoreSurfacePalette can undo this. */
    if (pal && start >= 0 && length > 0 && start + length <= 256) {
        if (s_savedStart < 0) {  /* save only once per map/restore cycle */
            memcpy(s_savedPalEntries + start, currentPalette + start,
                length * sizeof(SDL_Color));
            s_savedStart = start;
            s_savedLength = length;
        }
        memcpy(currentPalette + start, pal, length * sizeof(SDL_Color));
    }
}

void Video::enableColorKey(SDL_Surface* s, unsigned int idx) {
    if (s) SDL_SetColorKey(s, SDL_SRCCOLORKEY, idx);
}

unsigned int Video::getColorKey(SDL_Surface* s) {
    return s ? s->format->colorkey : 0;
}

void Video::setClipRect(SDL_Surface* s, const SDL_Rect* r) {
    SDL_SetClipRect(s, r);
}

/* -----------------------------------------------------------------------
   Drawing
   ----------------------------------------------------------------------- */
void Video::drawRect(int x, int y, int width, int height,
    int index, bool fill) {
    if (!canvas) return;
    SDL_Rect r = { (Sint16)x, (Sint16)y, (Uint16)width, (Uint16)height };
    if (fill) {
        SDL_FillRect(canvas, &r, (Uint32)index);
    }
    else {
        SDL_Rect t;
        t = { (Sint16)x,           (Sint16)y,           (Uint16)width, 1 };
        SDL_FillRect(canvas, &t, index);
        t = { (Sint16)x,           (Sint16)(y + height - 1), (Uint16)width, 1 };
        SDL_FillRect(canvas, &t, index);
        t = { (Sint16)x,           (Sint16)y,           1, (Uint16)height };
        SDL_FillRect(canvas, &t, index);
        t = { (Sint16)(x + width - 1), (Sint16)y,           1, (Uint16)height };
        SDL_FillRect(canvas, &t, index);
    }
}

void Video::clearScreen(int index) {
    if (canvas) SDL_FillRect(canvas, nullptr, (Uint32)index);
}


void Video::blitSurface(SDL_Surface* src, SDL_Rect* srcRect,
    SDL_Surface* dst, SDL_Rect* dstRect) {
    if (!src || !dst || !src->pixels || !dst->pixels) return;
    int sx = srcRect ? srcRect->x : 0, sy = srcRect ? srcRect->y : 0;
    int sw = srcRect ? srcRect->w : src->w, sh = srcRect ? srcRect->h : src->h;
    int dx = dstRect ? dstRect->x : 0, dy = dstRect ? dstRect->y : 0;
    if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
    if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
    if (dx + sw > dst->w) sw = dst->w - dx;
    if (dy + sh > dst->h) sh = dst->h - dy;
    if (sw <= 0 || sh <= 0) return;
    const Uint8 colorkey = (Uint8)(src->format->colorkey & 0xFF);
    const bool  hasKey = (src->flags & SDL_SRCCOLORKEY) != 0;
    for (int row = 0; row < sh; row++) {
        const Uint8* srcRow = (const Uint8*)src->pixels + (sy + row) * src->pitch + sx;
        Uint8* dstRow = (Uint8*)dst->pixels + (dy + row) * dst->pitch + dx;
        if (hasKey) {
            for (int col = 0; col < sw; col++)
                if (srcRow[col] != colorkey) dstRow[col] = srcRow[col];
        }
        else {
            memcpy(dstRow, srcRow, sw);
        }
    }
}


void XbVideoApplyConfig() {
    s_textureFilter = (g_xbConfig.filterMode == XB_FILTER_SMOOTH) ? 1 : 0;
    build_quad();          /* re-apply aspect mode */
    setup_render_states();
}

/* -----------------------------------------------------------------------
   Stubs
   ----------------------------------------------------------------------- */
void Video::update(SDL_Event* e) { (void)e; }
void Video::setTitle(const char* t) { (void)t; }
void Video::setScaling(int f, scalerType m) { scaleFactor = f; scaleMethod = m; }
void Video::moviePlayback(bool s) { isPlayingMovie = s; }
void Video::findResolutions() { minW = maxW = screenW; minH = maxH = screenH; }
void Video::expose() {}