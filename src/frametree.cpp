#include "frametree.h"

#include "client.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"

FrameTree::FrameTree(HSTag* tag, Settings* settings)
    : tag_(tag)
    , settings_(settings)
{
    root_ = std::make_shared<HSFrameLeaf>(tag, settings, std::shared_ptr<HSFrameSplit>());
    (void) tag_;
    (void) settings_;
}

void FrameTree::foreachClient(std::function<void(Client*)> action)
{
    root_->foreachClient(action);
}

void FrameTree::dump(std::shared_ptr<HSFrame> frame, Output output)
{
    auto l = frame->isLeaf();
    if (l) {
        output << LAYOUT_DUMP_BRACKETS[0]
               << "clients"
               << LAYOUT_DUMP_WHITESPACES[0]
               << g_layout_names[l->layout] << ":"
               << l->selection;
        for (auto client : l->clients) {
            output << LAYOUT_DUMP_WHITESPACES[0]
                   << "0x"
                   << std::hex << client->x11Window() << std::dec;
        }
        output << LAYOUT_DUMP_BRACKETS[1];
    }
    auto s = frame->isSplit();
    if (s) {
        output
            << LAYOUT_DUMP_BRACKETS[0]
            << "split"
            << LAYOUT_DUMP_WHITESPACES[0]
            << g_align_names[s->align_]
            << LAYOUT_DUMP_SEPARATOR
            << ((double)s->fraction_) / (double)FRACTION_UNIT
            << LAYOUT_DUMP_SEPARATOR
            << s->selection_
            << LAYOUT_DUMP_WHITESPACES[0];
        FrameTree::dump(s->a_, output);
        output << LAYOUT_DUMP_WHITESPACES[0];
        FrameTree::dump(s->b_, output);
        output << LAYOUT_DUMP_BRACKETS[1];
    }
}


/*! look up a specific frame in the frame tree
 */
std::shared_ptr<HSFrame> FrameTree::lookup(const std::string& path) {
    std::shared_ptr<HSFrame> node = root_;
    // the string "@" is a special case
    if (path == "@") {
        return focusedFrame();
    }
    for (char c : path) {
        node = node->switchcase<std::shared_ptr<HSFrame>>(
            [](std::shared_ptr<HSFrameLeaf> l) {
                // nothing to do on a leaf
                return l;
            },
            [c](std::shared_ptr<HSFrameSplit> l) {
                switch (c) {
                    case '0': return l->a_;
                    case '1': return l->b_;
                    case '/': return (l->selection_ == 0) ? l->b_ : l->a_;
                    case '.': /* fallthru */
                    default: return (l->selection_ == 0) ? l->a_ : l->b_;
                }
            }
        );
    }
    return node;
}

/*! get the frame leaf that is focused within this frame tree.
 */
std::shared_ptr<HSFrameLeaf> FrameTree::focusedFrame() {
    return focusedFrame(root_);
}

/*! get the focused frame within the subtree of the given node
 */
std::shared_ptr<HSFrameLeaf> FrameTree::focusedFrame(std::shared_ptr<HSFrame> node) {
    while (node->isLeaf() == nullptr) {
        // node must be a frame split
        auto s = node->isSplit();
        node = (s->selection_ == 0) ? s->a_ : s->b_;
    }
    return node->isLeaf();
}


int FrameTree::cycle_selection(Input input, Output output) {
    int delta = 1;
    std::string deltaStr;
    if (input >> deltaStr) {
        delta = atoi(deltaStr.c_str());
    }
    // find current selection
    auto frame = focusedFrame();
    frame->cycleSelection(delta);
    return 0;
}

//! focus the nth window within the focused frame
int FrameTree::focus_nth(Input input, Output output) {
    std::string index;
    if (!(input >> index)) {
        return HERBST_NEED_MORE_ARGS;
    }
    focusedFrame()->setSelection(atoi(index.c_str()));
    return 0;
}

//! command that removes the focused frame
int FrameTree::removeFrame() {
    auto frame = focusedFrame();
    if (!frame->getParent()) {
        // do nothing if is toplevel frame
        return 0;
    }
    auto parent = frame->getParent();
    auto pp = parent->getParent();
    auto newparent = (frame == parent->firstChild())
                     ? parent->secondChild()
                     : parent->firstChild();
    focusedFrame(newparent)->addClients(frame->removeAllClients());
    // now, frame is empty
    if (pp) {
        pp->replaceChild(parent, newparent);
    } else {
        // if parent was root frame
        root_ = newparent;
    }
    frame_focus_recursive(parent);
    get_current_monitor()->applyLayout();
    return 0;
}

//! close the focused client or remove if the frame is empty
int FrameTree::close_or_remove() {
    Client* client = focusedFrame()->focusedClient();
    if (client) {
        window_close(client->x11Window());
        return 0;
    } else {
        return removeFrame();
    }
}

//! same as close or remove but directly remove frame after last client
int FrameTree::close_and_remove() {
    bool remove_after_close = false;
    auto cur_frame = focusedFrame();
    Client* client = cur_frame->focusedClient();
    if (client) {
        // note that this just sends the closing signal
        window_close(client->x11Window());
        // so the window is still in the frame at this point
    }
    if (cur_frame->clientCount() <= 1) {
        return removeFrame();
    }
    return 0;
}

