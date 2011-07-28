

#include "clientlist.h"
#include "globals.h"
#include "utils.h"
#include "ipc-protocol.h"
#include "settings.h"
#include "layout.h"
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int* g_window_gap;
int* g_frame_border_width;
int* g_always_show_frame;
int* g_default_frame_layout;
unsigned long g_frame_border_active_color;
unsigned long g_frame_border_normal_color;
unsigned long g_frame_bg_active_color;
unsigned long g_frame_bg_normal_color;

static void fetch_frame_colors() {
    // load settings
    g_window_gap = &(settings_find("window_gap")->value.i);
    g_frame_border_width = &(settings_find("frame_border_width")->value.i);
    g_always_show_frame = &(settings_find("always_show_frame")->value.i);
    g_default_frame_layout = &(settings_find("default_frame_layout")->value.i);
    *g_default_frame_layout = CLAMP(*g_default_frame_layout, 0, LAYOUT_COUNT);
    char* str = settings_find("frame_border_normal_color")->value.s;
    g_frame_border_normal_color = getcolor(str);
    str = settings_find("frame_border_active_color")->value.s;
    g_frame_border_active_color = getcolor(str);
    // background color
    str = settings_find("frame_bg_normal_color")->value.s;
    g_frame_bg_normal_color = getcolor(str);
    str = settings_find("frame_bg_active_color")->value.s;
    g_frame_bg_active_color = getcolor(str);
}

void layout_init() {
    g_cur_monitor = 0;
    g_tags = g_array_new(false, false, sizeof(HSTag*));
    g_monitors = g_array_new(false, false, sizeof(HSMonitor));
    fetch_frame_colors();
}
void reset_frame_colors() {
    fetch_frame_colors();
    all_monitors_apply_layout();
}
static void tag_free(HSTag* tag) {
    g_string_free(tag->name, true);
    g_free(tag);
}

void layout_destroy() {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        HSTag* tag = g_array_index(g_tags, HSTag*, i);
        tag_free(tag);
    }
    g_array_free(g_tags, true);
    g_array_free(g_monitors, true);
}


HSFrame* frame_create_empty() {
    HSFrame* frame = g_new0(HSFrame, 1);
    frame->type = TYPE_CLIENTS;
    frame->window_visible = false;
    frame->content.clients.layout = *g_default_frame_layout;
    // set window atributes
    XSetWindowAttributes at;
    at.background_pixel  = getcolor("red");
    at.background_pixmap = ParentRelative;
    at.override_redirect = True;
    at.bit_gravity       = StaticGravity;
    at.event_mask        = SubstructureRedirectMask|SubstructureNotifyMask
         |ExposureMask|VisibilityChangeMask
         |EnterWindowMask|LeaveWindowMask|FocusChangeMask;
    frame->window = XCreateWindow(g_display, g_root,
                        42, 42, 42, 42, *g_frame_border_width,
                        DefaultDepth(g_display, DefaultScreen(g_display)),
                        CopyFromParent,
                        DefaultVisual(g_display, DefaultScreen(g_display)),
                        CWOverrideRedirect | CWBackPixmap | CWEventMask, &at);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_FRAME_CLASS;
    hint->res_class = HERBST_FRAME_CLASS;
    XSetClassHint(g_display, frame->window, hint);
    XFree(hint);
    return frame;
}

void frame_insert_window(HSFrame* frame, Window window) {
    if (frame->type == TYPE_CLIENTS) {
        // insert it here
        Window* buf = frame->content.clients.buf;
        // append it to buf
        size_t count = frame->content.clients.count;
        count++;
        buf = g_renew(Window, buf, count);
        buf[count-1] = window;
        // write results back
        frame->content.clients.count = count;
        frame->content.clients.buf = buf;
        // check for focus
        if (g_cur_frame == frame
            && frame->content.clients.selection >= (count-1)) {
            frame->content.clients.selection = count - 1;
            window_focus(window);
        }
    } else { /* frame->type == TYPE_FRAMES */
        HSLayout* layout = &frame->content.layout;
        frame_insert_window((layout->selection == 0)? layout->a : layout->b, window);
    }
}

