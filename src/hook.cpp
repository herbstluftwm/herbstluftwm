#include "hook.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdarg.h>
#include <cassert>
#include <cstdio>

#include "globals.h"
#include "ipc-server.h"
#include "root.h"
#include "tag.h"

using std::string;
using std::vector;

void hook_emit(vector<string> args) {
    Root::get()->ipcServer_.emitHook(args);
}

void emit_tag_changed(HSTag* tag, int monitor) {
    assert(tag != nullptr);
    static char monitor_name[STRING_BUF_SIZE];
    snprintf(monitor_name, STRING_BUF_SIZE, "%d", monitor);
    hook_emit({"tag_changed", tag->name->c_str(), monitor_name});
}

void hook_emit_list(const char* name, ...) {
    assert(name != nullptr);
    vector<string> args;
    va_list ap;
    va_start(ap, name);
    while (true) {
        const char* next = va_arg(ap, const char*);
        if (!next) {
            break;
        }
        args.push_back(next);
    }
    va_end(ap);
    hook_emit(args);
}

