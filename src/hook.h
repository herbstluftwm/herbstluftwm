/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_HOOK_H_
#define __HERBSTLUFT_HOOK_H_

#include "object.h"
#include "utils.h" // split_path

#include <memory>
#include <vector>

namespace herbstluft {

class Hook : public Object {
public:

    Hook(const std::string &path) : Object(path), path_(split_path(path)) {}
    void hook_into(std::shared_ptr<Directory> root);

    Type type() { return Type::HOOK; }

    // emit hook, used by path elements
    void operator()(std::shared_ptr<Directory> sender, HookEvent event,
                    const std::string &name);

private:
    /* are we listening to an object rather an attribute? If so,
     * our complete chain is 1 element longer than the path (includes root) */
    bool targetIsObject() { return path_.size() < chain_.size(); }

    // for Event::CHILD_* cases
    void trigger(HookEvent event, const std::string &name);
    // for Event::ATTRIBUTE_CHANGED case
    void trigger(const std::string &old, const std::string &current);

    // check if chain needs to be altered
    void check_chain(std::shared_ptr<Directory> sender, HookEvent event,
                     const std::string &name);

    // remove tail from chain
    void cutoff_chain(size_t length);
    // rebuild chain after existing elements
    void complete_chain();

    void debug_hook(std::shared_ptr<Directory> sender = {},
                    HookEvent event = HookEvent::ATTRIBUTE_CHANGED,
                    const std::string &name = {});

    // chain of directories that report to us
    std::vector<std::weak_ptr<Directory>> chain_;
    // tokenized path (derived from our name)
    std::vector<std::string> path_;

    // last known value, used to print new value vs. old value
    std::string value_;
};

}

struct HSTag;

void hook_init();
void hook_destroy();

void hook_emit(int argc, const char** argv);
void emit_tag_changed(HSTag* tag, int monitor);
void hook_emit_list(const char* name, ...);

#endif

