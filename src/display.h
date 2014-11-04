#ifndef XRESTRICT_DISPLAY_H_
#define XRESTRICT_DISPLAY_H_

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "xrestrict.h"

#define ESCREENINFO_REQUEST_FAILED (-1)
#define ESCREEN_SIZE_NOT_FOUND     (-2)
void xlib_find_screen_size(Display * display, Rectangle * size);

#define EREGIONS_OVERFLOW          (-4)
int xlib_get_crtc_regions(Display * display, XRRScreenResources * resources, CRTCRegion * regions, const int max_regions);

#endif /* XRESTRICT_DISPLAY_H_ */
