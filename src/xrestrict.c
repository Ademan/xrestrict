#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

#include "input.h"
#include "display.h"
#include "xrestrict.h"

#define INVALID_DEVICE_ID -1

void calc_matrix(const XID deviceid, const CTMConfiguration * config, Rectangle * screen_size, const CRTCRegion * region, const PointerRegion * pointer_region, float * matrix) {
	Rectangle scaled, aligned;

	rectangle_scale_preserve_aspect(region, pointer_region, config->type, &scaled);

	rectangle_align(region, &scaled, &config->affinity, &aligned);

	calculate_coordinate_transform_matrix(&aligned, screen_size, matrix);
}

void print_usage(char * cmd) {
	// TODO: would like to eventually allow deviceid discovery
	//       based on pointer grab
	fprintf(stderr, "Usage: %s -d DEVICEID [-c CRTCINDEX][-f] [--dry]\n", cmd);
	fprintf(stderr, "   or: %s -i [-d DEVICEID] [-c CRTCINDEX][-f] [--dry]\n\n", cmd);

	fprintf(stderr, "\t-d DEVICEID, --device DEVICEID\n");
	fprintf(stderr, "\t\t\t\tSpecify the XID of the XInput2 device to modify.\n");
	fprintf(stderr, "\t-c CRTCID, --device CRTCID\n");
	fprintf(stderr, "\t\t\t\tThe CRTC to restrict the device to.\n");
	fprintf(stderr, "\t-i, --interactive\tInteractively determine the monitor and input device to use.\n");
	fprintf(stderr, "\t-f, --full\t\tUse the full screen area.\n");
	fprintf(stderr, "\t--dry\t\t\tOutput the \"Coordinate Transformation Matrix\" instead of setting it.\n");
	fprintf(stderr, "\nAlignment Control:\n");
	fprintf(stderr, "\t-t, --top\t\tAlign input region to top of screen (Default).\n");
	fprintf(stderr, "\t-l, --left\t\tAlign input region to left of screen (Default).\n");
	fprintf(stderr, "\t-b, --bottom\t\tAlign input region to bottom of screen.\n");
	fprintf(stderr, "\t-r, --right\t\tAlign input region to right of screen.\n");
	fprintf(stderr, "\nScaling Control:\n");
	fprintf(stderr, "\t-w, --match-width\tScale input region to match width of target region.\n");
	fprintf(stderr, "\t-H, --match-height\tScale input region to match height of target region.\n");
	fprintf(stderr, "\t--fit\t\t\tScale input region to completely contain target region (Default).\n");
}

#define PARSE_NONE      0
#define PARSE_DEVICEID  1
#define PARSE_CRTCINDEX 2

