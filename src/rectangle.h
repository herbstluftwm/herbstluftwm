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
int disjoin_rects_command(Input input, Output output);

#endif

