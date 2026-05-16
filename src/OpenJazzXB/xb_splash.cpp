/**
 * xb_splash.cpp
 * XbJazz -- RXDK + Darkone83 intro splash screens.
 * Isolated TU: <xtl.h> only.
 *
 * Copied verbatim from XbTyrian pattern.
 * KEY: Xbox D3D8 linear textures use PIXEL-SPACE UVs (0..width, 0..height),
 *      NOT normalised 0..1. That was the bug causing black screens.
 */

#include <xtl.h>
extern int g_bbWidth;
extern int g_bbHeight;
#include "rxdk_splash.h"
#include "darkone83_splash.h"
#include "xb_splash.h"

struct SplashVtx { float x, y, z, rhw, u, v; };
#define SPLASH_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

static void SetSplashRenderState(void)
{
    D3DDevice_SetRenderState(D3DRS_ZENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_LIGHTING, FALSE);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    D3DDevice_SetVertexShader(SPLASH_FVF);
}

static void BuildQuad(SplashVtx quad[4], int srcW, int srcH)
{
    /* Use actual backbuffer size -- matches XbTyrian pattern */
    const float x0 = 0.0f, y0 = 0.0f;
    const float x1 = (float)g_bbWidth, y1 = (float)g_bbHeight;
    const float u1 = (float)srcW, v1 = (float)srcH;

    quad[0].x = x0; quad[0].y = y0; quad[0].z = 0; quad[0].rhw = 1; quad[0].u = 0;  quad[0].v = 0;
    quad[1].x = x1; quad[1].y = y0; quad[1].z = 0; quad[1].rhw = 1; quad[1].u = u1; quad[1].v = 0;
    quad[2].x = x0; quad[2].y = y1; quad[2].z = 0; quad[2].rhw = 1; quad[2].u = 0;  quad[2].v = v1;
    quad[3].x = x1; quad[3].y = y1; quad[3].z = 0; quad[3].rhw = 1; quad[3].u = u1; quad[3].v = v1;
}

/* -----------------------------------------------------------------------
   RXDK splash  (uses RXDKSplashRun struct directly)
   ----------------------------------------------------------------------- */
static void ShowRXDKSplash(DWORD holdMs)
{
    D3DTexture* tex = D3DDevice_CreateTexture2(
        RXDK_SPLASH_WIDTH, RXDK_SPLASH_HEIGHT,
        1, 1, 0, D3DFMT_LIN_X8R8G8B8, D3DRTYPE_TEXTURE);
    if (!tex) return;

    SplashVtx quad[4];
    BuildQuad(quad, RXDK_SPLASH_WIDTH, RXDK_SPLASH_HEIGHT);
    SetSplashRenderState();

    for (int phase = 0; phase < 3; ++phase)
    {
        const int frames = (phase == 1) ? 1 : 16;
        for (int frame = 0; frame < frames; ++frame)
        {
            int brightness;
            if (phase == 0) brightness = frame * 16;
            else if (phase == 1) brightness = 255;
            else                 brightness = 255 - (frame * 16);
            if (brightness < 0)   brightness = 0;
            if (brightness > 255) brightness = 255;

            D3DLOCKED_RECT lr;
            if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); return; }

            unsigned int* dstBase = (unsigned int*)lr.pBits;
            const int dstPitch = lr.Pitch / sizeof(unsigned int);
            int x = 0, y = 0;

            for (unsigned int i = 0; i < RXDK_SPLASH_RLE_COUNT; ++i)
            {
                unsigned int count = rxdk_splash_rle[i].count;
                const unsigned int c = rxdk_splash_rle[i].color;

                unsigned int r = ((c >> 11) & 0x1f); r = (r << 3) | (r >> 2);
                unsigned int g = ((c >> 5) & 0x3f); g = (g << 2) | (g >> 4);
                unsigned int b = (c & 0x1f); b = (b << 3) | (b >> 2);

                r = (r * brightness) >> 8;
                g = (g * brightness) >> 8;
                b = (b * brightness) >> 8;

                const unsigned int out = (r << 16) | (g << 8) | b;

                while (count > 0 && y < RXDK_SPLASH_HEIGHT)
                {
                    dstBase[y * dstPitch + x] = out;
                    ++x;
                    if (x >= RXDK_SPLASH_WIDTH) { x = 0; ++y; }
                    --count;
                }
            }

            tex->UnlockRect(0);

            D3DDevice_BeginScene();
            D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
            SetSplashRenderState();
            D3DDevice_SetTexture(0, tex);
            D3DDevice_DrawVerticesUP(D3DPT_TRIANGLESTRIP, 4, quad, sizeof(SplashVtx));
            D3DDevice_EndScene();
            D3DDevice_Swap(0);

            Sleep(phase == 1 ? holdMs : 33);
        }
    }

    tex->Release();
    D3DDevice_SetTexture(0, NULL);
}

