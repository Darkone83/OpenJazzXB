/**
 * xb_netlog.cpp -- XbJazz network debug logger.
 *
 * RXDK TU: <xtl.h> first.
 * Uses Win32 CreateFile/WriteFile -- the RXDK CRT stdio is stripped
**/

#include <xtl.h>
#include <stdarg.h>
#include <string.h>
#include "xb_netlog.h"

     /* ── State ───────────────────────────────────────────────────────────────── */

     static HANDLE s_hFile = INVALID_HANDLE_VALUE;
 static int    s_enabled = 0;
 static DWORD  s_startMs = 0;

 /* ── Internal write ──────────────────────────────────────────────────────── */

 static void RawWrite(const char* s) {
     if (s_hFile == INVALID_HANDLE_VALUE) return;
     DWORD written;
     int len = 0;
     while (s[len]) len++;
     WriteFile(s_hFile, s, (DWORD)len, &written, NULL);
 }

 /* ── Packet type name ────────────────────────────────────────────────────── */

 static const char* PacketName(unsigned char type) {
     switch (type) {
     case 0x00: return "MT_G_PROPS";
     case 0x01: return "MT_G_PJOIN";
     case 0x02: return "MT_G_PQUIT";
     case 0x03: return "MT_G_LEVEL";
     case 0x04: return "MT_G_CHECK";
     case 0x05: return "MT_G_SCORE";
     case 0x06: return "MT_G_LTYPE";
     case 0x10: return "MT_L_PROP";
     case 0x11: return "MT_L_GRID";
     case 0x12: return "MT_L_STAGE";
     case 0x20: return "MT_P_ANIMS";
     case 0x21: return "MT_P_TEMP";
     case 0xF0: return "OJXB_HELLO";
     case 0xF1: return "OJXB_ROOMLIST";
     case 0xF2: return "OJXB_JOIN";
     case 0xF3: return "OJXB_ROOMINFO";
     case 0xF4: return "OJXB_START";
     case 0xF5: return "OJXB_MAPSEL";
     default:   return "UNKNOWN";
     }
 }

 /* ── XbNetLog_Open ───────────────────────────────────────────────────────── */

 void XbNetLog_Open(int isHost) {
     if (s_hFile != INVALID_HANDLE_VALUE) return;

     const char* path = isHost
         ? "D:\\openjazzxb_srv.txt"
         : "D:\\openjazzxb_client.txt";

     s_hFile = CreateFileA(
         path,
         GENERIC_WRITE,
         FILE_SHARE_READ,
         NULL,
         CREATE_ALWAYS,
         FILE_ATTRIBUTE_NORMAL,
         NULL);

     if (s_hFile == INVALID_HANDLE_VALUE) return;

     s_startMs = GetTickCount();

     RawWrite("=======================================================\r\n");
     RawWrite(isHost
         ? "  XbJazz Network Log -- HOST / SERVER\r\n"
         : "  XbJazz Network Log -- CLIENT\r\n");
     RawWrite("=======================================================\r\n\r\n");
 }

 /* ── XbNetLog_Enable ─────────────────────────────────────────────────────── */

 void XbNetLog_Enable(int enable) {
     s_enabled = enable;
 }

 /* ── XbNetLog_Write ──────────────────────────────────────────────────────── */

 void XbNetLog_Write(const char* fmt, ...) {
     if (!s_enabled || s_hFile == INVALID_HANDLE_VALUE) return;

     char hdr[32];
     DWORD ms = GetTickCount() - s_startMs;
     wsprintfA(hdr, "[%6lu] ", (unsigned long)ms);
     RawWrite(hdr);

     char body[480];
     va_list args;
     va_start(args, fmt);
     wvsprintfA(body, fmt, args);
     va_end(args);
     RawWrite(body);
     RawWrite("\r\n");
 }

 /* ── XbNetLog_Packet ─────────────────────────────────────────────────────── */

 void XbNetLog_Packet(const char* direction,
     unsigned char type,
     int length) {
     if (!s_enabled || s_hFile == INVALID_HANDLE_VALUE) return;

     char buf[128];
     DWORD ms = GetTickCount() - s_startMs;
     wsprintfA(buf, "[%6lu] %s  0x%02X %-16s  %d bytes\r\n",
         (unsigned long)ms, direction,
         (unsigned int)type, PacketName(type), length);
     RawWrite(buf);
 }

 /* ── XbNetLog_Close ──────────────────────────────────────────────────────── */

 void XbNetLog_Close(void) {
     if (s_hFile == INVALID_HANDLE_VALUE) return;
     RawWrite("\r\n[  END ] Log closed.\r\n");
     CloseHandle(s_hFile);
     s_hFile = INVALID_HANDLE_VALUE;
     s_enabled = 0;
 }