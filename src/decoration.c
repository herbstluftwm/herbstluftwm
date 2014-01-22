
#include "decoration.h"
#include "clientlist.h"
#include "globals.h"
#include "settings.h"
#include "ewmh.h"

#include <stdio.h>

HSDecTripple g_decorations[HSDecSchemeCount];

static GHashTable* g_decwin2client = NULL;

int* g_pseudotile_center_threshold;
HSObject* g_theme_object;
HSObject g_theme_active_object;
HSObject g_theme_normal_object;
HSObject g_theme_urgent_object;
static void init_dec_tripple_object(HSDecTripple* t, const char* name);
static void init_scheme_object(HSObject* obj, HSDecorationScheme* s, HSAttrCallback cb);
static GString* PROP2FLOAT(HSAttribute* attr);

// is called automatically after resize_outline
static void decoration_update_frame_extents(struct HSClient* client);

void decorations_init() {
    g_theme_object = hsobject_create_and_link(hsobject_root(), "theme");
    g_pseudotile_center_threshold = &(settings_find("pseudotile_center_threshold")->value.i);
    g_decwin2client = g_hash_table_new(g_int_hash, g_int_equal);
    // init default schemes
    HSDecTripple tiling = {
        { 2, getcolor("black"),     false },    // normal
        { 2, getcolor("green"),     false },    // active
        { 2, getcolor("orange"),    false },    // urgent
    };
    g_decorations[HSDecSchemeTiling] = tiling;
    HSDecTripple fs = {
        { 0, getcolor("black"),     false },    // normal
        { 0, getcolor("green"),     false },    // active
        { 0, getcolor("orange"),    false },    // urgent
    };
    g_decorations[HSDecSchemeFullscreen] = fs;
    HSDecTripple fl = {
        { 1, getcolor("black"),     true  },    // normal
        { 4, getcolor("green"),     true  },    // active
        { 1, getcolor("orange"),    true  },    // urgent
    };
    g_decorations[HSDecSchemeFloating] = fl;
    init_dec_tripple_object(g_decorations + HSDecSchemeTiling, "tiling");
    init_dec_tripple_object(g_decorations + HSDecSchemeFloating, "floating");
    // create mass-attribute-objects
    init_scheme_object(&g_theme_active_object,
                       &(g_decorations[HSDecSchemeTiling].active),
                       PROP2FLOAT);
    init_scheme_object(&g_theme_normal_object, &(g_decorations[HSDecSchemeTiling].normal), PROP2FLOAT);
    init_scheme_object(&g_theme_urgent_object, &(g_decorations[HSDecSchemeTiling].urgent), PROP2FLOAT);
    hsobject_link(g_theme_object, &g_theme_active_object, "active");
    hsobject_link(g_theme_object, &g_theme_normal_object, "normal");
    hsobject_link(g_theme_object, &g_theme_urgent_object, "urgent");
}

static GString* RELAYOUT(HSAttribute* attr) {
    (void) attr;
    all_monitors_apply_layout();
    return NULL;
}

static GString* PROP2FLOAT(HSAttribute* attr) {
    monitors_lock();
    // find out which object it was
    // then copy it to the appropriate floating scheme
    GString* output = g_string_new("");
    HSDecTripple* t = g_decorations + HSDecSchemeFloating;
    if (attr->object == &g_theme_active_object) {
        int idx = attr - attr->object->attributes;
        GString* val = hsattribute_to_string(attr);
        hsattribute_assign(t->obj_active.attributes + idx, val->str, output);
        g_string_free(val, true);
    } else if (attr->object == &g_theme_normal_object) {
        int idx = attr - attr->object->attributes;
        GString* val = hsattribute_to_string(attr);
        hsattribute_assign(t->obj_normal.attributes + idx, val->str, output);
        g_string_free(val, true);
    } else if (attr->object == &g_theme_urgent_object) {
        int idx = attr - attr->object->attributes;
        GString* val = hsattribute_to_string(attr);
        hsattribute_assign(t->obj_urgent.attributes + idx, val->str, output);
        g_string_free(val, true);
    }
    monitors_unlock();
    g_string_free(output, true);
    return NULL;
}

