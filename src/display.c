#include <limits.h>
#include <stdio.h>
#include "display.h"

void xlib_find_screen_size(Display * display, Rectangle * size) {
	size->top = size->left = 0;
	size->right = DisplayWidth(display, DefaultScreen(display));
	size->bottom = DisplayHeight(display, DefaultScreen(display));
}

int xlib_get_crtc_output_density(Display * display, XRRScreenResources * resources, CRTCRegion * region) {
	if (region->output == None) {
		return -1;
	}

	// TODO: test what happens accessing None output, may be able to eliminate above test
	XRROutputInfo * output_info = XRRGetOutputInfo(display, resources, region->output);

	if (!output_info) {
		return EOUTPUT_INFO_REQUEST_FAILED;
	}

	region->width = output_info->mm_width;
	region->height = output_info->mm_height;

	XRRFreeOutputInfo(output_info);
	return 0;
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
			return ECRTC_INFO_REQUEST_FAILED;
		}

		if (crtc_info->noutput < 1) {
			// We only care about crtcs that are actually being displayed
			continue;
		} else if (crtc_info->noutput == 1) {
			regions->output = crtc_info->outputs[0];
		} else {
			// Later code ignores regions with None outputs if needed
			regions->output = None;
		}

		regions->crtc = *crtc;

		regions->region.top = crtc_info->y;
		regions->region.left = crtc_info->x;
		regions->region.bottom = crtc_info->y + crtc_info->height;
		regions->region.right = crtc_info->x + crtc_info->width;
		regions++;
#		if DEBUG
			printf("%lu(%dx%d)+(%d,%d) mode=%lu\n", *crtc, crtc_info->width, crtc_info->height, crtc_info->x, crtc_info->y, crtc_info->mode);
#		endif
		XRRFreeCrtcInfo(crtc_info);
	}
	return crtc - resources->crtcs;
}

int find_containing_crtc(CRTCRegion * regions, const int region_count, const Point * point) {
	for (int i = 0; i < region_count; i++) {
		Rectangle region = regions[i].region;
		if (region.left <= point->x && point->x <= region.right && \
			region.top <= point->y && point->y <= region.bottom) {
			return i;
		}
	}
	return -1;
}
