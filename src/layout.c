/** Copyright 2011 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */


#include "clientlist.h"
#include "globals.h"
#include "utils.h"
#include "hook.h"
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
int* g_focus_follows_shift;
int* g_frame_bg_transparent;
int* g_swap_monitors_to_get_tag;
int* g_direction_external_only;
int* g_gapless_grid;
unsigned long g_frame_border_active_color;
unsigned long g_frame_border_normal_color;
unsigned long g_frame_bg_active_color;
unsigned long g_frame_bg_normal_color;
char*   g_tree_style = NULL;
bool    g_tag_flags_dirty = true;

char* g_align_names[] = {
    "vertical",
    "horizontal",
};

char* g_layout_names[] = {
    "vertical",
    "horizontal",
    "max",
    "grid",
};

static void fetch_frame_colors() {
    // load settings
    g_window_gap = &(settings_find("window_gap")->value.i);
    g_focus_follows_shift = &(settings_find("focus_follows_shift")->value.i);
    g_frame_border_width = &(settings_find("frame_border_width")->value.i);
    g_always_show_frame = &(settings_find("always_show_frame")->value.i);
    g_frame_bg_transparent = &(settings_find("frame_bg_transparent")->value.i);
    g_default_frame_layout = &(settings_find("default_frame_layout")->value.i);
    g_swap_monitors_to_get_tag = &(settings_find("swap_monitors_to_get_tag")->value.i);
    g_direction_external_only = &(settings_find("default_direction_external_only")->value.i);
    g_gapless_grid = &(settings_find("gapless_grid")->value.i);
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

    // tree style
    g_tree_style = settings_find("tree_style")->value.s;
    if (g_utf8_strlen(g_tree_style, -1) < 8) {
        g_warning("too few characters in setting tree_style\n");
        // ensure that it is long enough
        char* argv[] = { "set", "tree_style", "01234567" };
        settings_set(LENGTH(argv), argv);
    }
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
        frame_show_recursive(tag->frame);
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

void dump_frame_tree(HSFrame* frame, GString** output) {
    if (frame->type == TYPE_CLIENTS) {
        g_string_append_printf(*output, "%cclients%c%s:%d",
            LAYOUT_DUMP_BRACKETS[0],
            LAYOUT_DUMP_WHITESPACES[0],
            g_layout_names[frame->content.clients.layout],
            frame->content.clients.selection);
        Window* buf = frame->content.clients.buf;
        size_t i, count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            g_string_append_printf(*output, "%c0x%lx",
                LAYOUT_DUMP_WHITESPACES[0],
                buf[i]);
        }
        g_string_append_c(*output, LAYOUT_DUMP_BRACKETS[1]);
    } else {
        /* type == TYPE_FRAMES */
        g_string_append_printf(*output, "%csplit%c%s%c%lf%c%d%c",
            LAYOUT_DUMP_BRACKETS[0],
            LAYOUT_DUMP_WHITESPACES[0],
            g_align_names[frame->content.layout.align],
            LAYOUT_DUMP_SEPARATOR,
            ((double)frame->content.layout.fraction) / (double)FRACTION_UNIT,
            LAYOUT_DUMP_SEPARATOR,
            frame->content.layout.selection,
            LAYOUT_DUMP_WHITESPACES[0]);
        dump_frame_tree(frame->content.layout.a, output);
        g_string_append_c(*output, LAYOUT_DUMP_WHITESPACES[0]);
        dump_frame_tree(frame->content.layout.b, output);
        g_string_append_c(*output, LAYOUT_DUMP_BRACKETS[1]);
    }
}

