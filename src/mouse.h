#ifndef __HERBSTLUFT_MOUSE_H_
#define __HERBSTLUFT_MOUSE_H_

#include <string>
#include <vector>

#include "keycombo.h"

// various snap-flags
enum SnapFlags {
    // which edges are considered to snap
    SNAP_EDGE_TOP       = 0x01,
    SNAP_EDGE_BOTTOM    = 0x02,
    SNAP_EDGE_LEFT      = 0x04,
    SNAP_EDGE_RIGHT     = 0x08,
    SNAP_EDGE_ALL       =
        SNAP_EDGE_TOP | SNAP_EDGE_BOTTOM | SNAP_EDGE_LEFT | SNAP_EDGE_RIGHT,
};

// forward declarations
class Client;
class Monitor;
class MouseManager; // IWYU pragma: keep

using MouseFunction = void (MouseManager::*)(Client* client, const std::vector<std::string> &cmd);

class MouseCombo : public ModifierCombo {
public:
    MouseCombo() = default;
    unsigned int button_;
    bool operator==(const MouseCombo& other) const {
        return ModifierCombo::operator==(other)
            && button_ == other.button_;
    }
};

class MouseBinding {
public:
    MouseCombo mousecombo;
    MouseFunction action;
    std::vector<std::string> cmd;
};

// get the vector to snap a client to it's neighbour
void client_snap_vector(Client* client, Monitor* monitor,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy);

bool is_point_between(int point, int left, int right);

#endif

