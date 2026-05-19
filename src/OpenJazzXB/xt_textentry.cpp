/**
 * xb_textentry.cpp
 * XbJazz on-screen text entry widget.
 *
 * Layout (320x200 canvas):
 *   Title    -- fontmn2, centered, y=8
 *   Field    -- fontbig chars, centered horizontally, vertically centered
 *               between title and footer. Rect with 10px h-pad, 5px v-pad.
 *   Row 1    -- fontmn2, y=canvasH-18: "up/dn=char" left  "lt/rt=move" right
 *   Row 2    -- fontmn2, y=canvasH-8:  "start=ok"   left  "b=del"      right
 *
 * Controls:
 *   D-pad up/down   -- cycle charset at cursor position
 *   D-pad left/right -- move cursor
 *   A or Start      -- confirm
 *   B               -- delete char at cursor (replace with space)
 */

#include "xb_textentry.h"
#include "controls.h"
#include "font.h"
#include "video.h"
#include "loop.h"
#include "OpenJazz.h"

#include <string.h>


static const char CHARSET_NAME[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
static const char CHARSET_IP[] = " abcdefghijklmnopqrstuvwxyz0123456789.-:";

static int charsetLen(int mode) {
    return mode == XB_ENTRY_NAME
        ? (int)strlen(CHARSET_NAME)
        : (int)strlen(CHARSET_IP);
}

static const char* charset(int mode) {
    return mode == XB_ENTRY_NAME ? CHARSET_NAME : CHARSET_IP;
}

/* Find the charset index for a given character, default to 0 (space). */
static int charIndex(char c, int mode) {
    const char* cs = charset(mode);
    int i;
    for (i = 0; cs[i]; i++) {
        if (cs[i] == c) return i;
    }
    return 0;
}


int xbTextEntry(const char* title, char* buf, int maxLen, int mode) {

    /* Work on a local copy -- only write back on confirm */
    char work[64];
    int  len = maxLen < 63 ? maxLen : 63;
    int  i;

    /* Copy buf into work, pad remainder with spaces */
    strncpy(work, buf, len);
    work[len] = '\0';
    for (i = (int)strlen(work); i < len; i++) work[i] = ' ';
    work[len] = '\0';

    int cursor = 0;
    int csLen = charsetLen(mode);
    const char* cs = charset(mode);

    /* Determine field rendering metrics */
    /* fontbig glyphs are 8px wide, 7px tall based on previous measurements */
    const int slotW = 8;
    const int fontH = 7;
    const int padX = 10;
    const int padY = 5;
    /* Long entries such as SERVER HOST can be wider than the 320px canvas.
     * Render a scrolling window around the cursor instead of drawing off-screen.
     */
    int visibleSlots = len;
    if (visibleSlots > 30)
        visibleSlots = 30;

    const int fieldW = visibleSlots * slotW;
    const int fieldX = (canvasW - fieldW) >> 1;

    /* Vertically centre field between title and safe footer area. */
    const int areaTop = 24;
    const int areaBottom = canvasH - 34;
    const int fieldY = areaTop + ((areaBottom - areaTop - fontH) >> 1);

    bool upHeld = false;
    bool downHeld = false;
    bool leftHeld = false;
    bool rightHeld = false;

    while (true) {

        int ret = loop(NORMAL_LOOP);
        if (ret == E_QUIT) return E_QUIT;

        SDL_Delay(T_MENU_FRAME);

        /* ── Input ────────────────────────────────────────────────────── */

        /* Up -- cycle charset forward at cursor */
        bool upNow = controls.getState(C_UP);
        if (upNow && !upHeld) {
            int ci = charIndex(work[cursor], mode);
            ci = (ci + 1) % csLen;
            work[cursor] = cs[ci];
        }
        upHeld = upNow;

        /* Down -- cycle charset backward at cursor */
        bool downNow = controls.getState(C_DOWN);
        if (downNow && !downHeld) {
            int ci = charIndex(work[cursor], mode);
            ci = (ci + csLen - 1) % csLen;
            work[cursor] = cs[ci];
        }
        downHeld = downNow;

        /* Left -- move cursor back */
        bool leftNow = controls.getState(C_LEFT);
        if (leftNow && !leftHeld) {
            if (cursor > 0) cursor--;
        }
        leftHeld = leftNow;

        /* Right -- move cursor forward */
        bool rightNow = controls.getState(C_RIGHT);
        if (rightNow && !rightHeld) {
            if (cursor < len - 1) cursor++;
        }
        rightHeld = rightNow;

        /* B -- delete (space) at cursor */
        if (controls.release(C_ESCAPE)) {
            work[cursor] = ' ';
        }

        /* A or Start -- confirm */
        if (controls.release(C_ENTER) || controls.release(C_PAUSE)) {
            /* Strip trailing spaces, write back */
            int last = len - 1;
            while (last > 0 && work[last] == ' ') last--;
            work[last + 1] = '\0';
            strncpy(buf, work, len);
            buf[len] = '\0';
            return E_NONE;
        }

        /* ── Render ───────────────────────────────────────────────────── */

        video.clearScreen(0);

        /* Title */
        fontmn2->showString(title, canvasW >> 1, 8, alignX::Center);

        /* Keep the visible window centered around the cursor where possible. */
        int viewStart = 0;
        if (len > visibleSlots) {
            viewStart = cursor - (visibleSlots >> 1);
            if (viewStart < 0)
                viewStart = 0;
            if (viewStart > len - visibleSlots)
                viewStart = len - visibleSlots;
        }

        /* Field background and border */
        video.drawRect(
            fieldX - padX, fieldY - padY,
            fieldW + padX * 2, fontH + padY * 2,
            0, true);
        video.drawRect(
            fieldX - padX, fieldY - padY,
            fieldW + padX * 2, fontH + padY * 2,
            79, false);

        /* Small scroll indicators for long entries. */
        if (viewStart > 0)
            fontmn2->showString("<", fieldX - padX - 8, fieldY);
        if (viewStart + visibleSlots < len)
            fontmn2->showString(">", fieldX + fieldW + padX + 3, fieldY);

        /* Characters */
        for (i = 0; i < visibleSlots; i++) {
            int wi = viewStart + i;
            char ch[2] = { work[wi], '\0' };
            int cx = fieldX + i * slotW;
            fontbig->showString(ch, cx, fieldY);

            /* Cursor underline */
            if (wi == cursor)
                video.drawRect(cx, fieldY + fontH + 1, slotW - 1, 1, 79, true);
        }

        /* Footer row 1 */
        fontmn2->showString("up/dn=char",
            3, canvasH - 24, alignX::Left);
        fontmn2->showString("lt/rt=move",
            canvasW - 3, canvasH - 24, alignX::Right);

        /* Footer row 2 */
        fontmn2->showString("start=ok",
            3, canvasH - 10, alignX::Left);
        fontmn2->showString("b=del",
            canvasW - 3, canvasH - 10, alignX::Right);
    }
}