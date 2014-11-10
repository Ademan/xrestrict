#include <limits.h>
#include <stdio.h>
#include "display.h"

void xlib_find_screen_size(Display * display, Rectangle * size) {
	size->top = size->left = 0;
	size->right = DisplayWidth(display, DefaultScreen(display));
	size->bottom = DisplayHeight(display, DefaultScreen(display));
}

int xlib_get_crtc_regions(Display * display, XRRScreenResources * resources, CRTCRegion * regions, const int max_regions) {
	const CRTCRegion * regions_end = regions + max_regions;
	const RRCrtc * crtcs_end = resources->crtcs + resources->ncrtc;
	RRCrtc * crtc;
	for (crtc = resources->crtcs; crtc < crtcs_end; crtc++) {
		if (regions >= regions_end) {
			return EREGIONS_OVERFLOW;
		}

		XRRCrtcInfo * crtc_info = XRRGetCrtcInfo(display, resources, *crtc);
		if (!crtc_info) {
			return -1; // FIXME: select error code
		}

		if (crtc_info->noutput < 1) {
			continue; // We only care about crtcs that are actually being displayed
		}

		regions->top = crtc_info->y;
		regions->left = crtc_info->x;
		regions->bottom = crtc_info->y + crtc_info->height;
		regions->right = crtc_info->x + crtc_info->width;
		regions++;
#		if DEBUG
			printf("%d(%dx%d)+(%d,%d) mode=%d\n", *crtc, crtc_info->width, crtc_info->height, crtc_info->x, crtc_info->y, crtc_info->mode);
#		endif
		XRRFreeCrtcInfo(crtc_info);
	}
	return crtc - resources->crtcs;
}

int find_containing_crtc(CRTCRegion * regions, const int region_count, const Point * point) {
	for (CRTCRegion * region = regions; region < (regions + region_count); region++) {
		if (region->left <= point->x && point->x <= region->right && \
			region->top <= point->y && point->y <= region->bottom) {
			return (region - regions);
		}
	}
	return -1;
}