bool frame_remove_window(HSFrame* frame, Window window) {
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        int i;
        for (i = 0; i < count; i++) {
            if (buf[i] == window) {
                // if window was found
                // them remove it
                memmove(buf+i, buf+i+1, sizeof(Window)*(count - i - 1));
                count--;
                buf = g_renew(Window, buf, count);
                frame->content.clients.buf = buf;
                frame->content.clients.count = count;
                // find out new selection
                int selection = frame->content.clients.selection;
                // if selection was before removed window
                // then do nothing
                // else shift it by 1
                selection -= (selection <= i) ? 0 : 1;
                // ensure, that it's a valid index
                selection = count ? CLAMP(selection, 0, count-1) : 0;
                frame->content.clients.selection = selection;
                if (selection < count) {
                    window_focus(buf[selection]);
                }
                return true;
            }
        }
        return false;
    } else { /* frame->type == TYPE_FRAMES */
        bool found = frame_remove_window(frame->content.layout.a, window);
        found = found || frame_remove_window(frame->content.layout.b, window);
        return found;
    }
}

void frame_destroy(HSFrame* frame, Window** buf, size_t* count) {
    if (frame->type == TYPE_CLIENTS) {
        *buf = frame->content.clients.buf;
        *count = frame->content.clients.count;
    } else { /* frame->type == TYPE_FRAMES */
        size_t c1, c2;
        Window *buf1, *buf2;
        frame_destroy(frame->content.layout.a, &buf1, &c1);
        frame_destroy(frame->content.layout.b, &buf2, &c2);
        // append buf2 to buf1
        buf1 = g_renew(Window, buf1, c1 + c2);
        memcpy(buf1+c1, buf2, sizeof(Window) * c2);
        // free unused things
        g_free(buf2);
        // return;
        *buf = buf1;
        *count = c1 + c2;
    }
    // free other things
    XDestroyWindow(g_display, frame->window);
    g_free(frame);
}

void print_frame_tree(HSFrame* frame, int indent, GString** output) {
    unsigned int j;
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        int selection = frame->content.clients.selection;
        unsigned int i;
        for (j = 0; j < indent; j++) {
            *output = g_string_append(*output, " ");
        }
        g_string_append_printf(*output, "frame with wins:%s\n",
            (g_cur_frame == frame) ? "[FOCUS]" : "");
        for (i = 0; i < count; i++) {
            for (j = 0; j < indent; j++) {
                *output = g_string_append(*output, " ");
            }
            g_string_append_printf(*output, "  %s win %d\n",
                (selection == i) ? "*" : " ",
                (int)buf[i]);
        }
    } else { /* frame->type == TYPE_FRAMES */
        for (j = 0; j < indent; j++) {
            *output = g_string_append(*output, " ");
        }
        HSLayout* layout = &frame->content.layout;
        g_string_append_printf(*output,
            "frame: layout %s, size %f%%\n", (layout->align == LAYOUT_VERTICAL
                                        ? "vert" : "horz"),
                ((double)layout->fraction*100)/(double)FRACTION_UNIT);
        assert(layout->a->parent == frame);
        assert(layout->b->parent == frame);
        print_frame_tree(layout->a, indent+2, output);
        print_frame_tree(layout->b, indent+2, output);
    }

}

void print_tag_tree(GString** output) {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        HSTag* tag = g_array_index(g_tags, HSTag*, i);
        char* name = tag->name->str;
        g_string_append_printf(*output, "tag \"%s\" with:\n", name);
        print_frame_tree(tag->frame, 2, output);
    }
}

