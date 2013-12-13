/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
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
#include "stack.h"
#include "monitor.h"

#include "glib-backports.h"
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
int* g_frame_border_inner_width;
int* g_always_show_frame;
int* g_default_frame_layout;
int* g_frame_bg_transparent;
int* g_direction_external_only;
int* g_gapless_grid;
int* g_smart_frame_surroundings;
int* g_smart_window_surroundings;
int* g_frame_padding;
unsigned long g_frame_border_active_color;
unsigned long g_frame_border_normal_color;
unsigned long g_frame_border_inner_color;
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
    g_frame_gap = &(settings_find("frame_gap")->value.i);
    g_frame_padding = &(settings_find("frame_padding")->value.i);
    g_window_gap = &(settings_find("window_gap")->value.i);
    g_frame_border_width = &(settings_find("frame_border_width")->value.i);
    g_frame_border_inner_width = &(settings_find("frame_border_inner_width")->value.i);
    g_always_show_frame = &(settings_find("always_show_frame")->value.i);
    g_frame_bg_transparent = &(settings_find("frame_bg_transparent")->value.i);
    g_default_frame_layout = &(settings_find("default_frame_layout")->value.i);
    g_direction_external_only = &(settings_find("default_direction_external_only")->value.i);
    g_gapless_grid = &(settings_find("gapless_grid")->value.i);
    g_smart_frame_surroundings = &(settings_find("smart_frame_surroundings")->value.i);
    g_smart_window_surroundings = &(settings_find("smart_window_surroundings")->value.i);
    *g_default_frame_layout = CLAMP(*g_default_frame_layout, 0, LAYOUT_COUNT - 1);
    char* str = settings_find("frame_border_normal_color")->value.s;
    g_frame_border_normal_color = getcolor(str);
    str = settings_find("frame_border_active_color")->value.s;
    g_frame_border_active_color = getcolor(str);
    str = settings_find("frame_border_inner_color")->value.s;
    g_frame_border_inner_color = getcolor(str);
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
        settings_set_command(LENGTH(argv), argv, NULL);
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


/* create a new frame
 * you can either specify a frame or a tag as its parent
 */
HSFrame* frame_create_empty(HSFrame* parent, HSTag* parenttag) {
    HSFrame* frame = g_new0(HSFrame, 1);
    frame->type = TYPE_CLIENTS;
    frame->window_visible = false;
    frame->content.clients.layout = *g_default_frame_layout;
    frame->parent = parent;
    frame->tag = parent ? parent->tag : parenttag;
    // set window attributes
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
    // insert it to the stack
    frame->slice = slice_create_frame(frame->window);
    stack_insert_slice(frame->tag->stack, frame->slice);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_FRAME_CLASS;
    hint->res_class = HERBST_FRAME_CLASS;
    XSetClassHint(g_display, frame->window, hint);
    XFree(hint);
    return frame;
}

void frame_insert_client(HSFrame* frame, struct HSClient* client) {
    if (frame->type == TYPE_CLIENTS) {
        // insert it here
        HSClient** buf = frame->content.clients.buf;
        // append it to buf
        size_t count = frame->content.clients.count;
        count++;
        // insert it after the selection
        int index = frame->content.clients.selection + 1;
        index = CLAMP(index, 0, count - 1);
        buf = g_renew(HSClient*, buf, count);
        // shift other windows to the back to insert the new one at index
        memmove(buf + index + 1, buf + index, sizeof(*buf) * (count - index - 1));
        buf[index] = client;
        // write results back
        frame->content.clients.count = count;
        frame->content.clients.buf = buf;
        // check for focus
        if (g_cur_frame == frame
            && frame->content.clients.selection >= (count-1)) {
            frame->content.clients.selection = count - 1;
            window_focus(client->window);
        }
    } else { /* frame->type == TYPE_FRAMES */
        HSLayout* layout = &frame->content.layout;
        frame_insert_client((layout->selection == 0)? layout->a : layout->b, client);
    }
}

