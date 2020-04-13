#ifndef __HLWM_TILINGSTEP_H_
#define __HLWM_TILINGSTEP_H_

#include <list>

#include "framedecoration.h"
#include "x11-types.h"

class Client;

// a tilingstep describes for the given window how the tiling affects the
// window
class TilingStep {
public:
    TilingStep(Rectangle rect);
    Rectangle geometry;
    bool floated = false;
    bool needsRaise = false;
    bool visible = true; //! whether this window is entirely covered
                         //! by another window (e.g. in max layout)
};

// a tiling result contains the movement commands etc. for all clients
class TilingResult {
public:
    TilingResult() = default;
    TilingStep& operator[](Client* client);
    void add(FrameDecoration* dec, const FrameDecorationData& frame_data);

    Client* focus = {}; // the focused client
    FrameDecoration* focused_frame = {};

    // merge all the tiling steps from other into this
    void mergeFrom(TilingResult& other);

    std::list<std::pair<FrameDecoration*,FrameDecorationData>> frames;
    std::list<std::pair<Client*,TilingStep>> data;
};


#endif