void monitor_apply_layout(HSMonitor* monitor) {
    if (monitor) {
        XRectangle rect = monitor->rect;
        rect.x += *g_window_gap;
        rect.y += *g_window_gap;
        rect.height -= *g_window_gap;
        rect.width -= *g_window_gap;
        frame_apply_layout(monitor->tag->frame, rect);
    }
}

int frame_current_cycle_client_layout(int argc, char** argv) {
    int delta = 1;
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    if (g_cur_frame && g_cur_frame->type == TYPE_CLIENTS) {
        delta %= LAYOUT_COUNT;
        g_cur_frame->content.clients.layout += delta + LAYOUT_COUNT;
        g_cur_frame->content.clients.layout %= LAYOUT_COUNT;
        monitor_apply_layout(get_current_monitor());
    }
    return 0;
}
void frame_apply_client_layout_linear(HSFrame* frame, XRectangle rect, bool vertical) {
    Window* buf = frame->content.clients.buf;
    size_t count = frame->content.clients.count;
    int selection = frame->content.clients.selection;
    XRectangle cur = rect;
    int last_step_y;
    int last_step_x;
    int step_y;
    int step_x;
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
    int i;
    unsigned long colors[] = {
        g_window_border_normal_color, // normal color
        (g_cur_frame == frame) ?
            g_window_border_active_color : // frame has focus and window is focused
            g_window_border_normal_color, // window is selected but frame isnot focused
    };
    for (i = 0; i < count; i++) {
        // add the space, if count doesnot divide frameheight without remainder
        cur.height += (i == count-1) ? last_step_y : 0;
        cur.width += (i == count-1) ? last_step_x : 0;
        XSetWindowBorder(g_display, buf[i], colors[i == selection]);
        window_resize(buf[i], cur);
        cur.y += step_y;
        cur.x += step_x;
    }
}

void frame_apply_client_layout_horizontal(HSFrame* frame, XRectangle rect) {
    frame_apply_client_layout_linear(frame, rect, false);
}

void frame_apply_client_layout_vertical(HSFrame* frame, XRectangle rect) {
    frame_apply_client_layout_linear(frame, rect, true);
}

void frame_apply_layout(HSFrame* frame, XRectangle rect) {
    if (frame->type == TYPE_CLIENTS) {
        size_t count = frame->content.clients.count;
        // frame only -> apply window_gap
        rect.height -= *g_window_gap;
        rect.width -= *g_window_gap;
        // apply frame width
        rect.x += *g_frame_border_width;
        rect.y += *g_frame_border_width;
        rect.height -= *g_frame_border_width * 2;
        rect.width -= *g_frame_border_width * 2;
        if (rect.width <= WINDOW_MIN_WIDTH || rect.height <= WINDOW_MIN_HEIGHT) {
            // do nothing on invalid size
            return;
        }
        XSetWindowBorderWidth(g_display, frame->window, *g_frame_border_width);
        // set indicator frame
        unsigned long border_color = g_frame_border_normal_color;
        unsigned long bg_color = g_frame_bg_normal_color;
        if (g_cur_frame == frame) {
            border_color = g_frame_border_active_color;
            bg_color = g_frame_bg_active_color;
        }
        XSetWindowBorder(g_display, frame->window, border_color);
        XMoveResizeWindow(g_display, frame->window,
                          rect.x - *g_frame_border_width,
                          rect.y - *g_frame_border_width,
                          rect.width, rect.height);
        XSetWindowBackground(g_display, frame->window, bg_color);
        XClearWindow(g_display, frame->window);
        XLowerWindow(g_display, frame->window);
        frame_set_visible(frame, *g_always_show_frame
            || (count != 0) || (g_cur_frame == frame));
        // move windows
        if (count == 0) {
            return;
        }
        frame_apply_client_layout_linear(frame, rect,
            (frame->content.clients.layout == LAYOUT_VERTICAL));
    } else { /* frame->type == TYPE_FRAMES */
        HSLayout* layout = &frame->content.layout;
        XRectangle first = rect;
        XRectangle second = rect;
        if (layout->align == LAYOUT_VERTICAL) {
            first.height = (rect.height * layout->fraction) / FRACTION_UNIT;
            second.y += first.height;
            second.height -= first.height;
        } else { // (layout->align == LAYOUT_HORIZONTAL)
            first.width = (rect.width * layout->fraction) / FRACTION_UNIT;
            second.x += first.width;
            second.width -= first.width;
        }
        frame_set_visible(frame, false);
        frame_apply_layout(layout->a, first);
        frame_apply_layout(layout->b, second);
    }
}