HSFrame* lookup_frame(HSFrame* root, char *index) {
    if (index == NULL || index[0] == '\0') return root;
    if (root->type == TYPE_CLIENTS) return root;

    HSFrame* new_root;
    char *new_index = index + 1;
    HSLayout* layout = &root->content.layout;

    switch (index[0]) {
        case '0': new_root = layout->a; break;
        case '1': new_root = layout->b; break;
        /* opposite selection */
        case '/': new_root = (layout->selection == 0)
                            ?  layout->b
                            :  layout->a;
                  break;
        /* else just follow selection */
        case '@': new_index = index;
        case '.':
        default:  new_root = (layout->selection == 0)
                            ?  layout->a
                            : layout->b; break;
    }
    return lookup_frame(new_root, new_index);
}

HSFrame* find_frame_with_client(HSFrame* frame, struct HSClient* client) {
    if (frame->type == TYPE_CLIENTS) {
        HSClient** buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (int i = 0; i < count; i++) {
            if (buf[i] == client) {
                return frame;
            }
        }
        return NULL;
    } else { /* frame->type == TYPE_FRAMES */
        HSFrame* found = find_frame_with_client(frame->content.layout.a, client);
        if (!found) {
            found = find_frame_with_client(frame->content.layout.b, client);
        }
        return found;
    }
}

