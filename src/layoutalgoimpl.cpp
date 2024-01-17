#include "layoutalgoimpl.h"

#include "client.h"
#include "layout.h"
#include "settings.h"
#include "utils.h"

using namespace std;

LayoutAlgoImpl::LayoutAlgoImpl(Params p)
    : frame_(p.frame)
    , settings_(p.frame.settingsConst())
    , algoName_(p.algoName)
{
}

LayoutAlgoImpl::~LayoutAlgoImpl()
{
}


static inline TilingResult layoutLinear(const vector<Client*>& clients, Rectangle rect, bool vertical) {
    TilingResult res;
    auto cur = rect;
    int last_step_y;
    int last_step_x;
    int step_y;
    int step_x;
    int count = static_cast<int>(clients.size());
    if (vertical) {
        // only do steps in y direction
        last_step_y = cur.height % count; // get the space on bottom
        last_step_x = 0;
        cur.height /= count;
        step_y = cur.height;
        step_x = 0;
    } else {
        // only do steps in x direction
        last_step_y = 0;
        last_step_x = cur.width % count; // get the space on the right
        cur.width /= count;
        step_y = 0;
        step_x = cur.width;
    }
    int i = 0;
    for (auto client : clients) {
        // add the space, if count does not divide frameheight without remainder
        cur.height += (i == count-1) ? last_step_y : 0;
        cur.width += (i == count-1) ? last_step_x : 0;
        res.add(client, TilingStep(cur));
        cur.y += step_y;
        cur.x += step_x;
        i++;
    }
    return res;
}


class LayoutVertical : public LayoutAlgoImpl {
public:
    LayoutVertical(Params p) : LayoutAlgoImpl(p) {}
    virtual TilingResult compute(Rectangle rect) override {
        return layoutLinear(frame_.clientsConst(), rect, true);
    }
    virtual int neighbour(Direction direction, DirectionLevel depth, int startIndex) override {
        switch (direction) {
        case Direction::Up:
            return startIndex - 1;
        case Direction::Down:
            return startIndex + 1;
        default:
            return -1;
        }
    }
};

class LayoutHorizontal : public LayoutAlgoImpl {
public:
    LayoutHorizontal(Params p) : LayoutAlgoImpl(p) {}
    virtual TilingResult compute(Rectangle rect) override {
        return layoutLinear(frame_.clientsConst(), rect, false);
    }
    virtual int neighbour(Direction direction, DirectionLevel depth, int startIndex) override {
        switch (direction) {
        case Direction::Left:
            return startIndex - 1;
        case Direction::Right:
            return startIndex + 1;
        default:
            return -1;
        }
    }
};


class LayoutMax : public LayoutAlgoImpl {
public:
    LayoutMax(Params p) : LayoutAlgoImpl(p) {}
    virtual TilingResult compute(Rectangle rect) override {
        const vector<Client*>& clients = frame_.clientsConst();
        int selection = frame_.getSelection();
        TilingResult res;
        // go through all clients from top to bottom and remember
        // whether they are still visible. The stacking order is such that
        // the windows at the end of 'clients' are on top of the windows
        // at the beginning of 'clients'. So start at the selection and go
        // downwards in the stack, i.e. backwards in the 'clients' array
        bool stillVisible = true;
        for (size_t idx = 0; idx < clients.size(); idx++) {
            Client* client = clients[(selection + clients.size() - idx) % clients.size()];
            TilingStep step(rect);
            step.visible = stillVisible;
            // the next is only visible, if the current client is visible
            // and if the current client is pseudotiled
            stillVisible = stillVisible && client->pseudotile_();
            if (client == clients[selection]) {
                step.needsRaise = true;
            }
            if (settings_->tabbed_max()) {
                step.tabs = clients;
            }
            res.add(client, step);
        }
        return res;
    }
    virtual int neighbour(Direction direction, DirectionLevel depth, int startIndex) override {
        if (settings_->tabbed_max()) {
            if (depth >= DirectionLevel::Tabs) {
                switch (direction) {
                case Direction::Right:
                    return startIndex + 1;
                case Direction::Left:
                    return startIndex - 1;
                default:
                    return -1;
                }
            }
        } else {
            // ordinary max layout without tabs:
            if (depth == DirectionLevel::All) {
                switch (direction) {
                    case Direction::Right:
                    case Direction::Down:
                        return startIndex + 1;
                    case Direction::Left:
                    case Direction::Up:
                        return startIndex - 1;
                }
            }
        }
        // can't be reached anyway
        return -1;
    }
};

