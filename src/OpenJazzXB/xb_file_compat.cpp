/**
 * xb_file_compat.cpp
 * XbJazz -- Xbox file I/O compatibility.
 * Isolated TU: <xtl.h> only.
 *
 * Plain fopen() does not work on Xbox D:\ paths.
 * XbTyrian pattern: CreateFileA + _open_osfhandle + _fdopen.
 */

#include <xtl.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

void xbox_detect_fs(void) { /* no-op for now -- always HDD */ }
int  xbox_fs_writable(void) { return 1; }

FILE* xbox_fopen(const char* path, const char* mode)
{
    if (!path || !mode) return NULL;

    bool want_write = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL);
    bool want_read = (strchr(mode, 'r') != NULL) || !want_write;
    bool want_update = (strchr(mode, '+') != NULL);
    bool want_binary = (strchr(mode, 'b') != NULL);
    bool want_append = (strchr(mode, 'a') != NULL);

    DWORD access = (want_read ? GENERIC_READ : 0)
        | (want_write || want_update ? GENERIC_WRITE : 0);
    DWORD creation = want_write ? CREATE_ALWAYS
        : want_append ? OPEN_ALWAYS
        : OPEN_EXISTING;

    int oflag = want_binary ? _O_BINARY : _O_TEXT;
    if (want_write && !want_update) oflag |= _O_WRONLY | _O_CREAT | _O_TRUNC;
    else if (want_append)                oflag |= _O_WRONLY | _O_CREAT | _O_APPEND;
    else if (want_update && want_write)  oflag |= _O_RDWR | _O_CREAT;
    else if (want_update)                oflag |= _O_RDWR;
    else                                 oflag |= _O_RDONLY;

    HANDLE h = CreateFileA(path, access, FILE_SHARE_READ, NULL,
        creation, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    int fd = _open_osfhandle((long)h, oflag);
    if (fd < 0) { CloseHandle(h); return NULL; }

    FILE* f = _fdopen(fd, mode);
    if (!f) { _close(fd); return NULL; }

    if (want_append) fseek(f, 0, SEEK_END);
    return f;
}

/* -----------------------------------------------------------------------
   Config load/save + video mode helpers -- matches XbTyrian architecture.
   Lives here because xb_file_compat.cpp already has correct RXDK env.
   ----------------------------------------------------------------------- */
#include "xb_config.h"
#include "xb_input.h"

int XbVideoModeAvailable(XbVideoMode mode) {
    DWORD flags = XGetVideoFlags();
    switch (mode) {
    case XB_VIDEO_480I: return 1;
    case XB_VIDEO_480P: return (flags & XC_VIDEO_FLAGS_HDTV_480p) != 0;
    case XB_VIDEO_720P: return (flags & XC_VIDEO_FLAGS_HDTV_720p) != 0;
    default: return 0;
    }
}

XbVideoMode XbVideoResolve(XbVideoMode mode) {
    if (mode != XB_VIDEO_AUTO && XbVideoModeAvailable(mode))
        return mode;
    /* AUTO: prefer 720p, fall back to 480p, then 480i */
    if (XbVideoModeAvailable(XB_VIDEO_720P)) return XB_VIDEO_720P;
    if (XbVideoModeAvailable(XB_VIDEO_480P)) return XB_VIDEO_480P;
    return XB_VIDEO_480I;
}

void XbVideoGetDimensions(XbVideoMode mode, int* w, int* h) {
    if (mode == XB_VIDEO_720P) { *w = 1280; *h = 720; }
    else { *w = 640;  *h = 480; }
}

unsigned int XbVideoPresentFlags(XbVideoMode mode) {
    if (mode == XB_VIDEO_720P) return D3DPRESENTFLAG_PROGRESSIVE | D3DPRESENTFLAG_WIDESCREEN;
    if (mode == XB_VIDEO_480P) return D3DPRESENTFLAG_PROGRESSIVE;
    return 0;
}



void XbConfigLoad(void) {
    XbConfigDefaults();
    FILE* f = xbox_fopen("D:\\xbjazz.cfg", "rb");
    if (!f) {
        /* First run -- write defaults so file exists for next boot */
        XbConfigSave();
        return;
    }
    XbJazzConfig tmp;
    if (fread(&tmp, 1, sizeof(tmp), f) == sizeof(tmp))
        if (tmp.magic == XBJAZZ_CFG_MAGIC) g_xbConfig = tmp;
    fclose(f);
}

void XbConfigSave(void) {
    FILE* f = xbox_fopen("D:\\xbjazz.cfg", "wb");
    if (!f) return;
    fwrite(&g_xbConfig, 1, sizeof(g_xbConfig), f);
    fclose(f);
}