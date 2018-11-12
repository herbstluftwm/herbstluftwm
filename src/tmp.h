#ifndef __HERBSTLUFT_TMP_H_
#define __HERBSTLUFT_TMP_H_

#include "object.h"
class Root;

class Tmp : public Object {
public:
    Tmp();
    int mktemp(Input input, Output output);
private:
    unsigned long number_active;
    unsigned long number_total;
};

#endif

