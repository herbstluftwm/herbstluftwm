
#include "decoration.h"
#include "clientlist.h"
#include "globals.h"
#include "settings.h"

#include <stdio.h>

HSDecTripple g_decorations[HSDecSchemeCount];

int* g_pseudotile_center_threshold;
HSObject* g_theme_object;
static void init_dec_tripple_object(HSDecTripple* t, const char* name);

void decorations_init() {
    g_theme_object = hsobject_create_and_link(hsobject_root(), "theme");
    g_pseudotile_center_threshold = &(settings_find("pseudotile_center_threshold")->value.i);
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
}

GString* RELAYOUT(HSAttribute* attr) {
    (void) attr;
    all_monitors_apply_layout();
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

// initializes the specified object to edit the scheme
static void init_scheme_object(HSObject* obj, HSDecorationScheme* s) {
    hsobject_init(obj);
    HSAttribute attributes[] = {
        ATTRIBUTE_INT(      "border_width",     s->border_width,    RELAYOUT),
        ATTRIBUTE_INT(      "padding_top",      s->padding_top,     RELAYOUT),
        ATTRIBUTE_INT(      "padding_right",    s->padding_right,   RELAYOUT),
        ATTRIBUTE_INT(      "padding_bottom",   s->padding_bottom,  RELAYOUT),
        ATTRIBUTE_INT(      "padding_left",     s->padding_left,    RELAYOUT),
        ATTRIBUTE_COLOR(    "color",            s->border_color,    RELAYOUT),
        ATTRIBUTE_LAST,
    };
    hsobject_set_attributes(obj, attributes);
}

static void init_dec_tripple_object(HSDecTripple* t, const char* name) {
    hsobject_init(&t->object);
    init_scheme_object(&t->obj_normal, &t->normal);
    init_scheme_object(&t->obj_active, &t->active);
    init_scheme_object(&t->obj_urgent, &t->urgent);
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
    hsobject_unlink_and_destroy(hsobject_root(), g_theme_object);
}

void decoration_init(HSDecoration* dec, struct HSClient* client) {
    dec->client = client;
    XSetWindowAttributes at;
    dec->decwin = XCreateWindow(g_display, g_root, 0,0, 30, 30, 0,
                        DefaultDepth(g_display, DefaultScreen(g_display)),
                        CopyFromParent,
                        DefaultVisual(g_display, DefaultScreen(g_display)),
                        0, &at);
    dec->last_rect.width = -1;
    dec->last_rect_inner = false;
    // set wm_class for window
    XClassHint *hint = XAllocClassHint();
    hint->res_name = HERBST_FRAME_CLASS;
    hint->res_class = HERBST_FRAME_CLASS;
    XSetClassHint(g_display, dec->decwin, hint);
    XFree(hint);
}

void decoration_free(HSDecoration* dec) {
    XDestroyWindow(g_display, dec->decwin);
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
    client->dec.last_rect = inner;
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
    client->dec.last_rect = outline;
    client->dec.last_rect_inner = false;
    client->last_size = inner;
    XMoveResizeWindow(g_display, decwin,
                      outline.x, outline.y, outline.width, outline.height);
    client_send_configure(client);
    XSync(g_display, False);
}

void decoration_change_scheme(struct HSClient* client,
                              HSDecorationScheme scheme) {
    if (client->dec.last_rect.width < 0) {
        // TODO: do something useful here
        return;
    }
    if (client->dec.last_rect_inner) {
        decoration_resize_inner(client, client->dec.last_rect, scheme);
    } else {
        decoration_resize_outline(client, client->dec.last_rect, scheme);
    }
}

