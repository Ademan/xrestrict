#include <limits.h>
#include <stdio.h>
#include "display.h"

void find_screen_size_xlib(Display * display, xcb_randr_screen_size_t * size) {
	size->width = DisplayWidth(display, DefaultScreen(display));
	size->height = DisplayHeight(display, DefaultScreen(display));
}

int get_crtc_regions(xcb_connection_t * connection, xcb_screen_t * screen, CRTCRegion * regions, const int max_regions) {
	xcb_randr_get_screen_resources_cookie_t screen_resources_cookie = xcb_randr_get_screen_resources(connection, screen->root);

	xcb_randr_get_screen_resources_reply_t * screen_resources = xcb_randr_get_screen_resources_reply(connection,
			screen_resources_cookie, NULL);

	CRTCRegion * region = regions;
	CRTCRegion * regions_end = regions + max_regions;

	if (!screen_resources) {
		return -1;
	}

	xcb_randr_crtc_t * crtc = xcb_randr_get_screen_resources_crtcs (screen_resources);
	int crtc_count = xcb_randr_get_screen_resources_crtcs_length (screen_resources);

	for (int i = 0; i < crtc_count; i++, crtc++) {
		if (region >= regions_end) {
			return EREGIONS_OVERFLOW;
		}

		xcb_randr_get_crtc_info_cookie_t crtc_info_cookie = xcb_randr_get_crtc_info (connection, *crtc, XCB_CURRENT_TIME);
		xcb_randr_get_crtc_info_reply_t * crtc_reply = xcb_randr_get_crtc_info_reply (connection, crtc_info_cookie, NULL);

		if (crtc_reply) {
#			if DEBUG
					printf("%d(%dx%d)+(%d,%d) mode=%d ", *crtc, crtc_reply->width, crtc_reply->height, crtc_reply->x, crtc_reply->y, crtc_reply->mode);
#			endif
			if (crtc_reply->width > 0 && crtc_reply->height > 0) {
				region->top = crtc_reply->y;
				region->left = crtc_reply->x;
				region->bottom = crtc_reply->y + crtc_reply->height;
				region->right = crtc_reply->x + crtc_reply->width;
				region++;
			}
		}
	}
	return region - regions;
}
