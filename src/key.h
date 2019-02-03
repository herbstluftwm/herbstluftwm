#ifndef __HERBST_KEY_H_
#define __HERBST_KEY_H_

#include <X11/Xlib.h>

#include "keymanager.h"
#include "types.h"

#define KEY_COMBI_SEPARATORS "+-"

unsigned int modifiername2mask(const char* name);
const char* modifiermask2name(unsigned int mask);

bool string2modifiers(const std::string& string, unsigned int* modmask);
bool string2key(const std::string& str, unsigned int* modmask, KeySym* keysym);

int list_keysyms(int argc, char** argv, Output output);
bool key_remove_bind_with_keysym(unsigned int modifiers, KeySym sym);
void key_remove_all_binds();
std::string keybinding_to_string(KeyBinding* binding);
void key_find_binds(const char* needle, Output output);
void complete_against_modifiers(const char* needle, char seperator,
                                char* prefix, Output output);
void complete_against_keysyms(const char* needle, char* prefix, Output output);
void ungrab_all();
void regrab_keys();
void grab_keybind(KeyBinding* binding);
void update_numlockmask();
unsigned int* get_numlockmask_ptr();
void key_set_keymask(const std::string& keymask);
void handle_key_press(XEvent* ev);

#endif
