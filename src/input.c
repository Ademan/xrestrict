#include <stdio.h>
#include <stdbool.h>
#include "input.h"
#include <X11/cursorfont.h>

const float identity[9] = {
	1, 0, 0,
	0, 1, 0,
	0, 0, 1
};

void calculate_coordinate_transform_matrix(const Rectangle * region, const Rectangle * screen_size, float * matrix) {
	float x_scale = RECT_WIDTH(*region) / (float)RECT_WIDTH(*screen_size);
	float y_scale = RECT_HEIGHT(*region) / (float)RECT_HEIGHT(*screen_size);

	float x_offset = region->left / (float)RECT_WIDTH(*screen_size);
	float y_offset = region->top / (float)RECT_HEIGHT(*screen_size);

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
		int y_offset = (height - RECT_HEIGHT(*reference));

		result->top = reference->top;
		result->left = reference->left;
		result->bottom = reference->top + height;
		result->right = reference->left + width;

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

int xi2_device_set_matrix(Display * display, const XID id, const float * matrix) {
	static Atom atoms[2] = {0};
	static char * names[] = {"Coordinate Transformation Matrix", "FLOAT"};

	if (!XInternAtoms(display, names, 2, True, atoms)) {
		return EINTERN_FAILED;
	}

	XIChangeProperty(display,
			id,
			atoms[0], // "Coordinate Transformation Matrix"
			atoms[1], // "FLOAT"
			32,
			PropModeReplace,
			(unsigned char *)matrix, 9);
	return 0;
}

int xi2_device_get_matrix(Display * display, const XID id, float * matrix) {
	static Atom atoms[2] = {0};
	static char * names[] = {"Coordinate Transformation Matrix", "FLOAT"};

	if (!XInternAtoms(display, names, 2, True, atoms)) {
		return EINTERN_FAILED;
	}

	Atom type_return;
	int format_return;
	unsigned long num_items_return, bytes_after_return;
	float * retrieved_matrix;
	Status result = XIGetProperty(display,
								  id,
								  atoms[0],
								  0, 9 /* Length in 32 bit words */,
								  False,
								  atoms[1],
								  &type_return, &format_return,
								  &num_items_return, &bytes_after_return,
								  (unsigned char **)&retrieved_matrix);

	if (result != Success) {
		return -1; // TODO: select error code
	} else if (format_return != 32 && num_items_return != 9 && bytes_after_return != 0) {
		return -1; // TODO: select error code
	} else {
		for (int i = 0; i < 9; i++) {
			matrix[i] = retrieved_matrix[i];
		}

		XFree(retrieved_matrix);
		return 0;
	}
}

int xi2_device_check_matrix(Display * display, const XID id, const float * matrix) {
	float retrieved_matrix[9];
	int result = xi2_device_get_matrix(display, id, retrieved_matrix);

	if (!result) {
		return result;
	}

	for (int i = 0; i < 9; i++) {
		if (retrieved_matrix[i] != matrix[i]) {
			return EMATRIX_NOT_EQUAL; // TODO: select error code
		}
	}

	return 0;
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
					region->region.left = (int)valuator->min;
					region->region.right = (int)valuator->max;
					region->hres = valuator->resolution;
					found_x = true;
				} else if (valuator->number == valuator_indices->y) {
					region->region.top = (int)valuator->min;
					region->region.bottom = (int)valuator->max;
					region->vres = valuator->resolution;
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

int xi2_pointer_get_next_click(Display * display, XID * deviceid, Point * point) {
	unsigned char mask_data[4] = {0};
	XIEventMask mask = {
		.deviceid = *deviceid,
		.mask_len = 1,
		.mask = mask_data
	};

	XISetMask(mask_data, XI_Motion);
	XISetMask(mask_data, XI_ButtonRelease);

	Cursor cross = XCreateFontCursor(display, XC_crosshair);

	Status grab_result = XIGrabDevice(display, *deviceid,
		 DefaultRootWindow(display),
		 CurrentTime,
		 cross, /* cursor */
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

	while (true) {
		if (XNextEvent(display, (XEvent *)&event) != Success) {
			return -1; // TODO: Specific return value
		}
		if (XGetEventData(display, cookie)) {
			if (cookie->type == GenericEvent) {
				if (cookie->evtype == XI_ButtonRelease) {
					XIDeviceEvent * device_event = (XIDeviceEvent *)cookie->data;

					XIUngrabDevice(display, *deviceid, CurrentTime);

					point->x = device_event->root_x;
					point->y = device_event->root_y;
					*deviceid = device_event->sourceid;
					return 0;
				}
			}
			XFreeEventData(display, cookie);
		}
	}
	
	return -1;
}

int xi2_find_absolute_pointers(Display *display, XIDeviceInfo * info, const XIDeviceInfo * info_end, XID * pointers, const int max_pointers) {
	const XID * pointers_base = pointers;
	const XID * pointers_end = pointers + max_pointers;

	for (; info < info_end; info++) {
		ValuatorIndices valuators;
		if (!xi2_device_info_find_xy_valuators(display, info, &valuators)) {
			if (pointers >= pointers_end) {
				return EDEVICES_OVERFLOW;
			}

			*pointers = info->deviceid;
			pointers++;
		}
	}
	return pointers - pointers_base;
}

int xi2_find_master_pointers(XIDeviceInfo * info, const XIDeviceInfo * info_end, XID * pointers, const int max_pointers) {
	const XID * pointers_base = pointers;
	const XID * pointers_end = pointers + max_pointers;

	for (; info < info_end; info++) {
		if (info->use == XIMasterPointer) {
			if (pointers >= pointers_end) {
				return EMASTER_POINTERS_OVERFLOW;
			}

			*pointers = info->deviceid;
			pointers++;
		}
	}
	return pointers - pointers_base;
}
