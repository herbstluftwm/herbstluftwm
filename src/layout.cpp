#include "client.h"
#include "globals.h"
#include "utils.h"
#include "x11-utils.h"
#include "hook.h"
#include "ewmh.h"
#include "ipc-protocol.h"
#include "settings.h"
#include "layout.h"
#include "stack.h"
#include "monitor.h"
#include "floating.h"
#include "tagmanager.h"
#include "monitormanager.h"
#include "utils.h"

#include <glib.h>
#include "glib-backports.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <stdint.h>

#include <memory>
#include <iomanip>
#include <sstream>
#include <algorithm>

using namespace std;

const char* g_align_names[] = {
    "vertical",
    "horizontal",
};

const char* g_layout_names[] = {
    "vertical",
    "horizontal",
    "max",
    "grid",
    nullptr,
};

void layout_init() {
}

void reset_frame_colors() {
    all_monitors_apply_layout();
}

void layout_destroy() {
}


/* create a new frame
 * you can either specify a frame or a tag as its parent
 */
HSFrame::HSFrame(HSTag* tag, Settings* settings, weak_ptr<HSFrameSplit> parent)
    : tag_(tag)
    , settings_(settings)
    , parent_(parent)
{}
HSFrame::~HSFrame() = default;

HSFrameLeaf::HSFrameLeaf(HSTag* tag, Settings* settings, weak_ptr<HSFrameSplit> parent)
    : HSFrame(tag, settings, parent)
    , selection(0)
{
    layout = settings->default_frame_layout();

    decoration = new FrameDecoration(tag, settings);
}

HSFrameSplit::HSFrameSplit(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent, int fraction, int align,
                 std::shared_ptr<HSFrame> a, std::shared_ptr<HSFrame> b)
             : HSFrame(tag, settings, parent) {
    this->align = align;
    selection = 0;
    this->fraction = fraction;
    this->a = a;
    this->b = b;
}

void HSFrameLeaf::insertClient(HSClient* client) {
    // insert it after the selection
    clients.insert(clients.begin() + std::min((selection + 1), (int)clients.size()), client);
    // FRAMETODO: if we we are focused, and were empty before, we have to focus
    // the client now
}

void HSFrameSplit::insertClient(HSClient* client) {
    if (selection == 0) a->insertClient(client);
    else                b->insertClient(client);
}

std::shared_ptr<HSFrame> HSFrameLeaf::lookup(const char*) {
    return shared_from_this(); // we are last one left
}

std::shared_ptr<HSFrame> HSFrameSplit::lookup(const char* index) {
    if (!index || index[0] == '\0') {
        return shared_from_this();
    }

    auto selected = (selection == 0) ? a : b;
    auto not_selected = (selection == 0) ? b : a;

    if (index[0] == '@') {
        // Special case: always follow selection
        return selected->lookup("@");
    }

    std::shared_ptr<HSFrame> new_root;
    switch (index[0]) {
        case '0': new_root = a; break;
        case '1': new_root = b; break;
        case '/': new_root = not_selected; break;
        case '.': /* fallthru */
        default: new_root = selected;
    }

    return new_root->lookup(index + 1);
}

std::shared_ptr<HSFrameLeaf> HSFrameSplit::frameWithClient(HSClient* client) {
    auto found = a->frameWithClient(client);
    if (found) return found;
    else return b->frameWithClient(client);
}

std::shared_ptr<HSFrameLeaf> HSFrameLeaf::frameWithClient(HSClient* client) {
    if (find(clients.begin(), clients.end(), client) != clients.end()) {
        return thisLeaf();
    } else {
        return std::shared_ptr<HSFrameLeaf>();
    }
}

bool HSFrameLeaf::removeClient(HSClient* client) {
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
        selection = std::max(std::min(selection, (int)clients.size()), 0);
        return true;
    } else {
        return false;
    }
}

bool HSFrameSplit::removeClient(HSClient* client) {
    return a->removeClient(client) || b->removeClient(client);
}


HSFrameSplit::~HSFrameSplit() = default;

HSFrameLeaf::~HSFrameLeaf() {
    // free other things
    delete decoration;
}

void HSFrameLeaf::dump(Output output) {
    output << LAYOUT_DUMP_BRACKETS[0]
           << "clients"
           << LAYOUT_DUMP_WHITESPACES[0]
           << g_layout_names[layout] << ":"
           << selection;
    for (auto client : clients) {
        output << LAYOUT_DUMP_WHITESPACES[0]
               << "0x"
               << std::hex << client->x11Window() << std::dec;
    }
    output << LAYOUT_DUMP_BRACKETS[1];
}

void HSFrameSplit::dump(Output output) {
    output
        << LAYOUT_DUMP_BRACKETS[0]
        << "split"
        << LAYOUT_DUMP_WHITESPACES[0]
        << g_align_names[align]
        << LAYOUT_DUMP_SEPARATOR
        << ((double)fraction) / (double)FRACTION_UNIT
        << LAYOUT_DUMP_SEPARATOR
        << selection
        << LAYOUT_DUMP_WHITESPACES[0];
    a->dump(output);
    output << LAYOUT_DUMP_WHITESPACES[0];
    b->dump(output);
    output << LAYOUT_DUMP_BRACKETS[1];
}

/*
 * FRAMETODO: clean up this shit...
 */
