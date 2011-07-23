
#ifndef __HERBST_KEY_H_
#define __HERBST_KEY_H_

#include <stdbool.h>
#include <X11/Xlib.h>

typedef struct KeyBinding {
    KeySym keysym;
    unsigned int modifiers;
    int     cmd_argc; // number of arguments for command
    char**  cmd_argv; // arguemnts for command to call
} KeyBinding;

unsigned int modifiername2mask(const char* name);

bool string2key(char* string, unsigned int* modmask, KeySym* keysym);
int keybind(int argc, char** argv);

void handle_key_press(XEvent* ev);

void key_init();
void key_destroy();

#endif

