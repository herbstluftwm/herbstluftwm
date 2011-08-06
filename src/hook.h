

#ifndef __HERBSTLUFT_HOOK_H_
#define __HERBSTLUFT_HOOK_H_


void hook_init();
void hook_destroy();

int hook_emit(int argc, char** argv);

#endif