char* load_frame_tree(shared_ptr<HSFrame> frame, char* description, Output output) {
    return description;
}
/*
    // find next (
    description = strchr(description, LAYOUT_DUMP_BRACKETS[0]);
    if (!description) {
        output << "Missing " << LAYOUT_DUMP_BRACKETS[0] << "\n";
        return nullptr;
    }
    description++; // jump over (

    // goto frame type
    description += strspn(description, LAYOUT_DUMP_WHITESPACES);
    int type = TYPE_CLIENTS;
    if (description[0] == 's') {
        // if it could be "split"
        type = TYPE_FRAMES;
    }

    // get substring with frame args
    // jump to whitespaces and over them
    description += strcspn(description, LAYOUT_DUMP_WHITESPACES);
    description += strspn(description, LAYOUT_DUMP_WHITESPACES);
    // jump to whitespaces or brackets
    size_t args_len = strcspn(description, LAYOUT_DUMP_WHITESPACES LAYOUT_DUMP_BRACKETS);
    char* args = new char[args_len + 1];
    std::unique_ptr<char> free_args_correctly (args);
    strncpy(args, description, args_len);
    args[args_len] = '\0';
    // jump over args substring
    description += args_len;
    if (!*description) {
        output << "Missing " << LAYOUT_DUMP_BRACKETS[1] << " or arguments\n";
        return nullptr;
    }
    description += strspn(description, LAYOUT_DUMP_WHITESPACES);
    if (!*description) {
        output << "Missing " << LAYOUT_DUMP_BRACKETS[1] << " or arguments\n";
        return nullptr;
    }

    // apply type to frame
    if (type == TYPE_FRAMES) {
        // parse args
        char* align_name = g_new(char, strlen(args)+1);
        int selection;
        double fraction_double;
#define SEP LAYOUT_DUMP_SEPARATOR_STR
        if (3 != sscanf(args, "%[^" SEP "]" SEP "%lf" SEP "%d",
            align_name, &fraction_double, &selection)) {
            output << "Can not parse frame args \"" << args << "\"\n";
            return nullptr;
        }
#undef SEP
        int align = find_align_by_name(align_name);
        g_free(align_name);
        if (align < 0) {
            output << "Invalid alignment name in args \"" << args << "\"\n";
            return nullptr;
        }
        selection = !!selection; // CLAMP it to [0;1]
        int fraction = (int)(fraction_double * (double)FRACTION_UNIT);

        // ensure that it is split
        if (frame->type == TYPE_FRAMES) {
            // nothing to do
            frame->content.layout.align = align;
            frame->content.layout.fraction = fraction;
        } else {
            frame_split(frame, align, fraction);
            if (frame->type != TYPE_FRAMES) {
                output << "Can not split frame\n";
                return nullptr;
            }
        }
        frame->content.layout.selection = selection;

        // now parse subframes
        description = load_frame_tree(frame->content.layout.a,
                        description, output);
        if (!description) return nullptr;
        description = load_frame_tree(frame->content.layout.b,
                        description, output);
        if (!description) return nullptr;
    } else {
        // parse args
        char* layout_name = g_new(char, strlen(args)+1);
        int selection;
#define SEP LAYOUT_DUMP_SEPARATOR_STR
        if (2 != sscanf(args, "%[^" SEP "]" SEP "%d",
            layout_name, &selection)) {
            output << "Can not parse frame args \"" << args << "\"\n";
            return nullptr;
        }
#undef SEP
        int layout = find_layout_by_name(layout_name);
        g_free(layout_name);
        if (layout < 0) {
            output << "Can not parse layout from args \"" << args << "\"\n";
            return nullptr;
        }

        // ensure that it is a client frame
        if (frame->type == TYPE_FRAMES) {
            // remove childs
            HSClient **buf1, **buf2;
            size_t count1, count2;
            frame_destroy(frame->content.layout.a, &buf1, &count1);
            frame_destroy(frame->content.layout.b, &buf2, &count2);

            // merge bufs
            size_t count = count1 + count2;
            HSClient** buf = g_new(HSClient*, count);
            memcpy(buf,             buf1, sizeof(buf[0]) * count1);
            memcpy(buf + count1,    buf2, sizeof(buf[0]) * count2);
            g_free(buf1);
            g_free(buf2);

            // setup frame
            frame->type = TYPE_CLIENTS;
            frame->content.clients.buf = buf;
            frame->content.clients.count = count;
            frame->content.clients.selection = 0; // only some sane defaults
            frame->content.clients.layout = 0; // only some sane defaults
        }

        // bring child wins
        // jump over whitespaces
        description += strspn(description, LAYOUT_DUMP_WHITESPACES);
        int index = 0;
        HSTag* tag = find_tag_with_toplevel_frame(get_toplevel_frame(frame));
        while (*description != LAYOUT_DUMP_BRACKETS[1]) {
            Window win;
            if (1 != sscanf(description, "0x%lx\n", &win)) {
                output << "Can not parse window id from \"" << description << "\"\n";
                return nullptr;
            }
            // jump over window id and over whitespaces
            description += strspn(description, "0x123456789abcdef");
            description += strspn(description, LAYOUT_DUMP_WHITESPACES);

            // bring window here
            HSClient* client = get_client_from_window(win);
            if (!client) {
                // client not managed... ignore it
                continue;
            }

            // remove window from old tag
            HSMonitor* clientmonitor = find_monitor_with_tag(client->tag());
            if (!frame_remove_client(client->tag()->frame, client)) {
                g_warning("window %lx was not found on tag %s\n",
                    win, client->tag()->name->str);
            }
            if (clientmonitor) {
                clientmonitor->applyLayout();
            }
            stack_remove_slice(client->tag()->stack, client->slice);

            // insert it to buf
            HSClient** buf = frame->content.clients.buf;
            size_t count = frame->content.clients.count;
            count++;
            index = CLAMP(index, 0, count - 1);
            buf = g_renew(HSClient*, buf, count);
            memmove(buf + index + 1, buf + index,
                    sizeof(buf[0]) * (count - index - 1));
            buf[index] = client;
            frame->content.clients.buf = buf;
            frame->content.clients.count = count;

            client->setTag(tag);
            stack_insert_slice(client->tag()->stack, client->slice);
            ewmh_window_update_tag(client->x11Window(), client->tag());

            index++;
        }
        // apply layout and selection
        selection = (selection < frame->content.clients.count) ? selection : 0;
        selection = (selection >= 0) ? selection : 0;
        frame->content.clients.layout = layout;
        frame->content.clients.selection = selection;
    }
    // jump over closing bracket
    if (*description == LAYOUT_DUMP_BRACKETS[1]) {
        description++;
    } else {
        output << "warning: missing closing bracket " << LAYOUT_DUMP_BRACKETS[1] << "\n";
    }
    // and over whitespaces
    description += strspn(description, LAYOUT_DUMP_WHITESPACES);
    return description;
}
*/