int list_monitors(int argc, char** argv, GString** output) {
    (void)argc;
    (void)argv;
    int i;
    for (i = 0; i < g_monitors->len; i++) {
        HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, i);
        g_string_append_printf(*output, "%d: %dx%d @ (%d,%d) with tag \"%s\"%s\n",
            i,
            monitor->rect.width, monitor->rect.height,
            monitor->rect.x, monitor->rect.x,
            monitor->tag ? monitor->tag->name->str : "???",
            (g_cur_monitor == i) ? " [FOCUS]" : "");
    }
    return 0;
}

HSMonitor* add_monitor(XRectangle rect, HSTag* tag) {
    assert(tag != NULL);
    HSMonitor m;
    m.rect = rect;
    m.tag = tag;
    g_array_append_val(g_monitors, m);
    return &g_array_index(g_monitors, HSMonitor, g_monitors->len-1);
}

int add_monitor_command(int argc, char** argv) {
    // usage: add_monitor RECTANGLE TAG
    if (argc < 3) {
        return HERBST_INVALID_ARGUMENT;
    }
    XRectangle rect = parse_rectangle(argv[1]);
    HSTag* tag = find_tag(argv[2]);
    if (!tag) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (find_monitor_with_tag(tag)) {
        return HERBST_TAG_IN_USE;
    }
    HSMonitor* monitor = add_monitor(rect, tag);
    monitor_apply_layout(monitor);
    return 0;
}

int remove_monitor_command(int argc, char** argv) {
    // usage: remove_monitor INDEX
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    int index = atoi(argv[1]);
    if (index < 0 || index >= g_monitors->len) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (g_monitors->len <= 1) {
        return HERBST_FORBIDDEN;
    }
    HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, index);
    // adjust selection
    if (g_cur_monitor > index) {
        // same monitor shall be selected after remove
        g_cur_monitor--;
    }
    assert(monitor->tag);
    assert(monitor->tag->frame);
    // hide clients
    frame_hide_recursive(monitor->tag->frame);
    // and remove monitor completly
    g_array_remove_index(g_monitors, index);
    return 0;
}

int move_monitor_command(int argc, char** argv) {
    // usage: move_monitor INDEX RECT
    // moves monitor with number to RECT
    if (argc < 3) {
        return HERBST_INVALID_ARGUMENT;
    }
    int index = atoi(argv[1]);
    if (index < 0 || index >= g_monitors->len) {
        return HERBST_INVALID_ARGUMENT;
    }
    XRectangle rect = parse_rectangle(argv[2]);
    if (rect.width < WINDOW_MIN_WIDTH || rect.height < WINDOW_MIN_HEIGHT) {
        return HERBST_INVALID_ARGUMENT;
    }
    // else: just move it:
    HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, index);
    monitor->rect = rect;
    monitor_apply_layout(monitor);
    return 0;
}

HSMonitor* find_monitor_with_tag(HSTag* tag) {
    int i;
    for (i = 0; i < g_monitors->len; i++) {
        HSMonitor* m = &g_array_index(g_monitors, HSMonitor, i);
        if (m->tag == tag) {
            return m;
        }
    }
    return NULL;
}

