/** Copyright 2018 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBSTLUFT_TMP_H_
#define __HERBSTLUFT_TMP_H_

#include "object.h"
class Root;

class Tmp : public Object {
public:
    Tmp(Root* root);
    int mktemp(Input input, Output output);
private:
    Root* root;
};

#endif

