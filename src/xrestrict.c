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
#define MAX_ABSOLUTE_POINTERS 16
#define MAX_CRTC 10

void calc_matrix(const XID deviceid, const CTMConfiguration * config, Rectangle * screen_size, const CRTCRegion * crtc, const Rectangle * input_region, float * matrix) {
	Rectangle scaled, aligned;

	rectangle_scale_preserve_aspect(&(crtc->region), input_region, config->type, &scaled);

	rectangle_align(&(crtc->region), &scaled, &config->affinity, &aligned);

	calculate_coordinate_transform_matrix(&aligned, screen_size, matrix);
}

void print_usage(FILE * file, char * cmd) {
	fprintf(file, "Usage: %s -d DEVICEID [-c CRTCINDEX][-f] [--dry]\n", cmd);
	fprintf(file, "   or: %s -i|-I [-d DEVICEID] [-c CRTCINDEX][-f] [--dry]\n\n", cmd);

	fprintf(file, "\t-d DEVICEID, --device DEVICEID\n");
	fprintf(file, "\t\t\t\tSpecify the XID of the XInput2 device to modify.\n");
	fprintf(file, "\t-c CRTCID, --device CRTCID\n");
	fprintf(file, "\t\t\t\tThe CRTC to restrict the device to.\n");
	fprintf(file, "\t-i, --interactive\tInteractively determine the monitor and input device to use.\n");
	fprintf(file, "\t-I, --interactive-identity\n");
	fprintf(file, "\t\t\t\tSame as -i but prior to engaging interactive selection, reverts all Coordinate Transformation Matrices to identity and attempts to restore them afterwards.\n");
	fprintf(file, "\t-f, --full\t\tUse the full screen area.\n");
	fprintf(file, "\t--dry\t\t\tOutput the \"Coordinate Transformation Matrix\" instead of setting it.\n");
	fprintf(file, "\nAlignment Control:\n");
	fprintf(file, "\t-H, --horiztontal left|center|right\n");
	fprintf(file, "\t\t\t\tAlign input region horizontally (Default: left).\n");
	fprintf(file, "\t-V, --vertical top|center|bottom\n");
	fprintf(file, "\t\t\t\tAlign input region vertically (Default: top).\n");
	fprintf(file, "\t-t, --top\t\tAlign input region to top of screen.\n");
	fprintf(file, "\t-l, --left\t\tAlign input region to left of screen.\n");
	fprintf(file, "\t-b, --bottom\t\tAlign input region to bottom of screen.\n");
	fprintf(file, "\t-r, --right\t\tAlign input region to right of screen.\n");
	fprintf(file, "\nScaling Control:\n");
	fprintf(file, "\t-o, --one\t\tAttempt to discover device and monitor size such that one centimeter on the device corresponds to a centimeter on the monitor.\n");
	fprintf(file, "\t-w, --match-width\tScale input region to match width of target region.\n");
	fprintf(file, "\t-H, --match-height\tScale input region to match height of target region.\n");
	fprintf(file, "\t--fit\t\t\tScale input region to completely contain target region (Default).\n");
}

#define PARSE_NONE      0
#define PARSE_DEVICEID  1
#define PARSE_CRTCINDEX 2

