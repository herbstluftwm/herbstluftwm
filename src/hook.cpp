#include "hook.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cassert>
#include <cstdio>

#include "globals.h"
#include "ipc-protocol.h"
#include "tag.h"
#include "utils.h"

using std::string;
using std::vector;

static Window g_event_window;

void hook_init() {
    g_event_window = XCreateSimpleWindow(g_display, g_root, 42, 42, 42, 42, 0, 0, 0);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = (char*)HERBST_HOOK_CLASS;
    hint->res_class = (char*)HERBST_HOOK_CLASS;
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

void hook_emit(vector<string> args) {
    static int last_property_number = 0;
    if (args.size() <= 0) {
        // nothing to do
        return;
    }
    vector<const char*> args_c_str;
    args_c_str.reserve(args.size());
    for (const auto& s : args) {
        args_c_str.push_back(s.c_str());
    }
    XTextProperty text_prop;
    static char atom_name[STRING_BUF_SIZE];
    snprintf(atom_name, STRING_BUF_SIZE, HERBST_HOOK_PROPERTY_FORMAT, last_property_number);
    Atom atom = ATOM(atom_name);
    Xutf8TextListToTextProperty(g_display, (char**) args_c_str.data(), args.size(), XUTF8StringStyle, &text_prop);
    XSetTextProperty(g_display, g_event_window, &text_prop, atom);
    XFree(text_prop.value);
    // set counter for next property
    last_property_number += 1;
    last_property_number %= HERBST_HOOK_PROPERTY_COUNT;
}

void emit_tag_changed(HSTag* tag, int monitor) {
    assert(tag != nullptr);
    static char monitor_name[STRING_BUF_SIZE];
    snprintf(monitor_name, STRING_BUF_SIZE, "%d", monitor);
    hook_emit({"tag_changed", tag->name->c_str(), monitor_name});
}

