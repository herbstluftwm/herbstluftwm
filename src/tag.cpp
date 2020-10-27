#include "tag.h"

#include <type_traits>

#include "argparse.h"
#include "client.h"
#include "completion.h"
#include "floating.h"
#include "frametree.h"
#include "hlwmcommon.h"
#include "hook.h"
#include "ipc-protocol.h"
#include "layout.h"
#include "monitormanager.h"
#include "root.h"
#include "settings.h"
#include "stack.h"
#include "tagmanager.h"

using std::endl;
using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;

static bool    g_tag_flags_dirty = true;

HSTag::HSTag(string name_, TagManager* tags, Settings* settings)
    : frame(*this, "tiling")
    , index(this, "index", 0)
    , floating(this, "floating", false, [](bool){return "";})
    , floating_focused(this, "floating_focused", false, [](bool){return "";})
    , name(this, "name", name_,
        [tags](string newName) { return tags->isValidTagName(newName); })
    , frame_count(this, "frame_count", &HSTag::computeFrameCount)
    , client_count(this, "client_count", &HSTag::computeClientCount)
    , urgent_count(this, "urgent_count", &HSTag::countUrgentClients)
    , curframe_windex(this, "curframe_windex",
        [this] () { return frame->focusedFrame()->getSelection(); } )
    , curframe_wcount(this, "curframe_wcount",
        [this] () { return frame->focusedFrame()->clientCount(); } )
    , flags(0)
    , floating_clients_focus_(0)
    , oldName_(name_)
    , settings_(settings)
{
    stack = make_shared<Stack>();
    frame.init(this, settings);
    floating.changed().connect(this, &HSTag::onGlobalFloatingChange);
    // FIXME: actually this connection of the signals like this
    // must work:
    //   floating_focused.changedByUser().connect(needsRelayout_);
    // however, we need to call this:
    floating_focused.changedByUser().connect([this] () {
        this->needsRelayout_.emit();
    });
    floating_focused.setValidator([this](bool v) {
        return this->floatingLayerCanBeFocused(v);
    });
}

HSTag::~HSTag() {
    frame.reset();
}

void HSTag::setIndexAttribute(unsigned long new_index) {
    index = new_index;
}

//! give the focus within this tag to the specified client
bool HSTag::focusClient(Client* client)
{
    if (frame->focusClient(client)) {
        floating_focused = false;
        return true;
    } else {
        auto& v = floating_clients_;
        auto it = std::find(v.begin(), v.end(), client);
        if (it == v.end()) {
            return false;
        }
        floating_focused = true;
        floating_clients_focus_ = it - v.begin();
        return true;
    }
}

void HSTag::applyFloatingState(Client* client)
{
    if (!client) {
        return;
    }
    bool focused = client == focusedClient();
    if (focused) {
        // make it that client stays focused
        floating_focused = client->floating_();
    }
    if (client->floating_()) {
        // client wants to be floated
        if (!frame->root_->removeClient(client)) {
            return;
        }
        floating_clients_.push_back(client);
        if (focused) {
            floating_clients_focus_ = floating_clients_.size() - 1;
        }
        stack->sliceRemoveLayer(client->slice, LAYER_NORMAL);
        stack->sliceAddLayer(client->slice, LAYER_FLOATING);
    } else {
        // client wants to be tiled again
        auto it = std::find(floating_clients_.begin(), floating_clients_.end(), client);
        if (it == floating_clients_.end()) {
            return;
        }
        floating_clients_.erase(it);
        frame->focusedFrame()->insertClient(client, true);
        if (!floating()) {
            stack->sliceRemoveLayer(client->slice, LAYER_FLOATING);
        }
        stack->sliceAddLayer(client->slice, LAYER_NORMAL);
    }
    needsRelayout_.emit();
}

void HSTag::setVisible(bool visible)
{
    frame->root_->setVisibleRecursive(visible);
    for (Client* c : floating_clients_) {
        c->set_visible(visible);
    }
}

bool HSTag::removeClient(Client* client) {
    if (frame->root_->removeClient(client)) {
        return true;
    }
    auto it = std::find(floating_clients_.begin(), floating_clients_.end(), client);
    if (it == floating_clients_.end()) {
        return false;
    }
    floating_clients_.erase(it);
    fixFocusIndex();
    if (floating_clients_.empty()) {
        // if it was the last floating client
        // focus back the tiling
        floating_focused = false;
    }
    return true;
}

void HSTag::foreachClient(function<void (Client *)> loopBody)
{

    frame->root_->foreachClient(loopBody);
    for (Client* c: floating_clients_) {
        loopBody(c);
    }
}

void HSTag::focusFrame(shared_ptr<FrameLeaf> frameToFocus)
{
    floating_focused = false;
    FrameTree::focusFrame(frameToFocus);
    needsRelayout_.emit();
}

Client *HSTag::focusedClient()
{
    if (floating_focused()) {
        fixFocusIndex();
        if (floating_clients_focus_ < floating_clients_.size()) {
            return floating_clients_[floating_clients_focus_];
        } else {
            return nullptr;
        }
    } else {
        return frame->root_->focusedClient();
    }
}

