
#include "key.h"
#include "globals.h"
#include "utils.h"
#include "ipc-protocol.h"
#include "command.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <X11/keysym.h>

static unsigned int numlockmask = 0;
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask))

GList* g_key_binds = NULL;

void key_init() {
    // nothing to do yet
}

void key_destroy() {
#if GLIB_CHECK_VERSION(2, 28, 0)
    g_list_free_full(g_key_binds, g_free); // only available since glib 2.28
#else
    // actually this is not c-standard-compatible because of casting
    // an one-parameter-function to an 2-parameter-function.
    // but it should work on almost all architectures (maybe not amd64?)
    g_list_foreach(g_key_binds, (GFunc)g_free, 0);
    g_list_free(g_key_binds);
#endif
}


unsigned int modifiername2mask(const char* name) {
    static struct {
        char* name;
        unsigned int mask;
    } table[] = {
        { "Mod1",       Mod1Mask },
        { "Alt",        Mod1Mask },
        { "Mod4",       Mod4Mask },
        { "Super",      Mod4Mask },
        { "Shift",      ShiftMask },
        { "Control",    ControlMask },
    };
    int i;
    for (i = 0; i < LENGTH(table); i++) {
        if (!strcmp(table[i].name, name)) {
            return table[i].mask;
        }
    }
    return 0;
}

int keybind(int argc, char** argv) {
    if (argc <= 2) {
        fprintf(stderr, "keybind: not enough arguments\n");
        return HERBST_INVALID_ARGUMENT;
    }
    KeyBinding new_bind;
    // get keycode
    if (!string2key(argv[1], &(new_bind.modifiers), &(new_bind.keysym))) {
        return HERBST_INVALID_ARGUMENT;
    }
    // remove existing binding with same keysym/modifiers
    key_remove_bind_with_keysym(new_bind.modifiers, new_bind.keysym);
    // create a copy of the command to execute on this key
    new_bind.cmd_argc = argc - 2;
    new_bind.cmd_argv = argv_duplicate(new_bind.cmd_argc, argv+2);
    // add keybinding
    KeyBinding* data = g_new(KeyBinding, 1);
    *data = new_bind;
    g_key_binds = g_list_append(g_key_binds, data);
    // grab for events on this keycode
    XGrabKey(g_display, XKeysymToKeycode(g_display, new_bind.keysym),
             new_bind.modifiers, g_root, True, GrabModeAsync, GrabModeAsync);
    return 0;
}

bool string2key(char* string, unsigned int* modmask, KeySym* keysym) {
    // example strings: "Mod1-space" "Mod4+f" "f"
    // splitt string at "+-"
    char** splitted = g_strsplit_set(string, "+-", 0);
    // this should give at least one part
    if (NULL == splitted[0]) {
        fprintf(stderr, "warning: empty keysym\n");
        g_strfreev(splitted);
        return false;
    }
    // all parts except last one are modifiers
    int i;
    *modmask = 0;
    for (i = 0; splitted[i+1] != NULL; i++) {
        // while the i'th element is not the last part
        unsigned int cur_mask = modifiername2mask(splitted[i]);
        if (cur_mask == 0) {
            fprintf(stderr, "warning: unknown Modifier \"%s\"\n", splitted[i]);
            g_strfreev(splitted);
            return false;
        }
        *modmask |= cur_mask;
    }
    // last one is the modifier
    *keysym = XStringToKeysym(splitted[i]);
    if (*keysym == NoSymbol) {
        fprintf(stderr, "warning: unknown KeySym \"%s\"\n", splitted[i]);
        g_strfreev(splitted);
        return false;
    }
    // splitted string is not needed anymore
    g_strfreev(splitted);
    return true;
}

static gint keysym_equals(const KeyBinding* a, const KeyBinding* b) {
    bool equal = (CLEANMASK(a->modifiers) == CLEANMASK(b->modifiers));
    equal = equal && (a->keysym == b->keysym);
    return equal ? 0 : -1;
}

void handle_key_press(XEvent* ev) {
    KeyBinding pressed;
    pressed.keysym = XKeycodeToKeysym(g_display, ev->xkey.keycode, 0);
    pressed.modifiers = ev->xkey.state;
    GList* element = g_list_find_custom(g_key_binds, &pressed, (GCompareFunc)keysym_equals);
    if (element && element->data) {
        KeyBinding* found = (KeyBinding*)element->data;
        // call the command
        call_command_no_ouput(found->cmd_argc, found->cmd_argv);
    }
}

int keyunbind(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "keybind: not enough arguments\n");
        return HERBST_INVALID_ARGUMENT;
    }
    unsigned int modifiers;
    KeySym keysym;
    // get keycode
    if (!string2key(argv[1], &modifiers, &keysym)) {
        return HERBST_INVALID_ARGUMENT;
    }
    key_remove_bind_with_keysym(modifiers, keysym);
    XUngrabKey(g_display, XKeysymToKeycode(g_display, keysym), modifiers, g_root);
    return 0;
}

void key_remove_bind_with_keysym(unsigned int modifiers, KeySym keysym){
    KeyBinding bind;
    bind.modifiers = modifiers;
    bind.keysym = keysym;
    // search this keysym in list and remove it
    GList* element = g_list_find_custom(g_key_binds, &bind, (GCompareFunc)keysym_equals);
    if (!element) {
        return;
    }
    g_free(element->data);
    g_key_binds = g_list_remove_link(g_key_binds, element);
    g_list_free_1(element);
}