HSTag* find_tag(char* name) {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        if (!strcmp(g_array_index(g_tags, HSTag*, i)->name->str, name)) {
            return g_array_index(g_tags, HSTag*, i);
        }
    }
    return NULL;
}

HSTag* add_tag(char* name) {
    HSTag* find_result = find_tag(name);
    if (find_result) {
        // nothing to do
        return find_result;
    }
    HSTag* tag = g_new(HSTag, 1);
    tag->frame = frame_create_empty();
    tag->name = g_string_new(name);
    g_array_append_val(g_tags, tag);
    return tag;
}

int tag_add_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    add_tag(argv[1]);
    return 0;
}

int tag_rename_command(int argc, char** argv) {
    if (argc < 3) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = find_tag(argv[1]);
    if (!tag) {
        return HERBST_INVALID_ARGUMENT;
    }
    if (find_tag(argv[2])) {
        return HERBST_TAG_IN_USE;
    }
    tag->name = g_string_assign(tag->name, argv[2]);
    return 0;
}

int tag_remove_command(int argc, char** argv) {
    // usage: remove TAG [TARGET]
    // it removes an TAG and moves all its wins to TARGET
    // if no TARGET is given, current tag is used
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = find_tag(argv[1]);
    HSTag* target = (argc >= 3) ? find_tag(argv[2]) : get_current_monitor()->tag;
    if (!tag || !target || (tag == target)) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSMonitor* monitor = find_monitor_with_tag(tag);
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    if (monitor) {
        return HERBST_TAG_IN_USE;
    }
    // save all these windows
    Window* buf;
    size_t count;
    frame_destroy(tag->frame, &buf, &count);
    int i;
    for (i = 0; i < count; i++) {
        frame_insert_window(target->frame, buf[i]);
    }
    if (monitor_target) {
        // if target monitor is viewed, then show windows
        monitor_apply_layout(monitor_target);
        for (i = 0; i < count; i++) {
            XMapWindow(g_display, buf[i]);
        }
    }
    g_free(buf);
    // remove tag
    tag_free(tag);
    for (i = 0; i < g_tags->len; i++) {
        if (g_array_index(g_tags, HSTag*, i) == tag) {
            g_array_remove_index(g_tags, i);
            break;
        }
    }
    return 0;
}

void ensure_tags_are_available() {
    if (g_tags->len > 0) {
        // nothing to do
        return;
    }
    add_tag("default");
}

void ensure_monitors_are_available() {
    if (g_monitors->len > 0) {
        // nothing to do
        return;
    }
    // add monitor if necessary
    XRectangle rect = {
        .x = 0, .y = 0,
        .width = DisplayWidth(g_display, DefaultScreen(g_display)),
        .height = DisplayHeight(g_display, DefaultScreen(g_display)),
    };
    ensure_tags_are_available();
    // add monitor with first tag
    HSMonitor* m = add_monitor(rect, g_array_index(g_tags, HSTag*, 0));
    g_cur_monitor = 0;
    g_cur_frame = m->tag->frame;
}

HSFrame* frame_current_selection() {
    HSMonitor* m = get_current_monitor();
    if (!m->tag) return NULL;
    HSFrame* frame = m->tag->frame;
    while (frame->type == TYPE_FRAMES) {
        frame = (frame->content.layout.selection == 0) ?
                frame->content.layout.a :
                frame->content.layout.b;
    }
    return frame;
}

int frame_current_cycle_selection(int argc, char** argv) {
    int delta = 1;
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    // find current selection
    HSFrame* frame = frame_current_selection();
    if (frame->content.clients.count == 0) {
        // nothing to do
        return 0;
    }
    int index = frame->content.clients.selection;
    index += delta;
    index %= frame->content.clients.count;
    index += frame->content.clients.count;
    index %= frame->content.clients.count;
    frame->content.clients.selection = index;
    Window window = frame->content.clients.buf[index];
    window_focus(window);
    return 0;
}