void HSTag::insertClient(Client* client, string frameIndex, bool focus)
{
    if (client->floating_()) {
        floating_clients_.push_back(client);
        if (focus) {
            floating_clients_focus_ = floating_clients_.size() - 1;
            floating_focused = true;
        }
    } else {
        auto target = FrameTree::focusedFrame(frame->lookup(frameIndex));
        if (focus) {
            // ensure that the target frame is focused in the entire tree
            frame->focusFrame(target);
            floating_focused = false;
        }
        target->insertClient(client, focus);
    }
}

void HSTag::insertClientSlice(Client* client)
{
    stack->insertSlice(client->slice);
    if (floating()) {
        stack->sliceAddLayer(client->slice, LAYER_FLOATING);
    } else if (!client->floating_()) {
        stack->sliceRemoveLayer(client->slice, LAYER_FLOATING);
    }
}

void HSTag::removeClientSlice(Client* client)
{
    if (floating() && !client->floating_()) {
        stack->sliceRemoveLayer(client->slice, LAYER_FLOATING);
    }
    stack->removeSlice(client->slice);
}

//! directional focus command
int HSTag::focusInDirCommand(Input input, Output output)
{
    bool external_only = settings_->default_direction_external_only();
    if (input.size() >= 2) {
        string internextern;
        input >> internextern;
        if (internextern == "-i") {
            external_only = false;
        }
        if (internextern == "-e") {
            external_only = true;
        }
    }
    Direction direction = Direction::Left; // some default to satisfy the linter
    ArgParse ap;
    ap.mandatory(direction);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }

    auto focusedFrame = frame->focusedFrame();
    bool neighbour_found = true;
    if (floating || floating_focused) {
        neighbour_found = floating_focus_direction(direction);
    } else {
        neighbour_found = frame->focusInDirection(direction, external_only);
        if (neighbour_found) {
            needsRelayout_.emit();
        }
    }
    if (!neighbour_found && settings_->focus_crosses_monitor_boundaries()) {
        // find monitor in the specified direction
        int idx = g_monitors->indexInDirection(get_current_monitor(), direction);
        if (idx >= 0) {
            monitor_focus_by_index(idx);
            return 0;
        }
    }
    if (!neighbour_found) {
        output << input.command() << ": No neighbour found\n";
        return HERBST_FORBIDDEN;
    }
    return 0;
}

void HSTag::focusInDirCompletion(Completion &complete)
{
    if (complete == 0) {
        complete.full({"-i", "-e"});
        Converter<Direction>::complete(complete, nullptr);
    } else if (complete == 1
               && (complete[0] == "-i" || complete[0] == "-e"))
    {
        Converter<Direction>::complete(complete, nullptr);
    } else {
        complete.none();
    }
}

int HSTag::cycleAllCommand(Input input, Output output)
{
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
    if (floating_focused()) {
        int newIndex = static_cast<int>(floating_clients_focus_) + delta;
        if (newIndex < 0) {
            floating_focused = false;
            frame->cycleAll(FrameTree::CycleDelta::End, skip_invisible);
        } else if (static_cast<size_t>(newIndex) >= floating_clients_.size()) {
            floating_focused = false;
            frame->cycleAll(FrameTree::CycleDelta::Begin, skip_invisible);
        } else {
            floating_clients_focus_ = static_cast<size_t>(newIndex);
            floating_clients_[floating_clients_focus_]->raise();
        }
    } else {
        FrameTree::CycleDelta cdelta = (delta == 1)
                ? FrameTree::CycleDelta::Next
                : FrameTree::CycleDelta::Previous;
        bool focusChanged = frame->cycleAll(cdelta, skip_invisible);
        if (!focusChanged) {
            // if frame->cycleAll() reached the end of the tiling layer
            if (floating_clients_.empty()) {
                // we need to wrap. when cycling forward, we wrap to the beginning
                FrameTree::CycleDelta rewind = (delta == 1)
                            ? FrameTree::CycleDelta::Begin
                            : FrameTree::CycleDelta::End;
                frame->cycleAll(rewind, skip_invisible);
            } else {
                // if there are floating clients, switch to the floating layer
                floating_focused = true;
                if (delta == 1) {
                    // wrap (forward) to first client
                    floating_clients_focus_ = 0;
                } else {
                    // wrap (backward) to last client
                    floating_clients_focus_ = floating_clients_.size() - 1;
                }
            }
        }
    }
    Client* newFocus = focusedClient();
    if (newFocus && newFocus->is_client_floated()) {
        newFocus->raise();
    }
    // finally, redraw the layout
    needsRelayout_.emit();
    return 0;
}

void HSTag::cycleAllCompletion(Completion& complete)
{
    if (complete == 0) {
        complete.full({"+1", "-1", "--skip-invisible" });
    } else if (complete == 1 && complete[0] == "--skip-invisible") {
        complete.full({"+1", "-1"});
    } else {
        complete.none();
    }
}