int main(int argc, char ** argv) {
	if (argc < 2) {
		print_usage(stderr, argv[0]);
		return -1;
	}

	int device_id = INVALID_DEVICE_ID;
	int crtc_index = 0;
	bool dry_run = false;
	bool full_screen = false;
	bool interactive = false;
	bool set_identity = false;
	bool one_to_one = false;

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
				print_usage(stderr, argv[0]);
				return -1;
			}

			device_id = strtol(argv[i], &invalid, 10);
			if (invalid && *invalid != '\0') {
				fprintf(stderr, "Failed to parse device id \"%s\".\n", argv[i]);
				print_usage(stderr, argv[0]);
				return -1;
			}
		} else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--crtc") == 0) {
			char * invalid;
			if (++i >= argc) {
				print_usage(stderr, argv[0]);
				return -1;
			}

			crtc_index = strtol(argv[i], &invalid, 10);
			if (invalid && *invalid != '\0') {
				fprintf(stderr, "Failed to parse crtc index \"%s\".\n", argv[i]);
				print_usage(stderr, argv[0]);
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
		} else if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--horizontal") == 0) {
			if (++i >= argc) {
				print_usage(stderr, argv[0]);
				return -1;
			}

			if (strcmp(argv[i], "left") == 0) {
				config.affinity.horizontal = HA_Left;
			} else if (strcmp(argv[i], "center") == 0) {
				config.affinity.horizontal = HA_Centered;
			} else if (strcmp(argv[i], "right") == 0) {
				config.affinity.horizontal = HA_Right;
			} else {
				fprintf(stderr, "Unknown horizontal alignment \"%s\".\n", argv[i]);
				print_usage(stderr, argv[0]);
				return -1;
			}
		} else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--vertical") == 0) {
			if (++i >= argc) {
				print_usage(stderr, argv[0]);
				return -1;
			}

			if (strcmp(argv[i], "top") == 0) {
				config.affinity.vertical = VA_Top;
			} else if (strcmp(argv[i], "center") == 0) {
				config.affinity.vertical = VA_Centered;
			} else if (strcmp(argv[i], "bottom") == 0) {
				config.affinity.vertical = VA_Bottom;
			} else {
				fprintf(stderr, "Unknown vertical alignment \"%s\".\n", argv[i]);
				print_usage(stderr, argv[0]);
				return -1;
			}
		} else if (strcmp(argv[i], "--fit") == 0) {
			config.type = CTM_Fit;
		} else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--match-width") == 0) {
			config.type = CTM_MatchWidth;
		} else if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--match-height") == 0) {
			config.type = CTM_MatchHeight;
		} else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
			interactive = true;
		} else if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--interactive-identity") == 0) {
			interactive = true;
			set_identity = true;
		} else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--one") == 0) {
			one_to_one = true;
			config.type = CTM_None;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			print_usage(stdout, argv[0]);
			return 0;
		} else {
			print_usage(stderr, argv[0]);
			return -1;
		}
	}

	Display * display = XOpenDisplay(NULL);

	if (!display) {
		fprintf(stderr, "Failed to open display.\n");
		return -1;
	}

	CRTCRegion screen_size;

	xlib_find_screen_size(display, &(screen_size.region));

	CRTCRegion crtc_regions[MAX_CRTC];

	XRRScreenResources * resources = XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));

	if (!resources) {
		XCloseDisplay(display);
		fprintf(stderr, "Failed to retrieve screen resources for monitor information.\n");
		return -1;
	}

	int region_count = xlib_get_crtc_regions(display, resources, crtc_regions, MAX_CRTC);
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

		XID absolute_pointers[MAX_ABSOLUTE_POINTERS];
		float absolute_pointer_matrices[MAX_ABSOLUTE_POINTERS][9];
		int absolute_pointer_count = 0;

		if (set_identity) {
			absolute_pointer_count = xi2_find_absolute_pointers(display, info, info_end, absolute_pointers, MAX_ABSOLUTE_POINTERS);

			if (absolute_pointer_count == EDEVICES_OVERFLOW) {
				XCloseDisplay(display);
				fprintf(stderr, "More than %d absolute pointers detected, aborting.\n", MAX_ABSOLUTE_POINTERS);
				return -1;
			} else if (absolute_pointer_count < 0) {
				XCloseDisplay(display);
				fprintf(stderr, "Error");
				return -1;
			}

			int current_pointer = 0;
			for (int i = 0; i < absolute_pointer_count; i++) {
				if (!xi2_device_get_matrix(display, absolute_pointers[i], absolute_pointer_matrices[current_pointer])) {
					absolute_pointers[current_pointer] = absolute_pointers[i];
					current_pointer++;
				} 
			}
			absolute_pointer_count = current_pointer;

			for (int i = 0; i < absolute_pointer_count; i++) {
				if (xi2_device_set_matrix(display, absolute_pointers[i], identity)) {
					fprintf(stderr, "Error setting Coordinate Transformation Matrix for device %lu, attempting to revert.\n", absolute_pointers[i]);

					// Attempt to revert matrices for all devices we've touched
					// including this one
					for (; i >= 0; i--) {
						if (xi2_device_set_matrix(display, absolute_pointers[i], absolute_pointer_matrices[i])) {
							fprintf(stderr, "Error reverting the Coordinate Transformation matrix for device %lu. It was [", absolute_pointers[i]);
							for (int j = 0; j < 9; j++) {
								fprintf(stderr, " %f", absolute_pointer_matrices[i][j]);
							}
							fprintf(stderr, " ]\n");
						}
					}
					XIFreeDeviceInfo(info);
					XCloseDisplay(display);
					return -1;
				}
			}
		}

		XIFreeDeviceInfo(info);

		Point point;

		printf("Please use the device you wish to configure, and click on the monitor you wish to use.\n");
		if (xi2_pointer_get_next_click(display, &pointerid, &point)) {
			XCloseDisplay(display);
			fprintf(stderr, "Failed to use pointer grab to determine CRTC and device id.\n");
			return -1;
		}

		if (set_identity) {
			for (int i = 0; i < absolute_pointer_count; i++) {
				if (xi2_device_set_matrix(display, absolute_pointers[i], absolute_pointer_matrices[i])) {
					fprintf(stderr, "Error restoring Coordinate Transformation Matrix for device %lu. ", absolute_pointers[i]);
					fprintf(stderr, "Original matrix was [");
					for (int j = 0; j < 9; j++) {
						fprintf(stderr, " %f", absolute_pointer_matrices[i][j]);
					}
					fprintf(stderr, "]\n");
				}
			}
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
		print_usage(stderr, argv[0]);
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

	PointerRegion pointer_region;
	int region_result = xi2_device_get_region(device, &valuator_indices, &pointer_region);

	if (region_result) {
		XCloseDisplay(display);
		fprintf(stderr, "Failed to retrieve region from device.\n");
		return -1;
	}

	CRTCRegion * region;
	Rectangle input_region = pointer_region.region;
	if (full_screen) {
		region = &screen_size;
	} else {
		region = crtc_regions + crtc_index;

		if (one_to_one) {
			resources = XRRGetScreenResourcesCurrent(display, DefaultRootWindow(display));
			int output_result = xlib_get_crtc_output_density(display, resources, region);
			XRRFreeScreenResources(resources);

			if (output_result) {
				XCloseDisplay(display);
				fprintf(stderr, "Failed to retrieve CRTC %d output density.\n", (int)region->crtc);
				return -1;
			}

			input_region = region->region;

			input_region.right = input_region.left + 1000L * RECT_WIDTH(region->region) * RECT_WIDTH(pointer_region.region) / pointer_region.hres / region->width;
			input_region.bottom = input_region.top + 1000L * RECT_HEIGHT(region->region) * RECT_HEIGHT(pointer_region.region) / pointer_region.vres / region->height;
		}
	}

	float matrix[9] = {0.0};

	calc_matrix(device_id, &config, &(screen_size.region), region, &input_region, matrix);

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
