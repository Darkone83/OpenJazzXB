/**
 * xb_textentry.h
 * Xbox d-pad character cycling text entry widget.
 * Ported from XbTyrian's JE_xboxCycleTextChar pattern.
 */
#pragma once

 /* Character sets */
#define XB_ENTRY_NAME  0   /* A-Z 0-9 space dot dash -- player names    */
#define XB_ENTRY_IP    1   /* 0-9 dot -- IP addresses                   */

/**
 * Run an Xbox text entry widget.
 *
 * @param title      Prompt shown above the field (e.g. "PLAYER NAME")
 * @param buf        Buffer to edit in-place (must be at least maxLen+1 bytes)
 * @param maxLen     Max number of characters (not including null terminator)
 * @param mode       XB_ENTRY_NAME or XB_ENTRY_IP
 * @return E_NONE on confirm (Start/A on last char), E_RETURN on B cancel, E_QUIT on quit
 */
int xbTextEntry(const char* title, char* buf, int maxLen, int mode);