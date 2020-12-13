#include "layout.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

#include "client.h"
#include "command.h"
#include "floating.h"
#include "frametree.h" // TODO: remove this dependency!
#include "globals.h"
#include "ipc-protocol.h"
#include "monitor.h"
#include "monitormanager.h"
#include "settings.h"
#include "tagmanager.h"
#include "utils.h"

using std::dynamic_pointer_cast;
using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::swap;
using std::vector;
using std::weak_ptr;

/* create a new frame
 * you can either specify a frame or a tag as its parent
 */
Frame::Frame(HSTag* tag, Settings* settings, weak_ptr<FrameSplit> parent)
    : frameIndexAttr_(this, "index", &Frame::frameIndex)
    , tag_(tag)
    , settings_(settings)
    , parent_(parent)
{}
Frame::~Frame() = default;

FrameLeaf::FrameLeaf(HSTag* tag, Settings* settings, weak_ptr<FrameSplit> parent)
    : Frame(tag, settings, parent)
    , client_count_(this, "client_count", [this]() {return clientCount(); })
    , selectionAttr_(this, "selection", &FrameLeaf::getSelection)
    , algorithmAttr_(this, "algorithm", &FrameLeaf::getLayout)
{
    auto l = settings->default_frame_layout();
    if (l >= layoutAlgorithmCount()) {
        l = 0;
    }
    layout = (LayoutAlgorithm) l;

    decoration = new FrameDecoration(*this, tag, settings);
}

FrameSplit::FrameSplit(HSTag* tag, Settings* settings, weak_ptr<FrameSplit> parent,
                       FixPrecDec fraction, SplitAlign align, shared_ptr<Frame> a,
                       shared_ptr<Frame> b)
             : Frame(tag, settings, parent)
             , splitTypeAttr_(this, "split_type", &FrameSplit::getAlign)
             , fractionAttr_(this, "fraction", &FrameSplit::getFraction)
             , selectionAttr_(this, "selection", [this]() { return selection_;})
             , aLink_(*this, "0")
             , bLink_(*this, "1")
{
    this->align_ = align;
    selection_ = 0;
    this->fraction_ = fraction;
    this->a_ = a;
    this->b_ = b;
    aLink_ = a.get();
    bLink_ = b.get();
}

void FrameLeaf::insertClient(Client* client, bool focus) {
    // insert it after the selection
    int index = std::min((selection + 1), (int)clients.size());
    clients.insert(clients.begin() + index, client);
    if (focus) {
        selection = index;
    }
    // FRAMETODO: if we we are focused, and were empty before, we have to focus
    // the client now
}

shared_ptr<FrameLeaf> FrameSplit::frameWithClient(Client* client) {
    auto found = a_->frameWithClient(client);
    if (found) {
        return found;
    } else {
        return b_->frameWithClient(client);
    }
}

shared_ptr<FrameLeaf> FrameLeaf::frameWithClient(Client* client) {
    if (find(clients.begin(), clients.end(), client) != clients.end()) {
        return thisLeaf();
    } else {
        return shared_ptr<FrameLeaf>();
    }
}

bool FrameLeaf::removeClient(Client* client) {
    auto it = find(clients.begin(), clients.end(), client);
    if (it != clients.end()) {
        auto idx = it - clients.begin();
        clients.erase(it);
        // find out new selection
        // if selection was before removed window
        // then do nothing
        // else shift it by 1
        selection -= (selection < idx) ? 0 : 1;
        // ensure valid index
        selection = std::max(std::min(selection, ((int)clients.size()) - 1), 0);
        return true;
    } else {
        return false;
    }
}

bool FrameSplit::removeClient(Client* client) {
    return a_->removeClient(client) || b_->removeClient(client);
}


FrameSplit::~FrameSplit() = default;

FrameLeaf::~FrameLeaf() {
    // free other things
    delete decoration;
}

shared_ptr<Frame> Frame::root() {
    auto parent_shared = parent_.lock();
    if (parent_shared) {
        return parent_shared->root();
    } else {
        return shared_from_this();
    }
}