char* load_frame_tree(HSFrame* frame, char* description, GString** errormsg) {
    // find next (
    description = strchr(description, LAYOUT_DUMP_BRACKETS[0]);
    if (!description) {
        g_string_append_printf(*errormsg, "missing %c\n",
            LAYOUT_DUMP_BRACKETS[0]);
        return NULL;
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
    char args[args_len + 1];
    strncpy(args, description, args_len);
    args[args_len] = '\0';
    // jump over args substring
    description += args_len;
    if (!*description) {
        return NULL;
    }
    description += strspn(description, LAYOUT_DUMP_WHITESPACES);
    if (!*description) {
        return NULL;
    }

    // apply type to frame
    if (type == TYPE_FRAMES) {
        // parse args
        char* align_name = g_new(char, strlen(args)+1);
        int selection;
        double fraction_double;
#define SEP LAYOUT_DUMP_SEPARATOR_STR
        if (3 != sscanf(args, "%[^"SEP"]"SEP"%lf"SEP"%d",
            align_name, &fraction_double, &selection)) {
            g_string_append_printf(*errormsg,
                    "cannot parse frame args \"%s\"\n", args);
            return NULL;
        }
#undef SEP
        int align = find_align_by_name(align_name);
        g_free(align_name);
        if (align < 0) {
            g_string_append_printf(*errormsg,
                    "invalid align name in args \"%s\"\n", args);
            return NULL;
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
                g_string_append_printf(*errormsg,
                    "cannot split frame");
                return NULL;
            }
        }
        frame->content.layout.selection = selection;

        // now parse subframes
        description = load_frame_tree(frame->content.layout.a,
                        description, errormsg);
        if (!description) return NULL;
        description = load_frame_tree(frame->content.layout.b,
                        description, errormsg);
        if (!description) return NULL;
    } else {
        // parse args
        char* layout_name = g_new(char, strlen(args)+1);
        int selection;
#define SEP LAYOUT_DUMP_SEPARATOR_STR
        if (2 != sscanf(args, "%[^"SEP"]"SEP"%d",
            layout_name, &selection)) {
            g_string_append_printf(*errormsg,
                    "cannot parse frame args \"%s\"\n", args);
            return NULL;
        }
#undef SEP
        int layout = find_layout_by_name(layout_name);
        g_free(layout_name);
        if (layout < 0) {
            g_string_append_printf(*errormsg,
                    "cannot parse layout from args \"%s\"\n", args);
            return NULL;
        }

        // ensure that it is a client frame
        if (frame->type == TYPE_FRAMES) {
            // remove childs
            Window *buf1, *buf2;
            size_t count1, count2;
            frame_destroy(frame->content.layout.a, &buf1, &count1);
            frame_destroy(frame->content.layout.b, &buf2, &count2);

            // merge bufs
            size_t count = count1 + count2;
            Window* buf = g_new(Window, count);
            memcpy(buf,             buf1, sizeof(Window) * count1);
            memcpy(buf + count1,    buf2, sizeof(Window) * count2);
            g_free(buf1);
            g_free(buf2);

            // setup frame
            frame->type = TYPE_CLIENTS;
            frame->content.clients.buf = buf;
            frame->content.clients.count = count;
            frame->content.clients.selection = 0; // only some sane defauts
            frame->content.clients.layout = 0; // only some sane defauts
        }

        // bring child wins
        // jump over whitespaces
        description += strspn(description, LAYOUT_DUMP_WHITESPACES);
        int index = 0;
        HSTag* tag = find_tag_with_toplevel_frame(get_toplevel_frame(frame));
        while (*description != LAYOUT_DUMP_BRACKETS[1]) {
            Window win;
            if (1 != sscanf(description, "0x%lx\n", &win)) {
                g_string_append_printf(*errormsg,
                        "cannot parse window id from \"%s\"\n", description);
                return NULL;
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
            HSMonitor* clientmonitor = find_monitor_with_tag(client->tag);
            if (!frame_remove_window(client->tag->frame, win)) {
                g_warning("window %lx wasnot found on tag %s\n",
                    win, client->tag->name->str);
            }
            if (clientmonitor) {
                monitor_apply_layout(clientmonitor);
            }

            // insert it to buf
            Window* buf = frame->content.clients.buf;
            size_t count = frame->content.clients.count;
            count++;
            index = CLAMP(index, 0, count - 1);
            buf = g_renew(Window, buf, count);
            memmove(buf + index + 1, buf + index,
                    sizeof(Window) * (count - index - 1));
            buf[index] = win;
            frame->content.clients.buf = buf;
            frame->content.clients.count = count;

            client->tag = tag;

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
        g_string_append_printf(*errormsg, "warning: missing closing bracket %c\n", LAYOUT_DUMP_BRACKETS[1]);
    }
    // and over whitespaces
    description += strspn(description, LAYOUT_DUMP_WHITESPACES);
    return description;
}

int find_layout_by_name(char* name) {
    for (int i = 0; i < LENGTH(g_layout_names); i++) {
        if (!strcmp(name, g_layout_names[i])) {
            return i;
        }
    }
    return -1;
}

int find_align_by_name(char* name) {
    for (int i = 0; i < LENGTH(g_align_names); i++) {
        if (!strcmp(name, g_align_names[i])) {
            return i;
        }
    }
    return -1;
}

HSFrame* get_toplevel_frame(HSFrame* frame) {
    if (!frame) return NULL;
    while (frame->parent) {
        frame = frame->parent;
    }
    return frame;
}

void print_frame_tree(HSFrame* frame, char* indent, char* rootprefix, GString** output) {
    if (frame->type == TYPE_CLIENTS) {
        g_string_append(*output, rootprefix);
        *output = g_string_append_unichar(*output,
            UTF8_STRING_AT(g_tree_style, 5));
        // list of clients
        g_string_append_printf(*output, " %s:",
            g_layout_names[frame->content.clients.layout]);
        Window* buf = frame->content.clients.buf;
        size_t i, count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            g_string_append_printf(*output, " 0x%lx", buf[i]);
        }
        if (g_cur_frame == frame) {
            *output = g_string_append(*output, " [FOCUS]");
        }
        *output = g_string_append(*output, "\n");
    } else {
        /* type == TYPE_FRAMES */
        g_string_append_printf(*output, "%s", rootprefix);
        *output = g_string_append_unichar(*output,
            UTF8_STRING_AT(g_tree_style, 6));
        *output = g_string_append_unichar(*output,
            UTF8_STRING_AT(g_tree_style, 7));
        // insert frame description
        g_string_append_printf(*output, "%s %d%% selection=%d",
            g_layout_names[frame->content.layout.align],
            frame->content.layout.fraction * 100 / FRACTION_UNIT,
            frame->content.layout.selection);

        *output = g_string_append_c(*output, '\n');

        // first child
        GString* child_indent = g_string_new(indent);
        child_indent = g_string_append_c(child_indent, ' ');
        child_indent = g_string_append_unichar(child_indent,
            UTF8_STRING_AT(g_tree_style, 1));

        GString* child_prefix = g_string_new(indent);
        child_prefix = g_string_append_c(child_prefix, ' ');
        child_prefix = g_string_append_unichar(child_prefix,
            UTF8_STRING_AT(g_tree_style, 3));
        print_frame_tree(frame->content.layout.a, child_indent->str,
                         child_prefix->str, output);

        // second child
        g_string_printf(child_indent, "%s ", indent);
        child_indent = g_string_append_unichar(child_indent,
            UTF8_STRING_AT(g_tree_style, 2));
        g_string_printf(child_prefix, "%s ", indent);
        child_prefix = g_string_append_unichar(child_prefix,
            UTF8_STRING_AT(g_tree_style, 4));
        print_frame_tree(frame->content.layout.b, child_indent->str,
                         child_prefix->str, output);

        // cleanup
        g_string_free(child_indent, true);
        g_string_free(child_prefix, true);
    }
}

void print_tag_tree(HSTag* tag, GString** output) {
    GString* root_indicator = g_string_new("");
    root_indicator = g_string_append_unichar(root_indicator,
            UTF8_STRING_AT(g_tree_style, 0));
    print_frame_tree(tag->frame, " ", root_indicator->str, output);
    g_string_free(root_indicator, true);
}

void frame_apply_floating_layout(HSFrame* frame, HSMonitor* m) {
    if (!frame) return;
    if (frame->type == TYPE_FRAMES) {
        frame_apply_floating_layout(frame->content.layout.a, m);
        frame_apply_floating_layout(frame->content.layout.b, m);
    } else {
        /* if(frame->type == TYPE_CLIENTS) */
        frame_set_visible(frame, false);
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        size_t selection = frame->content.clients.selection;
        /* border color */
        for (int i = 0; i < count; i++) {
            HSClient* client = get_client_from_window(buf[i]);
            client_setup_border(client, (g_cur_frame == frame) && (i == selection));
            client_resize_floating(client, m);
        }
    }
}

void monitor_apply_layout(HSMonitor* monitor) {
    if (monitor) {
        XRectangle rect = monitor->rect;
        // apply pad
        rect.x += monitor->pad_left;
        rect.width -= (monitor->pad_left + monitor->pad_right);
        rect.y += monitor->pad_up;
        rect.height -= (monitor->pad_up + monitor->pad_down);
        // apply window gap
        rect.x += *g_window_gap;
        rect.y += *g_window_gap;
        rect.height -= *g_window_gap;
        rect.width -= *g_window_gap;
        if (monitor->tag->floating) {
            frame_apply_floating_layout(monitor->tag->frame, monitor);
        } else {
            frame_apply_layout(monitor->tag->frame, rect);
        }
        if (get_current_monitor() == monitor) {
            frame_focus_recursive(monitor->tag->frame);
        }
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
    for (int i = 0; i < count; i++) {
        HSClient* client = get_client_from_window(buf[i]);
        // add the space, if count doesnot divide frameheight without remainder
        cur.height += (i == count-1) ? last_step_y : 0;
        cur.width += (i == count-1) ? last_step_x : 0;
        client_setup_border(client, (g_cur_frame == frame) && (i == selection));
        client_resize_tiling(client, cur);
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

void frame_apply_client_layout_max(HSFrame* frame, XRectangle rect) {
    Window* buf = frame->content.clients.buf;
    size_t count = frame->content.clients.count;
    int selection = frame->content.clients.selection;
    for (int i = 0; i < count; i++) {
        HSClient* client = get_client_from_window(buf[i]);
        client_setup_border(client, (g_cur_frame == frame) && (i == selection));
        client_resize_tiling(client, rect);
        if (i == selection) {
            XRaiseWindow(g_display, buf[i]);
        }
    }
}

void frame_layout_grid_get_size(size_t count, int* res_rows, int* res_cols) {
    int cols = 0;
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

void frame_apply_client_layout_grid(HSFrame* frame, XRectangle rect) {
    Window* buf = frame->content.clients.buf;
    size_t count = frame->content.clients.count;
    int selection = frame->content.clients.selection;
    if (count == 0) {
        return;
    }

    int rows, cols;
    frame_layout_grid_get_size(count, &rows, &cols);
    int width = rect.width / cols;
    int height = rect.height / rows;
    int i = 0;
    XRectangle cur = rect; // current rectangle
    for (int r = 0; r < rows; r++) {
        // reset to left
        cur.x = rect.x;
        cur.width = width;
        cur.height = height;
        if (r == rows -1) {
            // fill small pixel gap below last row
            cur.height += rect.height % rows;
        }
        for (int c = 0; c < cols && i < count; c++) {
            if (*g_gapless_grid && (i == count - 1) // if last client
                && (count % cols != 0)) {           // if cols remain
                // fill remaining cols with client
                cur.width = rect.x + rect.width - cur.x;
            } else if (c == cols - 1) {
                // fill small pixel gap in last col
                cur.width += rect.width % cols;
            }

            // apply size
            HSClient* client = get_client_from_window(buf[i]);
            client_setup_border(client, (g_cur_frame == frame) && (i == selection));
            client_resize_tiling(client, cur);
            cur.x += width;
            i++;
        }
        cur.y += height;
    }

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
        if (*g_frame_bg_transparent) {
            XSetWindowBackgroundPixmap(g_display, frame->window, ParentRelative);
        } else {
            XSetWindowBackground(g_display, frame->window, bg_color);
        }
        XClearWindow(g_display, frame->window);
        XLowerWindow(g_display, frame->window);
        frame_set_visible(frame, *g_always_show_frame
            || (count != 0) || (g_cur_frame == frame));
        // move windows
        if (count == 0) {
            return;
        }
        if (frame->content.clients.layout == LAYOUT_MAX) {
            frame_apply_client_layout_max(frame, rect);
        } else if (frame->content.clients.layout == LAYOUT_GRID) {
            frame_apply_client_layout_grid(frame, rect);
        } else {
            frame_apply_client_layout_linear(frame, rect,
                (frame->content.clients.layout == LAYOUT_VERTICAL));
        }
    } else { /* frame->type == TYPE_FRAMES */
        HSLayout* layout = &frame->content.layout;
        XRectangle first = rect;
        XRectangle second = rect;
        if (layout->align == ALIGN_VERTICAL) {
            first.height = (rect.height * layout->fraction) / FRACTION_UNIT;
            second.y += first.height;
            second.height -= first.height;
        } else { // (layout->align == ALIGN_HORIZONTAL)
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
        g_string_append_printf(*output, "%d: %dx%d%+d%+d with tag \"%s\"%s\n",
            i,
            monitor->rect.width, monitor->rect.height,
            monitor->rect.x, monitor->rect.y,
            monitor->tag ? monitor->tag->name->str : "???",
            (g_cur_monitor == i) ? " [FOCUS]" : "");
    }
    return 0;
}

HSMonitor* add_monitor(XRectangle rect, HSTag* tag) {
    assert(tag != NULL);
    HSMonitor m;
    memset(&m, 0, sizeof(m));
    m.rect = rect;
    m.tag = tag;
    m.mouse.x = 0;
    m.mouse.y = 0;
    g_array_append_val(g_monitors, m);
    return &g_array_index(g_monitors, HSMonitor, g_monitors->len-1);
}

int add_monitor_command(int argc, char** argv) {
    // usage: add_monitor RECTANGLE TAG [PADUP [PADRIGHT [PADDOWN [PADLEFT]]]]
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
    if (argc > 3 && argv[3][0] != '\0') monitor->pad_up       = atoi(argv[3]);
    if (argc > 4 && argv[4][0] != '\0') monitor->pad_right    = atoi(argv[4]);
    if (argc > 5 && argv[5][0] != '\0') monitor->pad_down     = atoi(argv[5]);
    if (argc > 6 && argv[6][0] != '\0') monitor->pad_left     = atoi(argv[6]);
    frame_show_recursive(tag->frame);
    monitor_apply_layout(monitor);
    emit_tag_changed(tag, g_monitors->len - 1);
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
    if (g_cur_monitor >= g_monitors->len) {
        g_cur_monitor--;
        // if selection has changed, then relayout focused monitor
        monitor_apply_layout(get_current_monitor());
    }
    return 0;
}

int move_monitor_command(int argc, char** argv) {
    // usage: move_monitor INDEX RECT [PADUP [PADRIGHT [PADDOWN [PADLEFT]]]]
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
    if (argc > 3 && argv[3][0] != '\0') monitor->pad_up       = atoi(argv[3]);
    if (argc > 4 && argv[4][0] != '\0') monitor->pad_right    = atoi(argv[4]);
    if (argc > 5 && argv[5][0] != '\0') monitor->pad_down     = atoi(argv[5]);
    if (argc > 6 && argv[6][0] != '\0') monitor->pad_left     = atoi(argv[6]);
    monitor_apply_layout(monitor);
    return 0;
}

int monitor_rect_command(int argc, char** argv, GString** result) {
    // usage: monitor_rect [[-p] INDEX]
    char* index_str = NULL;
    HSMonitor* m = NULL;
    bool with_pad = false;

    // if index is supplied
    if (argc > 1) {
        index_str = argv[1];
    }
    // if -p is supplied
    if (argc > 2) {
        index_str = argv[2];
        if (!strcmp("-p", argv[1])) {
            with_pad = true;
        } else {
            fprintf(stderr, "monitor_rect_command: invalid argument \"%s\"\n",
                    argv[1]);
            return HERBST_INVALID_ARGUMENT;
        }
    }
    // if an index is set
    if (index_str) {
        int index;
        if (1 == sscanf(index_str, "%d", &index)) {
            m = monitor_with_index(index);
            if (!m) {
                fprintf(stderr,"monitor_rect_command: invalid index \"%s\"\n",
                        index_str);
                return HERBST_INVALID_ARGUMENT;
            }
        }
    }

    if (!m) {
        m = get_current_monitor();
    }
    XRectangle rect = m->rect;
    if (with_pad) {
        rect.x += m->pad_left;
        rect.width -= m->pad_left + m->pad_right;
        rect.y += m->pad_up;
        rect.height -= m->pad_up + m->pad_down;
    }
    g_string_printf(*result, "%d %d %d %d",
                    rect.x, rect.y, rect.width, rect.height);
    return 0;
}

int monitor_set_pad_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    int index = atoi(argv[1]);
    if (index < 0 || index >= g_monitors->len) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, index);
    if (argc > 2 && argv[2][0] != '\0') monitor->pad_up       = atoi(argv[2]);
    if (argc > 3 && argv[3][0] != '\0') monitor->pad_right    = atoi(argv[3]);
    if (argc > 4 && argv[4][0] != '\0') monitor->pad_down     = atoi(argv[4]);
    if (argc > 5 && argv[5][0] != '\0') monitor->pad_left     = atoi(argv[5]);
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
    tag->floating = false;
    g_array_append_val(g_tags, tag);
    tag_set_flags_dirty();
    return tag;
}

int tag_add_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = add_tag(argv[1]);
    hook_emit_list("tag_added", tag->name->str, NULL);
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
    hook_emit_list("tag_renamed", tag->name->str, NULL);
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
        HSClient* client = get_client_from_window(buf[i]);
        client->tag = target;
        frame_insert_window(target->frame, buf[i]);
    }
    if (monitor_target) {
        // if target monitor is viewed, then show windows
        monitor_apply_layout(monitor_target);
        for (i = 0; i < count; i++) {
            window_show(buf[i]);
        }
    }
    g_free(buf);
    // remove tag
    char* oldname = g_strdup(tag->name->str);
    tag_free(tag);
    for (i = 0; i < g_tags->len; i++) {
        if (g_array_index(g_tags, HSTag*, i) == tag) {
            g_array_remove_index(g_tags, i);
            break;
        }
    }
    tag_set_flags_dirty();
    hook_emit_list("tag_removed", oldname, target->name->str, NULL);
    g_free(oldname);
    return 0;
}

int tag_set_floating_command(int argc, char** argv, GString** result) {
    // usage: floating [[tag] on|off|toggle]
    HSTag* tag = get_current_monitor()->tag;
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    char* action = argv[1];
    if (argc >= 3) {
        // if a tag is specified
        tag = find_tag(argv[1]);
        action = argv[2];
        if (!tag) {
            return HERBST_INVALID_ARGUMENT;
        }
    }

    bool new_value = false;
    if (!strcmp(action, "toggle"))      new_value = ! tag->floating;
    else if (!strcmp(action, "on"))     new_value = true;
    else if (!strcmp(action, "off"))    new_value = false;

    if (!strcmp(action, "status")) {
        // just print status
        *result = g_string_assign(*result, tag->floating ? "on" : "off");
    } else {
        // asign new value and rearrange if needed
        tag->floating = new_value;

        HSMonitor* m = find_monitor_with_tag(tag);
        HSDebug("setting tag:%s->floating to %s\n", tag->name->str, tag->floating ? "on" : "off");
        if (m != NULL) {
            monitor_apply_layout(m);
        }
    }
    return 0;
}

static void client_update_tag_flags(void* key, void* client_void, void* data) {
    (void) key;
    (void) data;
    HSClient* client = client_void;
    if (client) {
        TAG_SET_FLAG(client->tag, TAG_FLAG_USED);
        if (client->urgent) {
            TAG_SET_FLAG(client->tag, TAG_FLAG_URGENT);
        }
    }
}

void tag_force_update_flags() {
    g_tag_flags_dirty = false;
    // unset all tags
    for (int i = 0; i < g_tags->len; i++) {
        g_array_index(g_tags, HSTag*, i)->flags = 0;
    }
    // update flags
    clientlist_foreach(client_update_tag_flags, NULL);
}

void tag_update_flags() {
    if (g_tag_flags_dirty) {
        tag_force_update_flags();
    }
}

void tag_set_flags_dirty() {
    g_tag_flags_dirty = true;
    hook_emit_list("tag_flags", NULL);
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

int cycle_all_command(int argc, char** argv) {
    int delta = 1;
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    delta = CLAMP(delta, -1, 1); // only delta -1, 0, 1 is allowed
    if (delta == 0) {
        // nothing to do
        return 0;
    }
    // find current selection
    HSFrame* frame = frame_current_selection();
    int index = frame->content.clients.selection;
    bool change_frame = false;
    int direction;
    int other_direction;
    int new_window_index;
    if (delta > 0 && (index + 1) >= frame->content.clients.count) {
        // change selection from 0 to 1
        direction = 1;
        other_direction = 0;
        change_frame = true;
        new_window_index = 0; // focus first window in in a frame
    }
    if (delta < 0 && index == 0) {
        // change selection from 1 to 0
        direction = 0;
        other_direction = 1;
        change_frame = true;
        new_window_index = -1; // focus last window in in a frame
    }
    if (change_frame) {
        HSFrame* top_frame;
        //   these things can be visualized easily for direction = 1
        /*
         *         .
         *        / \
         *       .   \
         *      / \  ...
         *     /   \
         *    .     .
         *   / \   / \
         *  .   * .   .
         *   the star shows the current focus
         */
        // go to next frame in tree
        // find first frame, where we can change the selection from 0 to 1
        // i.e. from other_direction to direction we want to use
        while (frame->parent && frame->parent->content.layout.selection == direction) {
            frame = frame->parent;
        }
        /*
         *         .
         *        / \
         *       .   \
         *      / \  ...
         *     /   \
         *    *     .
         *   / \   / \
         *  .   . .   .
         */
        if (frame->parent) {
            // go to the top
            frame = frame->parent;
        } else {
            // if we reached the top, do nothing..
        }
        top_frame = frame;
        /* \       .
         *  \     / \
         *   `-> *   \
         *      / \  ...
         *     /   \
         *    .     .
         *   / \   / \
         *  .   . .   .
         */
        // go one step to the right (i.e. in desired direction
        if (frame->type == TYPE_FRAMES) {
            int oldselection = frame->content.layout.selection;
            if (oldselection == direction) {
                // if we already reached the end,
                // i.e. if we cannot go in the disired direction
                // then wrap around
                frame->content.layout.selection = other_direction;
                frame = frame->content.layout.a;
            } else {
                frame->content.layout.selection = direction;
                frame = frame->content.layout.b;
            }
        }
        /*
         *         .
         *        / \
         *       .   \
         *      / \  ...
         *     /   \
         *    .     *
         *   / \   / \
         *  .   . .   .
         */
        // and then to the left (i.e. find first leave
        while (frame->type == TYPE_FRAMES) {
            // then go deeper, with the other direction
            frame->content.layout.selection = other_direction;
            frame = frame->content.layout.a;
        }
        /*         .
         *        / \
         *       .   \
         *      / \  ...
         *     /   \
         *    .     .
         *   / \   / \
         *  .   . *   .
         */
        // now we reached the next client containing frame

        if (frame->content.clients.count > 0) {
            frame->content.clients.selection = new_window_index;
            // ensure it is a valid index
            frame->content.clients.selection += frame->content.clients.count;
            frame->content.clients.selection %= frame->content.clients.count;
        }

        // reset focus and g_cur_frame
        // all changes were made below top_frame
        frame_focus_recursive(top_frame);

    } else {
        // only change the selection within one frame
        index += delta;
        // ensure it is a valid index
        index %= frame->content.clients.count;
        index += frame->content.clients.count;
        index %= frame->content.clients.count;
        frame->content.clients.selection = index;
    }
    monitor_apply_layout(get_current_monitor());
    return 0;
}


// counts the splits of a kind align to root window
int frame_split_count_to_root(HSFrame* frame, int align) {
    int height = 0;
    // count steps until root node
    // root node has no parent
    while (frame->parent) {
        frame = frame->parent;
        if (frame->content.layout.align == align) {
            height++;
        }
    }
    return height;
}

void frame_split(HSFrame* frame, int align, int fraction) {
    if (frame_split_count_to_root(frame, align) > HERBST_MAX_TREE_HEIGHT) {
        // do nothing if tree would be to large
        return;
    }
    // ensure fraction is allowed
    fraction = CLAMP(fraction,
                     FRACTION_UNIT * (0.0 + FRAME_MIN_FRACTION),
                     FRACTION_UNIT * (1.0 - FRAME_MIN_FRACTION));

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
    // usage: split h|v FRACTION
    if (argc < 3) {
        return HERBST_INVALID_ARGUMENT;
    }
    int align = ALIGN_VERTICAL;
    if (argv[1][0] == 'h') {
        align = ALIGN_HORIZONTAL;
    } // else: layout ist vertical
    int fraction = FRACTION_UNIT* CLAMP(atof(argv[2]),
                                        0.0 + FRAME_MIN_FRACTION,
                                        1.0 - FRAME_MIN_FRACTION);
    HSFrame* frame = frame_current_selection();
    if (!frame) return 0; // nothing to do
    frame_split(frame, align, fraction);
    return 0;
}

int frame_change_fraction_command(int argc, char** argv) {
    // usage: fraction DIRECTION DELTA
    if (argc < 3) {
        return HERBST_INVALID_ARGUMENT;
    }
    char direction = argv[1][0];
    double delta_double = atof(argv[2]);
    delta_double = CLAMP(delta_double, -1.0, 1.0);
    int delta = FRACTION_UNIT * delta_double;
    // if direction is left or up we have to flip delta
    // because e.g. resize up by 0.1 actually means:
    // reduce fraction by 0.1, i.e. delta = -0.1
    switch (direction) {
        case 'l':   delta *= -1; break;
        case 'r':   break;
        case 'u':   delta *= -1; break;
        case 'd':   break;
        default:    return HERBST_INVALID_ARGUMENT;
    }
    HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
    if (!neighbour) {
        // then try opposite direction
        switch (direction) {
            case 'l':   direction = 'r'; break;
            case 'r':   direction = 'l'; break;
            case 'u':   direction = 'd'; break;
            case 'd':   direction = 'u'; break;
            default:    assert(false); break;
        }
        neighbour = frame_neighbour(g_cur_frame, direction);
        if (!neighbour) {
            // nothing to do
            return 0;
        }
    }
    HSFrame* parent = neighbour->parent;
    assert(parent != NULL); // if has neighbour, it also must have a parent
    assert(parent->type == TYPE_FRAMES);
    int fraction = parent->content.layout.fraction;
    fraction += delta;
    fraction = CLAMP(fraction, (int)(FRAME_MIN_FRACTION * FRACTION_UNIT), (int)((1.0 - FRAME_MIN_FRACTION) * FRACTION_UNIT));
    parent->content.layout.fraction = fraction;
    // arrange monitor
    monitor_apply_layout(get_current_monitor());
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
                if (layout->align == ALIGN_HORIZONTAL
                    && layout->a == frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'l':
                if (layout->align == ALIGN_HORIZONTAL
                    && layout->b == frame) {
                    found = true;
                    other = layout->a;
                }
                break;
            case 'd':
                if (layout->align == ALIGN_VERTICAL
                    && layout->a == frame) {
                    found = true;
                    other = layout->b;
                }
                break;
            case 'u':
                if (layout->align == ALIGN_VERTICAL
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

// finds a neighbour within frame in the specified direction
// returns its index or -1 if there is none
int frame_inner_neighbour_index(HSFrame* frame, char direction) {
    int index = -1;
    if (frame->type != TYPE_CLIENTS) {
        fprintf(stderr, "warning: frame has invalid type\n");
        return -1;
    }
    int selection = frame->content.clients.selection;
    int count = frame->content.clients.count;
    int rows, cols;
    switch (frame->content.clients.layout) {
        case LAYOUT_VERTICAL:
            if (direction == 'd') index = selection + 1;
            if (direction == 'u') index = selection - 1;
            break;
        case LAYOUT_HORIZONTAL:
            if (direction == 'r') index = selection + 1;
            if (direction == 'l') index = selection - 1;
            break;
        case LAYOUT_MAX:
            break;
        case LAYOUT_GRID:
            frame_layout_grid_get_size(count, &rows, &cols);
            if (cols == 0) break;
            int r = selection / cols;
            int c = selection % cols;
            switch (direction) {
                case 'd':
                    index = selection + cols;
                    if (*g_gapless_grid && index >= count && r == (rows - 2)) {
                        // if grid is gapless and we're in the second-last row
                        // then it means last client is below us
                        index = count - 1;
                    }
                    break;
                case 'u': index = selection - cols; break;
                case 'r': if (c < cols-1) index = selection + 1; break;
                case 'l': if (c > 0)      index = selection - 1; break;
            }
            break;
        default:
            break;
    }
    // check that index is valid
    if (index < 0 || index >= count) {
        index = -1;
    }
    return index;
}

int frame_focus_command(int argc, char** argv) {
    // usage: focus [-e|-i] left|right|up|down
    if (argc < 2) return HERBST_INVALID_ARGUMENT;
    if (!g_cur_frame) {
        fprintf(stderr, "warning: no frame is selected\n");
        return HERBST_UNKNOWN_ERROR;
    }
    int external_only = *g_direction_external_only;
    char direction = argv[1][0];
    if (argc > 2 && !strcmp(argv[1], "-i")) {
        external_only = false;
        direction = argv[2][0];
    }
    if (argc > 2 && !strcmp(argv[1], "-e")) {
        external_only = true;
        direction = argv[2][0];
    }
    //HSFrame* frame = g_cur_frame;
    int index;
    if (!external_only &&
        (index = frame_inner_neighbour_index(g_cur_frame, direction)) != -1) {
        g_cur_frame->content.clients.selection = index;
        frame_focus_recursive(g_cur_frame);
        monitor_apply_layout(get_current_monitor());
    } else {
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
    int external_only = *g_direction_external_only;
    if (argc > 2 && !strcmp(argv[1], "-i")) {
        external_only = false;
        direction = argv[2][0];
    }
    if (argc > 2 && !strcmp(argv[1], "-e")) {
        external_only = true;
        direction = argv[2][0];
    }
    int index;
    if (!external_only &&
        (index = frame_inner_neighbour_index(g_cur_frame, direction)) != -1) {
        int selection = g_cur_frame->content.clients.selection;
        Window* buf = g_cur_frame->content.clients.buf;
        // if internal neighbour was found, then swap
        Window tmp = buf[selection];
        buf[selection] = buf[index];
        buf[index] = tmp;

        if (*g_focus_follows_shift) {
            g_cur_frame->content.clients.selection = index;
        }
        frame_focus_recursive(g_cur_frame);
        monitor_apply_layout(get_current_monitor());
    } else {
        HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
        Window win = frame_focused_window(g_cur_frame);
        if (win && neighbour != NULL) { // if neighbour was found
            // move window to neighbour
            frame_remove_window(g_cur_frame, win);
            frame_insert_window(neighbour, win);
            if (*g_focus_follows_shift) {
                // change selection in parrent
                HSFrame* parent = neighbour->parent;
                assert(parent);
                parent->content.layout.selection = ! parent->content.layout.selection;
                frame_focus_recursive(parent);
                // focus right window in frame
                HSFrame* frame = g_cur_frame;
                assert(frame);
                int i;
                Window* buf = frame->content.clients.buf;
                size_t count = frame->content.clients.count;
                for (i = 0; i < count; i++) {
                    if (buf[i] == win) {
                        frame->content.clients.selection = i;
                        window_focus(buf[i]);
                        break;
                    }
                }
            } else {
                frame_focus_recursive(g_cur_frame);
            }
            // layout was changed, so update it
            monitor_apply_layout(get_current_monitor());
        }
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

// try to focus window in frame
// returns true if win was found and focused, else returns false
bool frame_focus_window(HSFrame* frame, Window win) {
    if (!frame) {
        return false;
    }
    if (frame->type == TYPE_CLIENTS) {
        int i;
        size_t count = frame->content.clients.count;
        Window* buf = frame->content.clients.buf;
        // search for win in buf
        for (i = 0; i < count; i++) {
            if (buf[i] == win) {
                // if found, set focus to it
                frame->content.clients.selection = i;
                return true;
            }
        }
        return false;
    } else {
        // type == TYPE_FRAMES
        // search in subframes
        bool found = frame_focus_window(frame->content.layout.a, win);
        if (found) {
            // set selection to first frame
            frame->content.layout.selection = 0;
            return true;
        }
        found = frame_focus_window(frame->content.layout.b, win);
        if (found) {
            // set selection to second frame
            frame->content.layout.selection = 1;
            return true;
        }
        return false;
    }
}

// focus a window
// switch_tag tells, wether to switch tag to focus to window
// switch_monitor tells, wether to switch monitor to focus to window
// returns if window was focused or not
bool focus_window(Window win, bool switch_tag, bool switch_monitor) {
    HSClient* client = get_client_from_window(win);
    if (!client) {
        // client is not managed
        return false;
    }
    HSTag* tag = client->tag;
    assert(client->tag);
    HSMonitor* monitor = find_monitor_with_tag(tag);
    HSMonitor* cur_mon = get_current_monitor();
    if (monitor != cur_mon && !switch_monitor) {
        // if we are not allowed to switch tag
        // and tag is not on current monitor (or on no monitor)
        // than we cannot focus the window
        return false;
    }
    if (monitor == NULL && !switch_tag) {
        return false;
    }
    if (monitor != cur_mon && monitor != NULL) {
        if (!switch_monitor) {
            return false;
        } else {
            // switch monitor
            monitor_focus_by_index(monitor_index_of(monitor));
            cur_mon = get_current_monitor();
            assert(cur_mon == monitor);
        }
    }
    monitor_set_tag(cur_mon, tag);
    cur_mon = get_current_monitor();
    if (cur_mon->tag != tag) {
        // could not set tag on monitor
        return false;
    }
    // now the right tag is visible
    // now focus it
    bool found = frame_focus_window(tag->frame, win);
    frame_focus_recursive(tag->frame);
    monitor_apply_layout(cur_mon);
    return found;
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
        window_unfocus_last();
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
            window_hide(buf[i]);
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
            window_show(buf[i]);
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
        window_show(frame->window);
    } else {
        window_hide(frame->window);
    }
    frame->window_visible = visible;
}

// executes action for each client within frame and its subframes
// if action fails (i.e. returns something != 0), then it abborts with this code
int frame_foreach_client(HSFrame* frame, ClientAction action, void* data) {
    int status;
    if (frame->type == TYPE_FRAMES) {
        status = frame_foreach_client(frame->content.layout.a, action, data);
        if (0 != status) {
            return status;
        }
        status = frame_foreach_client(frame->content.layout.b, action, data);
        if (0 != status) {
            return status;
        }
    } else {
        // frame->type == TYPE_CLIENTS
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        HSClient* client;
        for (int i = 0; i < count; i++) {
            client = get_client_from_window(buf[i]);
            // do action for each client
            status = action(client, data);
            if (0 != status) {
                return status;
            }
        }
    }
    return 0;
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
    if (monitor == other) {
        // nothing to do
        return;
    }
    if (other != NULL) {
        if (*g_swap_monitors_to_get_tag) {
            // swap tags
            other->tag = monitor->tag;
            monitor->tag = tag;
            // reset focus
            frame_focus_recursive(tag->frame);
            monitor_apply_layout(other);
            monitor_apply_layout(monitor);
            emit_tag_changed(other->tag, other - (HSMonitor*)g_monitors->data);
            emit_tag_changed(tag, g_cur_monitor);
        }
        return;
    }
    HSTag* old_tag = monitor->tag;
    // 1. hide old tag
    frame_hide_recursive(old_tag->frame);
    // 2. show new tag
    monitor->tag = tag;
    // first reset focus and arrange windows
    frame_focus_recursive(tag->frame);
    monitor_apply_layout(monitor);
    // then show them (should reduce flicker)
    frame_show_recursive(tag->frame);
    // focus window just has been shown
    // focus again to give input focus
    frame_focus_recursive(tag->frame);
    emit_tag_changed(tag, g_cur_monitor);
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
    HSClient* client = get_client_from_window(window);
    assert(client != NULL);
    client->tag = target;
    // refresh things
    if (monitor && !monitor_target) {
        // window is moved to unvisible tag
        // so hide it
        window_hide(window);
    }
    frame_focus_recursive(frame);
    monitor_apply_layout(monitor);
    if (monitor_target) {
        monitor_apply_layout(monitor_target);
    }
    tag_set_flags_dirty();
    return 0;
}

int monitor_focus_command(int argc, char** argv) {
    if (argc < 2) {
        return HERBST_INVALID_ARGUMENT;
    }
    int new_selection = atoi(argv[1]);
    // really change selection
    monitor_focus_by_index(new_selection);
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
    monitor_focus_by_index(new_selection);
    return 0;
}

int monitor_index_of(HSMonitor* monitor) {
    return monitor - (HSMonitor*)g_monitors->data;
}

void monitor_focus_by_index(int new_selection) {
    new_selection = CLAMP(new_selection, 0, g_monitors->len - 1);
    HSMonitor* old = &g_array_index(g_monitors, HSMonitor, g_cur_monitor);
    HSMonitor* monitor = &g_array_index(g_monitors, HSMonitor, new_selection);
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
        new_x = monitor->rect.x + monitor->mouse.x;
        new_y = monitor->rect.y + monitor->mouse.y;
        XWarpPointer(g_display, None, g_root, 0, 0, 0, 0, new_x, new_y);
    }
    // emit hooks
    emit_tag_changed(monitor->tag, new_selection);
}

int monitor_get_relative_x(HSMonitor* m, int x_root) {
    return x_root - m->rect.x - m->pad_left;
}

int monitor_get_relative_y(HSMonitor* m, int y_root) {
    return y_root - m->rect.y - m->pad_up;
}

HSMonitor* monitor_with_coordinate(int x, int y) {
    int i;
    for (i = 0; i < g_monitors->len; i++) {
        HSMonitor* m = &g_array_index(g_monitors, HSMonitor, i);
        if (m->rect.x + m->pad_left <= x
            && m->rect.x + m->rect.width - m->pad_right > x
            && m->rect.y + m->pad_up <= y
            && m->rect.y + m->rect.height - m->pad_down > y) {
            return m;
        }
    }
    return NULL;
}

HSMonitor* monitor_with_index(int index) {
    if (index < 0 || index >= g_monitors->len) {
        return NULL;
    }
    return &g_array_index(g_monitors, HSMonitor, index);
}

