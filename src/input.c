#include <stdio.h>
#include <X11/Xlib-xcb.h>
#include <stdbool.h>
#include "input.h"

static const CTMAffinity default_affinity = {
	.horizontal = HA_Left,
	.vertical = VA_Top
};

static const CTMConfiguration default_config = {
	.type = CTM_MatchWidth,
	.affinity = {
		.horizontal = HA_Left,
		.vertical = VA_Top
	}
};

void calculate_coordinate_transform_matrix(const Rectangle * region, const xcb_randr_screen_size_t * screen, float * matrix) {
	float x_scale = RECT_WIDTH(*region) / (float)screen->width;
	float y_scale = RECT_HEIGHT(*region) / (float)screen->height;

	float x_offset = region->top / (float)screen->width;
	float y_offset = region->left / (float)screen->height;

	matrix[0] = x_scale;
	matrix[1] = 0;
	matrix[2] = x_offset;

	matrix[3] = 0;
	matrix[4] = y_scale;
	matrix[5] = y_offset;

	matrix[6] = 0;
	matrix[7] = 0;
	matrix[8] = 1;
}

void rectangle_scale(const Rectangle * rectangle, const float x, const float y, Rectangle * result) {
	result->left = rectangle->left * x;
	result->right = rectangle->right * x;
	result->top = rectangle->top * y;
	result->bottom = rectangle->bottom * y;
}

float rectangle_select_ratio_preserve_aspect(const Rectangle * target, const Rectangle * original, const CTMAspectPreserveType type) {
	int target_width = RECT_WIDTH(*target);
	int target_height = RECT_HEIGHT(*target);

	float horizontal_ratio = (float)RECT_WIDTH(*target) / RECT_WIDTH(*original);
	float vertical_ratio = (float)RECT_HEIGHT(*target) / RECT_HEIGHT(*original);

	if (type == CTM_Fit) {
		if (horizontal_ratio > vertical_ratio) {
			return horizontal_ratio;
		} else {
			return vertical_ratio;
		}
	} else if (type == CTM_MatchWidth) {
		return horizontal_ratio;
	} else if (type == CTM_MatchHeight) {
		return vertical_ratio;
	} else {
		return 1; // FIXME: error somehow?
	}
}

float rectangle_scale_preserve_aspect(const Rectangle * target, const Rectangle * original, const CTMAspectPreserveType type, Rectangle * result) {
	float ratio = rectangle_select_ratio_preserve_aspect(target, original, type);

	int original_width = RECT_WIDTH(*original);
	int original_height = RECT_HEIGHT(*original);

	result->top = result->left = 0;
	result->right = original_width;
	result->bottom = original_height;
	rectangle_scale(result, ratio, ratio, result);

	return ratio;
}

void rectangle_align(const Rectangle * reference,
					 const Rectangle * alignee,
					 const CTMAffinity * affinity,
					 Rectangle * result) {
	int top = reference->top;
	int left = reference->left;

	int width = RECT_WIDTH(*alignee);
	int height = RECT_HEIGHT(*alignee);

	if (!affinity) {
		result->top = top;
		result->left = left;
		result->bottom = top + height;
		result->right = left + width;
	} else {
		int x_offset = (width - RECT_WIDTH(*reference));
		int y_offset = (height - RECT_WIDTH(*reference));

		result->top = reference->top;
		result->left = reference->left;
		result->bottom = reference->top + width;
		result->right = reference->left + height;

		if (affinity->horizontal == HA_Right) {
			result->left -= x_offset;
			result->right -= x_offset;
		} else if (affinity->horizontal == HA_Centered) {
			result->left -= x_offset / 2;
			result->right -= x_offset / 2;
		}

		if (affinity->vertical == VA_Bottom) {
			result->top -= y_offset;
			result->bottom -= y_offset;
		} else if (affinity->vertical == VA_Centered) {
			result->top -= y_offset / 2;
			result->bottom -= y_offset / 2;
		}
	}
}

XIDeviceInfo * find_device(Display * display, const XID id) {
    XIDeviceInfo *info;
    XIDeviceInfo *found = NULL;
    int ndevices;

	XIQueryDevice(display, id, &ndevices);

    for(int i = 0; i < ndevices; i++)
    {
        if (info[i].deviceid == id) {
			if (found) {
				// Found duplicate
				XIFreeDeviceInfo(info);
				return NULL;
			}
		} else {
			found = &info[i];
		}
	}
	return found;
}

int xi2_device_set_matrix(Display * display, const XID id, const float * matrix) {
	static Atom atoms[2] = {0};
	static char * names[] = {"Coordinate Transformation Matrix", "FLOAT"};

	if (atoms[0] == 0 && atoms[1] == 0) {
		if (!XInternAtoms(display, names, 2, True, atoms)) { // FIXME: test
			return EINTERN_FAILED; // FIXME: error message
		}
	}

	XIChangeProperty(display,
			id,
			atoms[0], // "Coordiante Transformation Matrix"
			atoms[1], // "FLOAT"
			32,
			PropModeReplace,
			(char *)matrix, 9);
}

