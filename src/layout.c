/** Copyright 2011-2012 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */


#include "clientlist.h"
#include "globals.h"
#include "utils.h"
#include "hook.h"
#include "ewmh.h"
#include "ipc-protocol.h"
#include "settings.h"
#include "layout.h"
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

int* g_frame_border_width;
int* g_always_show_frame;
int* g_default_frame_layout;
int* g_focus_follows_shift;
int* g_frame_bg_transparent;
int* g_direction_external_only;
int* g_gapless_grid;
int* g_smart_frame_surroundings;
unsigned long g_frame_border_active_color;
unsigned long g_frame_border_normal_color;
unsigned long g_frame_bg_active_color;
unsigned long g_frame_bg_normal_color;
unsigned long g_frame_active_opacity;
unsigned long g_frame_normal_opacity;
char*   g_tree_style = NULL;

char* g_align_names[] = {
    "vertical",
    "horizontal",
};

char* g_layout_names[] = {
    "vertical",
    "horizontal",
    "max",
    "grid",
    NULL,
};

static void fetch_frame_colors() {
    // load settings
    g_window_gap = &(settings_find("window_gap")->value.i);
    g_focus_follows_shift = &(settings_find("focus_follows_shift")->value.i);
    g_frame_border_width = &(settings_find("frame_border_width")->value.i);
    g_always_show_frame = &(settings_find("always_show_frame")->value.i);
    g_frame_bg_transparent = &(settings_find("frame_bg_transparent")->value.i);
    g_default_frame_layout = &(settings_find("default_frame_layout")->value.i);
    g_direction_external_only = &(settings_find("default_direction_external_only")->value.i);
    g_gapless_grid = &(settings_find("gapless_grid")->value.i);
    g_smart_frame_surroundings = &(settings_find("smart_frame_surroundings")->value.i);
    *g_default_frame_layout = CLAMP(*g_default_frame_layout, 0, LAYOUT_COUNT - 1);
    char* str = settings_find("frame_border_normal_color")->value.s;
    g_frame_border_normal_color = getcolor(str);
    str = settings_find("frame_border_active_color")->value.s;
    g_frame_border_active_color = getcolor(str);
    // background color
    str = settings_find("frame_bg_normal_color")->value.s;
    g_frame_bg_normal_color = getcolor(str);
    str = settings_find("frame_bg_active_color")->value.s;
    g_frame_bg_active_color = getcolor(str);
    g_frame_active_opacity = CLAMP(settings_find("frame_active_opacity")->value.i, 0, 100);
    g_frame_normal_opacity = CLAMP(settings_find("frame_normal_opacity")->value.i, 0, 100);

    // tree style
    g_tree_style = settings_find("tree_style")->value.s;
    if (g_utf8_strlen(g_tree_style, -1) < 8) {
        g_warning("too few characters in setting tree_style\n");
        // ensure that it is long enough
        char* argv[] = { "set", "tree_style", "01234567" };
        settings_set_command(LENGTH(argv), argv);
    }
}

void layout_init() {
    fetch_frame_colors();
}
void reset_frame_colors() {
    fetch_frame_colors();
    all_monitors_apply_layout();
}

