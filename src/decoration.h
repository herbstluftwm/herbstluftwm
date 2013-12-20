/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __DECORATION_H_
#define __DECORATION_H_

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "glib-backports.h"
#include <stdbool.h>

#include "utils.h"

struct HSClient;

typedef struct {
    struct HSClient*    client; // the client to decorate
    Window              decwin; // the decoration winodw
} HSDecoration;


void decoration_init(HSDecoration* dec, struct HSClient* client);
void decoration_free(HSDecoration* dec);


#endif

