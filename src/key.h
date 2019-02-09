#ifndef __HERBST_KEY_H_
#define __HERBST_KEY_H_

#include "types.h"

void complete_against_modifiers(const char* needle, char seperator,
                                char* prefix, Output output);
void complete_against_keysyms(const char* needle, char* prefix, Output output);

#endif