GString* PROPAGATE(HSAttribute* attr) {
    HSDecTripple* t = attr->object->data;
    monitors_lock();
    // find out which attribute it was
    int idx = attr - attr->object->attributes;
    // then copy it to active and urgent scheme
    GString* output = g_string_new("");
    GString* val = hsattribute_to_string(attr);
    hsattribute_assign(t->obj_active.attributes + idx, val->str, output);
    hsattribute_assign(t->obj_urgent.attributes + idx, val->str, output);
    monitors_unlock();
    g_string_free(output, true);
    g_string_free(val, true);
    return NULL;
}

void reset_helper(void* data, GString* output) {
    (void) data;
    g_string_append(output,
                    "Writing this resets all attributes to a default value\n");
}

static GString* trigger_attribute_reset(struct HSAttribute* attr, const char* new_value) {
    (void) attr;
    (void) new_value;
    HSObject* obj = attr->object;
    HSAttribute* attrs = obj->attributes;
    size_t cnt = obj->attribute_count;
    GString* out = g_string_new("");
    monitors_lock();
    for (int i = 0; i < cnt; i ++) {
        HSAttribute* a = attrs+i;
        if (a->type == HSATTR_TYPE_INT
            || a->type == HSATTR_TYPE_UINT) {
            hsattribute_assign(a, "0", out);
        }
        else if (a->type == HSATTR_TYPE_COLOR) {
            hsattribute_assign(a, "black", out);
        }
    }
    if (out->len <= 0) {
        g_string_free(out, true);
        out = NULL;
    }
    monitors_unlock();
    return out;
}

// initializes the specified object to edit the scheme
static void init_scheme_object(HSObject* obj, HSDecorationScheme* s, HSAttrCallback cb) {
    hsobject_init(obj);
    HSAttribute attributes[] = {
        ATTRIBUTE_INT(      "border_width",     s->border_width,    cb),
        ATTRIBUTE_INT(      "padding_top",      s->padding_top,     cb),
        ATTRIBUTE_INT(      "padding_right",    s->padding_right,   cb),
        ATTRIBUTE_INT(      "padding_bottom",   s->padding_bottom,  cb),
        ATTRIBUTE_INT(      "padding_left",     s->padding_left,    cb),
        ATTRIBUTE_COLOR(    "color",            s->border_color,    cb),
        ATTRIBUTE_INT(      "inner_width",      s->inner_width,     cb),
        ATTRIBUTE_COLOR(    "inner_color",      s->inner_color,     cb),
        ATTRIBUTE_INT(      "outer_width",      s->outer_width,     cb),
        ATTRIBUTE_COLOR(    "outer_color",      s->outer_color,     cb),
        ATTRIBUTE_CUSTOM(   "reset",            reset_helper,       trigger_attribute_reset),
        ATTRIBUTE_LAST,
    };
    hsobject_set_attributes(obj, attributes);
}

static void init_dec_tripple_object(HSDecTripple* t, const char* name) {
    hsobject_init(&t->object);
    init_scheme_object(&t->obj_normal, &t->normal, RELAYOUT);
    init_scheme_object(&t->obj_active, &t->active, RELAYOUT);
    init_scheme_object(&t->obj_urgent, &t->urgent, RELAYOUT);
    hsobject_link(&t->object, &t->obj_normal, "normal");
    hsobject_link(&t->object, &t->obj_active, "active");
    hsobject_link(&t->object, &t->obj_urgent, "urgent");
    HSAttribute attributes[] = {
        ATTRIBUTE_INT(      "border_width",     t->normal.border_width,    PROPAGATE),
        ATTRIBUTE_INT(      "padding_top",      t->normal.padding_top,     PROPAGATE),
        ATTRIBUTE_INT(      "padding_right",    t->normal.padding_right,   PROPAGATE),
        ATTRIBUTE_INT(      "padding_bottom",   t->normal.padding_bottom,  PROPAGATE),
        ATTRIBUTE_INT(      "padding_left",     t->normal.padding_left,    PROPAGATE),
        ATTRIBUTE_COLOR(    "color",            t->normal.border_color,    PROPAGATE),
        ATTRIBUTE_INT(      "inner_width",      t->normal.inner_width,     PROPAGATE),
        ATTRIBUTE_COLOR(    "inner_color",      t->normal.inner_color,     PROPAGATE),
        ATTRIBUTE_INT(      "outer_width",      t->normal.outer_width,     PROPAGATE),
        ATTRIBUTE_COLOR(    "outer_color",      t->normal.outer_color,     PROPAGATE),
        ATTRIBUTE_CUSTOM(   "reset",            reset_helper,       trigger_attribute_reset),
        ATTRIBUTE_LAST,
    };
    t->object.data = t;
    hsobject_set_attributes(&t->object, attributes);
    hsobject_link(g_theme_object, &t->object, name);
}