bool Frame::isFocused() {
    auto p = parent_.lock();
    if (!p) {
        return true;
    } else {
        return p->selectedChild() == shared_from_this()
               && p->isFocused();
    }
}

shared_ptr<FrameLeaf> FrameLeaf::thisLeaf() {
    return dynamic_pointer_cast<FrameLeaf>(shared_from_this());
}

shared_ptr<FrameSplit> FrameSplit::thisSplit() {
    return dynamic_pointer_cast<FrameSplit>(shared_from_this());
}

shared_ptr<FrameLeaf> Frame::getGloballyFocusedFrame() {
    return get_current_monitor()->tag->frame->focusedFrame();
}

string Frame::frameIndex() const
{
    auto p = parent_.lock();
    if (p) {
        string parent_index = p->frameIndex();
        bool first_child = p->firstChild() == shared_from_this();
        return parent_index + (first_child ? "0" : "1");
    } else {
        // this is the root
        return "";
    }
}

TilingResult FrameLeaf::layoutLinear(Rectangle rect, bool vertical) {
    TilingResult res;
    auto cur = rect;
    int last_step_y;
    int last_step_x;
    int step_y;
    int step_x;
    int count = clients.size();
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

TilingResult FrameLeaf::layoutMax(Rectangle rect) {
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
        res.add(client, step);
    }
    return res;
}

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

