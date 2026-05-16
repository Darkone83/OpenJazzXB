/**
 * xb_d3d_init.cpp
 * XbJazz -- D3D8 device init. Mirrors XbTyrian pattern:
 *   resolve video mode, set backbuffer dimensions, set present flags.
 */

#include <xtl.h>
#include "xb_config.h"

 /* Exported -- video.cpp reads these to build the quad */
int g_bbWidth = 640;
int g_bbHeight = 480;

void XbFlash(DWORD colour, DWORD ms) {
    D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, colour & 0x00FFFFFF, 1.0f, 0);
    D3DDevice_Swap(0);
    Sleep(ms);
}

void XbD3DInit(void) {
    /* Resolve video mode from config -- same pattern as XbTyrian */
    XbVideoMode mode = XbVideoResolve(g_xbConfig.videoMode);
    XbVideoGetDimensions(mode, &g_bbWidth, &g_bbHeight);

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = (DWORD)g_bbWidth;
    pp.BackBufferHeight = (DWORD)g_bbHeight;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.EnableAutoDepthStencil = FALSE;
    pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.Flags = XbVideoPresentFlags(mode);

    Direct3DCreate8(D3D_SDK_VERSION);
    HRESULT hr = Direct3D_CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pp, NULL
    );

    /* Fall back to safe 480i if requested mode failed */
    if (FAILED(hr) && mode != XB_VIDEO_480I) {
        g_bbWidth = 640; g_bbHeight = 480;
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = 480;
        pp.Flags = 0;
        Direct3D_CreateDevice(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &pp, NULL
        );
    }

    Sleep(250);
}