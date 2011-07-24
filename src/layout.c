

#include "globals.h"
#include "layout.h"
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>


void layout_init() {
    g_cur_monitor = 0;
    g_tags = g_array_new(false, false, sizeof(HSTag));
    g_monitors = g_array_new(false, false, sizeof(HSMonitor));
}

void layout_destroy() {
    int i;
    for (i = 0; i < g_tags->len; i++) {
        g_string_free(g_array_index(g_tags, HSTag, i).name, true);
    }
    g_array_free(g_tags, true);
    g_array_free(g_monitors, true);
}


HSFrame* frame_create_empty() {
    HSFrame* frame = g_new0(HSFrame, 1);
    frame->type = TYPE_CLIENTS;
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
    } else { /* frame->type == TYPE_FRAMES */
        frame_insert_window(frame->content.layout->a, window);
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
                return true;
            }
        }
        return false;
    } else { /* frame->type == TYPE_FRAMES */
        bool found = frame_remove_window(frame->content.layout->a, window);
        found = found || frame_remove_window(frame->content.layout->b, window);
        return found;
    }
}

void frame_destroy(HSFrame* frame, Window** buf, size_t* count) {
    if (frame->type == TYPE_CLIENTS) {
        *buf = frame->content.clients.buf;
        *count = frame->content.clients.count;
        g_free(frame);
    } else { /* frame->type == TYPE_FRAMES */
        size_t c1, c2;
        Window *buf1, *buf2;
        frame_destroy(frame->content.layout->a, &buf1, &c1);
        frame_destroy(frame->content.layout->b, &buf2, &c2);
        // append buf2 to buf1
        buf1 = g_renew(Window, buf1, c1 + c2);
        memcpy(buf1+c1, buf2, sizeof(Window) * c2);
        // free unused things
        g_free(buf2);
        g_free(frame);
        // return;
        *buf = buf1;
        *count = c1 + c2;
    }
}

void print_frame_tree(HSFrame* frame, int indent, GString** output) {
    unsigned int j;
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        unsigned int i;
        for (j = 0; j < indent; j++) {
            *output = g_string_append(*output, " ");
        }
        g_string_append_printf(*output, "frame with wins:\n");
        for (i = 0; i < count; i++) {
            for (j = 0; j < indent; j++) {
                *output = g_string_append(*output, " ");
            }
            g_string_append_printf(*output, "  -> win %d\n", (int)buf[i]);
        }
    } else { /* frame->type == TYPE_FRAMES */
        for (j = 0; j < indent; j++) {
            *output = g_string_append(*output, " ");
        }
        HSLayout* layout = frame->content.layout;
        g_string_append_printf(*output,
            "layout %s, size %f%%\n", (layout->align ? "vert" : "horz"),
                ((double)layout->fraction*100)/(double)FRACTION_UNIT);
        print_frame_tree(layout->a, indent+2, output);
        print_frame_tree(layout->b, indent+2, output);
    }

}

void frame_apply_layout(HSFrame* frame, XRectangle rect) {
    if (frame->type == TYPE_CLIENTS) {
        Window* buf = frame->content.clients.buf;
        size_t count = frame->content.clients.count;
        if (count == 0) {
            return;
        }
        XRectangle cur = rect;
        cur.height /= count;
        int step = cur.height;
        int i;
        for (i = 0; i < count; i++) {
            XMoveWindow(g_display, buf[i], cur.x, cur.y);
            XResizeWindow(g_display, buf[i], cur.width, cur.height);
            cur.y += step;
        }
    } else { /* frame->type == TYPE_FRAMES */
        // TODO
    }
}

HSMonitor* add_monitor(XRectangle rect) {
    HSMonitor m;
    m.rect = rect;
    m.tag = NULL;
    // find an tag
    int i;
    for (i = 0; i < g_tags->len; i++) {
        HSTag* tag = &g_array_index(g_tags, HSTag, i);
        if (find_monitor_with_tag(tag) == NULL) {
            m.tag = tag;
        }
    }
    g_array_append_val(g_monitors, m);
    return &g_array_index(g_monitors, HSMonitor, g_monitors->len-1);
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

HSTag* add_tag(char* name) {
    HSTag tag;
    tag.frame = frame_create_empty();
    tag.name = g_string_new(name);
    g_array_append_val(g_tags, tag);
    return &g_array_index(g_tags, HSTag, g_tags->len-1);
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
    add_monitor(rect);
}

