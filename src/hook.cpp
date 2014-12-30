/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "hook.h"
#include "object.h"

#include <algorithm>
#include <iostream>

namespace herbstluft {

void Hook::init(std::weak_ptr<Hook> self, std::shared_ptr<Object> root) {
    self_ = self;
    chain_ = { root };
    /* we do not register with root; zero-level objects should never change */
    complete_chain();
}

void Hook::operator()(std::shared_ptr<Object> sender, const std::string &attr) {
    if (attr.empty()) {
        // a child of the sender got removed. check if it affects us
        auto elem = chain_.begin();
        for (; elem != chain_.end(); ++elem) {
            auto e = elem->lock();
            if (!e)
                return; // TODO: throw
            if (e == sender)
                break;
        }
        if (elem == chain_.end())
            return; // TODO: throw
        if (elem == chain_.end() - 1)
            return; // we are not affected

        auto e = (elem+1)->lock();
        if (e && e == sender->children().at(e->name()))
            return; // everything is fine, we are not affected

        // next element in our chain was removed. we need to reconstruct
        cutoff_chain(elem);
        complete_chain();
    } else {
        if (attr != path_.back())
            return;
    }

    auto o = chain_.back().lock();
    if (!o)
        return; // TODO: throw
    if (!o->readable(path_.back()))
        return; // TODO: throw
    auto newvalue = o->read(path_.back());
    // TODO: properly emit
    std::cout << "Hook " << name_ << " emitting:\t";
    std::cout << "changed from " << value_ << " to " << newvalue
              << std::endl;
    value_ = newvalue;
}

void Hook::cutoff_chain(std::vector<std::weak_ptr<Object>>::iterator last) {
    for (auto elem = last+1; elem != chain_.end(); ++elem) {
        auto o = elem->lock();
        if (o)
            o->removeHook(name_);
    }
    chain_.erase(last+1, chain_.end());
}


void Hook::complete_chain() {
    auto current = chain_[chain_.size()-1].lock();
    // current should always be o.k., in the worst case it is the root

    for (size_t i = chain_.size(); i < path_.size() - 1; ++i) {
        auto next = current->children().find(path_[i]);
        if (next != current->children().end()) {
            next->second->addHook(self_.lock());
            chain_.emplace_back(next->second);
            current = next->second;
        } else {
            return; // TODO: throw
        }
    }
    auto o = chain_.back().lock(); // always works as we just added it
    if (!o->readable(path_.back()))
        return; // TODO: throw
    value_ = o->read(path_.back());
}



}

#include "globals.h"
#include "utils.h"
#include "ipc-protocol.h"
#include "layout.h"
// std
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
// other
#include "glib-backports.h"
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

void hook_emit(int argc, const char** argv) {
    static int last_property_number = 0;
    if (argc <= 0) {
        // nothing to do
        return;
    }
    XTextProperty text_prop;
    static char atom_name[STRING_BUF_SIZE];
    snprintf(atom_name, STRING_BUF_SIZE, HERBST_HOOK_PROPERTY_FORMAT, last_property_number);
    Atom atom = ATOM(atom_name);
    Xutf8TextListToTextProperty(g_display, (char**)argv, argc, XUTF8StringStyle, &text_prop);
    XSetTextProperty(g_display, g_event_window, &text_prop, atom);
    XFree(text_prop.value);
    // set counter for next property
    last_property_number += 1;
    last_property_number %= HERBST_HOOK_PROPERTY_COUNT;
}

void emit_tag_changed(HSTag* tag, int monitor) {
    assert(tag != NULL);
    static char monitor_name[STRING_BUF_SIZE];
    snprintf(monitor_name, STRING_BUF_SIZE, "%d", monitor);
    const char* argv[3];
    argv[0] = "tag_changed";
    argv[1] = tag->name->str;
    argv[2] = monitor_name;
    hook_emit(LENGTH(argv), argv);
}

void hook_emit_list(const char* name, ...) {
    assert(name != NULL);
    int count = 1;
    va_list ap;
    // first count number of arguments
    va_start(ap, name);
    while (va_arg(ap, char*)) {
        count++;
    }
    va_end(ap);
    // then fill arguments into argv array
    const char** argv = g_new(const char*, count);
    int i = 0;
    argv[i++] = name;
    va_start(ap, name);
    while (i < count) {
        argv[i] = va_arg(ap, char*);
        i++;
    }
    va_end(ap);
    hook_emit(count, argv);
    // cleanup
    g_free(argv);
}

