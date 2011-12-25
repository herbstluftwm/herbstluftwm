/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "ewmh.h"
#include "utils.h"
#include "globals.h"

#include <stdio.h>

void ewmh_init() {
    /* init ewmh net atoms */
    struct {
        int     atom;
        char*   name;
    } a2n[] = {
        { NetSupported,             "_NET_SUPPORTED"            },
    };
    for (int i = 0; i < LENGTH(a2n); i++) {
        g_netatom[a2n[i].atom] = XInternAtom(g_display, a2n[i].name, False);
    }
    /* tell which ewmh atoms are supported */
    XChangeProperty(g_display, g_root, g_netatom[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *) g_netatom, NetLast);
}

void ewmh_destroy() {
}

