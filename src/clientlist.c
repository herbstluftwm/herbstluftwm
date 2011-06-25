
#include "clientlist.h"
#include "globals.h"
#include <glib.h>
#include <stdio.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

GList* g_clients = NULL;

void update_client_properties(HerbstClient* c) {
    // update name
    XTextProperty name;
    XGetTextProperty(g_display, c->window, &name, XA_WM_NAME);
    // FIXME: do the right locale conversion
    c->name = g_string_assign(c->name, (char*)name.value);
    XFree(name.value);
}

void manage_client(Window win) {
    HerbstClient* c = g_new(HerbstClient, 1);
    // init client
    c->window = win;
    c->name = g_string_new("");
    // get properties
    update_client_properties(c);
    // add to list
    fprintf(stdout, "now managing: \"%s\"\n", c->name->str);
    g_clients = g_list_append(g_clients, c);
}

void free_clients() {
    // TODO:
}


