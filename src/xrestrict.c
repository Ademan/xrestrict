#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "input.h"
#include "display.h"
#include "xrestrict.h"
#include <X11/extensions/XInput2.h>

#define INVALID_CRTC_INDEX -1
#define INVALID_DEVICE_ID -1

int restrict_index(Display * display, const XID deviceid, const CTMConfiguration * config, const xcb_randr_screen_size_t * screen, const CRTCRegion * region) {
	XIDeviceInfo * device = XIQueryDevice(display, deviceid, CurrentTime);
	if (!device) {
		return -1; // TODO: select error code
	}

	ValuatorIndices valuator_indices;
	int valuator_result = xi2_device_info_find_xy_valuators(display, device, &valuator_indices);
	
	if (valuator_result) {
		return -1; // TODO: select error code
	}

	PointerRegion pointer_region;
	int region_result = xi2_device_get_region(device, &valuator_indices, &pointer_region);

	if (region_result) {
		return -1; // TODO: select error code
	}

	Rectangle scaled, aligned;

	rectangle_scale_preserve_aspect(region, &pointer_region, config->type, &scaled);

	rectangle_align(region, &scaled, &config->affinity, &aligned);

	float matrix[9] = {0.0};

	calculate_coordinate_transform_matrix(&aligned, screen, matrix);

	int set_matrix_result = xi2_device_set_matrix(display, deviceid, matrix);
}

void print_usage(char * cmd) {
	// TODO: would like to eventually allow deviceid discovery
	//       based on pointer grab
	fprintf(stderr, "Usage: %s -d DEVICEID [-c CRTCINDEX]\n", cmd);
}

#define PARSE_NONE      0
#define PARSE_DEVICEID  1
#define PARSE_CRTCINDEX 2

int main(int argc, char ** argv) {
	if (argc < 3) {
		print_usage(argv[0]);
		return -1;
	}

	Display * display = XOpenDisplay(NULL);

	if (!display) {
		fprintf(stderr, "Failed to open display.\n");
		return -1;
	}

	int device_id = INVALID_DEVICE_ID;
	int crtc_index = INVALID_CRTC_INDEX;

	// TODO: break out command line parsing into another function
	//       or use getopt
	int parsing_state = PARSE_NONE;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (parsing_state != PARSE_NONE) {
				XCloseDisplay(display);
				print_usage(argv[0]);
				return -1;
			}
			if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
				parsing_state = PARSE_DEVICEID;
			} else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--crtc")) {
				parsing_state = PARSE_CRTCINDEX;
			} else {
				XCloseDisplay(display);
				print_usage(argv[0]);
				return -1;
			}
		} else {
			if (parsing_state == PARSE_DEVICEID) {
				device_id = atoi(argv[i]);
				parsing_state = PARSE_NONE;
			} else if (parsing_state == PARSE_CRTCINDEX) {
				crtc_index = atoi(argv[i]);
				parsing_state = PARSE_NONE;
			} else {
				XCloseDisplay(display);
				print_usage(argv[0]);
				return -1;
			}
		}
	}

	if (parsing_state != PARSE_NONE) {
		XCloseDisplay(display);
		print_usage(argv[0]);
		return -1;
	}

	xcb_connection_t * connection = XGetXCBConnection(display);

	if (!connection) {
		fprintf(stderr, "Failed to open XCB connection.\n");
		XCloseDisplay(display);
		return -1;
	}

	xcb_screen_t *screen = xcb_setup_roots_iterator (xcb_get_setup (connection)).data;

	xcb_randr_screen_size_t size;

	if (find_screen_size(connection, screen, &size)) {
		fprintf(stderr, "Failed to find screen size\n");
		XCloseDisplay(display);
		return -1;
	}

	CRTCRegion crtc_regions[10];
	int region_count = get_crtc_regions(connection, screen, crtc_regions, 10);

	if (region_count < 1) {
		fprintf(stderr, "Failed to retrieve CRTC information.\n");
		XCloseDisplay(display);
		return -1;
	}

	if (crtc_index >= region_count) {
		fprintf(stderr, "CRTC index %d greater than highest index available %d.\n", crtc_index, region_count - 1);
		XCloseDisplay(display);
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

	//int restrict_result = restrict_index(display, deviceid, &config, screen, crtc_regions + crtc_index);

	if (crtc_index == INVALID_CRTC_INDEX) {
		// TODO: implement user-selected CRTC
		fprintf(stderr, "Interactive selection of CRTC not currently supported.\n");
		return -1;
	}

	xcb_randr_get_screen_resources_cookie_t screen_resources_cookie = xcb_randr_get_screen_resources(connection, screen->root);

	xcb_randr_get_screen_resources_reply_t * screen_resources = xcb_randr_get_screen_resources_reply(connection,
			screen_resources_cookie, NULL);

	if (!screen_resources) {
		fprintf(stderr, "Failed to retrieve screen resources.\n");
		return -1;
	}

	xcb_randr_mode_info_t * mode = xcb_randr_get_screen_resources_modes(screen_resources);
	int mode_count = xcb_randr_get_screen_resources_modes_length(screen_resources);
	for (int i = 0; i < mode_count; i++, mode++) {
		printf("mode %d: id=%d %dx%d\n", i, mode->id, mode->width, mode->height);
	}

	xcb_randr_output_t * output = xcb_randr_get_screen_resources_outputs (screen_resources);
	int output_count = xcb_randr_get_screen_resources_outputs_length (screen_resources);

	xcb_generic_iterator_t outputs_end = xcb_randr_get_screen_resources_outputs_end(screen_resources);

	printf("Output count: %d\n", output_count);

	for (int i = 0; i < output_count; output++, i++) {
		//xcb_randr_query_output_property_cookie_t property_cookie = xcb_randr_query_output_property(connection, *output, mode_atom);
		xcb_randr_get_output_info_cookie_t output_info_cookie = xcb_randr_get_output_info(connection, *output, XCB_CURRENT_TIME);


		xcb_randr_get_output_info_reply_t * output_info_reply = xcb_randr_get_output_info_reply(connection, output_info_cookie, NULL);

		if (!output_info_reply) {
			fprintf(stderr, "Failed to get output info for output %d\n", *output);
			continue;
		}

		xcb_randr_mode_t * output_info_mode = xcb_randr_get_output_info_modes(output_info_reply);
		int output_info_modes_length = xcb_randr_get_output_info_modes_length(output_info_reply);

		printf("Output %d status: %d found %d Modes: ", *output, output_info_reply->status, output_info_modes_length);
		for (int j = 0; j < output_info_modes_length; j++) {
			printf("%d ", output_info_mode[j]);
		}
		printf("\n");
	}

	xcb_disconnect(connection);
	return 0;
}
