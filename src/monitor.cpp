/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <sstream>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

#include "root.h"
#include "globals.h"
#include "ipc-protocol.h"
#include "utils.h"
#include "mouse.h"
#include "hook.h"
#include "layout.h"
#include "tag.h"
#include "ewmh.h"
#include "monitor.h"
#include "settings.h"
#include "stack.h"
#include "client.h"
#include "monitormanager.h"

#include <vector>

using namespace std;


// module internals:
static int g_cur_monitor;
static int* g_monitors_locked;
static int* g_swap_monitors_to_get_tag;
static int* g_smart_frame_surroundings;
static int* g_mouse_recenter_gap;
static ::HSStack* g_monitor_stack;
Ptr(MonitorManager) monitors;

typedef struct RectList {
    Rectangle rect;
    struct RectList* next;
} RectList;

static RectList* reclist_insert_disjoint(RectList* head, RectList* mt);
static RectList* disjoin_rects(const RectangleVec &buf);

void monitor_init() {
    g_monitors_locked = &(settings_find("monitors_locked")->value.i);
    g_cur_monitor = 0;
    g_swap_monitors_to_get_tag = &(settings_find("swap_monitors_to_get_tag")->value.i);
    g_smart_frame_surroundings = &(settings_find("smart_frame_surroundings")->value.i);
    g_mouse_recenter_gap       = &(settings_find("mouse_recenter_gap")->value.i);
    g_monitor_stack = stack_create();
    monitors = make_shared<MonitorManager>();
    Root::get()->addChild(monitors, "monitors");
}

HSMonitor::HSMonitor()
    : name("name", AT_THIS(onNameChange), "")
    , index("index", 0)
    , pad_up("pad_up", AT_THIS(onPadChange), 0)
    , pad_right("pad_right", AT_THIS(onPadChange), 0)
    , pad_down("pad_down", AT_THIS(onPadChange), 0)
    , pad_left("pad_left", AT_THIS(onPadChange), 0)
    , lock_tag("lock_tag", ACCEPT_ALL, false)
{
    wireAttributes({
        &index,
        &name,
        &pad_up,
        &pad_right,
        &pad_down,
        &pad_left,
        &lock_tag,
    });
}

HSMonitor::~HSMonitor() {
    stack_remove_slice(g_monitor_stack, slice);
}

std::string HSMonitor::onNameChange() {
    if (isdigit(name()[0])) {
        return "The monitor name may not start with a number";
    }
    for (auto m : *monitors) {
        if (&* m != this && name() == m->name()) {
            stringstream output;
            output << "Monitor " << m->index()
                   << " already has the name \""
                   << name() << "\"";
            return output.str();
        }
    }
    return {};
}

std::string HSMonitor::onPadChange() {
    monitor_apply_layout(this);
    return {};
}


void HSMonitor::setIndexAttribute(unsigned long new_index) {
    index = new_index;
}

void monitor_destroy() {
    monitors->clearChildren();
    stack_destroy(g_monitor_stack);
}

void monitor_apply_layout(HSMonitor* monitor) {
    if (monitor) {
        if (*g_monitors_locked) {
            monitor->dirty = true;
            return;
        }
        monitor->dirty = false;
        Rectangle rect = monitor->rect;
        // apply pad
        // FIXME: why does the following + work for attributes pad_* ?
        rect.x += monitor->pad_left;
        rect.width -= (monitor->pad_left + monitor->pad_right);
        rect.y += monitor->pad_up;
        rect.height -= (monitor->pad_up + monitor->pad_down);
        if (!*g_smart_frame_surroundings || monitor->tag->frame->isSplit()) {
            // apply frame gap
            rect.x += *g_frame_gap;
            rect.y += *g_frame_gap;
            rect.height -= *g_frame_gap;
            rect.width -= *g_frame_gap;
        }
        monitor_restack(monitor);
        if (get_current_monitor() == monitor) {
            frame_focus_recursive(monitor->tag->frame);
        }
        if (monitor->tag->floating) {
            monitor->tag->frame->applyFloatingLayout(monitor);
        } else {
            monitor->tag->frame->applyLayout(rect);
            if (!monitor->lock_frames && !monitor->tag->floating) {
                monitor->tag->frame->updateVisibility();
            }
        }
        // remove all enternotify-events from the event queue that were
        // generated while arranging the clients on this monitor
        drop_enternotify_events();
    }
}

int list_monitors(int argc, char** argv, Output output) {
    (void)argc;
    (void)argv;
    string monitor_name = "";
    int i = 0;
    for (auto monitor : *monitors) {
        if (monitor->name != "" ) {
            monitor_name = ", named \"" + *monitor->name + "\"";
        } else {
            monitor_name = "";
        }
        output << i << ": " << monitor->rect
               << " with tag \""
               << (monitor->tag ? monitor->tag->name->c_str() : "???")
               << "\""
               << monitor_name
               << (((unsigned int) g_cur_monitor == i) ? " [FOCUS]" : "")
               << (monitor->lock_tag ? " [LOCKED]" : "")
               << "\n";
        i++;
    }
    return 0;
}

