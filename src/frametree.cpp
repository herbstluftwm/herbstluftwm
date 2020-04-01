#include "frametree.h"

#include <algorithm>
#include <regex>

#include "client.h"
#include "completion.h"
#include "framedata.h"
#include "frameparser.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitor.h"
#include "stack.h"
#include "tag.h"
#include "tagmanager.h"
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
        output << "(clients "
               << Converter<LayoutAlgorithm>::str(l->layout) << ":"
               << l->selection;
        for (auto client : l->clients) {
            output << " " << WindowID(client->x11Window()).str();
        }
        output << ")";
    }
    auto s = frame->isSplit();
    if (s) {
        output
            << "(split "
            << Converter<SplitAlign>::str(s->align_)
            << ":"
            << ((double)s->fraction_) / (double)FRACTION_UNIT
            << ":"
            << s->selection_
            << " ";
        FrameTree::dump(s->a_, output);
        output << " ";
        FrameTree::dump(s->b_, output);
        output << ")";
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
    auto clientFocusIndex = frame->getSelection();
    auto parent = frame->getParent();
    bool insertAtFront = (frame == parent->firstChild());
    auto newparent =
            insertAtFront ? parent->secondChild() : parent->firstChild();
    auto removedFrameClients = frame->removeAllClients();
    // determine the 'targetFrameLeaf' where the clients of 'frame' will go to.
    // this target frame shall be the frame leaf that is closest
    // to the 'frame' that is removed (such that the clients visually travel as
    // little distance as possible).
    auto targetFrameAbstract = newparent;
    while (targetFrameAbstract->isSplit()) {
        auto s = targetFrameAbstract->isSplit();
        if (s->getAlign() == parent->getAlign()) {
            // for splits with the same alignment, go as near
            // as possible towards 'frame', and also adjust the focus
            // into that direction
            s->setSelection(insertAtFront ? 0 : 1);
        } else {
            // for splits with the other alignment, we can
            // follow the focus, since this is most likely the frame
            // used last
        }
        targetFrameAbstract = s->selectedChild();
    }
    // if targetFrameAbstract is not a split, it must be a leaf:
    auto targetFrameLeaf = targetFrameAbstract->isLeaf();
    int oldClientCount = (int)targetFrameLeaf->clientCount();
    targetFrameLeaf->addClients(removedFrameClients, insertAtFront);
    // now, frame is empty
    replaceNode(parent, newparent);

    // focus the same client again
    if (removedFrameClients.size() > 0) {
        if (insertAtFront) {
            targetFrameLeaf->setSelection(clientFocusIndex);
        } else {
            targetFrameLeaf->setSelection(clientFocusIndex + oldClientCount);
        }
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
            output << " " << Converter<LayoutAlgorithm>::str(l_->layout) << ":";
            for (auto client : l_->clients) {
                output << " " << WindowID(client->x11Window()).str();
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
            output << " " << Converter<SplitAlign>::str(s_->align_)
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

bool FrameTree::contains(std::shared_ptr<HSFrame> frame) const
{
    return frame->root() == this->root_;
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
            output << " " << WindowID(e.second).str()
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
        // root frame should never have a parent:
        root_->parent_ = {};
    } else {
        parent->replaceChild(old, replacement);
    }
}

int FrameTree::cycleLayoutCommand(Input input, Output output) {
    int delta = 1;
    auto cur_frame = focusedFrame();
    string deltaStr;
    if (input >> deltaStr) {
        try {
            delta = Converter<int>::parse(deltaStr);
        } catch (const std::exception& e) {
            output << input.command() << ": " << e.what();
            return HERBST_INVALID_ARGUMENT;
        }
    }
    int layout_index;
    if (!input.empty()) {
        /* cycle through a given list of layouts */
        string curname = Converter<LayoutAlgorithm>::str(cur_frame->getLayout());
        size_t count = input.end() - input.begin();
        auto curposition = std::find(input.begin(), input.end(), curname);
        size_t idx = (curposition - input.begin()) + delta;
        idx += count;
        idx %= count;
        try {
            layout_index = (int)Converter<LayoutAlgorithm>::parse(*(input.begin() + idx));
        } catch (const std::exception& e) {
            output << input.command() << ": " << e.what();
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        /* cycle through the default list of layouts */
        layout_index = (int)cur_frame->getLayout() + delta;
        layout_index %= layoutAlgorithmCount();
        layout_index += layoutAlgorithmCount();
        layout_index %= layoutAlgorithmCount();
    }
    cur_frame->setLayout((LayoutAlgorithm)layout_index);
    get_current_monitor()->applyLayout();
    return 0;
}

void FrameTree::cycleLayoutCompletion(Completion& complete) {
    if (complete == 0) {
        complete.full({ "-1", "+1" });
    } else if (complete <= layoutAlgorithmCount()) {
        // it does not make sense to mention layout names multiple times
        Converter<LayoutAlgorithm>::complete(complete, nullptr);
    } else {
        complete.none();
    }
}

//! modes for the 'split' command
class SplitMode {
public:
    string name;
    SplitAlign align;
    bool frameToFirst;  // if former frame moves to first child
    int selection;      // which child to select after the split
    static vector<SplitMode> modes(SplitAlign align_explode = SplitAlign::horizontal, SplitAlign align_auto = SplitAlign::horizontal);
};

vector<SplitMode> SplitMode::modes(SplitAlign align_explode, SplitAlign align_auto)
{
    return {
        { "top",        SplitAlign::vertical,     false,  1   },
        { "bottom",     SplitAlign::vertical,     true,   0   },
        { "vertical",   SplitAlign::vertical,     true,   0   },
        { "right",      SplitAlign::horizontal,   true,   0   },
        { "horizontal", SplitAlign::horizontal,   true,   0   },
        { "left",       SplitAlign::horizontal,   false,  1   },
        { "explode",    align_explode,            true,   0   },
        { "auto",       align_auto,               true,   0   },
    };
}

int FrameTree::splitCommand(Input input, Output output)
{
    // usage: split t|b|l|r|h|v FRACTION
    string splitType, strFraction;
    if (!(input >> splitType )) {
        return HERBST_NEED_MORE_ARGS;
    }
    bool userDefinedFraction = input >> strFraction;
    double fractionFloat = userDefinedFraction ? atof(strFraction.c_str()) : 0.5;
    int fraction = HSFrameSplit::clampFraction(FRACTION_UNIT * fractionFloat);
    auto frame = focusedFrame();
    int lh = frame->lastRect().height;
    int lw = frame->lastRect().width;
    SplitAlign align_auto = (lw > lh) ? SplitAlign::horizontal : SplitAlign::vertical;
    SplitAlign align_explode = SplitAlign::vertical;
    auto availableModes = SplitMode::modes(align_explode, align_auto);
    SplitMode m;
    for (auto &it : availableModes) {
        if (it.name[0] == splitType[0]) {
            m = it;
        }
    }
    bool exploding = m.name == "explode";
    if (m.name.empty()) {
        output << input.command() << ": Invalid alignment \"" << splitType << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    auto layout = frame->getLayout();
    auto windowcount = frame->clientCount();
    if (exploding) {
        if (windowcount <= 1) {
            m.align = align_auto;
        } else if (layout == LayoutAlgorithm::max) {
            m.align = align_auto;
        } else if (layout == LayoutAlgorithm::grid && windowcount == 2) {
            m.align = SplitAlign::horizontal;
        } else if (layout == LayoutAlgorithm::horizontal) {
            m.align = SplitAlign::horizontal;
        } else {
            m.align = SplitAlign::vertical;
        }
        size_t count1 = frame->clientCount();
        size_t nc1 = (count1 + 1) / 2;      // new count for the first frame
        if ((layout == LayoutAlgorithm::horizontal
            || layout == LayoutAlgorithm::vertical)
            && !userDefinedFraction && count1 > 1) {
            fraction = (nc1 * FRACTION_UNIT) / count1;
        }
    }
    // move second half of the window buf to second frame
    size_t childrenLeaving = 0;
    if (exploding) {
        childrenLeaving = frame->clientCount() / 2;
    }
    if (!frame->split(m.align, fraction, childrenLeaving)) {
        return 0;
    }
    if (!m.frameToFirst) {
        frame->getParent()->swapChildren();
    }
    frame->getParent()->setSelection(m.selection);
    // redraw monitor
    get_current_monitor()->applyLayout();
    return 0;
}


//! Implementation of the commands "dump" and "layout"
int FrameTree::dumpLayoutCommand(Input input, Output output) {
    shared_ptr<HSFrame> frame = root_;
    string tagName;
    if (input >> tagName) {
        shared_ptr<FrameTree> tree = shared_from_this();
        // an empty tagName means 'current tag'
        // (this is a special case that is not handled by find_tag()
        // so we handle it explicitly here)
        if (tagName != "") {
            HSTag* tag = find_tag(tagName.c_str());
            if (!tag) {
                output << input.command() << ": Tag \"" << tagName << "\" not found\n";
                return HERBST_INVALID_ARGUMENT;
            }
            tree = tag->frame;
        }
        string frameIndex;
        if (input >> frameIndex) {
            frame = tree->lookup(frameIndex);
        } else {
            frame = tree->root_;
        }
    }
    if (input.command() == "dump") {
        FrameTree::dump(frame, output);
    } else { // input.command() == "layout"
        FrameTree::prettyPrint(frame, output);
    }
    return 0;
}

void FrameTree::dumpLayoutCompletion(Completion& complete) {
    if (complete == 0) {
        global_tags->completeTag(complete);
    } else if (complete == 1) {
        // no completion for frame index
    } else {
        complete.none();
    }
}

