/**
 * platforms.h
 * XbJazz port -- Xbox platform only.
 *
 * Defines DefaultPlatform as a typedef for XboxPlatform so that the
 * original platform_interface.cpp (which calls new DefaultPlatform())
 * compiles without modification.
 */

#ifndef _PLATFORMS_H
#define _PLATFORMS_H

#include "OpenJazz.h"
#include "platform_interface.h"
#include "xb_platform.h"

 /* DefaultPlatform is XboxPlatform -- platform_interface.cpp uses this name */
typedef XboxPlatform DefaultPlatform;

EXTERN IPlatform* platform;

#endif /* _PLATFORMS_H */