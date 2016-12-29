/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "hook.h"
#include "object.h"

#include <algorithm>
#include <iostream>


Hook::Hook(const std::string &path) :
    counter_("counter", 0),
    active_("active", false),
    emit_("emit"),
    path_(Path::split(path)) {
    wireAttributes({ &counter_, &active_ });
    wireActions({ &emit_ });
}

void Hook::hook_into(std::shared_ptr<Object> root) {
    cutoff_chain(0);
    chain_ = { root };
    /* we don't register with root; root Object shall never change */
    complete_chain();
}

void Hook::operator()(std::shared_ptr<Object> sender, HookEvent event,
                      const std::string &name) {
    //debug_hook(sender, event, name);

    if (event == HookEvent::ATTRIBUTE_CHANGED) {
        if (targetIsObject())
            return;
        if (name != path_.back())
            return;
        auto last = chain_.back().lock();
        if (!last || sender != last)
            return;
        auto o = std::dynamic_pointer_cast<Object>(sender);
        if (!o)
            return; // TODO: throw
        auto newvalue = o->read(name);
        if (newvalue == value_)
            return;

        emit(value_, newvalue);
        value_ = newvalue;
    }
    if (event == HookEvent::CHILD_ADDED || event == HookEvent::CHILD_REMOVED) {
        auto last = chain_.back().lock();
        if (targetIsObject() && last && sender == last) {
            emit(event, name);
        } else { // originating from somewhere in the chain
            check_chain(sender, event, name);
        }
    }

    //debug_hook();
}

void Hook::trigger(const std::string &action, ArgList args)
{
    if (action == emit_.name()) {
        emit(args);
        return;
    }
    Object::trigger(action, args);
}

void Hook::emit(const ArgList args)
{
    counter_ = counter_ + 1;
    // TODO: properly emit
    std::cout << "Hook " << name_ << " emitting:\t";
    for (auto a : args)
        std::cout << a << " ";
    std::cout << std::endl;
}

void Hook::emit(HookEvent event, const std::string &name)
{
    emit({
        (event == HookEvent::CHILD_ADDED ? "added" : "removed"),
        "child", name
    });
}

void Hook::emit(const std::string &old, const std::string &current)
{
    if (!old.empty()) {
        if (current.empty()) {
            emit({"cleared from", old});
        } else {
            emit({"changed from", old, "to", current});
        }
    } else {
        emit({"initialized to", current});
    }
}

void Hook::check_chain(std::shared_ptr<Object> sender, HookEvent event,
                       const std::string &name) {
    if (event == HookEvent::CHILD_REMOVED) {
        // find sender in chain
        size_t i = 0;
        for (; i < chain_.size(); ++i) {
            auto elem = chain_[i].lock();
            if (!elem)
                return; // TODO: throw, element missing before sender
            if (elem == sender)
                break; // found sender in chain
        }
        if (i == chain_.size())
            return; // TODO: throw, sender not in chain

        if (name != path_[i]) // index i, not i+1, as path does not contain root
            return; // we are not affected

        // next element in our chain was removed. destroy it from there
        cutoff_chain(i+1);
    }
    if (event == HookEvent::CHILD_ADDED) {
        auto last = chain_.back().lock();
        if (!last || sender != last)
            return;

        // something was added. it might already contain children.
        complete_chain();
    }
}

void Hook::cutoff_chain(size_t length) {
    for (auto i = length; i < chain_.size(); ++i) {
        auto o = chain_[i].lock();
        if (o)
            o->removeHook(name_);
    }
    chain_.resize(length);
    active_ = false;
}

void Hook::complete_chain() {
    auto self = std::dynamic_pointer_cast<Hook>(shared_from_this());
    auto current = chain_.back().lock();
    // current should always be o.k., in the worst case it is the root

    // process everything until leaf (note: path_ does not include root)
    for (auto i = chain_.size() - 1; i < path_.size(); ++i) {
        auto next = current->children().find(path_[i]);
        if (next != current->children().end()) {
            next->second->addHook(self);
            chain_.emplace_back(next->second);
            current = next->second;
        }
    }

    if (targetIsObject()) {
        active_ = true;
        return;
    }

    // now process leaf in case it is an attribute
    if (chain_.size() < path_.size())
        return; // no chance, we are still incomplete

    auto o = std::dynamic_pointer_cast<Object>(chain_.back().lock());
    if (!o || !o->exists(path_.back()))
        return; // TODO: throw
    active_ = true;
    auto newvalue = o->read(path_.back());
    if (newvalue != value_) {
        emit(value_, newvalue);
        value_ = newvalue;
    }
}

void Hook::debug_hook(std::shared_ptr<Object> sender, HookEvent event,
                      const std::string &name)
{
    if (sender) {
        std::cerr << "\t" << name_ << " triggered";
        std::string eventstr = (event == HookEvent::CHILD_ADDED ? "added"
                             : (event == HookEvent::CHILD_REMOVED ? "removed"
                                                                  : "changed"));
        std::cerr << " with " << name << " being " << eventstr;
        std::cerr << std::endl;
    }
    // std::cerr << "\tChain is: ";
    // for (auto c : chain_) {
    //     auto d = c.lock();
    //     if (d)
    //         std::cerr << d->name() << ".";
    //     else
    //         std::cerr << "!";
    // }
    // std::cerr << std::endl;
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
    argv[1] = tag->name->c_str();
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