int list_padding(int argc, char** argv, Output output) {
    HSMonitor* monitor;
    if (argc < 2) {
        monitor = get_current_monitor();
    } else {
        monitor = string_to_monitor(argv[1]);
        if (monitor == NULL) {
            output << argv[0] << ": Monitor \"" << argv[1] << "\" not found!\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    output     << monitor->pad_up
        << " " << monitor->pad_right
        << " " << monitor->pad_down
        << " " << monitor->pad_left
        << "\n";
    return 0;
}

static bool rects_intersect(RectList* m1, RectList* m2) {
    Rectangle *r1 = &m1->rect, *r2 = &m2->rect;
    bool is = TRUE;
    is = is && intervals_intersect(r1->x, r1->x + r1->width,
                                   r2->x, r2->x + r2->width);
    is = is && intervals_intersect(r1->y, r1->y + r1->height,
                                   r2->y, r2->y + r2->height);
    return is;
}

static Rectangle intersection_area(RectList* m1, RectList* m2) {
    Rectangle r; // intersection between m1->rect and m2->rect
    r.x = std::max(m1->rect.x, m2->rect.x);
    r.y = std::max(m1->rect.y, m2->rect.y);
    // the bottom right coordinates of the rects
    int br1_x = m1->rect.x + m1->rect.width;
    int br1_y = m1->rect.y + m1->rect.height;
    int br2_x = m2->rect.x + m2->rect.width;
    int br2_y = m2->rect.y + m2->rect.height;
    r.width = std::min(br1_x, br2_x) - r.x;
    r.height = std::min(br1_y, br2_y) - r.y;
    return r;
}

static RectList* rectlist_create_simple(int x1, int y1, int x2, int y2) {
    if (x1 >= x2 || y1 >= y2) {
        return NULL;
    }
    RectList* r = g_new0(RectList, 1);
    r->rect.x = x1;
    r->rect.y = y1;
    r->rect.width  = x2 - x1;
    r->rect.height = y2 - y1;
    r->next = NULL;
    return r;
}

static RectList* insert_rect_border(RectList* head,
                                    Rectangle large, Rectangle center)
{
    // given a large rectangle and a center which guaranteed to be a subset of
    // the large rect, the task is to split "large" into pieces and insert them
    // like this:
    //
    // +------- large ---------+
    // |         top           |
    // |------+--------+-------|
    // | left | center | right |
    // |------+--------+-------|
    // |        bottom         |
    // +-----------------------+
    RectList *top, *left, *right, *bottom;
    // coordinates of the bottom right corner of large
    int br_x = large.x + large.width, br_y = large.y + large.height;
    RectList* (*r)(int,int,int,int) = rectlist_create_simple;
    top   = r(large.x, large.y, large.x + large.width, center.y);
    left  = r(large.x, center.y, center.x, center.y + center.height);
    right = r(center.x + center.width, center.y, br_x, center.y + center.height);
    bottom= r(large.x, center.y + center.height, br_x, br_y);

    RectList* parts[] = { top, left, right, bottom };
    for (unsigned int i = 0; i < LENGTH(parts); i++) {
        head = reclist_insert_disjoint(head, parts[i]);
    }
    return head;
}

// insert a new element without any intersections into the given list
static RectList* reclist_insert_disjoint(RectList* head, RectList* element) {
    if (!element) {
        return head;
    } else if (!head) {
        // if the list is empty, then intersection-free insertion is trivial
        element->next = NULL;
        return element;
    } else if (!rects_intersect(head, element)) {
        head->next = reclist_insert_disjoint(head->next, element);
        return head;
    } else {
        // element intersects with the head rect
        auto center = intersection_area(head, element);
        auto large = head->rect;
        head->rect = center;
        head->next = insert_rect_border(head->next, large, center);
        head->next = insert_rect_border(head->next, element->rect, center);
        g_free(element);
        return head;
    }
}

static void rectlist_free(RectList* head) {
    if (!head) return;
    RectList* next = head->next;
    g_free(head);
    rectlist_free(next);
}

static int rectlist_length_acc(RectList* head, int acc) {
    if (!head) return acc;
    else return rectlist_length_acc(head->next, acc + 1);
}

static int rectlist_length(RectList* head) {
    return rectlist_length_acc(head, 0);
}

static RectList* disjoin_rects(const RectangleVec &buf) {
    RectList* cur;
    struct RectList* rects = NULL;
    for (auto& rect : buf) {
        cur = g_new0(RectList, 1);
        cur->rect = rect;
        rects = reclist_insert_disjoint(rects, cur);
    }
    return rects;
}


int disjoin_rects_command(int argc, char** argv, Output output) {
    (void)SHIFT(argc, argv);
    if (argc < 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    RectangleVec buf(argc);
    for (int i = 0; i < argc; i++) {
        buf[i] = Rectangle::fromStr(argv[i]);
    }

    RectList* rects = disjoin_rects(buf);
    for (RectList* cur = rects; cur; cur = cur->next) {
        Rectangle &r = cur->rect;
        output << r;
    }
    rectlist_free(rects);
    return 0;
}

int set_monitor_rects_command(int argc, char** argv, Output output) {
    (void)SHIFT(argc, argv);
    if (argc < 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    RectangleVec templates(argc);
    for (int i = 0; i < argc; i++) {
        templates[i] = Rectangle::fromStr(argv[i]);
    }
    int status = set_monitor_rects(templates);

    if (status == HERBST_TAG_IN_USE) {
        output << argv[0] << ": There are not enough free tags\n";
    } else if (status == HERBST_INVALID_ARGUMENT) {
        output << argv[0] << ": Need at least one rectangle\n";
    }
    return status;
}

int set_monitor_rects(const RectangleVec &templates) {
    if (templates.empty()) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = NULL;
    int i;
    for (i = 0; i < std::min(templates.size(), monitors->size()); i++) {
        HSMonitor* m = monitor_with_index(i);
        m->rect = templates[i];
    }
    // add additional monitors
    for (; i < templates.size(); i++) {
        tag = find_unused_tag();
        if (!tag) {
            return HERBST_TAG_IN_USE;
        }
        add_monitor(templates[i], tag, NULL);
        tag->frame->setVisibleRecursive(true);
    }
    // remove monitors if there are too much
    while (i < monitors->size()) {
        remove_monitor(i);
    }
    monitor_update_focus_objects();
    all_monitors_apply_layout();
    return 0;
}

int find_monitor_index_by_name(char* name) {
    for (int i = 0; i < monitors->size(); i++) {
        if (monitors->byIdx(i)->name == name) {
            return i;
        }
    }
    return -1;
}

HSMonitor* find_monitor_by_name(char* name) {
    int i = find_monitor_index_by_name(name);
    if (i == -1) {
        return NULL;
    } else {
        return monitor_with_index(i);
    }
}

int string_to_monitor_index(char* string) {
    if (string[0] == '\0') {
        return g_cur_monitor;
    } else if (string[0] == '-' || string[0] == '+') {
        if (isdigit(string[1])) {
            // relative monitor index
            int idx = g_cur_monitor + atoi(string);
            idx %= monitors->size();
            idx += monitors->size();
            idx %= monitors->size();
            return idx;
        } else if (string[0] == '-') {
            enum HSDirection dir = char_to_direction(string[1]);
            if (dir < 0) return -1;
            return monitor_index_in_direction(get_current_monitor(), dir);
        } else {
            return -1;
        }
    } else if (isdigit(string[0])) {
        // absolute monitor index
        int idx = atoi(string);
        if (idx < 0 || idx >= monitors->size()) {
            return -1;
        }
        return idx;
    } else {
        // monitor string
        return find_monitor_index_by_name(string);
    }
}

int monitor_index_in_direction(HSMonitor* m, enum HSDirection dir) {
    int cnt = monitor_count();
    RectangleIdx* rects = g_new0(RectangleIdx, cnt);
    int relidx = -1;
    FOR (i,0,cnt) {
        rects[i].idx = i;
        rects[i].r = monitor_with_index(i)->rect;
        if (monitor_with_index(i) == m) relidx = i;
    }
    HSAssert(relidx >= 0);
    int result = find_rectangle_in_direction(rects, cnt, relidx, dir);
    g_free(rects);
    return result;
}

HSMonitor* string_to_monitor(char* string) {
    int idx = string_to_monitor_index(string);
    return monitor_with_index(idx);
}

static void monitor_foreach(void (*action)(HSMonitor*)) {
    for (auto m : *monitors) {
        action(&* m);
    }
}

HSMonitor* add_monitor(Rectangle rect, HSTag* tag, char* name) {
    assert(tag != NULL);
    Ptr(HSMonitor) m = make_shared<HSMonitor>();
    m->rect = rect;
    m->tag = tag;
    m->tag_previous = tag;
    m->name = name ? name : "";
    m->mouse.x = 0;
    m->mouse.y = 0;
    m->dirty = true;
    m->slice = slice_create_monitor(&* m);
    m->stacking_window = XCreateSimpleWindow(g_display, g_root,
                                             42, 42, 42, 42, 1, 0, 0);

    stack_insert_slice(g_monitor_stack, m->slice);
    monitors->addIndexed(m);

    return &* m;
}

int add_monitor_command(int argc, char** argv, Output output) {
    // usage: add_monitor RECTANGLE [TAG [NAME]]
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    auto rect = Rectangle::fromStr(argv[1]);
    HSTag* tag = NULL;
    char* name = NULL;
    if (argc == 2 || !strcmp("", argv[2])) {
        tag = find_unused_tag();
        if (!tag) {
            output << argv[0] << ": There are not enough free tags\n";
            return HERBST_TAG_IN_USE;
        }
    }
    else {
        tag = find_tag(argv[2]);
        if (!tag) {
            output << argv[0] << ": The tag \"" << argv[2] << "\" does not exist\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    if (find_monitor_with_tag(tag)) {
        output << argv[0] <<
            ": The tag \"" << argv[2] << "\" is already viewed on a monitor\n";
        return HERBST_TAG_IN_USE;
    }
    if (argc > 3) {
        name = argv[3];
        if (isdigit(name[0])) {
            output << argv[0] <<
                ": The monitor name may not start with a number\n";
            return HERBST_INVALID_ARGUMENT;
        }
        if (!strcmp("", name)) {
            output << argv[0] <<
                ": An empty monitor name is not permitted\n";
            return HERBST_INVALID_ARGUMENT;
        }
        if (find_monitor_by_name(name)) {
            output << argv[0] <<
                ": A monitor with the same name already exists\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    HSMonitor* monitor = add_monitor(rect, tag, name);
    monitor_apply_layout(monitor);
    tag->frame->setVisibleRecursive(true);
    emit_tag_changed(tag, monitors->size() - 1);
    drop_enternotify_events();
    return 0;
}

int remove_monitor_command(int argc, char** argv, Output output) {
    // usage: remove_monitor INDEX
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    int index = string_to_monitor_index(argv[1]);
    if (index == -1) {
        output << argv[0] << ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    int ret = remove_monitor(index);
    if (ret == HERBST_INVALID_ARGUMENT) {
        output << argv[0] <<
            ": Index needs to be between 0 and " << (monitors->size() - 1) << "\n";
    } else if (ret == HERBST_FORBIDDEN) {
        output << argv[0] << ": Can't remove the last monitor\n";
    }
    monitor_update_focus_objects();
    return ret;
}

int remove_monitor(int index) {
    if (index < 0 || index >= monitors->size()) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (monitors->size() <= 1) {
        return HERBST_FORBIDDEN;
    }
    HSMonitor* monitor = monitor_with_index(index);
    // adjust selection
    if (g_cur_monitor > index) {
        // same monitor shall be selected after remove
        g_cur_monitor--;
    }
    assert(monitor->tag);
    assert(monitor->tag->frame);
    // hide clients
    monitor->tag->frame->setVisibleRecursive(false);
    // remove from monitor stack
    stack_remove_slice(g_monitor_stack, monitor->slice);
    slice_destroy(monitor->slice);
    XDestroyWindow(g_display, monitor->stacking_window);
    // and remove monitor completely
    monitors->removeIndexed(index);
    if (g_cur_monitor >= monitors->size()) {
        g_cur_monitor--;
        // if selection has changed, then relayout focused monitor
        monitor_apply_layout(get_current_monitor());
        monitor_update_focus_objects();
        // also announce the new selection
        ewmh_update_current_desktop();
        emit_tag_changed(get_current_monitor()->tag, g_cur_monitor);
    }
    return 0;
}

int move_monitor_command(int argc, char** argv, Output output) {
    // usage: move_monitor INDEX RECT [PADUP [PADRIGHT [PADDOWN [PADLEFT]]]]
    // moves monitor with number to RECT
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSMonitor* monitor = string_to_monitor(argv[1]);
    if (monitor == NULL) {
        output << argv[0] <<
            ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    auto rect = Rectangle::fromStr(argv[2]);
    if (rect.width < WINDOW_MIN_WIDTH || rect.height < WINDOW_MIN_HEIGHT) {
        output << argv[0] << "%s: Rectangle is too small\n";
        return HERBST_INVALID_ARGUMENT;
    }
    // else: just move it:
    monitor->rect = rect;
    if (argc > 3 && argv[3][0] != '\0') monitor->pad_up       = atoi(argv[3]);
    if (argc > 4 && argv[4][0] != '\0') monitor->pad_right    = atoi(argv[4]);
    if (argc > 5 && argv[5][0] != '\0') monitor->pad_down     = atoi(argv[5]);
    if (argc > 6 && argv[6][0] != '\0') monitor->pad_left     = atoi(argv[6]);
    monitor_apply_layout(monitor);
    return 0;
}

int rename_monitor_command(int argc, char** argv, Output output) {
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSMonitor* mon = string_to_monitor(argv[1]);
    if (mon == NULL) {
        output << argv[0] <<
            ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    string error = mon->name.change(argv[2]);
    if (!error.empty()) {
        output << argv[0] << ": " << error << "\n";
        return HERBST_INVALID_ARGUMENT;
    } else {
        return 0;
    }
}

int monitor_rect_command(int argc, char** argv, Output output) {
    // usage: monitor_rect [[-p] INDEX]
    char* monitor_str = NULL;
    HSMonitor* m = NULL;
    bool with_pad = false;

    // if monitor is supplied
    if (argc > 1) {
        monitor_str = argv[1];
    }
    // if -p is supplied
    if (argc > 2) {
        monitor_str = argv[2];
        if (!strcmp("-p", argv[1])) {
            with_pad = true;
        } else {
            output << argv[0] <<
                ": Invalid argument \"" << argv[1] << "\"\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }
    // if an index is set
    if (monitor_str) {
        m = string_to_monitor(monitor_str);
        if (m == NULL) {
            output << argv[0] <<
                ": Monitor \"" << monitor_str << "\" not found!\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        m = get_current_monitor();
    }
    auto rect = m->rect;
    if (with_pad) {
        rect.x += m->pad_left;
        rect.width -= m->pad_left + m->pad_right;
        rect.y += m->pad_up;
        rect.height -= m->pad_up + m->pad_down;
    }
    output << rect.x << " " << rect.y << " " << rect.width << " " << rect.height;
    return 0;
}

int monitor_set_pad_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSMonitor* monitor = string_to_monitor(argv[1]);
    if (monitor == NULL) {
        output << argv[0] <<
            ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    if (argc > 2 && argv[2][0] != '\0') monitor->pad_up       = atoi(argv[2]);
    if (argc > 3 && argv[3][0] != '\0') monitor->pad_right    = atoi(argv[3]);
    if (argc > 4 && argv[4][0] != '\0') monitor->pad_down     = atoi(argv[4]);
    if (argc > 5 && argv[5][0] != '\0') monitor->pad_left     = atoi(argv[5]);
    monitor_apply_layout(monitor);
    return 0;
}

HSMonitor* find_monitor_with_tag(HSTag* tag) {
    for (auto m : *monitors) {
        if (m->tag == tag) {
            return &* m;
        }
    }
    return NULL;
}

void ensure_monitors_are_available() {
    if (monitors->size() > 0) {
        // nothing to do
        return;
    }
    // add monitor if necessary
    Rectangle rect = { 0, 0,
            DisplayWidth(g_display, DefaultScreen(g_display)),
            DisplayHeight(g_display, DefaultScreen(g_display))};
    ensure_tags_are_available();
    // add monitor with first tag
    add_monitor(rect, get_tag_by_index(0), NULL);
    g_cur_monitor = 0;

    monitor_update_focus_objects();
}

HSMonitor* monitor_with_frame(HSFrame* frame) {
    // find toplevel Frame
    HSTag* tag = find_tag_with_toplevel_frame(&* frame->root());
    return find_monitor_with_tag(tag);
}

HSMonitor* get_current_monitor() {
    return &* monitors->byIdx(g_cur_monitor);
}

int monitor_count() {
    return monitors->size();
}

void all_monitors_apply_layout() {
    monitor_foreach(monitor_apply_layout);
}

int monitor_set_tag(HSMonitor* monitor, HSTag* tag) {
    HSMonitor* other = find_monitor_with_tag(tag);
    if (monitor == other) {
        // nothing to do
        return 0;
    }
    if (monitor->lock_tag) {
        // If the monitor tag is locked, do not change the tag
        if (other != NULL) {
            // but if the tag is already visible, change to the
            // displaying monitor
            monitor_focus_by_index(monitor_index_of(other));
            return 0;
        }
        return 1;
    }
    if (other != NULL) {
        if (*g_swap_monitors_to_get_tag) {
            if (other->lock_tag) {
                // the monitor we want to steal the tag from is
                // locked. focus that monitor instead
                monitor_focus_by_index(monitor_index_of(other));
                return 0;
            }
            // swap tags
            other->tag = monitor->tag;
            monitor->tag = tag;
            // reset focus
            frame_focus_recursive(tag->frame);
            /* TODO: find the best order of restacking and layouting */
            monitor_restack(other);
            monitor_restack(monitor);
            monitor_apply_layout(other);
            monitor_apply_layout(monitor);
            // discard enternotify-events
            drop_enternotify_events();
            monitor_update_focus_objects();
            ewmh_update_current_desktop();
            emit_tag_changed(other->tag, monitor_index_of(other));
            emit_tag_changed(tag, g_cur_monitor);
        } else {
            // if we are not allowed to steal the tag, then just focus the
            // other monitor
            monitor_focus_by_index(monitor_index_of(other));
        }
        return 0;
    }
    HSTag* old_tag = monitor->tag;
    // save old tag
    monitor->tag_previous = old_tag;
    // 1. show new tag
    monitor->tag = tag;
    // first reset focus and arrange windows
    frame_focus_recursive(tag->frame);
    monitor_restack(monitor);
    monitor->lock_frames = true;
    monitor_apply_layout(monitor);
    monitor->lock_frames = false;
    // then show them (should reduce flicker)
    tag->frame->setVisibleRecursive(true);
    if (!monitor->tag->floating) {
        monitor->tag->frame->updateVisibility();
    }
    // 2. hide old tag
    old_tag->frame->setVisibleRecursive(false);
    // focus window just has been shown
    // focus again to give input focus
    frame_focus_recursive(tag->frame);
    // discard enternotify-events
    drop_enternotify_events();
    monitor_update_focus_objects();
    ewmh_update_current_desktop();
    emit_tag_changed(tag, g_cur_monitor);
    return 0;
}

int monitor_set_tag_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSMonitor* monitor = get_current_monitor();
    HSTag*  tag = find_tag(argv[1]);
    if (monitor && tag) {
        int ret = monitor_set_tag(monitor, tag);
        if (ret != 0) {
            output << argv[0] << ": Could not change tag";
            if (monitor->lock_tag) {
                output << " (monitor " << monitor_index_of(monitor) << " is locked)";
            }
            output << "\n";
        }
        return ret;
    } else {
        output << argv[0] <<
            ": Invalid tag \"" << argv[1] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
}

int monitor_set_tag_by_index_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    bool skip_visible = false;
    if (argc >= 3 && !strcmp(argv[2], "--skip-visible")) {
        skip_visible = true;
    }
    HSTag* tag = get_tag_by_index_str(argv[1], skip_visible);
    if (!tag) {
        output << argv[0] <<
            ": Invalid index \"" << argv[1] << "\"\n";
        return HERBST_INVALID_ARGUMENT;
    }
    int ret = monitor_set_tag(get_current_monitor(), tag);
    if (ret != 0) {
        output << argv[0] <<
            ": Could not change tag (maybe monitor is locked?)\n";
    }
    return ret;
}

int monitor_set_previous_tag_command(int argc, char** argv, Output output) {
    if (argc < 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    HSMonitor* monitor = get_current_monitor();
    HSTag*  tag = monitor->tag_previous;
    if (monitor && tag) {
        int ret = monitor_set_tag(monitor, tag);
        if (ret != 0) {
            output << argv[0] <<
                    ": Could not change tag (maybe monitor is locked?)\n";
        }
        return ret;
    } else {
        output << argv[0] <<
                ": Invalid monitor or tag\n";
        return HERBST_INVALID_ARGUMENT;
    }
}

int monitor_focus_command(int argc, char** argv, Output output) {
    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    int new_selection = string_to_monitor_index(argv[1]);
    if (new_selection < 0) {
        output << argv[0] <<
            ": Monitor \"" << argv[1] << "\" not found!\n";
        return HERBST_INVALID_ARGUMENT;
    }
    // really change selection
    monitor_focus_by_index(new_selection);
    return 0;
}

int monitor_cycle_command(int argc, char** argv) {
    int delta = 1;
    int count = monitors->size();
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    int new_selection = g_cur_monitor + delta;
    // fix range of index
    new_selection %= count;
    new_selection += count;
    new_selection %= count;
    // really change selection
    monitor_focus_by_index(new_selection);
    return 0;
}

int monitor_index_of(HSMonitor* monitor) {
    return monitors->index_of(monitor);
}

void monitor_focus_by_index(int new_selection) {
    new_selection = CLAMP(new_selection, 0, monitors->size() - 1);
    HSMonitor* old = get_current_monitor();
    HSMonitor* monitor = monitor_with_index(new_selection);
    if (old == monitor) {
        // nothing to do
        return;
    }
    // change selection globals
    assert(monitor->tag);
    assert(monitor->tag->frame);
    g_cur_monitor = new_selection;
    frame_focus_recursive(monitor->tag->frame);
    // repaint monitors
    monitor_apply_layout(old);
    monitor_apply_layout(monitor);
    int rx, ry;
    {
        // save old mouse position
        Window win, child;
        int wx, wy;
        unsigned int mask;
        if (True == XQueryPointer(g_display, g_root, &win, &child,
            &rx, &ry, &wx, &wy, &mask)) {
            old->mouse.x = rx - old->rect.x;
            old->mouse.y = ry - old->rect.y;
            old->mouse.x = CLAMP(old->mouse.x, 0, old->rect.width-1);
            old->mouse.y = CLAMP(old->mouse.y, 0, old->rect.height-1);
        }
    }
    // restore position of new monitor
    // but only if mouse pointer is not already on new monitor
    int new_x, new_y;
    if ((monitor->rect.x <= rx) && (rx < monitor->rect.x + monitor->rect.width)
        && (monitor->rect.y <= ry) && (ry < monitor->rect.y + monitor->rect.height)) {
        // mouse already is on new monitor
    } else {
        // If the mouse is located in a gap indicated by
        // mouse_recenter_gap at the outer border of the monitor,
        // recenter the mouse.
        if (std::min(monitor->mouse.x, abs(monitor->mouse.x - (int)monitor->rect.width))
                < *g_mouse_recenter_gap
            || std::min(monitor->mouse.y, abs(monitor->mouse.y - (int)monitor->rect.height))
                < *g_mouse_recenter_gap) {
            monitor->mouse.x = monitor->rect.width / 2;
            monitor->mouse.y = monitor->rect.height / 2;
        }
        new_x = monitor->rect.x + monitor->mouse.x;
        new_y = monitor->rect.y + monitor->mouse.y;
        XWarpPointer(g_display, None, g_root, 0, 0, 0, 0, new_x, new_y);
        // discard all mouse events caused by this pointer movage from the
        // event queue, so the focus really stays in the last focused window on
        // this monitor and doesn't jump to the window hovered by the mouse
        drop_enternotify_events();
    }
    // update objects
    monitor_update_focus_objects();
    // emit hooks
    ewmh_update_current_desktop();
    emit_tag_changed(monitor->tag, new_selection);
}

void monitor_update_focus_objects() {
    monitors->focus = monitors->byIdx(g_cur_monitor);
    tag_update_focus_objects();
}

int HSMonitor::relativeX(int x_root) {
    return x_root - rect.x - pad_left;
}

int HSMonitor::relativeY(int y_root) {
    return y_root - rect.y - pad_up;
}

HSMonitor* monitor_with_coordinate(int x, int y) {
    for (auto m : *monitors) {
        if (m->rect.x + m->pad_left <= x
            && m->rect.x + m->rect.width - m->pad_right > x
            && m->rect.y + m->pad_up <= y
            && m->rect.y + m->rect.height - m->pad_down > y) {
            return &* m;
        }
    }
    return NULL;
}

HSMonitor* monitor_with_index(int index) {
    return &* monitors->byIdx(index);
}

int monitors_lock_command(int argc, const char** argv) {
    monitors_lock();
    return 0;
}

void monitors_lock() {
    // lock-number must never be negative
    // ensure that lock value is valid
    if (*g_monitors_locked < 0) {
        *g_monitors_locked = 0;
    }
    // increase lock => it is definitely > 0, i.e. locked
    (*g_monitors_locked)++;
    monitors_lock_changed();
}

int monitors_unlock_command(int argc, const char** argv) {
    monitors_unlock();
    return 0;
}

void monitors_unlock() {
    // lock-number must never be lower than 1 if unlocking
    // so: ensure that lock value is valid
    if (*g_monitors_locked < 1) {
        *g_monitors_locked = 1;
    }
    // decrease lock => unlock
    (*g_monitors_locked)--;
    monitors_lock_changed();
}

void monitors_lock_changed() {
    if (*g_monitors_locked < 0) {
        *g_monitors_locked = 0;
        HSDebug("fixing invalid monitors_locked value to 0\n");
    }
    if (!*g_monitors_locked) {
        // if not locked anymore, then repaint all the dirty monitors
        for (auto m : *monitors) {
            if (m->dirty) {
                monitor_apply_layout(&* m);
            }
        }
    }
}

int monitor_lock_tag_command(int argc, char** argv, Output output) {
    char* cmd_name = argv[0];
    (void)SHIFT(argc, argv);
    HSMonitor *monitor;
    if (argc >= 1) {
        monitor = string_to_monitor(argv[0]);
        if (monitor == NULL) {
            output << cmd_name <<
                ": Monitor \"" << argv[0] << "\" not found!\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        monitor = get_current_monitor();
    }
    monitor->lock_tag = true;
    return 0;
}

int monitor_unlock_tag_command(int argc, char** argv, Output output) {
    char* cmd_name = argv[0];
    (void)SHIFT(argc, argv);
    HSMonitor *monitor;
    if (argc >= 1) {
        monitor = string_to_monitor(argv[0]);
        if (monitor == NULL) {
            output << cmd_name << ": Monitor \"" << argv[0] << "\" not found!\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        monitor = get_current_monitor();
    }
    monitor->lock_tag = false;
    return 0;
}

// monitor detection using xinerama (if available)
#ifdef XINERAMA
// inspired by dwm's isuniquegeom()
static bool geom_unique(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info) {
    while (n--)
        if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
        &&  unique[n].width == info->width && unique[n].height == info->height)
            return false;
    return true;
}

// inspired by dwm's updategeom()
bool detect_monitors_xinerama(RectangleVec &ret) {
    int i, j, n;
    if (!XineramaIsActive(g_display)) {
        return false;
    }
    XineramaScreenInfo *info = XineramaQueryScreens(g_display, &n);
    XineramaScreenInfo *unique = g_new(XineramaScreenInfo, n);
    /* only consider unique geometries as separate screens */
    for (i = 0, j = 0; i < n; i++) {
        if (geom_unique(unique, j, &info[i]))
        {
            memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
        }
    }
    XFree(info);
    n = j;

    RectangleVec monitor_rects(n);
    for (i = 0; i < n; i++) {
        monitor_rects[i].x = unique[i].x_org;
        monitor_rects[i].y = unique[i].y_org;
        monitor_rects[i].width = unique[i].width;
        monitor_rects[i].height = unique[i].height;
    }
    ret.swap(monitor_rects);
    g_free(unique);
    return true;
}
#else  /* XINERAMA */

bool detect_monitors_xinerama(RectangleVec &dest) {
    return false;
}

#endif /* XINERAMA */

// monitor detection that always works: one monitor across the entire screen
bool detect_monitors_simple(RectangleVec &dest) {
    XWindowAttributes attributes;
    XGetWindowAttributes(g_display, g_root, &attributes);
    dest = {{ 0, 0, attributes.width, attributes.height }};
    return true;
}

bool detect_monitors_debug_example(RectangleVec &dest) {
    dest = {{ 0, 0,
              g_screen_width * 2 / 3, g_screen_height * 2 / 3 },
            { g_screen_width / 3, g_screen_height / 3,
              g_screen_width * 2 / 3, g_screen_height * 2 / 3}};
    return true;
}


int detect_monitors_command(int argc, const char **argv, Output output) {
    MonitorDetection detect[] = {
        detect_monitors_xinerama,
        detect_monitors_simple,
        detect_monitors_debug_example, // move up for debugging
    };
    RectangleVec monitor_rects;
    // search for a working monitor detection
    // at least the simple detection must work
    for (int i = 0; i < LENGTH(detect); i++) {
        if (detect[i](monitor_rects)) {
            break;
        }
    }
    assert(!monitor_rects.empty());
    bool list_only = false;
    bool disjoin = true;
    //bool drop_small = true;
    FOR (i,1,argc) {
        if      (!strcmp(argv[i], "-l"))            list_only = true;
        else if (!strcmp(argv[i], "--list"))        list_only = true;
        else if (!strcmp(argv[i], "--no-disjoin"))  disjoin = false;
        // TOOD:
        // else if (!strcmp(argv[i], "--keep-small"))  drop_small = false;
        else {
            output << "detect_monitors: unknown flag \"" << argv[i] << "\"\n";
            return HERBST_INVALID_ARGUMENT;
        }
    }

    int ret = 0;
    if (list_only) {
        for (auto m : monitor_rects) {
            output << m << "\n";
        }
    } else {
        // possibly disjoin them
        if (disjoin) {
            RectList* rl = disjoin_rects(monitor_rects);
            monitor_rects.resize(rectlist_length(rl));
            RectList* cur = rl;
            FOR (i,0,monitor_rects.size()) {
                monitor_rects[i] = cur->rect;
                cur = cur->next;
            }
        }
        // apply it
        ret = set_monitor_rects(monitor_rects);
        if (ret == HERBST_TAG_IN_USE) {
            output << argv[0] << ": There are not enough free tags\n";
        }
    }
    return ret;
}

int monitor_stack_window_count(bool real_clients) {
    return stack_window_count(g_monitor_stack, real_clients);
}

void monitor_stack_to_window_buf(Window* buf, int len, bool real_clients,
                                 int* remain_len) {
    stack_to_window_buf(g_monitor_stack, buf, len, real_clients, remain_len);
}

HSStack* get_monitor_stack() {
    return g_monitor_stack;
}

int monitor_raise_command(int argc, char** argv, Output output) {
    char* cmd_name = argv[0];
    (void)SHIFT(argc, argv);
    HSMonitor* monitor;
    if (argc >= 1) {
        monitor = string_to_monitor(argv[0]);
        if (monitor == NULL) {
            output << cmd_name << ": Monitor \"" << argv[0] << "\" not found!\n";
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        monitor = get_current_monitor();
    }
    stack_raise_slide(g_monitor_stack, monitor->slice);
    return 0;
}

void monitor_restack(HSMonitor* monitor) {
    int count = 1 + stack_window_count(monitor->tag->stack, false);
    Window* buf = g_new(Window, count);
    buf[0] = monitor->stacking_window;
    stack_to_window_buf(monitor->tag->stack, buf + 1, count - 1, false, NULL);
    /* remove a focused fullscreen client */
    HSClient* client = monitor->tag->frame->focusedClient();
    if (client && client->fullscreen_) {
        Window win = client->decorationWindow();
        XRaiseWindow(g_display, win);
        int idx = array_find(buf, count, sizeof(*buf), &win);
        assert(idx >= 0);
        count--;
        memmove(buf + idx, buf + idx + 1, sizeof(*buf) * (count - idx));
    }
    XRestackWindows(g_display, buf, count);
    g_free(buf);
}

int shift_to_monitor(int argc, char** argv, Output output) {
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    char* monitor_str = argv[1];
    HSMonitor* monitor = string_to_monitor(monitor_str);
    if (!monitor) {
        output << monitor_str << ": Invalid monitor\n";
        return HERBST_INVALID_ARGUMENT;
    }
    tag_move_focused_client(monitor->tag);
    return 0;
}

void all_monitors_replace_previous_tag(HSTag *old, HSTag *newmon) {
    for (auto m : *monitors) {
        if (m->tag_previous == old) {
            m->tag_previous = newmon;
        }
    }
}

void drop_enternotify_events() {
    XEvent ev;
    XSync(g_display, False);
    while(XCheckMaskEvent(g_display, EnterWindowMask, &ev));
}

Rectangle HSMonitor::getFloatingArea() {
    auto m = this;
    auto r = m->rect;
    r.x += m->pad_left;
    r.width -= m->pad_left + m->pad_right;
    r.y += m->pad_up;
    r.height -= m->pad_up + m->pad_down;
    return r;
}