/* -----------------------------------------------------------------------
   Darkone83 splash  (uses Darkone83SplashRun struct directly)
   ----------------------------------------------------------------------- */
static void ShowDarkone83Splash(DWORD holdMs)
{
    D3DTexture* tex = D3DDevice_CreateTexture2(
        DARKONE83_SPLASH_WIDTH, DARKONE83_SPLASH_HEIGHT,
        1, 1, 0, D3DFMT_LIN_X8R8G8B8, D3DRTYPE_TEXTURE);
    if (!tex) return;

    SplashVtx quad[4];
    BuildQuad(quad, DARKONE83_SPLASH_WIDTH, DARKONE83_SPLASH_HEIGHT);
    SetSplashRenderState();

    for (int phase = 0; phase < 3; ++phase)
    {
        const int frames = (phase == 1) ? 1 : 16;
        for (int frame = 0; frame < frames; ++frame)
        {
            int brightness;
            if (phase == 0) brightness = frame * 16;
            else if (phase == 1) brightness = 255;
            else                 brightness = 255 - (frame * 16);
            if (brightness < 0)   brightness = 0;
            if (brightness > 255) brightness = 255;

            D3DLOCKED_RECT lr;
            if (FAILED(tex->LockRect(0, &lr, NULL, 0))) { tex->Release(); return; }

            unsigned int* dstBase = (unsigned int*)lr.pBits;
            const int dstPitch = lr.Pitch / sizeof(unsigned int);
            int x = 0, y = 0;

            for (unsigned int i = 0; i < DARKONE83_SPLASH_RLE_COUNT; ++i)
            {
                unsigned int count = darkone83_splash_rle[i].count;
                const unsigned int c = darkone83_splash_rle[i].color;

                unsigned int r = ((c >> 11) & 0x1f); r = (r << 3) | (r >> 2);
                unsigned int g = ((c >> 5) & 0x3f); g = (g << 2) | (g >> 4);
                unsigned int b = (c & 0x1f); b = (b << 3) | (b >> 2);

                r = (r * brightness) >> 8;
                g = (g * brightness) >> 8;
                b = (b * brightness) >> 8;

                const unsigned int out = (r << 16) | (g << 8) | b;

                while (count > 0 && y < DARKONE83_SPLASH_HEIGHT)
                {
                    dstBase[y * dstPitch + x] = out;
                    ++x;
                    if (x >= DARKONE83_SPLASH_WIDTH) { x = 0; ++y; }
                    --count;
                }
            }

            tex->UnlockRect(0);

            D3DDevice_BeginScene();
            D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
            SetSplashRenderState();
            D3DDevice_SetTexture(0, tex);
            D3DDevice_DrawVerticesUP(D3DPT_TRIANGLESTRIP, 4, quad, sizeof(SplashVtx));
            D3DDevice_EndScene();
            D3DDevice_Swap(0);

            Sleep(phase == 1 ? holdMs : 33);
        }
    }

    tex->Release();
    D3DDevice_SetTexture(0, NULL);
}

/* -----------------------------------------------------------------------
   Public entry point
   ----------------------------------------------------------------------- */
void XbJazzShowSplashes(void)
{
    ShowRXDKSplash(950);
    ShowDarkone83Splash(950);
}