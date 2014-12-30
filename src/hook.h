/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_HOOK_H_
#define __HERBSTLUFT_HOOK_H_

#include "entity.h"
#include "utils.h" // split_path

#include <vector>

namespace herbstluft {

class Object;

class Hook : public Entity {
public:
    Hook(const std::string &path) : Entity(path), path_(split_path(path)) {}
    void init(std::weak_ptr<Hook> self, std::shared_ptr<Object> root);

    Type type() { return Type::HOOK; }

    // emit hook, used by path elements
    void operator()(std::shared_ptr<Object> sender, const std::string &attr);

private:
    // remove tail from chain
    void cutoff_chain(std::vector<std::weak_ptr<Object>>::iterator last);
    // rebuild chain after existing elements
    void complete_chain();

    // chain of objects that report to us
    std::vector<std::weak_ptr<Object>> chain_;
    // tokenized path (derived from our name)
    std::vector<std::string> path_;

    // last known value, used to print new value vs. old value
    std::string value_;

    std::weak_ptr<Hook> self_;
};

}

struct HSTag;

void hook_init();
void hook_destroy();

void hook_emit(int argc, const char** argv);
void emit_tag_changed(HSTag* tag, int monitor);
void hook_emit_list(const char* name, ...);

#endif

