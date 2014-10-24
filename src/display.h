#ifndef XRESTRICT_DISPLAY_H_
#define XRESTRICT_DISPLAY_H_

#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "xrestrict.h"

#define ESCREENINFO_REQUEST_FAILED (-1)
#define ESCREEN_SIZE_NOT_FOUND     (-2)
int find_screen_size(xcb_connection_t * connection, xcb_screen_t * screen, xcb_randr_screen_size_t * size);

#define EREGIONS_OVERFLOW          (-4)
int get_crtc_regions(xcb_connection_t * connection, xcb_screen_t * screen, CRTCRegion * regions, const int max_regions);

#endif /* XRESTRICT_DISPLAY_H_ */
