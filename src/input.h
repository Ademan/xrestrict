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

typedef struct DeviceIdentifier {
	int vendor;
	int product;
} DeviceIdentifier;

typedef struct Rectangle PointerRegion;

typedef struct ValuatorIndices {
	int x, y;
} ValuatorIndices;

typedef struct AbsolutePointer {
	XID				id;
	ValuatorIndices	valuators;
} AbsolutePointer;

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
int xi2_find_absolute_pointers(Display *display, XIDeviceInfo * info, const XIDeviceInfo * info_end, XID * pointers, const int max_pointers);
int xi2_device_get_region(XIDeviceInfo * device, const ValuatorIndices * valuator_indices, PointerRegion * region);

int xi2_device_get_matrix(Display * display, const XID id, float * matrix);
int xi2_device_set_matrix(Display * display, const XID id, const float * matrix);
int xi2_device_check_matrix(Display * display, const XID id, const float * matrix);

int xi2_find_master_pointers(XIDeviceInfo * info, const XIDeviceInfo * info_end, XID * pointers, const int max_pointers);
int xi2_pointer_get_next_click(Display * display, XID * deviceid, Point * point);

const extern float identity[9];

// Error codes for xi2_...
#define EGET_PROPERTY_FAILED (-2)
#define ESET_PROPERTY_FAILED (-4)

// Error codes for xi2_device_get_region();
#define EINTERN_FAILED (-8)
#define EDEVICE_QUERY_FAILED (-16)
#define EDEVICE_NO_ABS_AXES (-32)
#define EMASTER_POINTERS_OVERFLOW (-64)
#define EABSOLUTE_POINTERS_OVERFLOW (-128)
#define EDEVICES_OVERFLOW (-256)
#define EMATRIX_NOT_EQUAL (-512)

#endif /* XRESTRICT_INPUT_H_ */
