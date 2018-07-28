#ifndef __HERBSTLUFT_ROOTCOMMANDS_H_
#define __HERBSTLUFT_ROOTCOMMANDS_H_

/** commands that don't belong to a particular object
 * but modify the global state */

#include "utils.h"

class Root;

int substitute_cmd(Root* root, Input input, Output output);
int sprintf_cmd(Root* root, Input input, Output output);

#endif