void layout_destroy() {
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
        // insert it after the selection
        int index = frame->content.clients.selection + 1;
        index = CLAMP(index, 0, count - 1);
        buf = g_renew(Window, buf, count);
        // shift other windows to the back to insert the new one at index
        memmove(buf + index + 1, buf + index, sizeof(*buf) * (count - index - 1));
        buf[index] = window;
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

void frame_insert_window_at_index(HSFrame* frame, Window window, char* index) {
    if (!index || index[0] == '\0') {
        frame_insert_window(frame, window);
    }
    else if (frame->type == TYPE_CLIENTS) {
        frame_insert_window(frame, window);
    } else { /* frame->type == TYPE_FRAMES */
        HSLayout* layout = &frame->content.layout;
        HSFrame* frame;
        switch (index[0]) {
            case '0': frame = layout->a; break;
            case '1': frame = layout->b; break;
            /* opposite selection */
            case '/': frame = (layout->selection == 0)
                                ?  layout->b
                                :  layout->a; break;
            /* else just follow selection */
            case '.':
            default:  frame = (layout->selection == 0)
                                ?  layout->a
                                : layout->b; break;
        }
        frame_insert_window_at_index(frame, window, index + 1);
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
                selection -= (selection < i) ? 0 : 1;
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
            ewmh_window_update_tag(client->window, client->tag);

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
        if (!g_layout_names[i]) {
            break;
        }
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

int frame_current_set_client_layout(int argc, char** argv) {
    int layout = 0;
    if (argc <= 1) {
        fprintf(stderr, "set_layout: not enough arguments\n");
        return HERBST_INVALID_ARGUMENT;
    }
    layout = find_layout_by_name(argv[1]);
    if (layout < 0) {
        HSDebug("set_layout: invalid layout name \"%s\"\n", argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    if (g_cur_frame && g_cur_frame->type == TYPE_CLIENTS) {
        g_cur_frame->content.clients.layout = layout;
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
        client_resize_tiling(client, cur, frame);
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
        client_resize_tiling(client, rect, frame);
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
            client_resize_tiling(client, cur, frame);
            cur.x += width;
            i++;
        }
        cur.y += height;
    }

}

void frame_apply_layout(HSFrame* frame, XRectangle rect) {
    if (frame->type == TYPE_CLIENTS) {
        size_t count = frame->content.clients.count;
        if (!*g_smart_frame_surroundings || frame->parent) {
            // frame only -> apply window_gap
            rect.height -= *g_window_gap;
            rect.width -= *g_window_gap;
            // apply frame width
            rect.x += *g_frame_border_width;
            rect.y += *g_frame_border_width;
            rect.height -= *g_frame_border_width * 2;
            rect.width -= *g_frame_border_width * 2;
        }
        if (rect.width <= WINDOW_MIN_WIDTH || rect.height <= WINDOW_MIN_HEIGHT) {
            // do nothing on invalid size
            return;
        }
        unsigned long border_color = g_frame_border_normal_color;
        unsigned long bg_color = g_frame_bg_normal_color;
        if (!*g_smart_frame_surroundings || frame->parent) {
            XSetWindowBorderWidth(g_display, frame->window, *g_frame_border_width);
            // set indicator frame
            if (g_cur_frame == frame) {
                border_color = g_frame_border_active_color;
                bg_color = g_frame_bg_active_color;
            }
            XSetWindowBorder(g_display, frame->window, border_color);
            XMoveResizeWindow(g_display, frame->window,
                              rect.x - *g_frame_border_width,
                              rect.y - *g_frame_border_width,
                              rect.width, rect.height);
        } else {
            XSetWindowBorderWidth(g_display, frame->window, 0);
            XMoveResizeWindow(g_display, frame->window, rect.x, rect.y, rect.width, rect.height);
        }

        if (*g_frame_bg_transparent) {
            XSetWindowBackgroundPixmap(g_display, frame->window, ParentRelative);
        } else {
            XSetWindowBackground(g_display, frame->window, bg_color);
        }
        if (g_cur_frame == frame) {
            ewmh_set_window_opacity(frame->window, g_frame_active_opacity/100.0);
        } else {
            ewmh_set_window_opacity(frame->window, g_frame_normal_opacity/100.0);
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
    // use an integer variable to avoid strange happenings when computing
    //       (-1) % (size_t)6
    int count = (int) frame->content.clients.count;
    index += delta;
    index %= count;
    index += count;
    index %= count;
    frame->content.clients.selection = index;
    Window window = frame->content.clients.buf[index];
    window_focus(window);
    return 0;
}

int cycle_all_command(int argc, char** argv) {
    int delta = 1;
    int skip_invisible = false;
    if (argc >= 2) {
        if (!strcmp(argv[1], "--skip-invisible")) {
            skip_invisible = true;
            SHIFT(argc, argv);
        }
    }
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    delta = CLAMP(delta, -1, 1); // only delta -1, 0, 1 is allowed
    printf("skpi = %d, delta= %d\n", delta, skip_invisible);
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
    int new_window_index; // tells where to start in a new frame
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
    if (skip_invisible && frame->content.clients.layout == LAYOUT_MAX) {
        direction = (delta > 0) ? 1 : 0;
        other_direction = 1 - direction;
        change_frame = true;
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
            if (!skip_invisible
                || frame->content.clients.layout != LAYOUT_MAX)
            {
                frame->content.clients.selection = new_window_index;
                // ensure it is a valid index
                size_t count = frame->content.clients.count;
                frame->content.clients.selection += count;
                frame->content.clients.selection %= count;
            }
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
    // reset focus
    g_cur_frame = frame_current_selection();
    // redraw monitor
    monitor_apply_layout(get_current_monitor());
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

static void frame_rotate(HSFrame* frame) {
    if (frame && frame->type == TYPE_FRAMES) {
        HSLayout* l = &frame->content.layout;
        switch (l->align) {
            case ALIGN_VERTICAL:
                l->align = ALIGN_HORIZONTAL;
                break;
            case ALIGN_HORIZONTAL:
                l->align = ALIGN_VERTICAL;
                l->selection = l->selection ? 0 : 1;
                HSFrame* temp = l->a;
                l->a = l->b;
                l->b = temp;
                l->fraction = FRACTION_UNIT - l->fraction;
                break;
        }
    }
}

int layout_rotate_command() {
    frame_do_recursive(get_current_monitor()->tag->frame, frame_rotate, -1);
    monitor_apply_layout(get_current_monitor());
    return 0;
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

int close_or_remove_command(int argc, char** argv) {
    if (frame_focused_window(g_cur_frame)) {
        return window_close_current();
    } else {
        return frame_remove_command(argc, argv);
    }
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


