/* xb_exit.cpp
 * Xbox RXDK exit -- returns to dashboard via XLaunchNewImage.
 * Isolated TU: <xtl.h> only, no STL headers. Same pattern as xb_audio.cpp.
 */
#include <xtl.h>
#include "xb_exit.h"

void XbReturnToDashboard(void)
{
    XLaunchNewImage(NULL, NULL);
    /* Fallback if launch returns */
    for (;;)
        Sleep(1000);
}