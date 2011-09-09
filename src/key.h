/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBST_KEY_H_
#define __HERBST_KEY_H_

#include <stdbool.h>
#include <X11/Xlib.h>

#define KEY_COMBI_SEPARATORS "+-"

typedef struct KeyBinding {
    KeySym keysym;
    unsigned int modifiers;
    int     cmd_argc; // number of arguments for command
    char**  cmd_argv; // arguemnts for command to call
} KeyBinding;

unsigned int modifiername2mask(const char* name);

bool string2modifiers(char* string, unsigned int* modmask);
bool string2key(char* string, unsigned int* modmask, KeySym* keysym);
int keybind(int argc, char** argv);
int keyunbind(int argc, char** argv); //removes a keybinding

void key_remove_bind_with_keysym(unsigned int modifiers, KeySym sym);
void regrab_keys();
void grab_keybind(KeyBinding* binding, void* useless_pointer);
void update_numlockmask();
unsigned int* get_numlockmask_ptr();

void handle_key_press(XEvent* ev);

void key_init();
void key_destroy();

#endif

