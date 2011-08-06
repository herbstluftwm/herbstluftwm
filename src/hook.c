
#include "hook.h"
#include "globals.h"
#include "utils.h"
#include "ipc-protocol.h"
// std
#include <stdio.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


static Window g_event_window;

void hook_init() {
    g_event_window = XCreateSimpleWindow(g_display, g_root, 42, 42, 42, 42, 0, 0, 0);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_HOOK_CLASS;
    hint->res_class = HERBST_HOOK_CLASS;
    XSetClassHint(g_display, g_event_window, hint);
    XFree(hint);
    // ignore all events for this window
    XSelectInput(g_display, g_event_window, 0l);
    // set its window id in root window
    XChangeProperty(g_display, g_root, ATOM(HERBST_HOOK_WIN_ID_ATOM),
        XA_ATOM, 32, PropModeReplace, (unsigned char*)&g_event_window, 1);
}

void hook_destroy() {
    // remove property from root window
    XDeleteProperty(g_display, g_root, ATOM(HERBST_HOOK_WIN_ID_ATOM));
    XDestroyWindow(g_display, g_event_window);
}


int hook_emit(int argc, char** argv) {
    static int last_property_number = 0;
    if (argc <= 0) {
        // nothing to do
        return 0;
    }
    XTextProperty text_prop;
    static char atom_name[STRING_BUF_SIZE];
    snprintf(atom_name, STRING_BUF_SIZE, HERBST_HOOK_PROPERTY_FORMAT, last_property_number);
    Atom atom = ATOM(atom_name);
    Xutf8TextListToTextProperty(g_display, argv, argc, XUTF8StringStyle, &text_prop);
    XSetTextProperty(g_display, g_event_window, &text_prop, atom);
    XFree(text_prop.value);
    // set counter for next property
    last_property_number += 1;
    last_property_number %= HERBST_HOOK_PROPERTY_COUNT;
    return 0;
}

