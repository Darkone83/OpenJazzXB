/**
 * xb_compat.h
 * XbJazz -- Xbox platform compatibility header.
 *
 * VS 2022 supports all C++17 features natively.  NO keyword macros here.
 * This header only provides things the compiler genuinely cannot supply.
 *
 * DO NOT redefine C++ keywords (constexpr, noexcept, nullptr, override,
 * final).  Modern MSVC STL headers (xkeycheck.h) hard-error on this.
 */

#ifndef XB_COMPAT_H
#define XB_COMPAT_H

 /*
	GCC-specific attribute stub.
	psmplug uses __attribute__((packed)) etc. under GCC.
	MSVC errors on __attribute__ syntax; strip it.
 */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

 /*
	__func__ -- C99 predefined identifier.
	MSVC provides __FUNCTION__; define __func__ as an alias if missing.
 */
#ifndef __func__
#define __func__ __FUNCTION__
#endif

 /*
	M_PI -- not always exposed without _USE_MATH_DEFINES.
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#endif /* XB_COMPAT_H */