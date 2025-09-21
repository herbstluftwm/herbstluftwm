#pragma once

#include "x11-types.h"

class SizeHints {
public:
    // size hints for client windows.
    // 0/0 means 'unset'
    Point2D minAspect = { 0, 0 }; // minimum aspect ratio
    Point2D maxAspect = { 0, 0 }; // maximum aspect ratio
    Point2D base = { 0, 0 };
    Point2D incr = { 0, 0 }; // size increment
    Point2D max = { 0, 0 }; // maximum size
    Point2D min = { 0, 0 }; // minimum size
};

