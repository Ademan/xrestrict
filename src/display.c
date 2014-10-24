#include <limits.h>
#include <stdio.h>
#include "display.h"

int find_screen_size(xcb_connection_t * connection, xcb_screen_t * screen, xcb_randr_screen_size_t * size) {
	xcb_randr_get_screen_info_cookie_t info_cookie = xcb_randr_get_screen_info(connection, screen->root);
	xcb_randr_get_screen_info_reply_t * info_reply = xcb_randr_get_screen_info_reply(connection, info_cookie, NULL);

	if (!info_reply) {
		fprintf(stderr, "Screen info request failed.\n");
		return -1;
	}

	int screen_size_id = info_reply->sizeID;
	int screen_size_count = info_reply->nSizes;

	xcb_randr_screen_size_iterator_t screen_iter = xcb_randr_get_screen_info_sizes_iterator (info_reply);
	xcb_generic_iterator_t screen_iter_end = xcb_randr_screen_size_end (screen_iter);

	int screen_width = INT_MIN;
	int screen_height = INT_MIN;
	for (int screen_index = 0;
			screen_iter.index < screen_iter_end.index;
			screen_index++, xcb_randr_screen_size_next(&screen_iter)) {

		if (screen_iter.data == NULL) {
			break;
		}

		if (screen_index == screen_size_id) {
			screen_width = screen_iter.data->width;
			screen_height = screen_iter.data->height;
			break;
		}
	}

	if (screen_width == INT_MIN || screen_height == INT_MIN) {
		return ESCREEN_SIZE_NOT_FOUND;
	} else {
		size->width = screen_width;
		size->height = screen_height;
		return 0;
	}
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