int HSTag::resizeCommand(Input input, Output output)
{
    Direction direction = Direction::Left;
    FixPrecDec delta = FixPrecDec::approxFrac(1, 50); // 0.02
    auto ap = ArgParse().mandatory(direction).optional(delta);
    if (ap.parsingFails(input, output)) {
        return ap.exitCode();
    }
    Client* client = focusedClient();
    if (client && client->is_client_floated()) {
        if (!floating_resize_direction(this, client, direction)) {
            // no error message because this shouldn't happen anyway
            return HERBST_FORBIDDEN;
        }
    } else {
        if (!frame->resizeFrame(delta, direction)) {
            output << input.command() << ": No neighbour found\n";
            return HERBST_FORBIDDEN;
        }
    }
    needsRelayout_.emit();
    return 0;
}

void HSTag::resizeCompletion(Completion& complete)
{
    if (complete == 0) {
        Converter<Direction>::complete(complete, nullptr);
    } else if (complete == 1) {
        complete.full({"-0.02", "0.02"});
    } else {
        complete.none();
    }
}

void HSTag::onGlobalFloatingChange(bool newState)
{
    // move tiling client slices between layers
    frame->foreachClient([this,newState](Client* client) {
        if (newState) {
            // we add the tiled clients from the bottom such that they do not
            // cover single-floated clients
            stack->sliceAddLayer(client->slice, LAYER_FLOATING, false);
        } else {
            stack->sliceRemoveLayer(client->slice, LAYER_FLOATING);
        }
    });
    needsRelayout_.emit();
}

void HSTag::fixFocusIndex()
{
   static_assert(std::is_same<decltype(floating_clients_focus_), size_t>::value,
                 "we assume that index cannot be negative.");
   if (floating_clients_focus_ >= floating_clients_.size()) {
       if (floating_clients_.empty()) {
           floating_clients_focus_  = 0;
       } else {
           floating_clients_focus_  = floating_clients_.size() - 1;
       }
   }
}

int HSTag::computeFrameCount() {
    int count = 0;
    frame->root_->fmap([](FrameSplit*) {},
                [&count](FrameLeaf*) { count++; },
                0);
    return count;
}

int HSTag::countUrgentClients()
{
    int count = 0;
    foreachClient([&](Client* client) {
        if (client->urgent_()) {
            count++;
        }
    });
    return count;
}

int HSTag::computeClientCount() {
    int count = static_cast<int>(floating_clients_.size());
    frame->root_->fmap([](FrameSplit*) {},
                [&count](FrameLeaf* l) { count += l->clientCount(); },
                0);
    return count;
}

int    tag_get_count() {
    return global_tags->size();
}

HSTag* find_tag(const char* name) {
    for (auto t : *global_tags) {
        if (t->name == name) {
            return &* t;
        }
    }
    return nullptr;
}

HSTag* get_tag_by_index(int index) {
    return &* global_tags->byIdx(index);
}

void tag_force_update_flags() {
    g_tag_flags_dirty = false;
    // unset all tags
    for (auto t : *global_tags) {
        t->flags = 0;
    }
    // update flags
    for (auto c : Root::common().clients()) {
        auto client = c.second;
        TAG_SET_FLAG(client->tag(), TAG_FLAG_USED);
        if (client->urgent_) {
            TAG_SET_FLAG(client->tag(), TAG_FLAG_URGENT);
        }
    }
}

void tag_update_flags() {
    if (g_tag_flags_dirty) {
        tag_force_update_flags();
    }
}

void tag_set_flags_dirty() {
    g_tag_flags_dirty = true;
    hook_emit({"tag_flags"});
}

HSTag* find_tag_with_toplevel_frame(Frame* frame) {
    for (auto t : *global_tags) {
        if (&* t->frame->root_ == frame) {
            return &* t;
        }
    }
    return nullptr;
}

//! close the focused client or remove if the frame is empty
int HSTag::closeOrRemoveCommand() {
    Client* client = focusedClient();
    if (client) {
        client->requestClose();
        return 0;
    } else if (!floating_focused) {
        // since the tiling layer is focused
        // and no client is focused, we know that the
        // focused frame is empty.
        return frame->removeFrameCommand();
    }
    return 0;
}

string HSTag::floatingLayerCanBeFocused(bool floatingFocused)
{
    if (floatingFocused && floating_clients_.empty()) {
        return "There are no floating windows;"
               " cannot focus empty floating layer.";
    } else {
        return "";
    }
}

//! same as close or remove but directly remove frame after last client
int HSTag::closeAndRemoveCommand() {
    Client* client = focusedClient();
    if (client) {
        // note that this just sends the closing signal
        client->requestClose();
        // so the client still exists in the following
    }
    // remove a frame if a frame is focused, that is if
    // the tag is in tiling mode and the tiling layer is focused
    bool frameFocused = !floating() && !floating_focused;
    if (frameFocused && frame->focusedFrame()->clientCount() <= 1) {
        return frame->removeFrameCommand();
    }
    return 0;
}

