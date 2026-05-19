#pragma once
/**
 * xb_textentry.h
 * XbJazz on-screen text entry widget.
 *
 * Controller-based character-by-character input.
 * Two modes: player name (uppercase) and IP/hostname (lowercase + colon).
 */

#ifndef XB_TEXTENTRY_H
#define XB_TEXTENTRY_H

#define XB_ENTRY_NAME  0   /* " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-"  */
#define XB_ENTRY_IP    1   /* " abcdefghijklmnopqrstuvwxyz0123456789.-:" */

 /**
  * Display an on-screen text entry widget.
  *
  * @param title   Title string shown at top of screen.
  * @param buf     Buffer to edit in-place (null-terminated, pre-filled).
  * @param maxLen  Maximum number of editable characters (excluding null).
  * @param mode    XB_ENTRY_NAME or XB_ENTRY_IP.
  * @return E_NONE on confirm, E_RETURN on cancel, E_QUIT on system quit.
  */
int xbTextEntry(const char* title, char* buf, int maxLen, int mode);

#endif /* XB_TEXTENTRY_H */