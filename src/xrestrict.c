#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <X11/Xlib-xcb.h>
#include <xcb/randr.h>

#include "xrestrict.h"
#include "input.h"
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

int request_property(xcb_connection_t * connection, xcb_randr_output_t output, xcb_atom_t property_atom) {
	xcb_randr_query_output_property_cookie_t cookie = xcb_randr_query_output_property_unchecked(connection,
			output, property_atom);
	xcb_randr_query_output_property_reply_t * reply = xcb_randr_query_output_property_reply(connection, cookie, NULL);

	if (!reply) {
		return -1;
	}

	if (xcb_randr_query_output_property_valid_values_length(reply) < 1) {
		return -1;
	}
	return xcb_randr_query_output_property_valid_values(reply)[0];
}

char * get_atom_name(xcb_connection_t * connection, xcb_atom_t atom) {
	xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(connection, atom); 
	xcb_get_atom_name_reply_t * reply = xcb_get_atom_name_reply (connection,
			cookie, NULL);
	if (!reply) {
		return NULL;
	}

	int length = xcb_get_atom_name_name_length(reply);
	char * name = xcb_get_atom_name_name(reply);

	char * result = (char *)malloc(sizeof(char) * (length + 1));
	memcpy(result, name, length);
	result[length] = 0;

	return result;
}

int print_output_properties(FILE * f, xcb_connection_t * connection, xcb_randr_output_t output) {
	xcb_randr_list_output_properties_cookie_t cookie = xcb_randr_list_output_properties (connection, output);
	xcb_randr_list_output_properties_reply_t * reply = xcb_randr_list_output_properties_reply (connection,
			cookie, NULL);

	if (!reply) {
		fprintf(stderr, "Problem retrieving list of output properties for output %d\n", output);
		return -1;
	}

	xcb_atom_t * atom = xcb_randr_list_output_properties_atoms(reply);
	int count = xcb_randr_list_output_properties_atoms_length(reply);

	for (int i = 0; i < count; atom++, i++) {
		fprintf(f, "%s\n", get_atom_name(connection, *atom));
	}
	return -1;
}

#define ESCREENINFO_REQUEST_FAILED (-1)
#define ESCREEN_SIZE_NOT_FOUND     (-2)

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

	for (int i = 0; i < crtc_count && region < regions_end; i++, crtc++) {
		xcb_randr_get_crtc_info_cookie_t crtc_info_cookie = xcb_randr_get_crtc_info (connection, *crtc, XCB_CURRENT_TIME);
		xcb_randr_get_crtc_info_reply_t * crtc_reply = xcb_randr_get_crtc_info_reply (connection, crtc_info_cookie, NULL);

		if (crtc_reply) {
			if (crtc_reply->width > 0 && crtc_reply->height > 0) {
				region->top = crtc_reply->y;
				region->left = crtc_reply->x;
				region->bottom = crtc_reply->y + crtc_reply->height;
				region->right = crtc_reply->x + crtc_reply->width;
				region++;
				if (DEBUG) {
					printf("%d(%dx%d)+(%d,%d) mode=%d ", *crtc, crtc_reply->width, crtc_reply->height, crtc_reply->x, crtc_reply->y, crtc_reply->mode);
				}
			}
		}
	}
	return region - regions;
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
				char * invalid;
				device_id = strtoul(argv[i], &invalid, 10);
				if (invalid && *invalid != '\0') {
					fprintf(stderr, "Failed to parse device id \"%s\".\n", argv[i]);
					print_usage(argv[0]);
					XCloseDisplay(display);
					return -1;
				}
				parsing_state = PARSE_NONE;
			} else if (parsing_state == PARSE_CRTCINDEX) {
				char * invalid;
				crtc_index = strtoul(argv[i], &invalid, 10);
				if (invalid && *invalid != '\0') {
					fprintf(stderr, "Failed to parse crtc index \"%s\".\n", argv[i]);
					print_usage(argv[0]);
					XCloseDisplay(display);
					return -1;
				}
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


		/*xcb_randr_query_output_property_reply_t * property_reply = xcb_randr_query_output_property_reply(connection, property_cookie, NULL);

		if (!property_reply) {
			continue;
		}

		int32_t * mode_values = xcb_randr_query_output_property_valid_values(property_reply);
		int mode_values_count = xcb_randr_query_output_property_valid_values_length(property_reply);

		if (mode_values_count < 1) {
			fprintf(stderr, "Zero mode values retrieved for output %d.\n", *output);
		} else {
			printf("Mode values: ");
			for (int j = 0; j < mode_values_count; j++) {
				printf("%d ", mode_values[j]);
			}
			printf("\n");
		}*/
	}

	xcb_disconnect(connection);
	return 0;
}