void frame_split(HSFrame* frame, int align, int fraction) {
    HSFrame* first = frame_create_empty();
    HSFrame* second = frame_create_empty();
    first->content = frame->content;
    first->type = frame->type;
    first->parent = frame;
    second->parent = frame;
    second->type = TYPE_CLIENTS;
    frame->type = TYPE_FRAMES;
    frame->content.layout.align = align;
    frame->content.layout.a = first;
    frame->content.layout.b = second;
    frame->content.layout.selection = 0;
    frame->content.layout.fraction = fraction;
    // set focus
    g_cur_frame = first;
    // redraw monitor if exists
    HSMonitor* m = monitor_with_frame(frame);
    monitor_apply_layout(m);
}

int frame_split_command(int argc, char** argv) {
    // usage: split (cur) h|v FRACTION
    if (argc < 4) {
        return HERBST_INVALID_ARGUMENT;
    }
    int align = LAYOUT_VERTICAL;
    if (argv[2][0] == 'h') {
        align = LAYOUT_HORIZONTAL;
    } // else: layout ist vertical
    int fraction = FRACTION_UNIT* CLAMP(atof(argv[3]), 0.0, 1.0);
    HSFrame* frame = frame_current_selection();
    if (!frame) return 0; // nothing to do
    frame_split(frame, align, fraction);
    return 0;
}

HSTag* find_tag_with_toplevel_frame(HSFrame* frame) {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        HSTag* m = g_array_index(g_tags, HSTag*, i);
        if (m->frame == frame) {
            return m;
        }
    }
    return NULL;
}

HSMonitor* monitor_with_frame(HSFrame* frame) {
    // find toplevel Frame
    while (frame->parent) {
        frame = frame->parent;
    }
    HSTag* tag = find_tag_with_toplevel_frame(frame);
    return find_monitor_with_tag(tag);
}

