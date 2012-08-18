/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */


#ifndef __HERBSTLUFT_HOOK_H_
#define __HERBSTLUFT_HOOK_H_

#include "layout.h"

void hook_init();
void hook_destroy();

void hook_emit(int argc, char** argv);
void emit_tag_changed(HSTag* tag, int monitor);
void hook_emit_list(char* name, ...);

#endif

