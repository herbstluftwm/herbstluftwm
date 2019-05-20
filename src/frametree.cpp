#include "frametree.h"

#include <algorithm>
#include <regex>

#include "client.h"
#include "framedata.h"
#include "frameparser.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"
#include "stack.h"
#include "tag.h"
#include "utils.h"

using std::endl;
using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;

FrameTree::FrameTree(HSTag* tag, Settings* settings)
    : tag_(tag)
    , settings_(settings)
{
    root_ = make_shared<HSFrameLeaf>(tag, settings, shared_ptr<HSFrameSplit>());
    (void) tag_;
    (void) settings_;
}

void FrameTree::foreachClient(function<void(Client*)> action)
{
    root_->foreachClient(action);
}

void FrameTree::dump(shared_ptr<HSFrame> frame, Output output)
{
    auto l = frame->isLeaf();
    if (l) {
        output << LAYOUT_DUMP_BRACKETS[0]
               << "clients"
               << LAYOUT_DUMP_WHITESPACES[0]
               << g_layout_names[(int)l->layout] << ":"
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
            << g_align_names[(int)s->align_]
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
shared_ptr<HSFrame> FrameTree::lookup(const string& path) {
    shared_ptr<HSFrame> node = root_;
    // the string "@" is a special case
    if (path == "@") {
        return focusedFrame();
    }
    for (char c : path) {
        node = node->switchcase<shared_ptr<HSFrame>>(
            [](shared_ptr<HSFrameLeaf> l) {
                // nothing to do on a leaf
                return l;
            },
            [c](shared_ptr<HSFrameSplit> l) {
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
shared_ptr<HSFrameLeaf> FrameTree::focusedFrame() {
    return focusedFrame(root_);
}

/*! get the focused frame within the subtree of the given node
 */
shared_ptr<HSFrameLeaf> FrameTree::focusedFrame(shared_ptr<HSFrame> node) {
    while (node->isLeaf() == nullptr) {
        // node must be a frame split
        auto s = node->isSplit();
        node = (s->selection_ == 0) ? s->a_ : s->b_;
    }
    assert(node->isLeaf() != nullptr);
    return node->isLeaf();
}


int FrameTree::cycleSelectionCommand(Input input, Output output) {
    int delta = 1;
    string deltaStr;
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
int FrameTree::focusNthCommand(Input input, Output output) {
    string index;
    if (!(input >> index)) {
        return HERBST_NEED_MORE_ARGS;
    }
    focusedFrame()->setSelection(atoi(index.c_str()));
    return 0;
}

//! command that removes the focused frame
int FrameTree::removeFrameCommand() {
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
    get_current_monitor()->applyLayout();
    return 0;
}

//! close the focused client or remove if the frame is empty
int FrameTree::closeOrRemoveCommand() {
    Client* client = focusedFrame()->focusedClient();
    if (client) {
        client->requestClose();
        return 0;
    } else {
        return removeFrameCommand();
    }
}

//! same as close or remove but directly remove frame after last client
int FrameTree::closeAndRemoveCommand() {
    auto cur_frame = focusedFrame();
    Client* client = cur_frame->focusedClient();
    if (client) {
        // note that this just sends the closing signal
        client->requestClose();
        // so the window is still in the frame at this point
    }
    if (cur_frame->clientCount() <= 1) {
        return removeFrameCommand();
    }
    return 0;
}


int FrameTree::rotateCommand() {
    void (*onSplit)(HSFrameSplit*) =
        [] (HSFrameSplit* s) {
            switch (s->align_) {
                case SplitAlign::vertical:
                    s->align_ = SplitAlign::horizontal;
                    break;
                case SplitAlign::horizontal:
                    s->align_ = SplitAlign::vertical;
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
            output << " " << g_layout_names[(int)l_->layout] << ":";
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
            output << " " << g_align_names[(int)s_->align_]
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
                    make_shared<LeafTI>(l, focus));
        },
        [focus] (shared_ptr<HSFrameSplit> s) {
            return std::static_pointer_cast<TreeInterface>(
                    make_shared<SplitTI>(s, focus));
        }
    );
}

void FrameTree::prettyPrint(shared_ptr<HSFrame> frame, Output output) {
    auto focus = get_current_monitor()->tag->frame->focusedFrame();
    tree_print_to(treeInterface(frame, focus), output);
}

shared_ptr<HSFrameLeaf> FrameTree::findFrameWithClient(Client* client) {
    shared_ptr<HSFrameLeaf> frame = {};
    root_->fmap(
        [](HSFrameSplit*) {},
        [&](HSFrameLeaf* l) {
            auto& cs = l->clients;
            if (std::find(cs.begin(), cs.end(), client) != cs.end()) {
                frame = l->thisLeaf();
            }
        });
    return frame;
}

bool FrameTree::focusClient(Client* client) {
    auto frameLeaf = findFrameWithClient(client);
    if (!frameLeaf) {
        return false;
    }
    // 1. focus client within its frame
    auto& cs = frameLeaf->clients;
    int index = std::find(cs.begin(), cs.end(), client) - cs.begin();
    frameLeaf->selection = index;
    // 2. make the frame focused
    focusFrame(frameLeaf);
    return true;
}

void FrameTree::focusFrame(shared_ptr<HSFrame> frame) {
    while (frame) {
        auto parent = frame->getParent();
        if (!parent) {
            break;
        }
        if (parent->firstChild() == frame) {
            parent->selection_ = 0;
        } else {
            parent->selection_ = 1;
        }
        frame = parent;
    }
}

int FrameTree::cycleAllCommand(Input input, Output output) {
    bool skip_invisible = false;
    int delta = 1;
    string s = "";
    input >> s;
    if (s == "--skip-invisible") {
        skip_invisible = true;
        // and load the next (optional) argument to s
        s = "0";
        input >> s;
    }
    try {
        delta = std::stoi(s);
    } catch (std::invalid_argument const& e) {
        output << "invalid argument: " << e.what() << endl;
        return HERBST_INVALID_ARGUMENT;
    } catch (std::out_of_range const& e) {
        output << "out of range: " << e.what() << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (delta < -1 || delta > 1) {
        output << "argument out of range." << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (delta == 0) {
        return 0; // nothing to do
    }
    shared_ptr<HSFrameLeaf> focus = focusedFrame();
    bool frameChanges = (focus->layout == LayoutAlgorithm::max && skip_invisible)
        || (delta == 1 && focus->getSelection() + 1 == focus->clientCount())
        || (delta == -1 && focus->getSelection() == 0)
        || (focus->clientCount() == 0);
    if (!frameChanges) {
        // if the focused frame does not change, it's simple
        auto count = focus->clientCount();
        if (count != 0) {
            focus->setSelection(MOD(focus->getSelection() + delta, count));
        }
    } else {
        // otherwise we need to find the next frame in direction 'delta'
        cycle_frame(delta);
        focus = focusedFrame();
        // fix the selection within the freshly focused frame.
        if (focus->layout == LayoutAlgorithm::max && skip_invisible) {
            // nothing to do
        } else if (delta == 1) {
            // focus the first client
            focus->setSelection(0);
        } else { // delta == -1
            // focus the last client
            if (focus->clientCount() > 0) {
                focus->setSelection(focus->clientCount() - 1);
            }
        }
    }
    // finally, redraw the layout
    get_current_monitor()->applyLayout();
    return 0;
}

void FrameTree::cycle_frame(int delta) {
    shared_ptr<HSFrameLeaf> focus = focusedFrame();
    // First, enumerate all frames in traversal order
    // and find the focused frame in there
    vector<shared_ptr<HSFrameLeaf>> frames;
    int index = 0;
    root_->fmap(
        [](HSFrameSplit*) {},
        [&](HSFrameLeaf* l) {
            if (l == focus.get()) {
                // the index of the next item we push back
                index = frames.size();
            }
            frames.push_back(l->thisLeaf());
        });
    index += delta;
    index = MOD(index, frames.size());
    focusFrame(frames[index]);
}

int FrameTree::cycleFrameCommand(Input input, Output output) {
    string s = "1";
    int delta = 1;
    input >> s; // try to read the optional argument
    try {
        delta = std::stoi(s);
    } catch (std::invalid_argument const& e) {
        output << "invalid argument: " << e.what() << endl;
        return HERBST_INVALID_ARGUMENT;
    } catch (std::out_of_range const& e) {
        output << "out of range: " << e.what() << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    cycle_frame(delta);
    get_current_monitor()->applyLayout();
    return 0;
}

int FrameTree::loadCommand(Input input, Output output) {
    // usage: load TAG LAYOUT
    HSTag* tag = nullptr;
    string layoutString, tagName;
    if (input.size() >= 2) {
        input >> tagName >> layoutString;
        tag = find_tag(tagName.c_str());
        if (!tag) {
            output << input.command() << ": Tag \"" << tagName << "\" not found\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else if (input.size() == 1) {
        input >> layoutString;
        tag = get_current_monitor()->tag;
    } else {
        return HERBST_NEED_MORE_ARGS;
    }
    assert(tag != nullptr);
    FrameParser parsingResult(layoutString);
    if (parsingResult.error_) {
        output << input.command() << ": Syntax error at "
               << parsingResult.error_->first.first << ": "
               << parsingResult.error_->second << ":"
               << endl;
        std::regex whitespace ("[ \n\t]");
        // print the layout again
        output << "\"" << std::regex_replace(layoutString, whitespace, string(" "))
               << "\"" << endl;
        // and underline the token
        int token_len = std::max(1ul, parsingResult.error_->first.second.size());
        output << " " // for the \" above
               << string(parsingResult.error_->first.first, ' ')
               << string(token_len, '~')
               << endl;
        return HERBST_INVALID_ARGUMENT;
    }
    if (!parsingResult.unknownWindowIDs_.empty()) {
        output << "Warning: Unknown window IDs";
        for (const auto& e : parsingResult.unknownWindowIDs_) {
            output << " 0x" << std::hex << e.second << std::dec
                   << "(\'" << e.first.second << "\')";
        }
        output << endl;
    }
    // apply the new frame tree
    tag->frame->applyFrameTree(tag->frame->root_, parsingResult.root_);
    tag_set_flags_dirty(); // we probably changed some window positions
    // arrange monitor
    Monitor* m = find_monitor_with_tag(tag);
    if (m) {
        tag->frame->root_->setVisibleRecursive(true);
        m->applyLayout();
        monitor_update_focus_objects();
    } else {
        tag->frame->root_->setVisibleRecursive(false);
    }
    return 0;
}

//! target must not be null, source may be null
void FrameTree::applyFrameTree(shared_ptr<HSFrame> target,
                               shared_ptr<RawFrameNode> source)
{
    if (!source) {
        // nothing to do
        return;
    }
    shared_ptr<HSFrameSplit> targetSplit = target->isSplit();
    shared_ptr<HSFrameLeaf> targetLeaf = target->isLeaf();
    shared_ptr<RawFrameSplit> sourceSplit = source->isSplit();
    shared_ptr<RawFrameLeaf> sourceLeaf = source->isLeaf();
    if (sourceLeaf) {
        // detach the clients from their current frame
        // this might even involve the above targetLeaf / targetSplit
        // so we need to do this before everything else
        for (const auto& client : sourceLeaf->clients) {
            client->tag()->frame->root_->removeClient(client);
            if (client->tag() != tag_) {
                client->tag()->stack->removeSlice(client->slice);
                client->setTag(tag_);
                client->tag()->stack->insertSlice(client->slice);
            }
        }
        vector<Client*> clients = sourceLeaf->clients;
        // collect all the remaining clients in the target
        target->foreachClient(
            [&clients](Client* c) {
                clients.push_back(c);
            }
        );
        // assert that "target" is a HSFrameLeaf
        if (targetSplit) {
            // if its a split, then replace the split
            targetLeaf = make_shared<HSFrameLeaf>(
                                tag_, settings_, target->getParent());
            replaceNode(target, targetLeaf);
            target = targetLeaf;
            targetSplit = {};
        }
        // make the targetLeaf look like the sourceLeaf
        targetLeaf->clients = clients;
        targetLeaf->setSelection(sourceLeaf->selection);
        targetLeaf->layout = sourceLeaf->layout;
    } else {
        // assert that target is a HSFrameSplit
        if (targetLeaf) {
            targetLeaf->split(sourceSplit->align_, sourceSplit->fraction_, 0);
            targetSplit = targetLeaf->getParent();
            target = targetSplit;
            targetLeaf = {}; // we don't need this anymore
        }
        assert(target == targetSplit);
        targetSplit->align_ = sourceSplit->align_;
        targetSplit->fraction_ = sourceSplit->fraction_;
        targetSplit->selection_ = sourceSplit->selection_;
        applyFrameTree(targetSplit->a_, sourceSplit->a_);
        applyFrameTree(targetSplit->b_, sourceSplit->b_);
    }
}

void FrameTree::replaceNode(shared_ptr<HSFrame> old,
                            shared_ptr<HSFrame> replacement) {
    auto parent = old->getParent();
    if (!parent) {
        assert(old == root_);
        root_ = replacement;
    } else {
        parent->replaceChild(old, replacement);
    }
}