int find_layout_by_name(char* name) {
    for (size_t i = 0; i < LENGTH(g_layout_names); i++) {
        if (!g_layout_names[i]) {
            break;
        }
        if (!strcmp(name, g_layout_names[i])) {
            return (int)i;
        }
    }
    return -1;
}

int find_align_by_name(char* name) {
    for (size_t i = 0; i < LENGTH(g_align_names); i++) {
        if (!strcmp(name, g_align_names[i])) {
            return (int)i;
        }
    }
    return -1;
}

std::shared_ptr<HSFrame> HSFrame::root() {
    auto parent_shared = parent_.lock();
    if (parent_shared) return parent_shared->root();
    else return shared_from_this();
}

// /* FRAMETODO: Implement all this stuff... */
//static void frame_append_caption(HSTree tree, Output output) {
//    HSFrame* frame = (HSFrame*) tree;
//    if (frame->type == TYPE_CLIENTS) {
//        // list of clients
//        output << g_layout_names[frame->content.clients.layout] << ":";
//        HSClient** buf = frame->content.clients.buf;
//        size_t i, count = frame->content.clients.count;
//        for (i = 0; i < count; i++) {
//            output << " 0x%lx" << std::hex << buf[i]->x11Window() << std::dec;
//        }
//        if (g_cur_frame == frame) {
//            output << " [FOCUS]";
//        }
//    } else {
//        // type == TYPE_FRAMES
//        output
//            << g_layout_names[frame->content.layout.align]
//            << " "
//            << frame->content.layout.fraction * 100 / FRACTION_UNIT
//            << "% selection="
//            << frame->content.layout.selection;
//    }
//}
//
//static size_t frame_child_count(HSTree tree) {
//    HSFrame* frame = (HSFrame*) tree;
//    return (frame->type == TYPE_CLIENTS) ? 0 : 2;
//}
//
//static HSTreeInterface frame_nth_child(HSTree tree, size_t idx) {
//    HSFrame* frame = (HSFrame*) tree;
//    assert(frame->type != TYPE_CLIENTS);
//    HSTreeInterface intf = {
//        /* .nth_child  = */ frame_nth_child,
//        /* .child_count    = */ frame_child_count,
//        /* .append_caption = */ frame_append_caption,
//        /* .data       = */ (idx == 0)
//                        ? frame->content.layout.a
//                        : frame->content.layout.b,
//        /* .destructor = */ nullptr,
//    };
//    return intf;
//}
//
void print_frame_tree(shared_ptr<HSFrame> frame, Output output) {
    output << "TODO: implement print_frame_tree()\n";
}
//    HSTreeInterface frameintf = {
//        /* .nth_child      = */ frame_nth_child,
//        /* .child_count    = */ frame_child_count,
//        /* .append_caption = */ frame_append_caption,
//        /* .data           = */ (HSTree) frame,
//        /* .destructor     = */ nullptr,
//    };
//    tree_print_to(&frameintf, output);
//}


bool HSFrame::isFocused() {
    auto p = parent_.lock();
    if (!p) {
        return true;
    } else {
        return p->selectedChild() == shared_from_this()
               && p->isFocused();
    }
}

std::shared_ptr<HSFrameLeaf> HSFrameLeaf::thisLeaf() {
    return dynamic_pointer_cast<HSFrameLeaf>(shared_from_this());
}

std::shared_ptr<HSFrameSplit> HSFrameSplit::thisSplit() {
    return dynamic_pointer_cast<HSFrameSplit>(shared_from_this());
}

std::shared_ptr<HSFrameLeaf> HSFrameLeaf::getFocusedFrame() {
    return thisLeaf();
}

std::shared_ptr<HSFrameLeaf> HSFrameSplit::getFocusedFrame() {
    if (selection == 0) {
        return a->getFocusedFrame();
    } else {
        return b->getFocusedFrame();
    }
}

std::shared_ptr<HSFrameLeaf> HSFrame::getGloballyFocusedFrame() {
    return get_current_monitor()->tag->frame->getFocusedFrame();
}

