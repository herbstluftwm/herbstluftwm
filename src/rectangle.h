#ifndef __HERBSTLUFT_RECTANGLE_H_
#define __HERBSTLUFT_RECTANGLE_H_

#include "commandio.h"
#include "x11-types.h"

RectangleVec disjoin_rects(const RectangleVec &buf);
int disjoin_rects_command(Input input, Output output);

#endif

