/*
 * This source code is public domain.
 *
 * Authors: Rani Assaf <rani@magic.metawire.com>,
 *          Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
 *
 * Xbox RXDK port: _WIN32 block patched to use <xtl.h> instead of
 * <windows.h>/<windowsx.h>/<mmsystem.h>.  Everything else unchanged.
 */

#ifndef MP_STDAFX_H
#define MP_STDAFX_H

#ifdef _WIN32

#ifdef _MSC_VER
#pragma warning (disable:4201)
#pragma warning (disable:4514)
#endif

 /* Xbox RXDK: xtl.h covers all Win32 + multimedia types.
  * mmsystem.h / windowsx.h are not available as standalone headers. */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

  /* GlobalAllocPtr/GlobalFreePtr from windowsx.h (not in Xbox RXDK).
   * GHND = GMEM_MOVEABLE|GMEM_ZEROINIT, so calloc is the right match. */
#ifndef GlobalAllocPtr
#  define GlobalAllocPtr(flags, size)  calloc(1, (size))
#  define GlobalFreePtr(ptr)           free(ptr)
#endif

#define srandom(_seed)  srand(_seed)
#define random()        rand()
#define sleep(_ms)      Sleep(_ms)

#undef strcasecmp
#undef strncasecmp
#define strcasecmp(a,b)     _stricmp(a,b)
#define strncasecmp(a,b,c)  _strnicmp(a,b,c)

#ifndef isblank
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif

#else

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef int8_t CHAR;
typedef uint8_t UCHAR;
typedef uint8_t* PUCHAR;
typedef uint16_t USHORT;
typedef uint32_t ULONG;
typedef uint32_t UINT;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int64_t LONGLONG;
typedef int32_t* LPLONG;
typedef uint32_t* LPDWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef uint8_t* LPBYTE;
typedef bool BOOL;
typedef char* LPSTR;
typedef void* LPVOID;
typedef uint16_t* LPWORD;
typedef const char* LPCSTR;
typedef void* PVOID;
typedef void VOID;

inline LONG MulDiv(long a, long b, long c)
{
	return ((uint64_t)a * (uint64_t)b) / c;
}

#define lstrcpynA strncpy
#define lstrcpyA strcpy
#define wsprintfA sprintf

#define WAVE_FORMAT_PCM 1

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#endif /* _WIN32 */

#endif /* MP_STDAFX_H */