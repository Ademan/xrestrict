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
	fprintf(stderr, "Usage: %s -d|--device DEVICEID [-c|--crtc CRTCINDEX] [--dry]\n", cmd);
}

#define PARSE_NONE      0
#define PARSE_DEVICEID  1
#define PARSE_CRTCINDEX 2

int main(int argc, char ** argv) {
	if (argc < 3) {
		print_usage(argv[0]);
		return -1;
	}

	int device_id = INVALID_DEVICE_ID;
	int crtc_index = 0;
	bool dry_run = false;

	// TODO: break out command line parsing into another function
	//       or use getopt
	int parsing_state = PARSE_NONE;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (parsing_state != PARSE_NONE) {
				print_usage(argv[0]);
				return -1;
			}
			if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
				parsing_state = PARSE_DEVICEID;
			} else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--crtc") == 0) {
				parsing_state = PARSE_CRTCINDEX;
			} else if (strcmp(argv[i], "--dry") == 0) {
				dry_run = true;
			} else {
				print_usage(argv[0]);
				return -1;
			}
		} else {
			if (parsing_state == PARSE_DEVICEID) {
				char * invalid;
				device_id = strtol(argv[i], &invalid, 10);
				if (invalid && *invalid != '\0') {
					fprintf(stderr, "Failed to parse device id \"%s\".\n", argv[i]);
					print_usage(argv[0]);
					return -1;
				}
				parsing_state = PARSE_NONE;
			} else if (parsing_state == PARSE_CRTCINDEX) {
				char * invalid;
				crtc_index = strtol(argv[i], &invalid, 10);
				if (invalid && *invalid != '\0') {
					fprintf(stderr, "Failed to parse crtc index \"%s\".\n", argv[i]);
					print_usage(argv[0]);
					return -1;
				}
				parsing_state = PARSE_NONE;
			} else {
				print_usage(argv[0]);
				return -1;
			}
		}
	}

	if (parsing_state != PARSE_NONE) {
		print_usage(argv[0]);
		return -1;
	}

	if (device_id < 0) {
		fprintf(stderr, "DEVICEID must be a positive integer\n");
		print_usage(argv[0]);
		return -1;
	}

	Display * display = XOpenDisplay(NULL);

	if (!display) {
		fprintf(stderr, "Failed to open display.\n");
		return -1;
	}

	XIDeviceInfo * device;
	ValuatorIndices valuator_indices;
	int device_count;

	if (device_id != INVALID_DEVICE_ID) {
		device = XIQueryDevice(display, device_id, &device_count);
		if (!device) {
			XCloseDisplay(display);
			fprintf(stderr, "Failed to query device %d.\n", device_id);
			return -1; // TODO: select error code
		}

		int valuator_result = xi2_device_info_find_xy_valuators(display, device, &valuator_indices);
		
		if (valuator_result) {
			XCloseDisplay(display);
			fprintf(stderr, "Failed to find absolute X and Y valuators for device %d.\n", device_id);
			return -1; // TODO: select error code
		}
	} else {
		XCloseDisplay(display);
		fprintf(stderr, "Currently do not support device id discovery, please specify device id with -d.\n");
		fprintf(stderr, "Device ID may be discovered using the `xinput` utility.\n");
		return -1;

		// TODO: implement device id discovery integrated with crtc selection
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

	if (crtc_index >= region_count) {
		XCloseDisplay(display);
		fprintf(stderr, "CRTC index %d greater than highest index available %d.\n", crtc_index, region_count - 1);
		return -1;
	}

	//TODO: sort crtcs?

	// TODO: pull config from command line
	CTMConfiguration config = {
		.type = CTM_Fit,
		.affinity = {
			.vertical = VA_Top,
			.horizontal = HA_Left
		}
	};

	CRTCRegion * region = crtc_regions + crtc_index;

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
