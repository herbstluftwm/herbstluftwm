/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HS_GLIB_BACKPORTS_H_
#define __HS_GLIB_BACKPORTS_H_

/**
 * This file re-implements newer glib functions that are missing in glib on
 * older systems. Note that this is mostly works correctly but isn't as
 * efficient. As the new glib function.
 */

#include <glib.h>

#ifndef G_QUEUE_INIT
#define G_QUEUE_INIT { NULL, NULL, 0 }
#endif

#if !(GLIB_CHECK_VERSION(2,14,0))
/* implement g_queue_clear for glib older than 2.14 */

#define g_queue_clear(Q)    do {    \
        g_list_free((Q)->head);     \
        (Q)->head = NULL;           \
        (Q)->length = 0;            \
        (Q)->tail = NULL;           \
    } while(0)

#endif

#endif