static void free_dec_tripple_object(HSDecTripple* t) {
    hsobject_unlink(g_theme_object, &t->object);
    hsobject_free(&t->object);
    hsobject_free(&t->obj_normal);
    hsobject_free(&t->obj_active);
    hsobject_free(&t->obj_urgent);
}

void decorations_destroy() {
    free_dec_tripple_object(g_decorations + HSDecSchemeTiling);
    free_dec_tripple_object(g_decorations + HSDecSchemeFloating);
    hsobject_unlink(g_theme_object, &g_theme_normal_object);
    hsobject_unlink(g_theme_object, &g_theme_active_object);
    hsobject_unlink(g_theme_object, &g_theme_urgent_object);
    hsobject_free(&g_theme_normal_object);
    hsobject_free(&g_theme_active_object);
    hsobject_free(&g_theme_urgent_object);
    hsobject_unlink_and_destroy(hsobject_root(), g_theme_object);
    g_hash_table_destroy(g_decwin2client);
    g_decwin2client = NULL;
}

void decoration_init(HSDecoration* dec, struct HSClient* client) {
    dec->client = client;
    XSetWindowAttributes at;
    dec->decwin = XCreateWindow(g_display, g_root, 0,0, 30, 30, 0,
                        DefaultDepth(g_display, DefaultScreen(g_display)),
                        CopyFromParent,
                        DefaultVisual(g_display, DefaultScreen(g_display)),
                        0, &at);
    dec->last_rect_inner = false;
    dec->last_inner_rect.width = -1;
    dec->last_outer_rect.width = -1;
    g_hash_table_insert(g_decwin2client, &(dec->decwin), client);
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_DECORATION_CLASS;
    hint->res_class = HERBST_DECORATION_CLASS;
    XSetClassHint(g_display, dec->decwin, hint);
    XFree(hint);
    dec->gc = XCreateGC(g_display, dec->decwin, 0, NULL);
}

void decoration_free(HSDecoration* dec) {
    if (g_decwin2client) {
        g_hash_table_remove(g_decwin2client, &(dec->decwin));
    }
    XFreeGC(g_display, dec->gc);
    XDestroyWindow(g_display, dec->decwin);
}

HSClient* get_client_from_decoration(Window decwin) {
    return (HSClient*) g_hash_table_lookup(g_decwin2client, &decwin);
}

Rectangle outline_to_inner_rect(Rectangle rect, HSDecorationScheme s) {
    Rectangle inner = {
        .x = rect.x + s.border_width + s.padding_left,
        .y = rect.y + s.border_width + s.padding_top,
        .width  = rect.width  - 2* s.border_width - s.padding_left - s.padding_right,
        .height = rect.height - 2* s.border_width - s.padding_top - s.padding_bottom,
    };
    return inner;
}

Rectangle inner_rect_to_outline(Rectangle rect, HSDecorationScheme s) {
    Rectangle out = {
        .x = rect.x - s.border_width - s.padding_left,
        .y = rect.y - s.border_width - s.padding_top,
        .width  = rect.width  + 2* s.border_width + s.padding_left + s.padding_right,
        .height = rect.height + 2* s.border_width + s.padding_top + s.padding_bottom,
    };
    return out;
}

void decoration_resize_inner(HSClient* client, Rectangle inner,
                             HSDecorationScheme scheme) {
    decoration_resize_outline(client, inner_rect_to_outline(inner, scheme), scheme);
    client->dec.last_rect_inner = true;
}

