/** Copyright 2018 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_RECTANGLE_H_
#define __HERBSTLUFT_RECTANGLE_H_

#include "x11-types.h"
#include "types.h"

typedef struct RectList {
    Rectangle rect;
    struct RectList* next;
} RectList;

RectList* reclist_insert_disjoint(RectList* head, RectList* mt);
int rectlist_length(RectList* head);
void rectlist_free(RectList* head);

RectList* disjoin_rects(const RectangleVec &buf);
int disjoin_rects_command(int argc, char** argv, Output output);

#endif