HSFrame* frame_neighbour(HSFrame* frame, char direction) {
    HSFrame* other;
    bool found = false;
    while (frame->parent) {
        // find frame, where we can change the
        // selection in the desired direction
        HSLayout* layout = &frame->parent->content.layout;
        switch(direction) {
            case 'r':
                if (layout->align == LAYOUT_HORIZONTAL
                    && layout->a == frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'l':
                if (layout->align == LAYOUT_HORIZONTAL
                    && layout->b == frame) {
                    found = true;
                    other = layout->a;
                }
                break;
            case 'd':
                if (layout->align == LAYOUT_VERTICAL
                    && layout->a == frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'u':
                if (layout->align == LAYOUT_VERTICAL
                    && layout->b == frame) {
                    found = true;
                    other = layout->a;
                }
                break;
            default:
                return NULL;
                break;
        }
        if (found) {
            break;
        }
        // else: go one step closer to root
        frame = frame->parent;
    }
    if (!found) {
        return NULL;
    }
    return other;
}

int frame_focus_command(int argc, char** argv) {
    // usage: focus left|right|up|down
    if (argc < 2) return HERBST_INVALID_ARGUMENT;
    if (!g_cur_frame) {
        fprintf(stderr, "warning: no frame is selected\n");
        return HERBST_UNKNOWN_ERROR;
    }
    char direction = argv[1][0];
    //HSFrame* frame = g_cur_frame;
    HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
    if (neighbour != NULL) { // if neighbour was found
        HSFrame* parent = neighbour->parent;
        // alter focus (from 0 to 1, from 1 to 0)
        int selection = parent->content.layout.selection;
        selection = (selection == 1) ? 0 : 1;
        parent->content.layout.selection = selection;
        // change focus if possible
        frame_focus_recursive(parent);
        monitor_apply_layout(get_current_monitor());
    }
    return 0;
}

int frame_move_window_command(int argc, char** argv) {
    // usage: move left|right|up|down
    if (argc < 2) return HERBST_INVALID_ARGUMENT;
    if (!g_cur_frame) {
        fprintf(stderr, "warning: no frame is selected\n");
        return HERBST_UNKNOWN_ERROR;
    }
    char direction = argv[1][0];
    HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
    Window win = frame_focused_window(g_cur_frame);
    if (win && neighbour != NULL) { // if neighbour was found
        // move window to neighbour
        frame_remove_window(g_cur_frame, win);
        frame_insert_window(neighbour, win);
        // layout was changed, so update it
        monitor_apply_layout(get_current_monitor());
    }
    return 0;
}

void frame_unfocus() {
    //XSetInputFocus(g_display, g_root, RevertToPointerRoot, CurrentTime);
}

Window frame_focused_window(HSFrame* frame) {
    if (!frame) {
        return (Window)0;
    }
    // follow the selection to a leave
    while (frame->type == TYPE_FRAMES) {
        frame = (frame->content.layout.selection == 0) ?
                frame->content.layout.a :
                frame->content.layout.b;
    }
    if (frame->content.clients.count) {
        int selection = frame->content.clients.selection;
        return frame->content.clients.buf[selection];
    } // else, if there are no windows
    return (Window)0;
}

int frame_focus_recursive(HSFrame* frame) {
    // follow the selection to a leave
    while (frame->type == TYPE_FRAMES) {
        frame = (frame->content.layout.selection == 0) ?
                frame->content.layout.a :
                frame->content.layout.b;
    }
    g_cur_frame = frame;
    frame_unfocus();
    if (frame->content.clients.count) {
        int selection = frame->content.clients.selection;
        window_focus(frame->content.clients.buf[selection]);
    } else {
        // else give focus to focus indicator
        XUngrabButton(g_display, AnyButton, AnyModifier, frame->window);
        XSetInputFocus(g_display, frame->window, RevertToPointerRoot, CurrentTime);
    }
    return 0;
}

// do recursive for each element of the (binary) frame tree
// if order <= 0 -> action(node); action(left); action(right);
// if order == 1 -> action(left); action(node); action(right);
// if order >= 2 -> action(left); action(right); action(node);
void frame_do_recursive(HSFrame* frame, void (*action)(HSFrame*), int order) {
    if (!frame) {
        return;
    }
    if (frame->type == TYPE_FRAMES) {
        // clients and subframes
        HSLayout* layout = &(frame->content.layout);
        if (order <= 0) action(frame);
        frame_do_recursive(layout->a, action, order);
        if (order == 1) action(frame);
        frame_do_recursive(layout->b, action, order);
        if (order >= 2) action(frame);
    } else {
        // action only
        action(frame);
    }
}

static void frame_hide(HSFrame* frame) {
    frame_set_visible(frame, false);
    if (frame->type == TYPE_CLIENTS) {
        int i;
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            XUnmapWindow(g_display, buf[i]);
        }
    }
}

void frame_hide_recursive(HSFrame* frame) {
    // first hide children => order = 2
    frame_do_recursive(frame, frame_hide, 2);
}

static void frame_show_clients(HSFrame* frame) {
    if (frame->type == TYPE_CLIENTS) {
        int i;
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            XMapWindow(g_display, buf[i]);
        }
    }
}

void frame_show_recursive(HSFrame* frame) {
    // first show parents, then childrend => order = 0
    frame_do_recursive(frame, frame_show_clients, 2);
}