void decoration_resize_outline(HSClient* client, Rectangle outline,
                               HSDecorationScheme scheme) {
    Rectangle inner = outline_to_inner_rect(outline, scheme);
    // get relative coordinates
    Window decwin = client->dec.decwin;
    Window win = client->window;

    Rectangle tile = inner;
    applysizehints(client, &inner.width, &inner.height);
    if (!scheme.tight_decoration) {
        // center the window in the outline tile
        // but only if it's relative coordinates would not be too close to the
        // upper left tile border
        int threshold = *g_pseudotile_center_threshold;
        int dx = tile.width/2 - inner.width/2;
        int dy = tile.height/2 - inner.height/2;
        inner.x = tile.x + ((dx < threshold) ? 0 : dx);
        inner.y = tile.y + ((dy < threshold) ? 0 : dy);
    }

    //if (RECTANGLE_EQUALS(client->last_size, rect)
    //    && client->last_border_width == border_width) {
    //    return;
    //}

    if (scheme.tight_decoration) {
        outline = inner_rect_to_outline(inner, scheme);
    }
    client->dec.last_inner_rect = inner;
    inner.x -= outline.x;
    inner.y -= outline.y;
    XWindowChanges changes = {
      .x = inner.x, .y = inner.y, .width = inner.width, .height = inner.height,
      .border_width = 0
    };
    int mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
    HSColor col = scheme.border_color;
    XSetWindowBackground(g_display, decwin, col);
    XClearWindow(g_display, decwin);
    XConfigureWindow(g_display, win, mask, &changes);
    //if (*g_window_border_inner_width > 0
    //    && *g_window_border_inner_width < *g_window_border_width) {
    //    unsigned long current_border_color = get_window_border_color(client);
    //    HSDebug("client_resize %s\n",
    //            current_border_color == g_window_border_active_color
    //            ? "ACTIVE" : "NORMAL");
    //    set_window_double_border(g_display, win, *g_window_border_inner_width,
    //                             g_window_border_inner_color,
    //                             current_border_color);
    //}
    // send new size to client
    client->dec.last_outer_rect = outline;
    client->dec.last_rect_inner = false;
    client->last_size = inner;
    client->dec.last_scheme = scheme;
    XMoveResizeWindow(g_display, decwin,
                      outline.x, outline.y, outline.width, outline.height);
    decoration_redraw(client);
    decoration_update_frame_extents(client);
    client_send_configure(client);
    XSync(g_display, False);
}

static void decoration_update_frame_extents(struct HSClient* client) {
    int left = client->dec.last_inner_rect.x - client->dec.last_outer_rect.x;
    int top  = client->dec.last_inner_rect.y - client->dec.last_outer_rect.y;
    int right = client->dec.last_outer_rect.width - client->dec.last_inner_rect.width - left;
    int bottom = client->dec.last_outer_rect.height - client->dec.last_inner_rect.height - top;
    ewmh_update_frame_extents(client->window, left,right, top,bottom);
}

void decoration_change_scheme(struct HSClient* client,
                              HSDecorationScheme scheme) {
    if (client->dec.last_inner_rect.width < 0) {
        // TODO: do something useful here
        return;
    }
    if (client->dec.last_rect_inner) {
        decoration_resize_inner(client, client->dec.last_inner_rect, scheme);
    } else {
        decoration_resize_outline(client, client->dec.last_outer_rect, scheme);
    }
}

void decoration_redraw(struct HSClient* client) {
    HSDecorationScheme s = client->dec.last_scheme;
    Window win = client->dec.decwin;
    GC gc = client->dec.gc;
    int iw = s.inner_width;
    Rectangle inner = client->dec.last_inner_rect;
    inner.x -= client->dec.last_outer_rect.x;
    inner.y -= client->dec.last_outer_rect.y;
    if (iw > 0) {
        XSetForeground(g_display, gc, s.inner_color);
        XSetLineAttributes(g_display, gc, iw, LineSolid, CapNotLast, JoinMiter);
        XDrawRectangle(g_display, win, gc,
            inner.x - (iw - iw/2), inner.y - (iw - iw/2),
            inner.width + iw, inner.height + iw);
    }
    int ow = s.outer_width;
    Rectangle outer = client->dec.last_outer_rect;
    outer.x -= client->dec.last_outer_rect.x;
    outer.y -= client->dec.last_outer_rect.y;
    if (ow > 0) {
        XSetForeground(g_display, gc, s.outer_color);
        XSetLineAttributes(g_display, gc, ow, LineSolid, CapProjecting, JoinMiter);
        XDrawRectangle(g_display, win, gc,
            ow/2, ow/2,
            outer.width - ow, outer.height - ow);
    }
}