int main(int argc, char ** argv) {
	if (argc < 2) {
		print_usage(argv[0]);
		return -1;
	}

	int device_id = INVALID_DEVICE_ID;
	int crtc_index = 0;
	bool dry_run = false;
	bool full_screen = false;
	bool interactive = false;

	// TODO: pull type from command line
	CTMConfiguration config = {
		.type = CTM_Fit,
		.affinity = {
			.vertical = VA_Top,
			.horizontal = HA_Left
		}
	};

	// TODO: break out command line parsing into another function
	//       or use getopt
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
			char * invalid;
			if (++i >= argc) {
				print_usage(argv[0]);
				return -1;
			}

			device_id = strtol(argv[i], &invalid, 10);
			if (invalid && *invalid != '\0') {
				fprintf(stderr, "Failed to parse device id \"%s\".\n", argv[i]);
				print_usage(argv[0]);
				return -1;
			}
		} else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--crtc") == 0) {
			char * invalid;
			if (++i >= argc) {
				print_usage(argv[0]);
				return -1;
			}

			crtc_index = strtol(argv[i], &invalid, 10);
			if (invalid && *invalid != '\0') {
				fprintf(stderr, "Failed to parse crtc index \"%s\".\n", argv[i]);
				print_usage(argv[0]);
				return -1;
			}
		} else if (strcmp(argv[i], "--dry") == 0) {
			dry_run = true;
		} else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--full") == 0) {
			full_screen = true;
		} else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--top") == 0) {
			config.affinity.vertical = VA_Top;
		} else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bottom") == 0) {
			config.affinity.vertical = VA_Bottom;
		} else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--left") == 0) {
			config.affinity.horizontal = HA_Left;
		} else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--right") == 0) {
			config.affinity.horizontal = HA_Right;
		} else if (strcmp(argv[i], "--fit") == 0) {
			config.type = CTM_Fit;
		} else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--match-width") == 0) {
			config.type = CTM_MatchWidth;
		} else if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--match-height") == 0) {
			config.type = CTM_MatchHeight;
		} else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
			interactive = true;
		} else {
			print_usage(argv[0]);
			return -1;
		}
	}

	Display * display = XOpenDisplay(NULL);

	if (!display) {
		fprintf(stderr, "Failed to open display.\n");
		return -1;
	}

	Rectangle screen_size;

	xlib_find_screen_size(display, &screen_size);

	CRTCRegion crtc_regions[10];

	XRRScreenResources * resources = XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));

	if (!resources) {
		XCloseDisplay(display);
		fprintf(stderr, "Failed to retrieve screen resources for monitor information.\n");
		return -1;
	}

	int region_count = xlib_get_crtc_regions(display, resources, crtc_regions, 10);
	XRRFreeScreenResources(resources);

	if (region_count < 0) {
		XCloseDisplay(display);
		fprintf(stderr, "Failed to retrieve crtc region information.\n");
		return -1;
	}

	ValuatorIndices valuator_indices = {0};
	int device_count;

	if (interactive) {
		// FIXME: we don't actually handle MPX
		XID pointerid = 0;

		XIDeviceInfo * info = XIQueryDevice(display, XIAllDevices, &device_count);

		if (!info) {
			XCloseDisplay(display);
			fprintf(stderr, "Failed to query input devices.\n");
			return -1;
		}

		const XIDeviceInfo * info_end = info + device_count;

		if (xi2_find_master_pointers(info, info_end, &pointerid, 1) < 0) {
			XCloseDisplay(display);
			fprintf(stderr, "xrestrict only functions correctly in single master pointer environments.\n");
			return -1;
		}

		XIFreeDeviceInfo(info);

		Point point;

		printf("Please use the device you wish to configure, and click on the monitor you wish to use.\n");
		if (xi2_pointer_get_next_click(display, &pointerid, &point)) {
			XCloseDisplay(display);
			fprintf(stderr, "Failed to use pointer grab to determine CRTC and device id.\n");
			return -1;
		}

		crtc_index = find_containing_crtc(crtc_regions, region_count, &point);
		if (crtc_index < 0) {
			XCloseDisplay(display);
			fprintf(stderr, "Click not in recognized CRTC.\n");
			return -1;
		}

		if (device_id == INVALID_DEVICE_ID) {
			device_id = pointerid;
		}
	}

	if (device_id < 0) {
		fprintf(stderr, "DEVICEID must be a positive integer\n");
		print_usage(argv[0]);
		return -1;
	}

	XIDeviceInfo * device = XIQueryDevice(display, device_id, &device_count);
	if (!device) {
		XCloseDisplay(display);
		fprintf(stderr, "Failed to query device %d.\n", device_id);
		return -1;
	}

	int valuator_result = xi2_device_info_find_xy_valuators(display, device, &valuator_indices);
	
	if (valuator_result) {
		XCloseDisplay(display);
		fprintf(stderr, "Failed to find absolute X and Y valuators for device %d.\n", device_id);
		return -1;
	}

	if (crtc_index >= region_count) {
		XCloseDisplay(display);
		fprintf(stderr, "CRTC index %d greater than highest index available %d.\n", crtc_index, region_count - 1);
		return -1;
	}

	//TODO: sort crtcs?

	CRTCRegion * region;
   
	if (full_screen) {
		region = &screen_size;
	} else {
		region = crtc_regions + crtc_index;
	}

	PointerRegion pointer_region;
	int region_result = xi2_device_get_region(device, &valuator_indices, &pointer_region);

	if (region_result) {
		XCloseDisplay(display);
		fprintf(stderr, "Failed to retrieve region from device.\n");
		return -1;
	}

	float matrix[9] = {0.0};

	calc_matrix(device_id, &config, &screen_size, region, &pointer_region, matrix);

	if (!dry_run) {
		int set_matrix_results = xi2_device_set_matrix(display, device_id, matrix);

		if (set_matrix_results) {
			XCloseDisplay(display);
			fprintf(stderr, "Failed to set Coordinate Transformation Matrix for device %d.\n", device_id);
			return -1;
		}

		set_matrix_results =  xi2_device_check_matrix(display, device_id, matrix);

		if (set_matrix_results) {
			XCloseDisplay(display);
			fprintf(stderr, "Failed to set Coordinate Transformation Matrix for device %d.\n", device_id);
			return -1;
		}
	} else {
		printf("Coordinate Transformation Matrix = %f", matrix[0]);
		for (float * x = matrix + 1; x < (matrix + 9); x++) {
			printf(" %f", *x);
		}
		printf("\n");
	}

	return 0;
}
