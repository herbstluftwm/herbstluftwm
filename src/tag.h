/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_TAG_H_
#define __HERBSTLUFT_TAG_H_

#include <glib.h>

struct HSFrame;

typedef struct HSTag {
    GString*        name;   // name of this tag
    struct HSFrame* frame;  // the master frame
    bool            floating;
    int             flags;
} HSTag;

// globals
GArray*     g_tags; // Array of HSTag*
bool        g_tag_flags_dirty;

void tag_init();
void tag_destroy();

// for tags
HSTag* add_tag(char* name);
HSTag* find_tag(char* name);
int    tag_index_of(HSTag* tag);
HSTag* find_unused_tag();
HSTag* find_tag_with_toplevel_frame(struct HSFrame* frame);
HSTag* get_tag_by_index(char* index_str, bool skip_visible_tags);
int tag_add_command(int argc, char** argv);
int tag_rename_command(int argc, char** argv);
int tag_move_window_command(int argc, char** argv);
int tag_move_window_by_index_command(int argc, char** argv);
void tag_move_window(HSTag* target);
int tag_remove_command(int argc, char** argv);
int tag_set_floating_command(int argc, char** argv, GString** result);
void tag_force_update_flags();
void tag_update_flags();
void tag_set_flags_dirty();
void ensure_tags_are_available();

#endif

