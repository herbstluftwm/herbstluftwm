/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBST_KEY_H_
#define __HERBST_KEY_H_

#include <stdbool.h>
#include <X11/Xlib.h>
#include <glib.h>

#define KEY_COMBI_SEPARATORS "+-"

typedef struct KeyBinding {
    KeySym keysym;
    unsigned int modifiers;
    int     cmd_argc; // number of arguments for command
    char**  cmd_argv; // arguments for command to call
} KeyBinding;

unsigned int modifiername2mask(const char* name);
char*   modifiermask2name(unsigned int mask);

bool string2modifiers(char* string, unsigned int* modmask);
bool string2key(char* string, unsigned int* modmask, KeySym* keysym);
int keybind(int argc, char** argv, GString* output);
int keyunbind(int argc, char** argv, GString* output); //removes a keybinding
void keybinding_free(KeyBinding* binding);

int key_list_binds(int argc, char** argv, GString* output);
bool key_remove_bind_with_keysym(unsigned int modifiers, KeySym sym);
void key_remove_all_binds();
GString* keybinding_to_g_string(KeyBinding* binding);
void key_find_binds(char* needle, GString* output);
void regrab_keys();
void grab_keybind(KeyBinding* binding, void* useless_pointer);
void update_numlockmask();
unsigned int* get_numlockmask_ptr();

void handle_key_press(XEvent* ev);

void key_init();
void key_destroy();

#endif