TilingResult FrameLeaf::layoutGrid(Rectangle rect) {
    TilingResult res;
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
        int count = clients.size();
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

TilingResult FrameLeaf::computeLayout(Rectangle rect) {
    last_rect = rect;
    if (!settings_->smart_frame_surroundings() || parent_.lock()) {
        // apply frame gap
        rect.height -= settings_->frame_gap();
        rect.width -= settings_->frame_gap();
        // apply frame border
        rect.x += settings_->frame_border_width();
        rect.y += settings_->frame_border_width();
        rect.height -= settings_->frame_border_width() * 2;
        rect.width -= settings_->frame_border_width() * 2;
    }

    rect.width = std::max(WINDOW_MIN_WIDTH, rect.width);
    rect.height = std::max(WINDOW_MIN_HEIGHT, rect.height);

    // move windows
    TilingResult res;
    FrameDecorationData frame_data;
    frame_data.geometry = rect;
    frame_data.visible = true;
    frame_data.hasClients = !clients.empty();
    frame_data.hasParent = (bool)parent_.lock();
    res.focused_frame = decoration;
    res.add(decoration, frame_data);
    if (clients.empty()) {
        return res;
    }
    // whether we should omit the gap around windows:
    bool smart_window_surroundings_active =
            // only omit the border
            // if 1. the settings is activated
            settings_->smart_window_surroundings()
            // and 2. only one window is shown
            && (clientCount() == 1 || layout == LayoutAlgorithm::max);

    auto window_gap = settings_->window_gap();
    if (!smart_window_surroundings_active) {
        // deduct 'window_gap' many pixels from the left
        // and from the top border. Later, we will deduct
        // 'window_gap' many pixels from the bottom and the
        // right from every window
        rect.x += window_gap;
        rect.y += window_gap;
        rect.width -= window_gap;
        rect.height -= window_gap;

        // apply frame padding: deduct 'frame_padding' pixels
        // from all four sides:
        auto frame_padding = settings_->frame_padding();
        rect.x += frame_padding;
        rect.y += frame_padding;
        rect.width  -= frame_padding * 2;
        rect.height -= frame_padding * 2;
    }
    TilingResult layoutResult;
    switch (layout) {
        case LayoutAlgorithm::max:
            layoutResult = layoutMax(rect);
            break;
        case LayoutAlgorithm::grid:
            layoutResult = layoutGrid(rect);
            break;
        case LayoutAlgorithm::vertical:
            layoutResult = layoutVertical(rect);
            break;
        case LayoutAlgorithm::horizontal:
            layoutResult = layoutHorizontal(rect);
            break;
    }
    if (smart_window_surroundings_active) {
        for (auto& it : layoutResult.data) {
            it.second.minimalDecoration = true;
        }
    } else {
        // apply window gap: deduct 'window_gap' many pixels from
        // bottom and right of every window:
        for (auto& it : layoutResult.data) {
            it.second.geometry.width -= window_gap;
            it.second.geometry.height -= window_gap;
        }
    }
    res.mergeFrom(layoutResult);
    res.focus = clients[selection];
    return res;
}

TilingResult FrameSplit::computeLayout(Rectangle rect) {
    last_rect = rect;
    auto first = rect;
    auto second = rect;
    if (align_ == SplitAlign::vertical) {
        first.height = (rect.height * fraction_.value_) / fraction_.unit_;
        second.y += first.height;
        second.height -= first.height;
    } else { // (align == SplitAlign::horizontal)
        first.width = (rect.width * fraction_.value_) / fraction_.unit_;
        second.x += first.width;
        second.width -= first.width;
    }
    TilingResult res;
    auto res1 = a_->computeLayout(first);
    auto res2 = b_->computeLayout(second);
    res.mergeFrom(res1);
    res.mergeFrom(res2);
    res.focus = (selection_ == 0) ? res1.focus : res2.focus;
    res.focused_frame = (selection_ == 0) ? res1.focused_frame : res2.focused_frame;
    return res;
}

void FrameSplit::fmap(function<void(FrameSplit*)> onSplit, function<void(FrameLeaf*)> onLeaf, int order) {
    if (order <= 0) {
        onSplit(this);
    }
    a_->fmap(onSplit, onLeaf, order);
    if (order == 1) {
        onSplit(this);
    }
    b_->fmap(onSplit, onLeaf, order);
    if (order >= 1) {
        onSplit(this);
    }
}

void FrameLeaf::fmap(function<void(FrameSplit*)> onSplit, function<void(FrameLeaf*)> onLeaf, int order) {
    (void) onSplit;
    (void) order;
    onLeaf(this);
}

void Frame::foreachClient(ClientAction action) {
    fmap([action] (FrameSplit* s) {},
         [action] (FrameLeaf* l) {
            for (Client* client : l->clients) {
                action(client);
            }
         },
         0);
}

int frame_current_bring(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto client = get_client(argv[1]);
    if (!client) {
        output << argv[0] << ": Could not find client";
        if (argc > 1) {
            output << " \"" << argv[1] << "\".\n";
        } else {
            output << ".\n";
        }
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = get_current_monitor()->tag;
    global_tags->moveClient(client, tag, {}, true);
    // mark as un-minimized first, such that a minimized tiling client
    // is added to the frame tree now.
    client->minimized_ = false;
    auto frame = tag->frame->root_->frameWithClient(client);
    if (frame && !frame->isFocused()) {
        // regardless of the client's floating or minimization state:
        // if the client was in the frame tree, it is moved
        // to the focused frame
        frame->removeClient(client);
        tag->frame->focusedFrame()->insertClient(client, true);
    }
    focus_client(client, false, false, true);
    return 0;
}

void FrameLeaf::setSelection(int index) {
    if (clients.empty()) {
        return;
    }
    if (index < 0 || index >= static_cast<int>(clients.size())) {
        index = clients.size() - 1;
    }
    selection = index;
}

int Frame::splitsToRoot(SplitAlign align) {
    if (!parent_.lock()) {
        return 0;
    }
    return parent_.lock()->splitsToRoot(align);
}
int FrameSplit::splitsToRoot(SplitAlign align) {
    if (!parent_.lock()) {
        return 0;
    }
    int delta = 0;
    if (this->align_ == align) {
        delta = 1;
    }
    return delta + parent_.lock()->splitsToRoot(align);
}

bool FrameSplit::split(SplitAlign alignment, FixPrecDec fraction)
{
    bool tooManySplits = false;
    fmap([] (FrameSplit*) {}, [&] (FrameLeaf* l) {
        tooManySplits = tooManySplits
                || l->splitsToRoot(alignment) > HERBST_MAX_TREE_HEIGHT;
    }, 0);
    if (tooManySplits) {
        return false;
    }
    auto first = shared_from_this();
    auto second = make_shared<FrameLeaf>(tag_, settings_, weak_ptr<FrameSplit>());
    auto new_this = make_shared<FrameSplit>(tag_, settings_, parent_, fraction, alignment, first, second);
    tag_->frame->replaceNode(shared_from_this(), new_this);
    first->parent_ = new_this;
    second->parent_ = new_this;
    return true;
}

void FrameSplit::replaceChild(shared_ptr<Frame> old, shared_ptr<Frame> newchild) {
    if (a_ == old) {
        a_ = newchild;
        newchild->parent_ = thisSplit();
        aLink_ = a_.get();
    }
    if (b_ == old) {
        b_ = newchild;
        newchild->parent_ = thisSplit();
        bLink_ = b_.get();
    }
}

void FrameLeaf::addClients(const vector<Client*>& vec, bool atFront) {
    auto targetPosition = atFront ? clients.begin() : clients.end();
    clients.insert(targetPosition, vec.begin(), vec.end());
}

bool FrameLeaf::split(SplitAlign alignment, FixPrecDec fraction, size_t childrenLeaving) {
    if (splitsToRoot(alignment) > HERBST_MAX_TREE_HEIGHT) {
        return false;
    }
    int childrenStaying = std::max((size_t)0, clients.size() - childrenLeaving);
    vector<Client*> leaves(clients.begin() + childrenStaying, clients.end());
    clients.erase(clients.begin() + childrenStaying, clients.end());
    // ensure fraction is allowed
    fraction = FrameSplit::clampFraction(fraction);
    auto first = shared_from_this();
    auto second = make_shared<FrameLeaf>(tag_, settings_, weak_ptr<FrameSplit>());
    second->layout = layout;
    auto new_this = make_shared<FrameSplit>(tag_, settings_, parent_, fraction, alignment, first, second);
    second->parent_ = new_this;
    second->addClients(leaves);
    tag_->frame->replaceNode(thisLeaf(), new_this);
    parent_ = new_this;
    if (selection >= childrenStaying) {
        second->setSelection(selection - childrenStaying);
        selection = std::max(0, childrenStaying - 1);
    }
    return true;
}


void FrameSplit::swapChildren() {
    swap(a_,b_);
    aLink_ = a_.get();
    bLink_ = b_.get();
}

void FrameSplit::adjustFraction(FixPrecDec delta) {
    fraction_ = fraction_ + delta;
    fraction_ = clampFraction(fraction_);
}

void FrameSplit::setFraction(FixPrecDec fraction)
{
    fraction_ = clampFraction(fraction);
}

FixPrecDec FrameSplit::clampFraction(FixPrecDec fraction)
{
    auto minFrac = FRAME_MIN_FRACTION;
    auto maxFrac = FixPrecDec::fromInteger(1) - minFrac;
    if (fraction < minFrac) {
        return minFrac;
    }
    if (fraction > maxFrac) {
        return maxFrac;
    }
    return fraction;
}

/**
 * @brief find a neighbour frame in the specified direction. The neighbour frame
 * can be a FrameLeaf or a FrameSplit. Its parent frame is the FrameSplit
 * that manages the border between the 'this' frame and the returned neighbour
 * @param direction
 * @return returns the neighbour, if there is any.
 */
shared_ptr<Frame> FrameLeaf::neighbour(Direction direction) {
    bool found = false;
    shared_ptr<Frame> other;
    shared_ptr<Frame> child = shared_from_this();
    shared_ptr<FrameSplit> frame = getParent();
    while (frame) {
        // find frame, where we can change the
        // selection in the desired direction
        switch(direction) {
            case Direction::Right:
                if (frame->getAlign() == SplitAlign::horizontal
                    && frame->firstChild() == child) {
                    found = true;
                    other = frame->secondChild();
                }
                break;
            case Direction::Left:
                if (frame->getAlign() == SplitAlign::horizontal
                    && frame->secondChild() == child) {
                    found = true;
                    other = frame->firstChild();
                }
                break;
            case Direction::Down:
                if (frame->getAlign() == SplitAlign::vertical
                    && frame->firstChild() == child) {
                    found = true;
                    other = frame->secondChild();
                }
                break;
            case Direction::Up:
                if (frame->getAlign() == SplitAlign::vertical
                    && frame->secondChild() == child) {
                    found = true;
                    other = frame->firstChild();
                }
        }
        if (found) {
            break;
        }
        // else: go one step closer to root
        child = frame;
        frame = frame->getParent();
    }
    if (!found) {
        return shared_ptr<Frame>();
    }
    return other;
}

//! finds the neighbour of the selected client in the specified direction
// within the frame
//! returns its index or -1 if there is none
int FrameLeaf::getInnerNeighbourIndex(Direction direction) {
    int index = -1;
    int count = clientCount();
    switch (getLayout()) {
        case LayoutAlgorithm::vertical:
            if (direction == Direction::Down) {
                index = selection + 1;
            }
            if (direction == Direction::Up) {
                index = selection - 1;
            }
            break;
        case LayoutAlgorithm::horizontal:
            if (direction == Direction::Right) {
                index = selection + 1;
            }
            if (direction == Direction::Left) {
                index = selection - 1;
            }
            break;
        case LayoutAlgorithm::max:
            break;
        case LayoutAlgorithm::grid: {
            int rows, cols;
            frame_layout_grid_get_size(count, &rows, &cols);
            if (cols == 0) {
                break;
            }
            int r = selection / cols;
            int c = selection % cols;
            switch (direction) {
                case Direction::Down:
                    index = selection + cols;
                    if (g_settings->gapless_grid() && index >= count && r == (rows - 2)) {
                        // if grid is gapless and we're in the second-last row
                        // then it means last client is below us
                        index = count - 1;
                    }
                    break;
                case Direction::Up: index = selection - cols; break;
                case Direction::Right:
                    if (c < cols - 1) {
                        index = selection + 1;
                    }
                    break;
                case Direction::Left:
                    if (c > 0) {
                        index = selection - 1;
                    }
                    break;
            }
            break;
        }
    }
    // check that index is valid
    if (index < 0 || index >= count) {
        index = -1;
    }
    return index;
}

void FrameLeaf::moveClient(int new_index) {
    swap(clients[new_index], clients[selection]);
    selection = new_index;
}

int frame_move_window_command(int argc, char** argv, Output output) {
    // usage: move left|right|up|down
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    string dirstr = argv[1];
    int external_only = g_settings->default_direction_external_only();
    if (argc > 2 && !strcmp(argv[1], "-i")) {
        external_only = false;
        dirstr = argv[2];
    }
    if (argc > 2 && !strcmp(argv[1], "-e")) {
        external_only = true;
        dirstr = argv[2];
    }
    Direction direction;
    try {
        direction = Converter<Direction>::parse(dirstr);
    } catch (const std::exception& e) {
        output << argv[0] << ": " << e.what() << "\n";
        return HERBST_INVALID_ARGUMENT;
    }
    shared_ptr<FrameLeaf> frame = Frame::getGloballyFocusedFrame();
    Client* currentClient = get_current_client();
    if (currentClient && currentClient->is_client_floated()) {
        // try to move the floating window
        bool success = floating_shift_direction(direction);
        return success ? 0 : HERBST_FORBIDDEN;
    }
    int index;
    if (!external_only &&
        (index = frame->getInnerNeighbourIndex(direction)) != -1) {
        frame->moveClient(index);
        get_current_monitor()->applyLayout();
    } else {
        shared_ptr<Frame> neighbour = frame->neighbour(direction);
        Client* client = frame->focusedClient();
        if (client && neighbour) { // if neighbour was found
            // move window to neighbour
            frame->removeClient(client);
            FrameTree::focusedFrame(neighbour)->insertClient(client);
            neighbour->frameWithClient(client)->select(client);

            // change selection in parent
            shared_ptr<FrameSplit> parent = neighbour->getParent();
            assert(parent);
            parent->swapSelection();

            // layout was changed, so update it
            get_current_monitor()->applyLayout();
        } else {
            output << argv[0] << ": No neighbour found\n";
            return HERBST_FORBIDDEN;
        }
    }
    return 0;
}

void FrameLeaf::select(Client* client) {
    auto it = find(clients.begin(), clients.end(), client);
    if (it != clients.end()) {
        selection = it - clients.begin();
    }
}

Client* FrameSplit::focusedClient() {
    return (selection_ == 0 ? a_->focusedClient() : b_->focusedClient());
}

Client* FrameLeaf::focusedClient() {
    if (!clients.empty()) {
        return clients[selection];
    }
    return nullptr;
}

// focus a window
// switch_tag tells, whether to switch tag to focus to window
// switch_monitor tells, whether to switch monitor to focus to window
// raise tells whether to also raise the client
// returns if window was focused or not
bool focus_client(Client* client, bool switch_tag, bool switch_monitor, bool raise) {
    if (!client) {
        // client is not managed
        return false;
    }
    HSTag* tag = client->tag();
    assert(client->tag());
    Monitor* monitor = find_monitor_with_tag(tag);
    Monitor* cur_mon = get_current_monitor();
    if (!monitor && !switch_tag) {
        return false;
    }
    if (monitor != cur_mon && !switch_monitor) {
        // if we are not allowed to switch tag
        // and tag is not on current monitor (or on no monitor)
        // than we cannot focus the window
        return false;
    }
    if (monitor && monitor != cur_mon) {
        // switch monitor
        monitor_focus_by_index((int)monitor->index());
        cur_mon = get_current_monitor();
        assert(cur_mon == monitor);
    }
    g_monitors->lock();
    monitor_set_tag(cur_mon, tag);
    cur_mon = get_current_monitor();
    if (cur_mon->tag != tag) {
        // could not set tag on monitor
        g_monitors->unlock();
        return false;
    }
    // now the right tag is visible
    // now focus it
    bool found = tag->focusClient(client);
    if (found && raise) {
        client->raise();
    }
    cur_mon->applyLayout();
    // the client will be visible already, but in most
    // WMs the client will stay un-minimized even
    // if the focus goes away, so mark it as un-minimized:
    client->minimized_ = false;
    g_monitors->unlock();
    return found;
}

void Frame::setVisibleRecursive(bool visible) {
    auto onSplit = [] (FrameSplit* frame) { };
    // X11 tweaks here.
    auto onLeaf =
        [visible] (FrameLeaf* frame) {
            if (!visible) {
                frame->decoration->hide();
            }
            for (auto c : frame->clients) {
                c->set_visible(visible);
            }
        };
    // first hide children => order = 2
    fmap(onSplit, onLeaf, 2);
}

vector<Client*> FrameLeaf::removeAllClients() {
    vector<Client*> result;
    swap(result, clients);
    selection = 0;
    return result;
}

int frame_focus_edge(Input input, Output output) {
    // Puts the focus to the edge in the specified direction
    g_monitors->lock();
    int oldval = g_settings->focus_crosses_monitor_boundaries();
    g_settings->focus_crosses_monitor_boundaries = false;
    Input inp = {"focus", input.toVector()};
    while (0 == Commands::call(inp, output)) {
        ;
    }
    g_settings->focus_crosses_monitor_boundaries = oldval;
    g_monitors->unlock();
    return 0;
}

int frame_move_window_edge(int argc, char** argv, Output output) {
    // Moves a window to the edge in the specified direction
    g_monitors->lock();
    int oldval = g_settings->focus_crosses_monitor_boundaries();
    g_settings->focus_crosses_monitor_boundaries = false;
    while (0 == frame_move_window_command(argc, argv, output)) {
        ;
    }
    g_settings->focus_crosses_monitor_boundaries = oldval;
    g_monitors->unlock();
    return 0;
}