int xi2_device_info_find_xy_valuators(Display * display, const XIDeviceInfo * info, ValuatorIndices * valuator_indices) {
	Atom atoms[2] = {0};
	char * names[] = {"Abs X", "Abs Y"};

	if (!XInternAtoms(display, names, 2, True, atoms)) {
		return EINTERN_FAILED;
	}

	bool found_x = false, found_y = false;

	XIAnyClassInfo ** end = info->classes + info->num_classes;
	for (XIAnyClassInfo ** class = info->classes; class < end; class++) {
		if (class && *class && (*class)->type == XIValuatorClass) {
			const XIValuatorClassInfo * valuator = (XIValuatorClassInfo *)(*class);

			if (valuator->mode == XIModeAbsolute) {
				if (valuator->label == atoms[0]) {
					found_x = true;
					valuator_indices->x = valuator->number;
				} else if (valuator->label == atoms[1]) {
					found_y = true;
					valuator_indices->y = valuator->number;
				}
			}
		}
	}

	if (!(found_x && found_y)) {
		return EDEVICE_NO_ABS_AXES;
	} else {
		return 0;
	}
}

int xi2_device_get_region(XIDeviceInfo * device, const ValuatorIndices * valuator_indices, PointerRegion * region) {
	bool found_x = false;
	bool found_y = false;

	XIAnyClassInfo ** end = device->classes + device->num_classes;
	for (XIAnyClassInfo ** class = device->classes; class < end; class++) {
		if (class && *class && (*class)->type == XIValuatorClass) {
			const XIValuatorClassInfo * valuator = (XIValuatorClassInfo *)(*class);

			if (valuator->mode == XIModeAbsolute) {
				if (valuator->number == valuator_indices->x) {
					region->left = (int)valuator->min;
					region->right = (int)valuator->max;
					found_x = true;
				} else if (valuator->number == valuator_indices->y) {
					region->top = (int)valuator->min;
					region->bottom = (int)valuator->max;
					found_y = true;
				}
			}
		}
	}

	if (!(found_x && found_y)) {
		return EDEVICE_NO_ABS_AXES;
	} else {
		return 0;
	}
}

// Extract x and y values from a set of valuators
int xi2_read_point(const XIValuatorState * valuators, const ValuatorIndices * valuator_indices, Point * result) {
	bool found_x = false, found_y = false;
	double * value = valuators->values;
	for (int i = 0; i < valuators->mask_len; i++) {
		if (valuators->mask[i >> 3] & (1 << i)) {
			if (i == valuator_indices->x) {
				found_x = true;
				result->x = *value;
			} else if (i == valuator_indices->y) {
				found_y = true;
				result->y = *value;
			}
			value++;
		}
	}
	if (!(found_x && found_y)) {
		return -1;
	} else {
		return 0;
	}
}

int xi2_pointer_get_next_click(Display * display, const XID deviceid, const ValuatorIndices * valuator_indices, Point * point) {
	XIEventMask mask;
	unsigned char mask_data[4] = {0};
	mask.deviceid = deviceid;
	mask.mask_len = 1;
	XISetMask(mask_data, XI_Motion);
	XISetMask(mask_data, XI_ButtonRelease);
	mask.mask = mask_data;

	Status grab_result = XIGrabDevice(display, deviceid,
		 DefaultRootWindow(display),
		 CurrentTime,
		 None, /* cursor */
		 XIGrabModeAsync, /* XXX: might not want this */
		 XIGrabModeAsync, /* XXX: paired_device_mode: might not want this */
		 XINoOwnerEvents, /* XXX: not sure */
		 &mask
	);

	if (grab_result != Success) {
		switch (grab_result) {
			case XIGrabSuccess:
			case XIAlreadyGrabbed:
			case XIGrabInvalidTime:
			case XIGrabNotViewable:
			case XIGrabFrozen:
			default:
				return -1; // TODO: specific return codes?
		}
	}

	XEvent event;
	XGenericEventCookie *cookie = (XGenericEventCookie*)&event.xcookie;

	Point temp_point = {0, 0};
	bool  have_point = false;

	while (true) {
		if (XNextEvent(display, (XEvent *)&event) != Success) {
			return -1; // TODO: Specific return value
		}
		if (XGetEventData(display, cookie)) {
			if (cookie->type == GenericEvent) {
				if (cookie->evtype == XI_Motion) {
					XIDeviceEvent * device_event = (XIDeviceEvent *)cookie->data;

					have_point |= xi2_read_point(&device_event->valuators, valuator_indices, &temp_point) == 0;
				} else if (cookie->evtype == XI_ButtonRelease) {
					XIDeviceEvent * device_event = (XIDeviceEvent *)cookie->data;

					have_point |= xi2_read_point(&device_event->valuators, valuator_indices, &temp_point) == 0;
					XIUngrabDevice(display, deviceid, CurrentTime);

					if (have_point) {
						*point = temp_point;
						return 0;
					} else {
						return -1; // TODO: specific failure code
					}
				}
			}
			XFreeEventData(display, cookie);
		}
	}
	
	return -1;
}

int xi2_find_containing_crtc(CRTCRegion * regions, const int region_count, const Point * point) {
	for (CRTCRegion * region = regions; region < (regions + region_count); region++) {
		if (region->left <= point->x && point->x <= region->right && \
			region->top <= point->y && point->y <= region->bottom) {
			return (region - regions);
		}
	}
	return -1;
}
