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

class MouseCombo : public ModifierCombo {
public:
    MouseCombo() = default;
    MouseCombo(unsigned int modifiers, unsigned int button);

    unsigned int button_ = 0;
    bool operator==(const MouseCombo& other) const {
        return ModifierCombo::operator==(other)
            && button_ == other.button_;
    }
    static std::vector<std::pair<std::string, unsigned int>> name2button;
};

ConverterInstance(MouseCombo)

// get the vector to snap a client to it's neighbour
void client_snap_vector(Client* client, Monitor* monitor,
                        enum SnapFlags flags,
                        int* return_dx, int* return_dy);

bool is_point_between(int point, int left, int right);

#endif

