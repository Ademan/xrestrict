#ifndef XRESTRICT_H_
#define XRESTRICT_H_

#ifndef DEBUG
#	define DEBUG 0
#endif

typedef struct Rectangle {
	int top, left;
	int bottom, right;
} Rectangle;

#define RECT_WIDTH(region) ((region).right - (region).left)
#define RECT_HEIGHT(region) ((region).bottom - (region).top)

// Double due to uncertainty about valuator values
typedef struct Point {
	double x, y;
} Point;

#endif /* XRESTRICT_H_ */
