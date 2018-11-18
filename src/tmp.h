#ifndef __HERBSTLUFT_TMP_H_
#define __HERBSTLUFT_TMP_H_

#include "object.h"
class Root;

class Tmp : public Object {
public:
    Tmp() = default;
    int mktemp(Input input, Output output);
private:
    unsigned long number_active{0};
    unsigned long number_total{0};
};

#endif

