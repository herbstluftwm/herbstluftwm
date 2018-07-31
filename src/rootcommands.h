#ifndef __HERBSTLUFT_ROOTCOMMANDS_H_
#define __HERBSTLUFT_ROOTCOMMANDS_H_

/** commands that don't belong to a particular object
 * but modify the global state */

#include "utils.h"
#include <functional>

class Root;
class Attribute;

int substitute_cmd(Root* root, Input input, Output output);
int sprintf_cmd(Root* root, Input input, Output output);
int new_attr_cmd(Root* root, Input input, Output output);
int remove_attr_cmd(Root* root, Input input, Output output);
int compare_cmd(Root* root, Input input, Output output);

Attribute* newAttributeWithType(std::string typestr, std::string attr_name, Output output);

#endif