class LayoutGrid : public LayoutAlgoImpl {
public:
    LayoutGrid(Params p) : LayoutAlgoImpl(p) {}
private:
    void frame_layout_grid_get_size(size_t count, int* res_rows, int* res_cols) {
        unsigned cols = 0;
        while (cols * cols < count) {
            cols++;
        }
        *res_cols = cols;
        if (*res_cols != 0) {
            *res_rows = (count / cols) + (count % cols ? 1 : 0);
        } else {
            *res_rows = 0;
        }
    }
public:
    virtual TilingResult compute(Rectangle rect) override {
        TilingResult res;
        const vector<Client*>& clients = frame_.clientsConst();
        if (clients.empty()) {
            return res;
        }

        int rows, cols;
        frame_layout_grid_get_size(clients.size(), &rows, &cols);
        int width = rect.width / cols;
        int height = rect.height / rows;
        int i = 0;
        auto cur = rect; // current rectangle
        for (int r = 0; r < rows; r++) {
            // reset to left
            cur.x = rect.x;
            cur.width = width;
            cur.height = height;
            if (r == rows -1) {
                // fill small pixel gap below last row
                cur.height += rect.height % rows;
            }
            int count = static_cast<int>(clients.size());
            for (int c = 0; c < cols && i < count; c++) {
                if (settings_->gapless_grid() && (i == count - 1) // if last client
                    && (count % cols != 0)) {           // if cols remain
                    // fill remaining cols with client
                    cur.width = rect.x + rect.width - cur.x;
                } else if (c == cols - 1) {
                    // fill small pixel gap in last col
                    cur.width += rect.width % cols;
                }

                // apply size
                res.add(clients[i], TilingStep(cur));
                cur.x += width;
                i++;
            }
            cur.y += height;
        }
        return res;
    }
    virtual int neighbour(Direction direction, DirectionLevel depth, int startIndex) override {
        size_t count = frame_.clientsConst().size();
        int rows, cols;
        frame_layout_grid_get_size(count, &rows, &cols);
        if (cols == 0) {
            return -1;
        }
        int r = startIndex / cols;
        int c = startIndex % cols;
        switch (direction) {
        case Direction::Down: {
            int index = startIndex + cols;
            if (g_settings->gapless_grid() && index >= static_cast<int>(count) && r == (rows - 2)) {
                // if grid is gapless and we're in the second-last row
                // then it means last client is below us
                index = static_cast<int>(count) - 1;
            }
            return index;
        }
        case Direction::Up:
            return startIndex - cols;
        case Direction::Right:
            if (c < cols - 1) {
                return startIndex + 1;
            } else {
                return -1;
            }
        case Direction::Left:
            if (c > 0) {
                return startIndex - 1;
            } else {
                return -1;
            }
        }
        // can't be reached anyway:
        return -1;
    }
};


unique_ptr<LayoutAlgoImpl> LayoutAlgoImpl::createInstance(const FrameLeaf& frame, LayoutAlgorithm algoName)
{
    Params p = {frame, algoName};
    switch (algoName) {
    case LayoutAlgorithm::vertical:
        return make_unique<LayoutVertical>(p);
    case LayoutAlgorithm::horizontal:
        return make_unique<LayoutHorizontal>(p);
    case LayoutAlgorithm::grid:
        return make_unique<LayoutGrid>(p);
    case LayoutAlgorithm::max:
        return make_unique<LayoutMax>(p);
    }
    // this can never be reached...
    return {};
}
