#include "frametree.h"

#include "client.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"
#include "tag.h"
#include "utils.h"

using std::shared_ptr;

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
    assert(node->isLeaf() != nullptr);
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
    auto count = frame->clientCount();
    if (count != 0) {
        frame->setSelection(MOD(frame->getSelection() + delta, count));
    }
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


int FrameTree::rotate() {
    void (*onSplit)(HSFrameSplit*) =
        [] (HSFrameSplit* s) {
            switch (s->align_) {
                case ALIGN_VERTICAL:
                    s->align_ = ALIGN_HORIZONTAL;
                    break;
                case ALIGN_HORIZONTAL:
                    s->align_ = ALIGN_VERTICAL;
                    s->selection_ = s->selection_ ? 0 : 1;
                    swap(s->a_, s->b_);
                    s->fraction_ = FRACTION_UNIT - s->fraction_;
                    break;
            }
        };
    void (*onLeaf)(HSFrameLeaf*) =
        [] (HSFrameLeaf*) {
        };
    // first hide children => order = 2
    root_->fmap(onSplit, onLeaf, -1);
    get_current_monitor()->applyLayout();
    return 0;
}

shared_ptr<TreeInterface> FrameTree::treeInterface(
        shared_ptr<HSFrame> frame,
        shared_ptr<HSFrameLeaf> focus)
{
    class LeafTI : public TreeInterface {
    public:
        LeafTI(shared_ptr<HSFrameLeaf> l, shared_ptr<HSFrameLeaf> focus)
            : l_(l), focus_(focus)
        {}
        shared_ptr<TreeInterface> nthChild(size_t idx) override {
            return {};
        }
        size_t childCount() override { return 0; };
        void appendCaption(Output output) override {
            output << g_layout_names[l_->layout] << ":";
            for (auto client : l_->clients) {
                output << " 0x"
                       << std::hex << client->x11Window() << std::dec;
            }
            if (l_ == focus_) {
                output << " [FOCUS]";
            }
        }
    private:
        shared_ptr<HSFrameLeaf> l_;
        shared_ptr<HSFrameLeaf> focus_;
    };
    class SplitTI : public TreeInterface {
    public:
        SplitTI(shared_ptr<HSFrameSplit> s, shared_ptr<HSFrameLeaf> focus)
            : s_(s), focus_(focus) {}
        shared_ptr<TreeInterface> nthChild(size_t idx) override {
            return treeInterface(((idx == 0) ? s_->firstChild()
                                            : s_->secondChild()),
                                 focus_);
        }
        size_t childCount() override { return 2; };
        void appendCaption(Output output) override {
            output << g_align_names[s_->align_]
                   << " " << (s_->fraction_ * 100 / FRACTION_UNIT) << "%"
                   << " selection=" << s_->selection_;
        }
    private:
        shared_ptr<HSFrameSplit> s_;
        shared_ptr<HSFrameLeaf> focus_;
    };
    return frame->switchcase<shared_ptr<TreeInterface>>(
        [focus] (shared_ptr<HSFrameLeaf> l) {
            return std::static_pointer_cast<TreeInterface>(
                    std::make_shared<LeafTI>(l, focus));
        },
        [focus] (shared_ptr<HSFrameSplit> s) {
            return std::static_pointer_cast<TreeInterface>(
                    std::make_shared<SplitTI>(s, focus));
        }
    );
}

void FrameTree::prettyPrint(shared_ptr<HSFrame> frame, Output output) {
    auto focus = get_current_monitor()->tag->frame->focusedFrame();
    tree_print_to(treeInterface(frame, focus), output);
}

