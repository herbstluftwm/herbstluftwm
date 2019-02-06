#ifndef __HERBST_KEY_H_
#define __HERBST_KEY_H_

#include "types.h"

#define KEY_COMBI_SEPARATORS "+-"

unsigned int modifiername2mask(const char* name);
const char* modifiermask2name(unsigned int mask);

int list_keysyms(int argc, char** argv, Output output);
void key_remove_all_binds();
void key_find_binds(const char* needle, Output output);
void complete_against_modifiers(const char* needle, char seperator,
                                char* prefix, Output output);
void complete_against_keysyms(const char* needle, char* prefix, Output output);

#endif
