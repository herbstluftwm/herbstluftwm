#ifndef __HLWM_TILINGSTEP_H_
#define __HLWM_TILINGSTEP_H_

#include <tuple>
#include <list>
#include "x11-types.h"

class HSClient;

// a tilingstep describes for the given window how the tiling affects the
// window
class TilingStep {
public:
    TilingStep(Rectangle rect);
    Rectangle geometry;
    bool floated;
    bool needsRaise;
};

// a tiling result contains the movement commands etc. for all clients
class TilingResult {
public:
    TilingResult();
    TilingStep& operator[](HSClient* client);
    HSClient* focus; // the focused client
    // merge all the tiling steps from other into this
    void mergeFrom(TilingResult& other);
    std::list<std::pair<HSClient*,TilingStep>> data;
};


#endif