int frame_current_cycle_client_layout(int argc, char** argv, Output output) {
    char* cmd_name = argv[0]; // save this before shifting
    int delta = 1;
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    auto cur_frame = HSFrame::getGloballyFocusedFrame();
    (void)SHIFT(argc, argv);
    (void)SHIFT(argc, argv);
    int layout_index;
    if (argc > 0) {
        /* cycle through a given list of layouts */
        const char* curname = g_layout_names[cur_frame->getLayout()];
        char** pcurrent = (char**)table_find(argv, sizeof(*argv), argc, 0,
                                     memberequals_string, curname);
        // signed for delta calculations
        int idx = pcurrent ? (INDEX_OF(argv, pcurrent) + delta) : 0;
        idx %= argc;
        idx += argc;
        idx %= argc;
        layout_index = find_layout_by_name(argv[idx]);
        if (layout_index < 0) {
            output << cmd_name << ": Invalid layout name \""
                   << argv[idx] << "\"\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        /* cycle through the default list of layouts */
        layout_index = cur_frame->getLayout() + delta;
        layout_index %= LAYOUT_COUNT;
        layout_index += LAYOUT_COUNT;
        layout_index %= LAYOUT_COUNT;
    }
    cur_frame->setLayout(layout_index);
    get_current_monitor()->applyLayout();
    return 0;
}

int frame_current_set_client_layout(int argc, char** argv, Output output) {
    int layout = 0;
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    layout = find_layout_by_name(argv[1]);
    if (layout < 0) {
        output << argv[0]
               << ": Invalid layout name \"" << argv[1] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    auto cur_frame = HSFrame::getGloballyFocusedFrame();
    cur_frame->setLayout(layout);
    get_current_monitor()->applyLayout();
    return 0;
}

TilingResult HSFrameLeaf::layoutLinear(Rectangle rect, bool vertical) {
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
        res[client] = TilingStep(cur);
        cur.y += step_y;
        cur.x += step_x;
        i++;
    }
    return res;
}

TilingResult HSFrameLeaf::layoutMax(Rectangle rect) {
    TilingResult res;
    for (auto client : clients) {
        TilingStep step(rect);
        if (client == clients[selection]) {
            step.needsRaise = true;
        }
        res[client] = step;
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

TilingResult HSFrameLeaf::layoutGrid(Rectangle rect) {
    TilingResult res;
    if (clients.size() == 0) return res;

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
            res[clients[i]] = TilingStep(cur);
            cur.x += width;
            i++;
        }
        cur.y += height;
    }
    return res;
}

TilingResult HSFrameLeaf::computeLayout(Rectangle rect) {
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
    frame_data.hasClients = clients.size() > 0;
    frame_data.hasParent = (bool)parent_.lock();
    res.focused_frame = decoration;
    res.add(decoration, frame_data);
    if (clients.size() == 0) {
        return res;
    }

    if (!smart_window_surroundings_active(this)) {
        // apply window gap
        auto window_gap = settings_->window_gap();
        rect.x += window_gap;
        rect.y += window_gap;
        rect.width -= window_gap;
        rect.height -= window_gap;

        // apply frame padding
        auto frame_padding = settings_->frame_padding();
        rect.x += frame_padding;
        rect.y += frame_padding;
        rect.width  -= frame_padding * 2;
        rect.height -= frame_padding * 2;
    }
    TilingResult layoutResult;
    if (layout == LAYOUT_MAX) {
        layoutResult = layoutMax(rect);
    } else if (layout == LAYOUT_GRID) {
        layoutResult = layoutGrid(rect);
    } else if (layout == LAYOUT_VERTICAL) {
        layoutResult = layoutVertical(rect);
    } else {
        layoutResult = layoutHorizontal(rect);
    }
    res.mergeFrom(layoutResult);
    res.focus = clients[selection];
    return res;
}

TilingResult HSFrameSplit::computeLayout(Rectangle rect) {
    auto first = rect;
    auto second = rect;
    if (align == ALIGN_VERTICAL) {
        first.height = (rect.height * fraction) / FRACTION_UNIT;
        second.y += first.height;
        second.height -= first.height;
    } else { // (align == ALIGN_HORIZONTAL)
        first.width = (rect.width * fraction) / FRACTION_UNIT;
        second.x += first.width;
        second.width -= first.width;
    }
    TilingResult res;
    auto res1 = a->computeLayout(first);
    auto res2 = b->computeLayout(second);
    res.mergeFrom(res1);
    res.mergeFrom(res2);
    res.focus = (selection == 0) ? res1.focus : res2.focus;
    res.focused_frame = (selection == 0) ? res1.focused_frame : res2.focused_frame;
    return res;
}

void HSFrameSplit::fmap(std::function<void(HSFrameSplit*)> onSplit, std::function<void(HSFrameLeaf*)> onLeaf, int order) {
    if (order <= 0) onSplit(this);
    a->fmap(onSplit, onLeaf, order);
    if (order == 1) onSplit(this);
    b->fmap(onSplit, onLeaf, order);
    if (order >= 1) onSplit(this);
}

void HSFrameLeaf::fmap(std::function<void(HSFrameSplit*)> onSplit, std::function<void(HSFrameLeaf*)> onLeaf, int order) {
    (void) onSplit;
    (void) order;
    onLeaf(this);
}

void HSFrameSplit::foreachClient(ClientAction action) {
    a->foreachClient(action);
    b->foreachClient(action);
}

void HSFrameLeaf::foreachClient(ClientAction action) {
    for (HSClient* client : clients) {
        action(client);
    }
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
    global_tags->moveClient(client, tag);
    auto frame = tag->frame->frameWithClient(client);
    if (!frame->isFocused()) {
        frame->removeClient(client);
        tag->frame->insertClient(client);
    }
    focus_client(client, false, false);
    return 0;
}

void HSFrameLeaf::setSelection(int index) {
    if (clients.size() == 0) return;
    if (index < 0 || index >= clients.size()) {
        index = clients.size() - 1;
    }
    selection = index;
    clients[selection]->window_focus();
    get_current_monitor()->applyLayout();
}

int frame_current_set_selection(int argc, char** argv) {
    int index = 0;
    if (argc >= 2) {
        index = atoi(argv[1]);
    } else {
        return HERBST_NEED_MORE_ARGS;
    }
    // find current selection
    auto frame = HSFrame::getGloballyFocusedFrame();
    frame->setSelection(index);
    return 0;
}
void HSFrameLeaf::cycleSelection(int delta) {
    if (clients.size() == 0) return;
    setSelection(((selection % clients.size()) + delta) % clients.size());
}

int frame_current_cycle_selection(int argc, char** argv) {
    int delta = 1;
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    // find current selection
    auto frame = HSFrame::getGloballyFocusedFrame();
    frame->cycleSelection(delta);
    return 0;
}

int cycle_all_command(int argc, char** argv) {
    return 0;
}
    /** FRAMETODO
    int delta = 1;
    bool skip_invisible = false;
    if (argc >= 2) {
        if (!strcmp(argv[1], "--skip-invisible")) {
            skip_invisible = true;
            (void) SHIFT(argc, argv);
        }
    }
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    delta = CLAMP(delta, -1, 1); // only delta -1, 0, 1 is allowed
    if (delta == 0) {
        // nothing to do
        return 0;
    }
    // find current selection
    (void)skip_invisible;
    auto frame = HSFrame::getGloballyFocusedFrame();
    ///int index = frame->content.clients.selection;
    bool change_frame = false;
    int direction;
    int new_window_index; // tells where to start in a new frame
    if (delta > 0 && (index + 1) >= frame->content.clients.count) {
        // change selection from 0 to 1
        direction = 1;
        change_frame = true;
        new_window_index = 0; // focus first window in in a frame
    }
    if (delta < 0 && index == 0) {
        // change selection from 1 to 0
        direction = 0;
        change_frame = true;
        new_window_index = -1; // focus last window in in a frame
    }
    if (skip_invisible && frame->content.clients.layout == LAYOUT_MAX) {
        direction = (delta > 0) ? 1 : 0;
        change_frame = true;
    }
    if (change_frame) {
        cycle_frame(direction, new_window_index, skip_invisible);
    } else {
        // only change the selection within one frame
        index += delta;
        // ensure it is a valid index
        index %= frame->content.clients.count;
        index += frame->content.clients.count;
        index %= frame->content.clients.count;
        frame->content.clients.selection = index;
    }
    HSClient* c = frame_focused_client(g_cur_frame);
    if (c) {
        c->raise();
    }
    get_current_monitor()->applyLayout();
    return 0;
    **/

int HSFrame::splitsToRoot(int align) {
    if (!parent_.lock()) return 0;
    return parent_.lock()->splitsToRoot(align);
}
int HSFrameSplit::splitsToRoot(int align) {
    if (!parent_.lock()) return 0;
    int delta = 0;
    if (this->align == align) delta = 1;
    return delta + parent_.lock()->splitsToRoot(align);
}

void HSFrameSplit::replaceChild(std::shared_ptr<HSFrame> old, std::shared_ptr<HSFrame> newchild) {
    if (a == old) {
        a = newchild;
        newchild->parent_ = thisSplit();
    }
    if (b == old) {
        b = newchild;
        newchild->parent_ = thisSplit();
    }
}

void HSFrameLeaf::addClients(const std::vector<HSClient*>& vec) {
    for (auto c : vec) clients.push_back(c);
}

bool HSFrameLeaf::split(int alignment, int fraction, size_t childrenLeaving) {
    if (splitsToRoot(alignment) > HERBST_MAX_TREE_HEIGHT) {
        return false;
    }
    int childrenStaying = std::max((size_t)0, clients.size() - childrenLeaving);
    vector<HSClient*> leaves(clients.begin() + childrenStaying, clients.end());
    clients.erase(clients.begin() + childrenStaying, clients.end());
    // ensure fraction is allowed
    fraction = CLAMP(fraction,
                     FRACTION_UNIT * (0.0 + FRAME_MIN_FRACTION),
                     FRACTION_UNIT * (1.0 - FRAME_MIN_FRACTION));
    auto first = shared_from_this();
    auto second = make_shared<HSFrameLeaf>(tag_, settings_, std::weak_ptr<HSFrameSplit>());
    second->layout = layout;
    auto new_this = make_shared<HSFrameSplit>(tag_, settings_, parent_, fraction, alignment, first, second);
    second->parent_ = new_this;
    second->addClients(leaves);
    if (parent_.lock()) {
        parent_.lock()->replaceChild(shared_from_this(), new_this);
    } else {
        tag_->frame = new_this;
    }
    parent_ = new_this;
    if (selection >= childrenStaying) {
        second->setSelection(selection - childrenStaying);
        selection = max(0, childrenStaying - 1);
    }
    return true;
}


int frame_split_command(Input input, Output output) {
    // usage: split t|b|l|r|h|v FRACTION
    std::string cmd, splitType, strFraction;
    if (!input.read({ &cmd, &splitType })) {
        return HERBST_NEED_MORE_ARGS;
    }
    bool userDefinedFraction = input.read({ &strFraction });
    int align = -1;
    bool frameToFirst = true;
    double fractionFloat = userDefinedFraction ? atof(strFraction.c_str()) : 0.5;
    fractionFloat = CLAMP(fractionFloat, 0.0 + FRAME_MIN_FRACTION,
                                         1.0 - FRAME_MIN_FRACTION);
    int fraction = FRACTION_UNIT * fractionFloat;
    int selection = 0;
    auto cur_frame = HSFrame::getGloballyFocusedFrame();
    int lh = cur_frame->lastRect().height;
    int lw = cur_frame->lastRect().width;
    int align_auto = (lw > lh) ? ALIGN_HORIZONTAL : ALIGN_VERTICAL;
    struct {
        const char* name;
        int align;
        bool frameToFirst;  // if former frame moves to first child
        int selection;      // which child to select after the split
    } splitModes[] = {
        { "top",        ALIGN_VERTICAL,     false,  1   },
        { "bottom",     ALIGN_VERTICAL,     true,   0   },
        { "vertical",   ALIGN_VERTICAL,     true,   0   },
        { "right",      ALIGN_HORIZONTAL,   true,   0   },
        { "horizontal", ALIGN_HORIZONTAL,   true,   0   },
        { "left",       ALIGN_HORIZONTAL,   false,  1   },
        { "explode",    ALIGN_EXPLODE,      true,   0   },
        { "auto",       align_auto,         true,   0   },
    };
    for (auto &m : splitModes) {
        if (m.name[0] == splitType[0]) {
            align           = m.align;
            frameToFirst    = m.frameToFirst;
            selection       = m.selection;
            break;
        }
    }
    if (align < 0) {
        output << cmd << ": Invalid alignment \"" << splitType << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    auto frame = HSFrame::getGloballyFocusedFrame();
    bool exploding = align == ALIGN_EXPLODE;
    int layout = frame->getLayout();
    auto windowcount = frame->clientCount();
    if (exploding) {
        if (windowcount <= 1) {
            align = align_auto;
        } else if (layout == LAYOUT_MAX) {
            align = align_auto;
        } else if (layout == LAYOUT_GRID && windowcount == 2) {
            align = ALIGN_HORIZONTAL;
        } else if (layout == LAYOUT_HORIZONTAL) {
            align = ALIGN_HORIZONTAL;
        } else {
            align = ALIGN_VERTICAL;
        }
        size_t count1 = frame->clientCount();
        size_t nc1 = (count1 + 1) / 2;      // new count for the first frame
        if ((layout == LAYOUT_HORIZONTAL || layout == LAYOUT_VERTICAL)
            && !userDefinedFraction && count1 > 1) {
            fraction = (nc1 * FRACTION_UNIT) / count1;
        }
    }
    // move second half of the window buf to second frame
    size_t childrenLeaving = 0;
    if (exploding) {
        childrenLeaving = frame->clientCount() / 2;
    }
    if (!frame->split(align, fraction, childrenLeaving)) {
        return 0;
    }
    if (!frameToFirst) {
        frame->getParent()->swapChildren();
    }
    frame->getParent()->setSelection(selection);
    // redraw monitor
    get_current_monitor()->applyLayout();
    return 0;
}

void HSFrameSplit::swapChildren() {
    swap(a,b);
}

int frame_change_fraction_command(int argc, char** argv, Output output) {
    // usage: fraction DIRECTION DELTA
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    Direction direction;
    try {
        direction = Converter<Direction>::parse(argv[1], nullptr);
    } catch (const std::exception& e) {
        output << argv[0] << ": " << e.what() << "\n";
        return HERBST_INVALID_ARGUMENT;
    }
    double delta_double = atof(argv[2]);
    delta_double = CLAMP(delta_double, -1.0, 1.0);
    int delta = FRACTION_UNIT * delta_double;
    // if direction is left or up we have to flip delta
    // because e.g. resize up by 0.1 actually means:
    // reduce fraction by 0.1, i.e. delta = -0.1
    if (direction == Direction::Left || direction == Direction::Up)
        delta *= -1;

    shared_ptr<HSFrame> neighbour = HSFrame::getGloballyFocusedFrame()->neighbour(direction);
    if (!neighbour) {
        // then try opposite direction
        std::map<Direction, Direction> flip = {
            {Direction::Left, Direction::Right},
            {Direction::Right, Direction::Left},
            {Direction::Down, Direction::Up},
            {Direction::Up, Direction::Down},
        };
        direction = flip[direction];
        neighbour = HSFrame::getGloballyFocusedFrame()->neighbour(direction);
        if (!neighbour) {
            output << argv[0] << ": No neighbour found\n";
            return HERBST_FORBIDDEN;
        }
    }
    auto parent = neighbour->getParent();
    assert(parent); // if has neighbour, it also must have a parent
    parent->adjustFraction(delta);
    // arrange monitor
    get_current_monitor()->applyLayout();
    return 0;
}

void HSFrameSplit::adjustFraction(int delta) {
    fraction += delta;
    fraction = CLAMP(fraction, (int)(FRAME_MIN_FRACTION * FRACTION_UNIT),
                               (int)((1.0 - FRAME_MIN_FRACTION) * FRACTION_UNIT));
}

shared_ptr<HSFrame> HSFrameLeaf::neighbour(Direction direction) {
    bool found = false;
    shared_ptr<HSFrame> other;
    shared_ptr<HSFrame> child = shared_from_this();
    shared_ptr<HSFrameSplit> frame = getParent();
    while (frame) {
        // find frame, where we can change the
        // selection in the desired direction
        switch(direction) {
            case Direction::Right:
                if (frame->getAlign() == ALIGN_HORIZONTAL
                    && frame->firstChild() == child) {
                    found = true;
                    other = frame->secondChild();
                }
                break;
            case Direction::Left:
                if (frame->getAlign() == ALIGN_HORIZONTAL
                    && frame->secondChild() == child) {
                    found = true;
                    other = frame->firstChild();
                }
                break;
            case Direction::Down:
                if (frame->getAlign() == ALIGN_VERTICAL
                    && frame->firstChild() == child) {
                    found = true;
                    other = frame->secondChild();
                }
                break;
            case Direction::Up:
                if (frame->getAlign() == ALIGN_VERTICAL
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
        return shared_ptr<HSFrame>();
    }
    return other;
}

// finds a neighbour within frame in the specified direction
// returns its index or -1 if there is none
int frame_inner_neighbour_index(shared_ptr<HSFrameLeaf> frame, Direction direction) {
    int index = -1;
    int selection = frame->getSelection();
    int count = frame->clientCount();
    int rows, cols;
    switch (frame->getLayout()) {
        case LAYOUT_VERTICAL:
            if (direction == Direction::Down) index = selection + 1;
            if (direction == Direction::Up) index = selection - 1;
            break;
        case LAYOUT_HORIZONTAL:
            if (direction == Direction::Right) index = selection + 1;
            if (direction == Direction::Left) index = selection - 1;
            break;
        case LAYOUT_MAX:
            break;
        case LAYOUT_GRID: {
            frame_layout_grid_get_size(count, &rows, &cols);
            if (cols == 0) break;
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
                case Direction::Right: if (c < cols-1) index = selection + 1; break;
                case Direction::Left:  if (c > 0)      index = selection - 1; break;
            }
            break;
        }
        default:
            break;
    }
    // check that index is valid
    if (index < 0 || index >= count) {
        index = -1;
    }
    return index;
}

int frame_focus_command(int argc, char** argv, Output output) {
    // usage: focus [-e|-i] left|right|up|down
    if (argc < 2) return HERBST_NEED_MORE_ARGS;
    int external_only = g_settings->default_direction_external_only();
    std::string dirstr = argv[1];
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
        direction = Converter<Direction>::parse(dirstr, nullptr);
    } catch (const std::exception& e) {
        output << argv[0] << ": " << e.what() << "\n";
        return HERBST_INVALID_ARGUMENT;
    }
    shared_ptr<HSFrameLeaf> frame = HSFrame::getGloballyFocusedFrame();
    int index;
    bool neighbour_found = true;
    if (frame->getTag()->floating) {
        neighbour_found = floating_focus_direction(direction);
    } else if (!external_only &&
        (index = frame_inner_neighbour_index(frame, direction)) != -1) {
        frame->setSelection(index);
        frame_focus_recursive(frame);
        get_current_monitor()->applyLayout();
    } else {
        shared_ptr<HSFrame> neighbour = frame->neighbour(direction);
        if (neighbour) { // if neighbour was found
            shared_ptr<HSFrameSplit> parent = neighbour->getParent();
            // alter focus (from 0 to 1, from 1 to 0)
            parent->swapSelection();
            // change focus if possible
            frame_focus_recursive(parent);
            get_current_monitor()->applyLayout();
        } else {
            neighbour_found = false;
        }
    }
    if (!neighbour_found && !g_settings->focus_crosses_monitor_boundaries()) {
        output << argv[0] << ": No neighbour found\n";
        return HERBST_FORBIDDEN;
    }
    if (!neighbour_found && g_settings->focus_crosses_monitor_boundaries()) {
        // find monitor in the specified direction
        int idx = g_monitors->indexInDirection(get_current_monitor(), direction);
        if (idx < 0) {
            output << argv[0] << ": No neighbour found\n";
            return HERBST_FORBIDDEN;
        }
        monitor_focus_by_index(idx);
    }
    return 0;
}

void HSFrameLeaf::moveClient(int new_index) {
    swap(clients[new_index], clients[selection]);
    selection = new_index;
}

int frame_move_window_command(int argc, char** argv, Output output) {
    // usage: move left|right|up|down
    if (argc < 2) return HERBST_NEED_MORE_ARGS;
    std::string dirstr = argv[1];
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
        direction = Converter<Direction>::parse(dirstr, nullptr);
    } catch (const std::exception& e) {
        output << argv[0] << ": " << e.what() << "\n";
        return HERBST_INVALID_ARGUMENT;
    }
    shared_ptr<HSFrameLeaf> frame = HSFrame::getGloballyFocusedFrame();
    HSClient* currentClient = get_current_client();
    if (currentClient && currentClient->is_client_floated()) {
        // try to move the floating window
        bool success = floating_shift_direction(direction);
        return success ? 0 : HERBST_FORBIDDEN;
    }
    int index;
    if (!external_only &&
        (index = frame_inner_neighbour_index(frame, direction)) != -1) {
        frame->moveClient(index);
        frame_focus_recursive(frame);
        get_current_monitor()->applyLayout();
    } else {
        shared_ptr<HSFrame> neighbour = frame->neighbour(direction);
        HSClient* client = frame->focusedClient();
        if (client && neighbour) { // if neighbour was found
            // move window to neighbour
            frame->removeClient(client);
            neighbour->insertClient(client);
            neighbour->frameWithClient(client)->select(client);

            // change selection in parent
            shared_ptr<HSFrameSplit> parent = neighbour->getParent();
            assert(parent);
            parent->swapSelection();
            frame_focus_recursive(parent);

            // layout was changed, so update it
            get_current_monitor()->applyLayout();
        } else {
            output << argv[0] << ": No neighbour found\n";
            return HERBST_FORBIDDEN;
        }
    }
    return 0;
}

void HSFrameLeaf::select(HSClient* client) {
    auto it = find(clients.begin(), clients.end(), client);
    if (it != clients.end()) {
        selection = it - clients.begin();
    }
}

HSClient* HSFrameSplit::focusedClient() {
    return (selection == 0 ? a->focusedClient() : b->focusedClient());
}

HSClient* HSFrameLeaf::focusedClient() {
    if (clients.size() > 0) {
        return clients[selection];
    }
    return nullptr;
}

// try to focus window in frame
// it does not require anything from the frame. it may be infocused or even
// hidden.
// returns true if win was found and focused, else returns false
bool HSFrameSplit::focusClient(HSClient* client) {
    if (a->focusClient(client)) {
        selection = 0;
        return true;
    } else if (b->focusClient(client)) {
        selection = 1;
        return true;
    }
    return false;
}

bool HSFrameLeaf::focusClient(HSClient* client) {
    for (unsigned i = 0; i < clients.size(); i++) {
        if (clients[i] == client) {
            selection = i;
            return true;
        }
    }
    return false;
}

// focus a window
// switch_tag tells, whether to switch tag to focus to window
// switch_monitor tells, whether to switch monitor to focus to window
// returns if window was focused or not
bool focus_client(HSClient* client, bool switch_tag, bool switch_monitor) {
    if (!client) {
        // client is not managed
        return false;
    }
    HSTag* tag = client->tag();
    assert(client->tag());
    HSMonitor* monitor = find_monitor_with_tag(tag);
    HSMonitor* cur_mon = get_current_monitor();
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
    bool found = tag->frame->focusClient(client);
    cur_mon->applyLayout();
    g_monitors->unlock();
    return found;
}

void HSFrame::setVisibleRecursive(bool visible) {
    auto onSplit = [] (HSFrameSplit* frame) { };
    // X11 tweaks here.
    auto onLeaf =
        [visible] (HSFrameLeaf* frame) {
            if (!visible) {
                frame->decoration->hide();
            }
            for (auto c : frame->clients) c->set_visible(visible);
        };
    // first hide children => order = 2
    fmap(onSplit, onLeaf, 2);
}

void HSFrameSplit::rotate() {
    switch (align) {
        case ALIGN_VERTICAL:
            align = ALIGN_HORIZONTAL;
            break;
        case ALIGN_HORIZONTAL:
            align = ALIGN_VERTICAL;
            selection = selection ? 0 : 1;
            swap(a, b);
            fraction = FRACTION_UNIT - fraction;
            break;
    }
}

int layout_rotate_command() {
    void (*onSplit)(HSFrameSplit*) =
        [] (HSFrameSplit* frame) {
            frame->rotate();
        };
    void (*onLeaf)(HSFrameLeaf*) =
        [] (HSFrameLeaf*) {
        };
    // first hide children => order = 2
    get_current_monitor()->tag->frame->fmap(onSplit, onLeaf, -1);
    get_current_monitor()->applyLayout();
    return 0;
}

std::vector<HSClient*> HSFrameLeaf::removeAllClients() {
    std::vector<HSClient*> result;
    swap(result, clients);
    selection = 0;
    return result;
}

int frame_remove_command(int argc, char** argv) {
    auto frame = HSFrame::getGloballyFocusedFrame();
    if (!frame->getParent()) {
        // do nothing if is toplevel frame
        return 0;
    }
    auto parent = frame->getParent();
    auto pp = parent->getParent();
    auto newparent = (frame == parent->firstChild())
                     ? parent->secondChild()
                     : parent->firstChild();
    newparent->getFocusedFrame()->addClients(frame->removeAllClients());
    // now, frame is empty
    if (pp) {
        pp->replaceChild(parent, newparent);
    } else {
        // if parent was root frame
        frame->getTag()->frame = newparent;
    }
    frame_focus_recursive(parent);
    get_current_monitor()->applyLayout();
    return 0;
}

int close_or_remove_command(int argc, char** argv) {
    HSClient* client = HSFrame::getGloballyFocusedFrame()->focusedClient();
    if (client) {
        window_close(client->x11Window());
        return 0;
    } else {
        return frame_remove_command(argc, argv);
    }
}

// ET: same as close or remove but remove after last client
int close_and_remove_command(int argc, char** argv) {
    bool remove_after_close = false;
    auto cur_frame = HSFrame::getGloballyFocusedFrame();
    HSClient* client = cur_frame->focusedClient();
    if (client) {
        if (cur_frame->clientCount() == 1 ) {
            remove_after_close = true;
        }

        window_close(client->x11Window());

        if (remove_after_close) {
            frame_remove_command(argc, argv);
        }

        return 0;

    } else {
        return frame_remove_command(argc, argv);
    }
}

int frame_focus_edge(int argc, char** argv, Output output) {
    // Puts the focus to the edge in the specified direction
    g_monitors->lock();
    int oldval = g_settings->focus_crosses_monitor_boundaries();
    g_settings->focus_crosses_monitor_boundaries = false;
    while (0 == frame_focus_command(argc,argv,output))
        ;
    g_settings->focus_crosses_monitor_boundaries = oldval;
    g_monitors->unlock();
    return 0;
}

int frame_move_window_edge(int argc, char** argv, Output output) {
    // Moves a window to the edge in the specified direction
    g_monitors->lock();
    int oldval = g_settings->focus_crosses_monitor_boundaries();
    g_settings->focus_crosses_monitor_boundaries = false;
    while (0 == frame_move_window_command(argc,argv,output))
        ;
    g_settings->focus_crosses_monitor_boundaries = oldval;
    g_monitors->unlock();
    return 0;
}

bool smart_window_surroundings_active(HSFrameLeaf* frame) {
    return g_settings->smart_window_surroundings()
            && (frame->clientCount() == 1
                || frame->getLayout() == LAYOUT_MAX);
}

void frame_focus_recursive(shared_ptr<HSFrame> frame) {
    shared_ptr<HSFrameLeaf> leaf = frame->getFocusedFrame();
    HSClient* client = leaf->focusedClient();
    if (client) {
        client->window_focus();
    } else {
        HSClient::window_unfocus_last();
    }
}

int cycle_frame_command(int argc, char** argv) {
    // FRAMETODO
    return 0;
}


