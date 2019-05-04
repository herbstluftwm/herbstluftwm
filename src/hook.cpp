#include "hook.h"

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

