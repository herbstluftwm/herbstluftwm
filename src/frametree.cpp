#include "frametree.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <regex>

#include "argparse.h"
#include "client.h"
#include "completion.h"
#include "fixprecdec.h"
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
    : rootLink_(*this, "root")
    , focused_frame_(*this, "focused_frame", &FrameTree::focusedFramePlainPtr)
    , tag_(tag)
    , settings_(settings)
{
    root_ = make_shared<FrameLeaf>(tag, settings, shared_ptr<FrameSplit>());
    rootLink_ = root_.get();
    (void) tag_;
    (void) settings_;
    focused_frame_.setDoc("The focused frame (leaf) in this frame tree");
}

void FrameTree::foreachClient(function<void(Client*)> action)
{
    root_->foreachClient(action);
}

void FrameTree::dump(shared_ptr<Frame> frame, Output output)
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
            << Converter<FixPrecDec>::str(s->fraction_)
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
shared_ptr<Frame> FrameTree::lookup(const string& path) {
    shared_ptr<Frame> node = root_;
    for (char c : path) {
        if (c == 'e') {
            shared_ptr<FrameLeaf> emptyFrame = findEmptyFrameNearFocus(node);
            if (emptyFrame) {
                // go to the empty node if we had found some
                node = emptyFrame;
            }
            continue;
        }
        if (c == '@') {
            node = focusedFrame(node);
            continue;
        }
        if (c == 'p') {
            auto parent = node->parent_;
            // only change the 'node' if 'parent' is set.
            // if 'parent' is not set, then 'node' is already
            // the root node; in this case we stay at the
            // root.
            if (parent.lock()) {
                node = parent.lock();
            }
            continue;
        }
        node = node->switchcase<shared_ptr<Frame>>(
            [](shared_ptr<FrameLeaf> l) {
                // nothing to do on a leaf
                return l;
            },
            [c](shared_ptr<FrameSplit> l) {
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
shared_ptr<FrameLeaf> FrameTree::focusedFrame() {
    return focusedFrame(root_);
}

/*! get the focused frame within the subtree of the given node
 */
shared_ptr<FrameLeaf> FrameTree::focusedFrame(shared_ptr<Frame> node) {
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
    get_current_monitor()->applyLayout();
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
    if (!removedFrameClients.empty()) {
        if (insertAtFront) {
            targetFrameLeaf->setSelection(clientFocusIndex);
        } else {
            targetFrameLeaf->setSelection(clientFocusIndex + oldClientCount);
        }
    }
    get_current_monitor()->applyLayout();
    return 0;
}

int FrameTree::rotateCommand() {
    void (*onSplit)(FrameSplit*) =
        [] (FrameSplit* s) {
            switch (s->align_) {
                case SplitAlign::vertical:
                    s->align_ = SplitAlign::horizontal;
                    break;
                case SplitAlign::horizontal:
                    s->align_ = SplitAlign::vertical;
                    s->selection_ = s->selection_ ? 0 : 1;
                    s->swapChildren();
                    s->fraction_ = FixPrecDec::fromInteger(1) - s->fraction_;
                    break;
            }
        };
    void (*onLeaf)(FrameLeaf*) =
        [] (FrameLeaf*) {
        };
    // first hide children => order = 2
    root_->fmap(onSplit, onLeaf, -1);
    get_current_monitor()->applyLayout();
    return 0;
}

template<>
Finite<FrameTree::MirrorDirection>::ValueList Finite<FrameTree::MirrorDirection>::values = ValueListPlain {
    { FrameTree::MirrorDirection::Horizontal, "horizontal" },
    { FrameTree::MirrorDirection::Vertical, "vertical" },
    { FrameTree::MirrorDirection::Both, "both" },
};

int FrameTree::mirrorCommand(Input input, Output output)
{
    using MD = MirrorDirection;
    MirrorDirection dir = MD::Horizontal;
    ArgParse ap = ArgParse().optional(dir);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }
    auto onSplit = [dir] (FrameSplit* s) {
            bool mirror =
                dir == MD::Both
                || (dir == MD::Horizontal && s->align_ == SplitAlign::horizontal)
                || (dir == MD::Vertical && s->align_ == SplitAlign::vertical);
            if (mirror) {
                s->selection_ = s->selection_ ? 0 : 1;
                s->swapChildren();
                s->fraction_ = FixPrecDec::fromInteger(1) - s->fraction_;
            }
        };
    root_->fmap(onSplit, [] (FrameLeaf*) { }, -1);
    get_current_monitor()->applyLayout();
    return 0;
}

void FrameTree::mirrorCompletion(Completion& complete)
{
    if (complete == 0) {
        Converter<MirrorDirection>::complete(complete);
    }
}

shared_ptr<TreeInterface> FrameTree::treeInterface(
        shared_ptr<Frame> frame,
        shared_ptr<FrameLeaf> focus)
{
    class LeafTI : public TreeInterface {
    public:
        LeafTI(shared_ptr<FrameLeaf> l, shared_ptr<FrameLeaf> focus)
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
        shared_ptr<FrameLeaf> l_;
        shared_ptr<FrameLeaf> focus_;
    };
    class SplitTI : public TreeInterface {
    public:
        SplitTI(shared_ptr<FrameSplit> s, shared_ptr<FrameLeaf> focus)
            : s_(s), focus_(focus) {}
        shared_ptr<TreeInterface> nthChild(size_t idx) override {
            return treeInterface(((idx == 0) ? s_->firstChild()
                                            : s_->secondChild()),
                                 focus_);
        }
        size_t childCount() override { return 2; };
        void appendCaption(Output output) override {
            output << " " << Converter<SplitAlign>::str(s_->align_)
                   << " " << (s_->fraction_.value_ * 100 / s_->fraction_.unit_)
                   << "%"
                   << " selection=" << s_->selection_;
        }
    private:
        shared_ptr<FrameSplit> s_;
        shared_ptr<FrameLeaf> focus_;
    };
    return frame->switchcase<shared_ptr<TreeInterface>>(
        [focus] (shared_ptr<FrameLeaf> l) {
            return std::static_pointer_cast<TreeInterface>(
                    make_shared<LeafTI>(l, focus));
        },
        [focus] (shared_ptr<FrameSplit> s) {
            return std::static_pointer_cast<TreeInterface>(
                    make_shared<SplitTI>(s, focus));
        }
    );
}

void FrameTree::prettyPrint(shared_ptr<Frame> frame, Output output) {
    auto focus = get_current_monitor()->tag->frame->focusedFrame();
    tree_print_to(treeInterface(frame, focus), output);
}

shared_ptr<FrameLeaf> FrameTree::findEmptyFrameNearFocusGeometrically(shared_ptr<Frame> subtree)
{
    // render frame geometries.
    TilingResult tileres = subtree->computeLayout({0, 0, 800, 800});
    function<Rectangle(shared_ptr<FrameLeaf>)> frame2geometry =
            [tileres] (shared_ptr<FrameLeaf> frame) -> Rectangle {
        for (auto& framedata : tileres.frames) {
            if (framedata.first == frame->decoration) {
                return framedata.second.geometry;
            }
        }
        // if not found, return an invalid rectangle;
        return { 0, 0, -1, -1};
    };
    vector<shared_ptr<FrameLeaf>> emptyLeafs;
    subtree->fmap([](FrameSplit*){}, [&emptyLeafs](FrameLeaf* leaf) {
        if (leaf->clientCount() == 0) {
            emptyLeafs.push_back(leaf->thisLeaf());
        }
    });
    Rectangle geoFocused = frame2geometry(focusedFrame(subtree));
    if (!geoFocused) { // this should never happen actually
        return {};
    }
    int bestDistance = std::numeric_limits<int>::max();
    shared_ptr<FrameLeaf> closestFrame = {};
    for (auto l : emptyLeafs) {
        Rectangle r = frame2geometry(l);
        if (!r) {
            continue;
        }
        int dist = geoFocused.manhattanDistanceTo(r);
        if (dist < bestDistance) {
            closestFrame = l;
            bestDistance = dist;
        }
    }
    return closestFrame;
}

FrameLeaf* FrameTree::focusedFramePlainPtr()
{
    auto shared = focusedFrame();
    if (shared) {
        return shared.get();
    } else {
        return nullptr;
    }
}

//! check whether there is an empty frame in the given subtree,
//! and if there are some, try to find one that is as close as possible to the
//! focused frame leaf. returns {} if there is no empty frame in the subtree
shared_ptr<FrameLeaf> FrameTree::findEmptyFrameNearFocus(shared_ptr<Frame> subtree)
{
    // the following algorithm is quadratic in the number of vertices in the
    // frame, because we look for empty frames in the same subtree over and
    // over again. However, this is much easier to implement then checking
    // only those frames for emptyness that have not been checked before.

    // start at the focused leaf
    shared_ptr<Frame> current = focusedFrame(subtree);
    // and then go upward in the tree
    while (current) {
        auto emptyNode = findEmptyFrameNearFocusGeometrically(current);
        if (emptyNode) {
            return emptyNode;
        }
        if (current == subtree) {
            // if we reached the root of the subtree, stop searching
            return {};
        }
        current = current->getParent();
    }
    return {};
}

shared_ptr<FrameLeaf> FrameTree::findFrameWithClient(Client* client) {
    shared_ptr<FrameLeaf> frame = {};
    root_->fmap(
        [](FrameSplit*) {},
        [&](FrameLeaf* l) {
            auto& cs = l->clients;
            if (std::find(cs.begin(), cs.end(), client) != cs.end()) {
                frame = l->thisLeaf();
            }
        });
    return frame;
}

bool FrameTree::contains(shared_ptr<Frame> frame) const
{
    return frame->root() == this->root_;
}

//! resize the borders of the focused client in the specific direction by 'delta'
//! returns whether the focused frame has a border in the specified direction.
bool FrameTree::resizeFrame(FixPrecDec delta, Direction direction)
{
    // if direction is left or up we have to flip delta
    // because e.g. resize up by 0.1 actually means:
    // reduce fraction by 0.1, i.e. delta = -0.1
    if (direction == Direction::Left || direction == Direction::Up) {
        delta.value_ = delta.value_ * -1;
    }

    shared_ptr<Frame> neighbour = focusedFrame()->neighbour(direction);
    if (!neighbour) {
        // then try opposite direction
        std::map<Direction, Direction> flip = {
            {Direction::Left, Direction::Right},
            {Direction::Right, Direction::Left},
            {Direction::Down, Direction::Up},
            {Direction::Up, Direction::Down},
        };
        direction = flip[direction];
        neighbour = focusedFrame()->neighbour(direction);
        if (!neighbour) {
            return false;
        }
    }
    auto parent = neighbour->getParent();
    assert(parent); // if has neighbour, it also must have a parent
    parent->adjustFraction(delta);
    return true;
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

void FrameTree::focusFrame(shared_ptr<Frame> frame) {
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

//! focus a client/frame in the given direction. if externalOnly=true,
//! focus the frame in the specified direction; otherwise, focus the frame or client
//! in the specified direction. Return whether the focus changed
bool FrameTree::focusInDirection(Direction direction, bool externalOnly)
{
    auto curframe = focusedFrame();
    if (!externalOnly) {
        int index = curframe->getInnerNeighbourIndex(direction);
        if (index >= 0) {
            curframe->setSelection(index);
            return true;
        }
    }
    // if this didn't succeed, find a frame:
    shared_ptr<Frame> neighbour = curframe->neighbour(direction);
    if (neighbour) { // if neighbour was found
        shared_ptr<FrameSplit> parent = neighbour->getParent();
        if (parent) {
            // alter focus (from 0 to 1, from 1 to 0)
            parent->swapSelection();
        }
        return true;
    }
    return false;
}

bool FrameTree::shiftInDirection(Direction direction, bool externalOnly) {
    shared_ptr<FrameLeaf> sourceFrame = this->focusedFrame();
    Client* client = sourceFrame->focusedClient();
    if (!client) {
        return false;
    }
    // don't look for neighbours within the frame if 'external_only' is set
    int indexInFrame = externalOnly ? (-1) : sourceFrame->getInnerNeighbourIndex(direction);
    if (indexInFrame >= 0) {
        sourceFrame->moveClient(indexInFrame);
        return true;
    } else {
        shared_ptr<Frame> neighbour = sourceFrame->neighbour(direction);
        if (neighbour) { // if neighbour was found
            // move window to neighbour
            sourceFrame->removeClient(client);
            FrameTree::focusedFrame(neighbour)->insertClient(client);
            neighbour->frameWithClient(client)->select(client);

            // change selection in parent
            shared_ptr<FrameSplit> parent = neighbour->getParent();
            assert(parent);
            parent->swapSelection();
            return true;
        } else {
            return false;
        }
    }
}

//! go to the specified frame. Return true on success, return false if
//! the end is reached (this command never wraps). Skips covered windows
//! if skipInvisible is set.
bool FrameTree::cycleAll(FrameTree::CycleDelta cdelta, bool skip_invisible)
{
    shared_ptr<FrameLeaf> focus = focusedFrame();
    if (cdelta == CycleDelta::Begin || cdelta == CycleDelta::End) {
        // go to first resp. last frame
        cycle_frame([cdelta] (size_t idx, size_t len) {
            if (cdelta == CycleDelta::Begin) {
                return size_t(0);
            } else { // cdelta == CycleDelta::End
                return len - 1;
            }
        });
        // go to first resp. last window in it
        auto frame = focusedFrame();
        if (!(frame->layout == LayoutAlgorithm::max && skip_invisible)) {
            auto count = frame->clientCount();
            if (cdelta == CycleDelta::Begin) {
                frame->setSelection(0);
            } else if (count > 0) { // cdelta == CycleDelta::End
                frame->setSelection(int(count - 1));
            }
        }
        return true;
    }
    int delta = (cdelta == CycleDelta::Next) ? 1 : -1;
    bool frameChanges = (focus->layout == LayoutAlgorithm::max && skip_invisible)
        || (focus->clientCount() == 0)
        || (delta == 1 && focus->getSelection() + 1 == static_cast<int>(focus->clientCount()))
        || (delta == -1 && focus->getSelection() == 0);
    if (!frameChanges) {
        // if the focused frame does not change, it's simple
        auto count = focus->clientCount();
        if (count != 0) {
            focus->setSelection(MOD(focus->getSelection() + delta, count));
        }
    } else { // if the frame changes:
        // otherwise we need to find the next frame in direction 'delta'
        bool wouldWrap = false;
        cycle_frame([delta, &wouldWrap](size_t idx, size_t len) {
            wouldWrap = (idx == 0 && delta == -1)
                    || (idx + 1 >= len && delta == 1);
            if (wouldWrap) {
                return idx; // do nothing
            } else {
                return idx + delta;
            }
        });
        if (wouldWrap) {
            // do not wrap, do not go there
            return false;
        }
        // if it does not wrap
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
    return true;
}

void FrameTree::cycle_frame(function<size_t(size_t,size_t)> indexAndLenToIndex) {
    shared_ptr<FrameLeaf> focus = focusedFrame();
    // First, enumerate all frames in traversal order
    // and find the focused frame in there
    vector<shared_ptr<FrameLeaf>> frames;
    size_t index = 0;
    root_->fmap(
        [](FrameSplit*) {},
        [&](FrameLeaf* l) {
            if (l == focus.get()) {
                // the index of the next item we push back
                index = frames.size();
            }
            frames.push_back(l->thisLeaf());
        });
    index = indexAndLenToIndex(index, frames.size());
    focusFrame(frames[index]);
}

void FrameTree::cycle_frame(int delta) {
    cycle_frame([delta](size_t index, size_t len) {
        return MOD(index + delta, len);
    });
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
        int token_len = std::max((size_t)1, parsingResult.error_->first.second.size());
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
void FrameTree::applyFrameTree(shared_ptr<Frame> target,
                               shared_ptr<RawFrameNode> source)
{
    if (!source) {
        // nothing to do
        return;
    }
    shared_ptr<FrameSplit> targetSplit = target->isSplit();
    shared_ptr<FrameLeaf> targetLeaf = target->isLeaf();
    shared_ptr<RawFrameSplit> sourceSplit = source->isSplit();
    shared_ptr<RawFrameLeaf> sourceLeaf = source->isLeaf();
    if (sourceLeaf) {
        // detach the clients from their current frame
        // this might even involve the above targetLeaf / targetSplit
        // so we need to do this before everything else
        for (const auto& client : sourceLeaf->clients) {
            // first un-minimize and un-float the client
            // such that we know that it is in the frame-tree
            client->floating_ = false;
            client->minimized_ = false;
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
        // assert that "target" is a FrameLeaf
        if (targetSplit) {
            // if its a split, then replace the split
            targetLeaf = make_shared<FrameLeaf>(
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
        // assert that target is a FrameSplit
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

void FrameTree::replaceNode(shared_ptr<Frame> old,
                            shared_ptr<Frame> replacement) {
    auto parent = old->getParent();
    if (!parent) {
        assert(old == root_);
        root_ = replacement;
        rootLink_ = root_.get();
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
        size_t idx = 0;  // take the first in the list per default
        if (curposition != input.end()) {
            // if the current layout name is in the list, take the next one
            // (respective to the delta)
            idx = MOD((curposition - input.begin()) + delta, count);
        }
        try {
            layout_index = (int)Converter<LayoutAlgorithm>::parse(*(input.begin() + idx));
        } catch (const std::exception& e) {
            output << input.command() << ": " << e.what();
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        /* cycle through the default list of layouts */
        layout_index = MOD((int)cur_frame->getLayout() + delta, layoutAlgorithmCount());
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

int FrameTree::setLayoutCommand(Input input, Output output) {
    LayoutAlgorithm layout = LayoutAlgorithm::vertical;
    ArgParse ap;
    ap.mandatory(layout);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }

    auto curFrame = focusedFrame();
    curFrame->setLayout(layout);
    get_current_monitor()->applyLayout();

    return HERBST_EXIT_SUCCESS;
}

void FrameTree::setLayoutCompletion(Completion& complete) {
    if (complete == 0) {
        Converter<LayoutAlgorithm>::complete(complete, nullptr);
    } else {
        complete.none();
    }
}

//! modes for the 'split' command
class SplitMode {
public:
    SplitMode(string name_, SplitAlign align_, bool frameToFirst_, int selection_)
        : name(name_)
          , align(align_)
          , frameToFirst(frameToFirst_)
          , selection(selection_)
    {}

    string name;
    SplitAlign align;

    //! If former frame moves to first child
    bool frameToFirst;

    //! Which child to select after the split
    int selection;

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
        { "explode",    align_explode,            true,   -1  },
        { "auto",       align_auto,               true,   0   },
    };
}

int FrameTree::splitCommand(Input input, Output output)
{
    // usage: split t|b|l|r|h|v FRACTION [Frameindex]
    string splitType;
    bool userDefinedFraction = false;
    FixPrecDec fraction = FixPrecDec::approxFrac(1, 2);
    string frameIndex = "@"; // split the focused frame per default
    ArgParse ap;
    ap.mandatory(splitType).optional(fraction, &userDefinedFraction);
    ap.optional(frameIndex);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }
    fraction = FrameSplit::clampFraction(fraction);
    shared_ptr<Frame> frame = lookup(frameIndex);
    int lh = frame->lastRect().height;
    int lw = frame->lastRect().width;
    SplitAlign align_auto = (lw > lh) ? SplitAlign::horizontal : SplitAlign::vertical;
    SplitAlign align_explode = SplitAlign::vertical;
    auto availableModes = SplitMode::modes(align_explode, align_auto);
    auto mode = std::find_if(
            availableModes.begin(), availableModes.end(),
            [=](const SplitMode &x){ return x.name[0] == splitType[0]; });
    if (mode == availableModes.end()) {
        output << input.command() << ": Invalid alignment \"" << splitType << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    SplitMode m = *mode;
    auto layout = frame->switchcase<LayoutAlgorithm>(&FrameLeaf::getLayout,
        [] (shared_ptr<FrameSplit> f) {
            return splitAlignToLayoutAlgorithm(f->getAlign());
    });
    // if 'frame' is a FrameSplit, we simply set the
    // window count to 0, because we do not have a count of windows
    // that stay in the 'old frame'.
    auto windowcount = frame->switchcase<size_t>(&FrameLeaf::clientCount,
        [](shared_ptr<FrameSplit>) { return 0; });
    bool exploding = m.name == "explode";
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
        size_t count1 = windowcount;
        size_t nc1 = (count1 + 1) / 2;      // new count for the first frame
        if ((layout == LayoutAlgorithm::horizontal
            || layout == LayoutAlgorithm::vertical)
            && !userDefinedFraction && count1 > 1) {
            fraction.value_ = (nc1 * fraction.unit_) / count1;
        }
    }
    // move second half of the window buf to second frame
    size_t childrenLeaving = 0;
    if (exploding) {
        childrenLeaving = windowcount / 2;
    }
    auto frameIsLeaf = frame->isLeaf();
    auto frameIsSplit = frame->isSplit();
    shared_ptr<FrameSplit> frameParent = {}; // the new parent
    if (frameIsLeaf) {
        if (!frameIsLeaf->split(m.align, fraction, childrenLeaving)) {
            return 0;
        }
        frameParent = frameIsLeaf->getParent();
    }
    if (frameIsSplit) {
        if (!frameIsSplit->split(m.align, fraction)) {
            return 0;
        }
        frameParent = frameIsSplit->getParent();
    }

    if (!m.frameToFirst) {
        frameParent->swapChildren();
    }
    if (m.selection >= 0) {
        frameParent->setSelection(m.selection);
    }

    // redraw monitor
    get_current_monitor()->applyLayout();
    return 0;
}


//! Implementation of the commands "dump" and "layout"
int FrameTree::dumpLayoutCommand(Input input, Output output) {
    shared_ptr<Frame> frame = root_;
    string tagName;
    if (input >> tagName) {
        FrameTree* tree = this;
        // an empty tagName means 'current tag'
        // (this is a special case that is not handled by find_tag()
        // so we handle it explicitly here)
        if (!tagName.empty()) {
            HSTag* tag = find_tag(tagName.c_str());
            if (!tag) {
                output << input.command() << ": Tag \"" << tagName << "\" not found\n";
                return HERBST_INVALID_ARGUMENT;
            }
            tree = tag->frame();
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
        global_tags->completeEntries(complete);
    } else if (complete == 1) {
        // no completion for frame index
    } else {
        complete.none();
    }
}