bool frame_remove_client(HSFrame* frame, HSClient* client) {
    if (frame->type == TYPE_CLIENTS) {
        HSClient** buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        int i;
        for (i = 0; i < count; i++) {
            if (buf[i] == client) {
                // if window was found
                // them remove it
                memmove(buf+i, buf+i+1, sizeof(buf[0])*(count - i - 1));
                count--;
                buf = g_renew(HSClient*, buf, count);
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
        bool found = frame_remove_client(frame->content.layout.a, client);
        found = found || frame_remove_client(frame->content.layout.b, client);
        return found;
    }
}

void frame_destroy(HSFrame* frame, HSClient*** buf, size_t* count) {
    if (frame->type == TYPE_CLIENTS) {
        *buf = frame->content.clients.buf;
        *count = frame->content.clients.count;
    } else { /* frame->type == TYPE_FRAMES */
        size_t c1, c2;
        HSClient **buf1, **buf2;
        frame_destroy(frame->content.layout.a, &buf1, &c1);
        frame_destroy(frame->content.layout.b, &buf2, &c2);
        // append buf2 to buf1
        buf1 = g_renew(HSClient*, buf1, c1 + c2);
        memcpy(buf1+c1, buf2, sizeof(buf1[0]) * c2);
        // free unused things
        g_free(buf2);
        // return;
        *buf = buf1;
        *count = c1 + c2;
    }
    stack_remove_slice(frame->tag->stack, frame->slice);
    slice_destroy(frame->slice);
    // free other things
    XDestroyWindow(g_display, frame->window);
    g_free(frame);
}

void dump_frame_tree(HSFrame* frame, GString* output) {
    if (frame->type == TYPE_CLIENTS) {
        g_string_append_printf(output, "%cclients%c%s:%d",
            LAYOUT_DUMP_BRACKETS[0],
            LAYOUT_DUMP_WHITESPACES[0],
            g_layout_names[frame->content.clients.layout],
            frame->content.clients.selection);
        HSClient** buf = frame->content.clients.buf;
        size_t i, count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            g_string_append_printf(output, "%c0x%lx",
                LAYOUT_DUMP_WHITESPACES[0],
                buf[i]->window);
        }
        g_string_append_c(output, LAYOUT_DUMP_BRACKETS[1]);
    } else {
        /* type == TYPE_FRAMES */
        g_string_append_printf(output, "%csplit%c%s%c%lf%c%d%c",
            LAYOUT_DUMP_BRACKETS[0],
            LAYOUT_DUMP_WHITESPACES[0],
            g_align_names[frame->content.layout.align],
            LAYOUT_DUMP_SEPARATOR,
            ((double)frame->content.layout.fraction) / (double)FRACTION_UNIT,
            LAYOUT_DUMP_SEPARATOR,
            frame->content.layout.selection,
            LAYOUT_DUMP_WHITESPACES[0]);
        dump_frame_tree(frame->content.layout.a, output);
        g_string_append_c(output, LAYOUT_DUMP_WHITESPACES[0]);
        dump_frame_tree(frame->content.layout.b, output);
        g_string_append_c(output, LAYOUT_DUMP_BRACKETS[1]);
    }
}

char* load_frame_tree(HSFrame* frame, char* description, GString* errormsg) {
    // find next (
    description = strchr(description, LAYOUT_DUMP_BRACKETS[0]);
    if (!description) {
        g_string_append_printf(errormsg, "Missing %c\n",
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
        g_string_append_printf(errormsg, "Missing %c or arguments\n", LAYOUT_DUMP_BRACKETS[1]);
        return NULL;
    }
    description += strspn(description, LAYOUT_DUMP_WHITESPACES);
    if (!*description) {
        g_string_append_printf(errormsg, "Missing %c or arguments\n", LAYOUT_DUMP_BRACKETS[1]);
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
            g_string_append_printf(errormsg,
                "Can not parse frame args \"%s\"\n", args);
            return NULL;
        }
#undef SEP
        int align = find_align_by_name(align_name);
        g_free(align_name);
        if (align < 0) {
            g_string_append_printf(errormsg,
                "Invalid alignment name in args \"%s\"\n", args);
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
                g_string_append_printf(errormsg,
                    "Can not split frame\n");
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
            g_string_append_printf(errormsg,
                "Can not parse frame args \"%s\"\n", args);
            return NULL;
        }
#undef SEP
        int layout = find_layout_by_name(layout_name);
        g_free(layout_name);
        if (layout < 0) {
            g_string_append_printf(errormsg,
                "Can not parse layout from args \"%s\"\n", args);
            return NULL;
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
                g_string_append_printf(errormsg,
                    "Can not parse window id from \"%s\"\n", description);
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
            if (!frame_remove_client(client->tag->frame, client)) {
                g_warning("window %lx was not found on tag %s\n",
                    win, client->tag->name->str);
            }
            if (clientmonitor) {
                monitor_apply_layout(clientmonitor);
            }
            stack_remove_slice(client->tag->stack, client->slice);

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

            client->tag = tag;
            stack_insert_slice(client->tag->stack, client->slice);
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
        g_string_append_printf(errormsg, "warning: missing closing bracket %c\n", LAYOUT_DUMP_BRACKETS[1]);
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

static void frame_append_caption(HSTree tree, GString* output) {
    HSFrame* frame = (HSFrame*) tree;
    if (frame->type == TYPE_CLIENTS) {
        // list of clients
        g_string_append_printf(output, "%s:",
            g_layout_names[frame->content.clients.layout]);
        HSClient** buf = frame->content.clients.buf;
        size_t i, count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            g_string_append_printf(output, " 0x%lx", buf[i]->window);
        }
        if (g_cur_frame == frame) {
            g_string_append(output, " [FOCUS]");
        }
    } else {
        /* type == TYPE_FRAMES */
        g_string_append_printf(output, "%s %d%% selection=%d",
            g_layout_names[frame->content.layout.align],
            frame->content.layout.fraction * 100 / FRACTION_UNIT,
            frame->content.layout.selection);
    }
}

static size_t frame_child_count(HSTree tree) {
    HSFrame* frame = (HSFrame*) tree;
    return (frame->type == TYPE_CLIENTS) ? 0 : 2;
}

static HSTreeInterface frame_nth_child(HSTree tree, size_t idx) {
    HSFrame* frame = (HSFrame*) tree;
    assert(frame->type != TYPE_CLIENTS);
    HSTreeInterface intf = {
        .nth_child  = frame_nth_child,
        .data       = (idx == 0)
                        ? frame->content.layout.a
                        : frame->content.layout.b,
        .destructor = NULL,
        .child_count    = frame_child_count,
        .append_caption = frame_append_caption,
    };
    return intf;
}

void print_frame_tree(HSFrame* frame, GString* output) {
    HSTreeInterface frameintf = {
        .child_count    = frame_child_count,
        .nth_child      = frame_nth_child,
        .append_caption = frame_append_caption,
        .destructor     = NULL,
        .data           = (HSTree) frame,
    };
    tree_print_to(&frameintf, output);
}

void frame_apply_floating_layout(HSFrame* frame, HSMonitor* m) {
    if (!frame) return;
    if (frame->type == TYPE_FRAMES) {
        frame_apply_floating_layout(frame->content.layout.a, m);
        frame_apply_floating_layout(frame->content.layout.b, m);
    } else {
        /* if(frame->type == TYPE_CLIENTS) */
        frame_set_visible(frame, false);
        HSClient** buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        size_t selection = frame->content.clients.selection;
        /* border color */
        for (int i = 0; i < count; i++) {
            HSClient* client = buf[i];
            client_setup_border(client, (g_cur_frame == frame) && (i == selection));
            client_resize_floating(client, m);
        }
    }
}

int frame_current_cycle_client_layout(int argc, char** argv, GString* output) {
    char* cmd_name = argv[0]; // save this before shifting
    int delta = 1;
    if (argc >= 2) {
        delta = atoi(argv[1]);
    }
    assert(g_cur_frame && g_cur_frame->type == TYPE_CLIENTS);
    (void)SHIFT(argc, argv);
    (void)SHIFT(argc, argv);
    int layout_index;
    if (argc > 0) {
        /* cycle through a given list of layouts */
        char* curname = g_layout_names[g_cur_frame->content.clients.layout];
        char** pcurrent = table_find(argv, sizeof(*argv), argc, 0,
                                     memberequals_string, curname);
        int idx = pcurrent ? (INDEX_OF(argv, pcurrent) + delta) : 0;
        idx %= argc;
        idx += argc;
        idx %= argc;
        layout_index = find_layout_by_name(argv[idx]);
        if (layout_index < 0) {
            g_string_append_printf(output,
                "%s: Invalid layout name \"%s\"\n", cmd_name, argv[idx]);
            return HERBST_INVALID_ARGUMENT;
        }
    } else {
        /* cycle through the default list of layouts */
        layout_index = g_cur_frame->content.clients.layout + delta;
        layout_index %= LAYOUT_COUNT;
        layout_index += LAYOUT_COUNT;
        layout_index %= LAYOUT_COUNT;
    }
    g_cur_frame->content.clients.layout = layout_index;
    monitor_apply_layout(get_current_monitor());
    return 0;
}

int frame_current_set_client_layout(int argc, char** argv, GString* output) {
    int layout = 0;
    if (argc <= 1) {
        return HERBST_NEED_MORE_ARGS;
    }
    layout = find_layout_by_name(argv[1]);
    if (layout < 0) {
        g_string_append_printf(output,
            "%s: Invalid layout name \"%s\"\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    if (g_cur_frame && g_cur_frame->type == TYPE_CLIENTS) {
        g_cur_frame->content.clients.layout = layout;
        monitor_apply_layout(get_current_monitor());
    }
    return 0;
}

void frame_apply_client_layout_linear(HSFrame* frame, XRectangle rect, bool vertical) {
    HSClient** buf = frame->content.clients.buf;
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
        HSClient* client = buf[i];
        // add the space, if count does not divide frameheight without remainder
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
    HSClient** buf = frame->content.clients.buf;
    size_t count = frame->content.clients.count;
    int selection = frame->content.clients.selection;
    for (int i = 0; i < count; i++) {
        HSClient* client = buf[i];
        client_setup_border(client, (g_cur_frame == frame) && (i == selection));
        client_resize_tiling(client, rect, frame);
        if (i == selection) {
            client_raise(client);
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
    HSClient** buf = frame->content.clients.buf;
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
            HSClient* client = buf[i];
            client_setup_border(client, (g_cur_frame == frame) && (i == selection));
            client_resize_tiling(client, cur, frame);
            cur.x += width;
            i++;
        }
        cur.y += height;
    }

}

void frame_apply_layout(HSFrame* frame, XRectangle rect) {
    frame->last_rect = rect;
    if (frame->type == TYPE_CLIENTS) {
        size_t count = frame->content.clients.count;
        if (!*g_smart_frame_surroundings || frame->parent) {
            // apply frame gap
            rect.height -= *g_frame_gap;
            rect.width -= *g_frame_gap;
            // apply frame border
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
        if (g_cur_frame == frame) {
            border_color = g_frame_border_active_color;
            bg_color = g_frame_bg_active_color;
        }
        if (!*g_smart_frame_surroundings || frame->parent) {
            XSetWindowBorderWidth(g_display, frame->window, *g_frame_border_width);
            XMoveResizeWindow(g_display, frame->window,
                              rect.x - *g_frame_border_width,
                              rect.y - *g_frame_border_width,
                              rect.width, rect.height);
        } else {
            XSetWindowBorderWidth(g_display, frame->window, 0);
            XMoveResizeWindow(g_display, frame->window, rect.x, rect.y, rect.width, rect.height);
        }

        frame_update_border(frame->window, border_color);

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
        // move windows
        if (count == 0) {
            return;
        }

        if (!*g_smart_window_surroundings
            || (frame->content.clients.count != 1
                && frame->content.clients.layout != LAYOUT_MAX)) {
            // apply window gap
            rect.x += *g_window_gap;
            rect.y += *g_window_gap;
            rect.width -= *g_window_gap;
            rect.height -= *g_window_gap;

            // apply frame padding
            rect.x += *g_frame_padding;
            rect.y += *g_frame_padding;
            rect.width  -= *g_frame_padding * 2;
            rect.height -= *g_frame_padding * 2;
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
        frame_apply_layout(layout->a, first);
        frame_apply_layout(layout->b, second);
    }
}

static void frame_update_frame_window_visibility_helper(HSFrame* frame) {
    if (frame->type == TYPE_CLIENTS) {
        int count = frame->content.clients.count;
        frame_set_visible(frame, *g_always_show_frame
            || (count != 0) || (g_cur_frame == frame));
    } else {
        frame_set_visible(frame, false);
    }
}

void frame_update_frame_window_visibility(HSFrame* frame) {
    frame_do_recursive(frame, frame_update_frame_window_visibility_helper, 2);
}

HSFrame* frame_current_selection_below(HSFrame* frame) {
    while (frame->type == TYPE_FRAMES) {
        frame = (frame->content.layout.selection == 0) ?
                frame->content.layout.a :
                frame->content.layout.b;
    }
    return frame;
}

HSFrame* frame_current_selection() {
    HSMonitor* m = get_current_monitor();
    if (!m->tag) return NULL;
    return frame_current_selection_below(m->tag->frame);
}

int frame_current_bring(int argc, char** argv, GString* output) {
    HSClient* client = NULL;

    if (argc < 2) {
        return HERBST_NEED_MORE_ARGS;
    }
    string_to_client(argv[1], &client);
    if (!client) {
        g_string_append_printf(output,
            "%s: Could not find client", argv[0]);
        if (argc > 1) {
            g_string_append_printf(output, " \"%s\".\n", argv[1]);
        } else {
            g_string_append(output, ".\n");
        }
        return HERBST_INVALID_ARGUMENT;
    }
    HSTag* tag = get_current_monitor()->tag;
    tag_move_client(client, tag);
    HSFrame* frame = find_frame_with_client(tag->frame, client);
    if (frame != g_cur_frame) {
        frame_remove_client(frame, client);
        frame_insert_client(g_cur_frame, client);
    }
    focus_client(client, false, false);
    return 0;
}

int frame_current_set_selection(int argc, char** argv) {
    int index = 0;
    if (argc >= 2) {
        index = atoi(argv[1]);
    } else {
        return HERBST_NEED_MORE_ARGS;
    }
    // find current selection
    HSFrame* frame = frame_current_selection();
    if (frame->content.clients.count == 0) {
        // nothing to do
        return 0;
    }
    if (index < 0 || index >= frame->content.clients.count) {
        index = frame->content.clients.count - 1;
    }
    frame->content.clients.selection = index;
    Window window = frame->content.clients.buf[index]->window;
    window_focus(window);
    return 0;
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
    Window window = frame->content.clients.buf[index]->window;
    window_focus(window);
    return 0;
}

int cycle_all_command(int argc, char** argv) {
    int delta = 1;
    int skip_invisible = false;
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
                // i.e. if we cannot go in the desired direction
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
        // and then to the left (i.e. find first leaf)
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
    HSClient* c = frame_focused_client(g_cur_frame);
    if (c) {
        client_raise(c);
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

bool frame_split(HSFrame* frame, int align, int fraction) {
    if (frame_split_count_to_root(frame, align) > HERBST_MAX_TREE_HEIGHT) {
        // do nothing if tree would be to large
        return false;
    }
    // ensure fraction is allowed
    fraction = CLAMP(fraction,
                     FRACTION_UNIT * (0.0 + FRAME_MIN_FRACTION),
                     FRACTION_UNIT * (1.0 - FRAME_MIN_FRACTION));

    HSFrame* first = frame_create_empty(frame, NULL);
    HSFrame* second = frame_create_empty(frame, NULL);
    first->content = frame->content;
    first->type = frame->type;
    second->type = TYPE_CLIENTS;
    frame->type = TYPE_FRAMES;
    frame->content.layout.align = align;
    frame->content.layout.a = first;
    frame->content.layout.b = second;
    frame->content.layout.selection = 0;
    frame->content.layout.fraction = fraction;
    return true;
}

int frame_split_command(int argc, char** argv, GString* output) {
    // usage: split t|b|l|r|h|v FRACTION
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
    }
    int align = -1;
    bool frameToFirst = true;
    int selection = 0;
    int lh = g_cur_frame->last_rect.height;
    int lw = g_cur_frame->last_rect.width;
    int align_auto = (lw > lh) ? ALIGN_HORIZONTAL : ALIGN_VERTICAL;
    struct {
        char* name;
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
        { "fragment",   ALIGN_FRAGMENT,     true,   0   },
    };
    for (int i = 0; i < LENGTH(splitModes); i++) {
        if (splitModes[i].name[0] == argv[1][0]) {
            align           = splitModes[i].align;
            frameToFirst    = splitModes[i].frameToFirst;
            selection       = splitModes[i].selection;
            break;
        }
    }
    if (align < 0) {
        g_string_append_printf(output,
            "%s: Invalid alignment \"%s\"\n", argv[0], argv[1]);
        return HERBST_INVALID_ARGUMENT;
    }
    int fraction = FRACTION_UNIT* CLAMP(atof(argv[2]),
                                        0.0 + FRAME_MIN_FRACTION,
                                        1.0 - FRAME_MIN_FRACTION);
    HSFrame* frame = frame_current_selection();
    if (!frame) return 0; // nothing to do
    bool fragmenting = align == ALIGN_FRAGMENT;
    int layout = frame->content.clients.layout;
    int windowcount = frame->content.clients.count;
    if (fragmenting) {
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
    }
    if (!frame_split(frame, align, fraction)) {
        return 0;
    }
    if (fragmenting) {
        // move second half of the window buf to second frame
        size_t count1 = frame->content.layout.a->content.clients.count;
        size_t count2 = frame->content.layout.b->content.clients.count;
        // assert: count2 == 0
        size_t nc1 = (count1 + 1) / 2;      // new count for the first frame
        size_t nc2 = count1 - nc1 + count2; // new count for the 2nd frame
        HSFrame* child1 = frame->content.layout.a;
        HSFrame* child2 = frame->content.layout.b;
        HSClient*** buf1 = &child1->content.clients.buf;
        HSClient*** buf2 = &child2->content.clients.buf;
        *buf2 = g_renew(HSClient*, *buf2, nc2);
        memcpy(*buf2 + count2, *buf1 + nc1, (nc2 - count2) * sizeof(**buf2));
        *buf1 = g_renew(HSClient*, *buf1, nc1);
        child1->content.clients.count = nc1;
        child2->content.clients.count = nc2;
        child2->content.clients.layout = child1->content.clients.layout;
        if (child1->content.clients.selection >= nc1 && nc1 > 0) {
            child2->content.clients.selection =
                child1->content.clients.selection - nc1 + count2;
            child1->content.clients.selection = nc1 - 1;
            selection = 1;
        } else {
            selection = 0;
        }

    } else if (!frameToFirst) {
        HSFrame* emptyFrame = frame->content.layout.b;
        frame->content.layout.b = frame->content.layout.a;
        frame->content.layout.a = emptyFrame;
    }
    frame->content.layout.selection = selection;
    // reset focus
    g_cur_frame = frame_current_selection();
    // redraw monitor
    monitor_apply_layout(get_current_monitor());
    return 0;
}

int frame_change_fraction_command(int argc, char** argv, GString* output) {
    // usage: fraction DIRECTION DELTA
    if (argc < 3) {
        return HERBST_NEED_MORE_ARGS;
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
        default:
            g_string_append_printf(output,
                "%s: Invalid direction \"%s\"\n", argv[0], argv[1]);
            return HERBST_INVALID_ARGUMENT;
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
            g_string_append_printf(output,
                "%s: No neighbour found\n", argv[0]);
            return HERBST_FORBIDDEN;
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

int frame_focus_command(int argc, char** argv, GString* output) {
    // usage: focus [-e|-i] left|right|up|down
    if (argc < 2) return HERBST_NEED_MORE_ARGS;
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
        } else {
            g_string_append_printf(output,
                "%s: No neighbour found\n", argv[0]);
            return HERBST_FORBIDDEN;
        }
    }
    return 0;
}

int frame_move_window_command(int argc, char** argv, GString* output) {
    // usage: move left|right|up|down
    if (argc < 2) return HERBST_NEED_MORE_ARGS;
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
        HSClient** buf = g_cur_frame->content.clients.buf;
        // if internal neighbour was found, then swap
        HSClient* tmp = buf[selection];
        buf[selection] = buf[index];
        buf[index] = tmp;

        g_cur_frame->content.clients.selection = index;
        frame_focus_recursive(g_cur_frame);
        monitor_apply_layout(get_current_monitor());
    } else {
        HSFrame* neighbour = frame_neighbour(g_cur_frame, direction);
        HSClient* client = frame_focused_client(g_cur_frame);
        if (client && neighbour != NULL) { // if neighbour was found
            // move window to neighbour
            frame_remove_client(g_cur_frame, client);
            frame_insert_client(neighbour, client);

            // change selection in parent
            HSFrame* parent = neighbour->parent;
            assert(parent);
            parent->content.layout.selection = ! parent->content.layout.selection;
            frame_focus_recursive(parent);
            // focus right window in frame
            HSFrame* frame = g_cur_frame;
            assert(frame);
            int i;
            HSClient** buf = frame->content.clients.buf;
            size_t count = frame->content.clients.count;
            for (i = 0; i < count; i++) {
                if (buf[i] == client) {
                    frame->content.clients.selection = i;
                    window_focus(buf[i]->window);
                    break;
                }
            }

            // layout was changed, so update it
            monitor_apply_layout(get_current_monitor());
        } else {
            g_string_append_printf(output,
                "%s: No neighbour found\n", argv[0]);
            return HERBST_FORBIDDEN;
        }
    }
    return 0;
}

void frame_unfocus() {
    //XSetInputFocus(g_display, g_root, RevertToPointerRoot, CurrentTime);
}

HSClient* frame_focused_client(HSFrame* frame) {
    if (!frame) {
        return NULL;
    }
    // follow the selection to a leaf
    while (frame->type == TYPE_FRAMES) {
        frame = (frame->content.layout.selection == 0) ?
                frame->content.layout.a :
                frame->content.layout.b;
    }
    if (frame->content.clients.count) {
        int selection = frame->content.clients.selection;
        return frame->content.clients.buf[selection];
    } // else, if there are no windows
    return NULL;
}

// try to focus window in frame
// it does not require anything from the frame. it may be infocused or even
// hidden.
// returns true if win was found and focused, else returns false
bool frame_focus_client(HSFrame* frame, HSClient* client) {
    if (!frame) {
        return false;
    }
    if (frame->type == TYPE_CLIENTS) {
        int i;
        size_t count = frame->content.clients.count;
        HSClient** buf = frame->content.clients.buf;
        // search for win in buf
        for (i = 0; i < count; i++) {
            if (buf[i] == client) {
                // if found, set focus to it
                frame->content.clients.selection = i;
                return true;
            }
        }
        return false;
    } else {
        // type == TYPE_FRAMES
        // search in subframes
        bool found = frame_focus_client(frame->content.layout.a, client);
        if (found) {
            // set selection to first frame
            frame->content.layout.selection = 0;
            return true;
        }
        found = frame_focus_client(frame->content.layout.b, client);
        if (found) {
            // set selection to second frame
            frame->content.layout.selection = 1;
            return true;
        }
        return false;
    }
}

// focus a window
// switch_tag tells, whether to switch tag to focus to window
// switch_monitor tells, whether to switch monitor to focus to window
// returns if window was focused or not
bool focus_client(struct HSClient* client, bool switch_tag, bool switch_monitor) {
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
    monitors_lock();
    monitor_set_tag(cur_mon, tag);
    cur_mon = get_current_monitor();
    if (cur_mon->tag != tag) {
        // could not set tag on monitor
        monitors_unlock();
        return false;
    }
    // now the right tag is visible
    // now focus it
    bool found = frame_focus_client(tag->frame, client);
    frame_focus_recursive(tag->frame);
    monitor_apply_layout(cur_mon);
    monitors_unlock();
    return found;
}

int frame_focus_recursive(HSFrame* frame) {
    // follow the selection to a leaf
    while (frame->type == TYPE_FRAMES) {
        frame = (frame->content.layout.selection == 0) ?
                frame->content.layout.a :
                frame->content.layout.b;
    }
    g_cur_frame = frame;
    frame_unfocus();
    if (frame->content.clients.count) {
        int selection = frame->content.clients.selection;
        window_focus(frame->content.clients.buf[selection]->window);
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

void frame_do_recursive_data(HSFrame* frame, void (*action)(HSFrame*,void*),
                             int order, void* data) {
    if (frame->type == TYPE_FRAMES) {
        // clients and subframes
        HSLayout* layout = &(frame->content.layout);
        if (order <= 0) action(frame, data);
        frame_do_recursive_data(layout->a, action, order, data);
        if (order == 1) action(frame, data);
        frame_do_recursive_data(layout->b, action, order, data);
        if (order >= 2) action(frame, data);
    } else {
        // action only
        action(frame, data);
    }
}

static void frame_hide(HSFrame* frame) {
    frame_set_visible(frame, false);
    if (frame->type == TYPE_CLIENTS) {
        int i;
        HSClient** buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            window_hide(buf[i]->window);
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
        HSClient** buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        for (i = 0; i < count; i++) {
            window_show(buf[i]->window);
        }
    }
}

void frame_show_recursive(HSFrame* frame) {
    // first show parents, then children => order = 0
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
    HSClient** wins;
    // get all wins from first child
    frame_destroy(first, &wins, &count);
    // and insert them to other child.. inefficiently
    int i;
    for (i = 0; i < count; i++) {
        frame_insert_client(second, wins[i]);
    }
    g_free(wins);
    XDestroyWindow(g_display, parent->window);
    // now do tree magic
    // and make second child the new parent
    // set parent
    second->parent = parent->parent;
    // TODO: call frame destructor here
    stack_remove_slice(parent->tag->stack, parent->slice);
    slice_destroy(parent->slice);
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
    HSClient* client = frame_focused_client(g_cur_frame);
    if (client) {
        window_close(client->window);
        return 0;
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
// if action fails (i.e. returns something != 0), then it aborts with this code
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
        HSClient** buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        HSClient* client;
        for (int i = 0; i < count; i++) {
            client = buf[i];
            // do action for each client
            status = action(client, data);
            if (0 != status) {
                return status;
            }
        }
    }
    return 0;
}

void frame_update_border(Window window, unsigned long color) {
    if (*g_frame_border_inner_width > 0 && *g_frame_border_inner_width < *g_frame_border_width) {
        set_window_double_border(g_display, window, *g_frame_border_inner_width, g_frame_border_inner_color, color);
    } else {
        XSetWindowBorder(g_display, window, color);
    }
}

int frame_focus_edge(int argc, char** argv, GString* output) {
    // Puts the focus to the edge in the specified direction
    char* args[] = { "" };
    monitors_lock_command(LENGTH(args), args);
    while (0 == frame_focus_command(argc,argv,output))
        ;
    monitors_unlock_command(LENGTH(args), args);
    return 0;
}

int frame_move_window_edge(int argc, char** argv, GString* output) {
    // Moves a window to the edge in the specified direction
    char* args[] = { "" };
    monitors_lock_command(LENGTH(args), args);
    while (0 == frame_move_window_command(argc,argv,output))
        ;
    monitors_unlock_command(LENGTH(args), args);
    return 0;
}