int frame_remove_command(int argc, char** argv) {
    if (!g_cur_frame->parent) {
        // do nothing if is toplevel frame
        return 0;
    }
    assert(g_cur_frame->type == TYPE_CLIENTS);
    HSFrame* parent = g_cur_frame->parent;
    HSFrame* first = g_cur_frame;
    HSFrame* second;
    if (first == parent->content.layout.a) {
        second = parent->content.layout.b;
    } else {
        assert(first == parent->content.layout.b);
        second = parent->content.layout.a;
    }
    size_t count;
    Window* wins;
    // get all wins from first child
    frame_destroy(first, &wins, &count);
    // and insert them to other child.. inefficiently
    int i;
    for (i = 0; i < count; i++) {
        frame_insert_window(second, wins[i]);
    }
    g_free(wins);
    XDestroyWindow(g_display, parent->window);
    // now do tree magic
    // and make second child the new parent
    // set parent
    second->parent = parent->parent;
    // copy all other elements
    *parent = *second;
    // fix childs' parent-pointer
    if (parent->type == TYPE_FRAMES) {
        parent->content.layout.a->parent = parent;
        parent->content.layout.b->parent = parent;
    }
    g_free(second);
    // re-layout
    frame_focus_recursive(parent);
    monitor_apply_layout(get_current_monitor());
    return 0;
}

HSMonitor* get_current_monitor() {
    return &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
}

void frame_set_visible(HSFrame* frame, bool visible) {
    if (!frame) {
        return;
    }
    if (frame->window_visible == visible) {
        return;
    }
    if (visible) {
        XMapWindow(g_display, frame->window);
    } else {
        XUnmapWindow(g_display, frame->window);
    }
    frame->window_visible = visible;
}

void all_monitors_apply_layout() {
    int i;
    for (i = 0; i < g_monitors->len; i++) {
        HSMonitor* m = &g_array_index(g_monitors, HSMonitor, i);
        monitor_apply_layout(m);
    }
}

void monitor_set_tag(HSMonitor* monitor, HSTag* tag) {
    HSMonitor* other = find_monitor_with_tag(tag);
    if (other != NULL) {
        // todo: swap tags
        // currently: do nothing
        g_warning("cannot swap tags yet..");
        return;
    }
    HSTag* old_tag = monitor->tag;
    // 1. hide old tag
    frame_hide_recursive(old_tag->frame);
    // 2. show new tag
    monitor->tag = tag;
    frame_show_recursive(tag->frame);
    // reset focus
    frame_focus_recursive(tag->frame);
}

int monitor_set_tag_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSMonitor* monitor = get_current_monitor();
    HSTag*  tag = find_tag(argv[1]);
    if (monitor && tag) {
        monitor_set_tag(get_current_monitor(), tag);
    }
    return 0;
}

int tag_move_window_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSFrame*  frame = g_cur_frame;
    if (!g_cur_frame) {
        // nothing to do
        return 0;
    }
    Window window = frame_focused_window(frame);
    if (window == 0) {
        // nothing to do
        return 0;
    }
    HSTag* target = find_tag(argv[1]);
    if (!target) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSMonitor* monitor = get_current_monitor();
    if (monitor->tag == target) {
        // nothing to do
        return 0;
    }
    HSMonitor* monitor_target = find_monitor_with_tag(target);
    frame_remove_window(frame, window);
    // insert window into target
    frame_insert_window(target->frame, window);
    // refresh things
    if (monitor && !monitor_target) {
        // window is moved to unvisible tag
        // so hide it
        XUnmapWindow(g_display, window);
    }
    monitor_apply_layout(monitor);
    if (monitor_target) {
        monitor_apply_layout(monitor_target);
    }
    return 0;
}

int monitor_cycle_command(int argc, char** argv) {
    int delta = 1;
    int count = g_monitors->len;
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    int new_selection = g_cur_monitor + delta;
    // fix range of index
    new_selection %= count;
    new_selection += count;
    new_selection %= count;
    // really change selection
    HSMonitor* old = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
    HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, new_selection);
    if (old == monitor) {
        // nothing to do
        return 0;
    }
    // change selection globals
    assert(monitor->tag);
    assert(monitor->tag->frame);
    g_cur_monitor = new_selection;
    frame_focus_recursive(monitor->tag->frame);
    // repaint monitors
    monitor_apply_layout(old);
    monitor_apply_layout(monitor);
    return 0;
}

