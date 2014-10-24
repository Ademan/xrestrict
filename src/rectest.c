#include <stdio.h>
#include "xrestrict.h"
#include "input.h"

static int verbosity = 1;
static int success = 0;
static int failed = 0;

#define ASSERT(test) do { \
	if (test) { \
		printf("."); \
		success++; \
	} else { \
		if (verbosity > 0) { \
			printf("E: " #test " failed!\n"); \
		} else { \
			printf("F"); \
			failed++; \
		} \
	} \
} while (0);

void rectangle_print(const Rectangle * rectangle) {
	printf("{ .top = %d, .left = %d, .bottom = %d, .right = %d }", rectangle->top, rectangle->left, rectangle->bottom, rectangle->right);
}

int main(int argc, char ** argv) {
	Rectangle reference = {
		.top = 5,
		.left = 7,
		.bottom = 11,
		.right = 13
	};
	Rectangle test1 = {
		.top = 0,
		.left = 0,
		.bottom = 16,
		.right = 16
	};

	Rectangle result;

	CTMAffinity affinity = {
		.vertical = VA_Top,
		.horizontal = HA_Left
	};

	rectangle_align(&reference, &test1, NULL, &result);

	ASSERT(reference.top == result.top);
	ASSERT(reference.left == result.left);
	ASSERT(result.bottom == reference.top + 16);
	ASSERT(result.right == reference.left + 16 );

	rectangle_align(&reference, &test1, &affinity, &result);

	ASSERT(reference.top == result.top);
	ASSERT(reference.left == result.left);
	ASSERT(result.bottom == reference.top + 16);
	ASSERT(result.right == reference.left + 16 );

	affinity.horizontal = HA_Right;

	rectangle_align(&reference, &test1, &affinity, &result);

	//rectangle_print(&result);

	ASSERT(result.top == reference.top);
	ASSERT(result.left == reference.right - 16);
	ASSERT(result.bottom == reference.top + 16);
	ASSERT(result.right == reference.right);

	affinity.vertical = VA_Bottom;

	rectangle_align(&reference, &test1, &affinity, &result);

	ASSERT(result.top == reference.bottom - 16);
	ASSERT(result.left == reference.right - 16);
	ASSERT(result.bottom == reference.bottom);
	ASSERT(result.right == reference.right);

	affinity.vertical = VA_Top;
	affinity.horizontal = HA_Centered;

	rectangle_align(&reference, &test1, &affinity, &result);

	ASSERT(result.top == reference.top);
	ASSERT(result.bottom == reference.top + 16);
	ASSERT(result.left == 2);
	ASSERT(result.right == 18);

	affinity.vertical = VA_Centered;

	rectangle_align(&reference, &test1, &affinity, &result);
	//rectangle_print(&result);

	ASSERT(result.top == 0);
	ASSERT(result.bottom == 16);
	ASSERT(result.left == 2);
	ASSERT(result.right == 18);

	Rectangle test2 = {
		.top = 17,
		.left = 19,
		.bottom = 17 + 3,
		.right = 19 + 4
	};

	printf("\nReference = %dx%d\n", RECT_WIDTH(reference), RECT_HEIGHT(reference));
	float ratio;
	ratio = rectangle_select_ratio_preserve_aspect(&reference, &test2, CTM_Fit);

	int result_width = 4 * ratio;
	int result_height = 3 * ratio;

	printf("\n%dx%d\n", result_width, result_height);

	test2.bottom = 17 + 16;
	test2.right = 17 + 10;
	ratio = rectangle_select_ratio_preserve_aspect(&reference, &test2, CTM_Fit);
	printf("\n%dx%d\n", (int)(10 * ratio), (int)(16 * ratio));
	
	printf("\nSuccess %d Failed %d\n", success, failed);
	return 0;
}
