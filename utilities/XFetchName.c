#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while(0)

int main(int argc, char** argv) {
    Display* d = XOpenDisplay(NULL);
    if (argc < 2) {
        DIE("Usage: %s DecimalWindowID\nE.g.: %s $((0xc89632))\nPrints the specified client\'s WM_NAME using XFetchName", argv[0],argv[0]);
    }
    unsigned int window = atoi(argv[1]);
    char* name = NULL;
    if (XFetchName(d, window, &name) == 0) {
        DIE("Can not read name from 0x%x\n", window);
    }
    printf("%s\n", name);
    XFree(name);
    return 0;
}
