#ifndef __HERBST_KEY_H_
#define __HERBST_KEY_H_

#include <X11/Xlib.h>

#include "glib-backports.h"
#include "types.h"

#define KEY_COMBI_SEPARATORS "+-"

class HSTag;
class Client;

typedef struct KeyBinding {
    KeySym keysym;
    unsigned int modifiers;
    int     cmd_argc; // number of arguments for command
    char**  cmd_argv; // arguments for command to call
    bool    enabled;  // Is the keybinding already grabbed
} KeyBinding;

unsigned int modifiername2mask(const char* name);
const char* modifiermask2name(unsigned int mask);

bool string2modifiers(char* string, unsigned int* modmask);
bool string2key(char* string, unsigned int* modmask, KeySym* keysym);
int keybind(int argc, char** argv, Output output);
int keyunbind(int argc, char** argv, Output output); //removes a keybinding
void keybinding_free(KeyBinding* binding);

int key_list_binds(Output output);
int list_keysyms(int argc, char** argv, Output output);
bool key_remove_bind_with_keysym(unsigned int modifiers, KeySym sym);
void key_remove_all_binds();
GString* keybinding_to_g_string(KeyBinding* binding);
void key_find_binds(const char* needle, Output output);
void complete_against_modifiers(const char* needle, char seperator,
                                char* prefix, Output output);
void complete_against_keysyms(const char* needle, char* prefix, Output output);
void regrab_keys();
void grab_keybind(KeyBinding* binding);
void update_numlockmask();
unsigned int* get_numlockmask_ptr();
void key_set_keymask(HSTag * tag, Client *client);
void handle_key_press(XEvent* ev);

void key_init();
void key_destroy();

#endif
