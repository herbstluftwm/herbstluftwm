

#ifndef __CLIENTLIST_H_
#define __CLIENTLIST_H_

#include <X11/Xlib.h>
#include <glib.h>


typedef struct HerbstClient HerbstClient;
struct HerbstClient {
    GString*    name;
    Window      window;
};

void clientlist_init();
void clientlist_destroy();

void window_focus(Window window);

// adds a new client to list of managed client windows
void manage_client(Window win);

// destroys a special client
void destroy_client(HerbstClient* client);

// destroys data on quit
void free_clients();

void window_resize(Window win, XRectangle rect);

#endif


