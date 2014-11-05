#ifndef XRESTRICT_INPUT_H_
#define XRESTRICT_INPUT_H_

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "xrestrict.h"

typedef enum CTMAspectPreserveType {
	CTM_None,
	CTM_Fit,	 // Input area will be greater than or equal to CRTC area
	CTM_MatchWidth,
	CTM_MatchHeight,
} CTMAspectPreserveType;

typedef enum CTMHorizontalAffinity {
	HA_Left,
	HA_Right,
	HA_Centered
} CTMHorizontalAffinity;

typedef enum CTMVerticalAffinity {
	VA_Top,
	VA_Bottom,
	VA_Centered
} CTMVerticalAffinity;

typedef struct CTMAffinity {
	CTMHorizontalAffinity horizontal;
	CTMVerticalAffinity   vertical;
} CTMAffinity;

typedef struct CTMConfiguration {
	CTMAspectPreserveType type;
	CTMAffinity			  affinity;
} CTMConfiguration;

void rectangle_align(const Rectangle * reference,
					  const Rectangle * alignee,
					  const CTMAffinity * affinity,
					  Rectangle * result);

float rectangle_select_ratio_preserve_aspect(const Rectangle * target,
											 const Rectangle * original,
											 const CTMAspectPreserveType type);

float rectangle_scale_preserve_aspect(const Rectangle * target, const Rectangle * original, const CTMAspectPreserveType type, Rectangle * result);

void calculate_coordinate_transform_matrix(const Rectangle * region, const Rectangle * screen_size, float * matrix);

int xi2_device_info_find_xy_valuators(Display * display, const XIDeviceInfo * info, ValuatorIndices * valuator_indices);
int xi2_device_get_region(XIDeviceInfo * device, const ValuatorIndices * valuator_indices, PointerRegion * region);
int xi2_device_set_matrix(Display * display, const XID id, const float * matrix);
int xi2_device_check_matrix(Display * display, const XID id, const float * matrix);

int xi2_pointer_get_next_click(Display * display, const XID deviceid, const ValuatorIndices * valuator_indices, Point * point);

// Error codes for xi2_...
#define EGET_PROPERTY_FAILED (-2)
#define ESET_PROPERTY_FAILED (-4)

// Error codes for xi2_device_get_region();
#define EINTERN_FAILED (-1)
#define EDEVICE_QUERY_FAILED (-2)
#define EDEVICE_NO_ABS_AXES (-4)

#endif /* XRESTRICT_INPUT_H_ */
