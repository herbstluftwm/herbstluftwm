/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_TAG_H_
#define __HERBSTLUFT_TAG_H_

#include "glib-backports.h"
#include <stdbool.h>
#include "x11-types.h"
#include "types.h"
#include <memory>
#include "object.h"
#include "attribute_.h"

struct HSStack;
class HSFrame;
class HSClient;
class Settings;

class HSTag : public Object {
public:
    HSTag(std::string name, Settings* settings);
    ~HSTag();
    std::shared_ptr<HSFrame>        frame;  // the master frame
    Attribute_<unsigned long> index;
    Attribute_<bool>         floating;
    Attribute_<std::string>  name;   // name of this tag
    int             flags;
    struct HSStack* stack;
    void setIndexAttribute(unsigned long new_index);
private:
    std::string validateNameChange();
    Settings* settings;
};

void tag_init();
void tag_destroy();

// for tags
HSTag* find_tag(const char* name);
HSTag* find_unused_tag();
HSTag* find_tag_with_toplevel_frame(class HSFrame* frame);
HSTag* get_tag_by_index(int index);
int    tag_get_count();
int tag_remove_command(int argc, char** argv, Output output);
int tag_set_floating_command(int argc, char** argv, Output output);
void tag_update_focus_layer(HSTag* tag);
void tag_foreach(void (*action)(HSTag*,void*), void* data);
void tag_update_each_focus_layer();
void tag_update_focus_objects();
void tag_force_update_flags();
void tag_update_flags();
void tag_set_flags_dirty();

#endif

