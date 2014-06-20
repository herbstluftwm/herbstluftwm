
#ifndef __HERBST_X11_TYPES_H_
#define __HERBST_X11_TYPES_H_

typedef unsigned long HSColor;

struct Rectangle {
    int x;
    int y;
    int width;
    int height;
    Rectangle() {};
    Rectangle(int x, int y, int width, int height)
        : x(x), y(y), width(width), height(height) {};
};

typedef struct {
    int x;
    int y;
} Point2D;

#endif

