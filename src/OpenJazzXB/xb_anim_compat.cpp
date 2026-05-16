/**
 * xb_anim_compat.cpp
 * Xbox compatibility wrapper for anim.cpp -> JJ1Level access.
 *
 * anim.cpp cannot include jj1level.h directly because that header chain
 * brings in file.h -> std::unique_ptr -> exception frames -> __CxxFrameHandler3.
 * This wrapper includes jj1level.h in its own TU and exposes a plain C
 * accessor so anim.cpp can call level->getAnim() without the heavy include.
 */

#include "jj1level.h"
#include "anim.h"

Anim* XbLevel_GetAnim(unsigned char id) {
    return level ? level->getAnim(id) : nullptr;
}