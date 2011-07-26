
#ifndef __HERBSTLUFT_SETTINGS_H_
#define __HERBSTLUFT_SETTINGS_H_

#include <glib.h>

enum {
    HS_String = 0,
    HS_Int
};

typedef struct {
    char*   name;
    union {
        int     i;
        char*   s;
    }   value;
    int type;
    void (*on_change)(); // what to call on change
} SettingsPair;

extern SettingsPair g_settings[];

void settings_init();
void settings_destroy();

SettingsPair* settings_find(char* name);

int settings_set(int argc, char** argv);
int settings_count();
int settings_get(int argc, char** argv, GString** output);


#endif

