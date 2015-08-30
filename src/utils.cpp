/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#include "globals.h"
#include "utils.h"
// standard
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// gui
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "glib-backports.h"

#include <time.h>
#include <sys/time.h>

#if defined(__MACH__) && ! defined(CLOCK_REALTIME)
#include <mach/clock.h>
#include <mach/mach.h>
#endif

namespace herbstluft {

using namespace std;

ArgList::ArgList(const std::initializer_list<std::string> &l)
    : c_(std::make_shared<Container>(l)) { reset(); }

ArgList::ArgList(const ArgList::Container &c) : c_(std::make_shared<Container>(c)) { reset(); }

ArgList::ArgList(const std::string &s, char delim) {
    c_ = std::make_shared<Container>();
    split(*c_, s, delim);
    reset();
}

ArgList ArgList::operator+(size_t shift_amount) {
    ArgList ret(*this);
    ret.shift(shift_amount);
    return ret;
}

void ArgList::split(Container &ret, const std::string &s, char delim) {
    std::stringstream tmp(s);
    std::string item;
    while (std::getline(tmp, item, delim))
        if (!item.empty()) // case for trailing delimiter
            ret.push_back(item);
}

ArgList::Container ArgList::split(const std::string &s, char delim) {
    Container ret;
    split(ret, s, delim);
    return ret;
}

std::string ArgList::join(ArgList::Container::const_iterator first,
                          ArgList::Container::const_iterator last,
                          char delim) {
    if (first == last)
        return {};
    std::stringstream tmp;
    tmp << *first;
    for (auto it = first + 1; it != last; ++it)
        tmp << delim << *it;
    return tmp.str();
}

}

using namespace herbstluft;

// globals
extern char*   g_tree_style; /* the one from layout.c */

time_t get_monotonic_timestamp() {
    struct timespec ts;
#if defined(__MACH__) && ! defined(CLOCK_REALTIME) // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return ts.tv_sec;
}

/// print a printf-like message to stderr and exit
// from dwm.c
void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

// inspired by dwm's gettextprop()
GString* window_property_to_g_string(Display* dpy, Window window, Atom atom) {
    GString* result = NULL;
    char** list = NULL;
    int n = 0;
    XTextProperty prop;

    if (0 == XGetTextProperty(dpy, window, &prop, atom)) {
        return NULL;
    }
    // convert text property to a gstring
    if (prop.encoding == XA_STRING
        || prop.encoding == XInternAtom(dpy, "UTF8_STRING", False)) {
        result = g_string_new((char*)prop.value);
    } else {
        if (XmbTextPropertyToTextList(dpy, &prop, &list, &n) >= Success
            && n > 0 && *list)
        {
            result = g_string_new(*list);
            XFreeStringList(list);
        }
    }
    XFree(prop.value);
    return result;
}

GString* window_class_to_g_string(Display* dpy, Window window) {
    XClassHint hint;
    if (0 == XGetClassHint(dpy, window, &hint)) {
        return g_string_new("");
    }
    GString* string = g_string_new(hint.res_class ? hint.res_class : "");
    if (hint.res_name) XFree(hint.res_name);
    if (hint.res_class) XFree(hint.res_class);
    return string;
}

GString* window_instance_to_g_string(Display* dpy, Window window) {
    XClassHint hint;
    if (0 == XGetClassHint(dpy, window, &hint)) {
        return g_string_new("");
    }
    GString* string = g_string_new(hint.res_name ? hint.res_name : "");
    if (hint.res_name) XFree(hint.res_name);
    if (hint.res_class) XFree(hint.res_class);
    return string;
}


bool is_herbstluft_window(Display* dpy, Window window) {
    GString* string = window_class_to_g_string(dpy, window);
    bool result;
    result = !strcmp(string->str, HERBST_FRAME_CLASS);
    g_string_free(string, true);
    return result;
}

bool is_window_mapable(Display* dpy, Window window) {
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, window,  &wa);
    return (wa.map_state == IsUnmapped);
}
bool is_window_mapped(Display* dpy, Window window) {
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, window,  &wa);
    return (wa.map_state == IsViewable);
}

bool window_has_property(Display* dpy, Window window, char* prop_name) {
    // find the properties this window has
    int num_properties_ret;
    Atom* properties= XListProperties(g_display, window, &num_properties_ret);

    bool atom_found = false;
    char* name;
    for(int i = 0; i < num_properties_ret; i++) {
        name = XGetAtomName(g_display, properties[i]);
        if(!strcmp(prop_name, name)) {
            atom_found = true;
            break;
        }
        XFree(name);
    }
    XFree(properties);

    return atom_found;
}

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv) {
    if (argc <= 0) return NULL;
    char** new_argv = new char*[argc];
    if (!new_argv) {
        die("cannot malloc - there is no memory available\n");
    }
    int i;
    for (i = 0; i < argc; i++) {
        new_argv[i] = g_strdup(argv[i]);
    }
    return new_argv;
}

// frees all entries in argument-vector and then the vector itself
void argv_free(int argc, char** argv) {
    if (argc <= 0) return;
    int i;
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    delete[] argv;
}

size_t utf8_string_length(const std::string& str) {
   // utf-strlen from stackoverflow:
   // http://stackoverflow.com/questions/5117393/utf-8-strings-length-in-linux-c
   size_t i = 0, j = 0;
   while (str[i]) {
     if ((str[i] & 0xc0) != 0x80) j++;
     i++;
   }
   return j;
}

std::string utf8_string_at(const std::string& str, size_t n) {
    // utf-strlen from stackoverflow:
    // http://stackoverflow.com/questions/5117393/utf-8-strings-length-in-linux-c
    //
    // int i = 0, j = 0;
    // while (s[i]) {
    //   if ((s[i] & 0xc0) != 0x80) j++;
    //     i++;
    // }
    // return j;
    //for (char ch : str) {
    //    std::cout << "\'"<< ch << "\' -> " << ((ch&0xc0) == 0x80) << std::endl;
    //}
    size_t i = 0, byte_offset = 0;
    std::string result;
    // find the beginning of the n'th character
    // find the n'th character ch, with (ch & 0xc0) == 0x80
    while (i < n) {
        // we are at some byte with (ch & 0xc0) != 0x80
        byte_offset++;
        while ((str[byte_offset] & 0xc0) == 0x80) {
            // if its a continuation byte, skip it
            byte_offset++;
        }
        // we are at some byte with (ch & 0xc0) != 0x80 again
        // and its the first byte of the (i+1)'th character
        i++;
    }
    result += str[byte_offset]; // add its first char
    // and add all continuation bytes
    while ((str[++byte_offset] & 0xc0) == 0x80) {
        result += str[byte_offset];
    }
    return result;
}

const char* strlasttoken(const char* str, const char* delim) {
    const char* next = str;
    while ((next = strpbrk(str, delim))) {
        next++;;
        str = next;
    }
    return str;
}


bool string_to_bool(const char* string, bool oldvalue) {
    return string_to_bool_error(string, oldvalue, NULL);
}

bool string_to_bool_error(const char* string, bool oldvalue, bool* error) {
    bool val = oldvalue;
    if (error) {
        *error = false;
    }
    if (!strcmp(string, "on")) {
        val = true;
    } else if (!strcmp(string, "off")) {
        val = false;
    } else if (!strcmp(string, "true")) {
        val = true;
    } else if (!strcmp(string, "false")) {
        val = false;
    } else if (!strcmp(string, "toggle")) {
        val = ! oldvalue;
    } else if (error) {
        *error = true;
    }
    return val;
}

int window_pid(Display* dpy, Window window) {
    Atom type;
    int format;
    unsigned long items, remain;
    int* buf;
    int status = XGetWindowProperty(dpy, window,
        ATOM("_NET_WM_PID"), 0, 1, False,
        XA_CARDINAL, &type, &format,
        &items, &remain, (unsigned char**)&buf);
    if (items == 1 && format == 32 && remain == 0
        && type == XA_CARDINAL && status == Success) {
        int value = *buf;
        XFree(buf);
        return value;
    } else {
        return -1;
    }
}

void g_queue_remove_element(GQueue* queue, GList* elem) {
    if (queue->length <= 0) {
        return;
    }
    bool was_tail = (queue->tail == elem);
    GList* before_elem = elem->prev;

    queue->head = g_list_delete_link(queue->head, elem);
    queue->length--;

    // reset pointers
    if (was_tail) {
        queue->tail = before_elem;
    }
}

int array_find(const void* buf, size_t elems, size_t size, const void* needle) {
    for (int i = 0; i < elems; i++) {
        if (0 == memcmp((const char*)buf + (size * i), needle, size)) {
            return i;
        }
    }
    return -1;
}

void array_reverse(void* void_buf, size_t elems, size_t size) {
    char* buf = (char*)void_buf;
    char* tmp = new char[size];
    for (int i = 0, j = elems - 1; i < j; i++, j--) {
        memcpy(tmp, buf + size * i, size);
        memcpy(buf + size * i, buf + size * j, size);
        memcpy(buf + size * j, tmp, size);
    }
    delete[] tmp;
}


/**
 * \brief   tells if the string needle is identical to the string *pmember
 */
bool memberequals_string(void* pmember, const void* needle) {
    return !strcmp(*(char**)pmember, (const char*)needle);
}

/**
 * \brief   tells if the ints pointed by pmember and needle are identical
 */
bool memberequals_int(void* pmember, const void* needle) {
    return (*(int*)pmember) == (*(const int*)needle);
}

/**
 * \brief   finds an element in a table (i.e. array of structs)
 *
 *          it consecutively searches from the beginning of the table for a
 *          table element whose member is equal to needle. It passes a pointer
 *          to the member and needle to the equals-function consecutively until
 *          equals returns something != 0.
 *
 * \param   start           address of the first element in the table
 * \param   elem_size       offset between two elements
 * \param   count           count of elements in that table
 * \param   member_offset   offset of the member that is used to compare
 * \param   equals          function that tells if the two values are equal
 * \param   needle          second parameter to equals
 * \return                  the found element or NULL
 */
void* table_find(void* start, size_t elem_size, size_t count,
                 size_t member_offset, MemberEquals equals, const void* needle)
{
    char* cstart = (char*) start;
    while (count > 0) {
        /* check the element */
        if (equals(cstart + member_offset, needle)) {
            return cstart;
        }
        /* go to the next element */
        cstart += elem_size;
        count--;
    }
    return NULL;
}

/**
 * \brief   emulates a double window border through the border pixmap mechanism
 */
void set_window_double_border(Display *dpy, Window win, int ibw,
                              unsigned long inner_color,
                              unsigned long outer_color)
{
    XWindowAttributes wa;

    if (!XGetWindowAttributes(dpy, win, &wa))
        return;

    int bw = wa.border_width;

    if (bw < 2 || ibw >= bw || ibw < 1)
        return;

    int width = wa.width;
    int height = wa.height;

    unsigned int depth = wa.depth;

    int full_width = width + 2 * bw;
    int full_height = height + 2 * bw;

    // the inner border is represented through the following pattern:
    //
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //                           ██  ██
    //   ██████████████████████████  ██
    //
    //   ██████████████████████████  ██

    XRectangle rectangles[] =
    {
        { width, 0, ibw, height + ibw },
        { full_width - ibw, 0, ibw, height + ibw },
        { 0, height, width + ibw, ibw },
        { 0, full_height - ibw, width + ibw, ibw },
        { full_width - ibw, full_height - ibw, ibw, ibw }
    };

    Pixmap pix = XCreatePixmap(dpy, win, full_width, full_height, depth);
    GC gc = XCreateGC(dpy, pix, 0, NULL);

    /* outer border */
    XSetForeground(dpy, gc, outer_color);
    XFillRectangle(dpy, pix, gc, 0, 0, full_width, full_height);

    /* inner border */
    XSetForeground(dpy, gc, inner_color);
    XFillRectangles(dpy, pix, gc, rectangles, LENGTH(rectangles));

    XSetWindowBorderPixmap(dpy, win, pix);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, pix);
}

static void subtree_print_to(HSTreeInterface* intface, const char* indent,
                          char* rootprefix, Output output) {
    HSTree root = intface->data;
    size_t child_count = intface->child_count(root);
    if (child_count == 0) {
        output << rootprefix;
        output << utf8_string_at(g_tree_style, 6);
        output << utf8_string_at(g_tree_style, 5);
        output << ' ';
        // append caption
        intface->append_caption(root, output);
        output << "\n";
    } else {
        output << rootprefix;
        output << utf8_string_at(g_tree_style, 6);
        output << utf8_string_at(g_tree_style, 7);
        // append caption
        output << ' ';
        intface->append_caption(root, output);
        output << '\n';
        // append children
        GString* child_indent = g_string_new("");
        GString* child_prefix = g_string_new("");
        for (size_t i = 0; i < child_count; i++) {
            bool last = (i == child_count - 1);
            g_string_printf(child_indent, "%s ", indent);
            g_string_append(child_indent,
                utf8_string_at(g_tree_style, last ? 2 : 1).c_str());
            g_string_printf(child_prefix, "%s ", indent);
            g_string_append(child_prefix,
                utf8_string_at(g_tree_style, last ? 4 : 3).c_str());
            HSTreeInterface child = intface->nth_child(root, i);
            subtree_print_to(&child, child_indent->str,
                             child_prefix->str, output);
            if (child.destructor) {
                child.destructor(child.data);
            }
        }
        g_string_free(child_indent, true);
        g_string_free(child_prefix, true);

    }
}

static void subtree_print_to(Ptr(TreeInterface) intface, const string& indent,
                          const string& rootprefix, Output output) {
    size_t child_count = intface->childCount();
    if (child_count == 0) {
        output << rootprefix;
        output << utf8_string_at(g_tree_style, 6);
        output << utf8_string_at(g_tree_style, 5);
        output << ' ';
        // append caption
        intface->appendCaption(output);
        output << "\n";
    } else {
        output << rootprefix;
        output << utf8_string_at(g_tree_style, 6);
        output << utf8_string_at(g_tree_style, 7);
        // append caption
        output << ' ';
        intface->appendCaption(output);
        output << '\n';
        // append children
        string child_indent;
        string child_prefix;
        for (size_t i = 0; i < child_count; i++) {
            bool last = (i == child_count - 1);
            child_indent =  indent + " ";
            child_indent += utf8_string_at(g_tree_style, last ? 2 : 1);
            child_prefix = indent + " ";
            child_prefix += utf8_string_at(g_tree_style, last ? 4 : 3);
            Ptr(TreeInterface) child = intface->nthChild(i);
            subtree_print_to(child, child_indent,
                             child_prefix, output);
        }
    }
}

void tree_print_to(Ptr(TreeInterface) intface, Output output) {
    string rootIndicator;
    rootIndicator += utf8_string_at(g_tree_style, 0);
    subtree_print_to(intface, " ", rootIndicator, output);
}

void tree_print_to(HSTreeInterface* intface, Output output) {
    GString* root_indicator = g_string_new("");
    g_string_append(root_indicator, utf8_string_at(g_tree_style, 0).c_str());
    subtree_print_to(intface, " ", root_indicator->str, output);
    g_string_free(root_indicator, true);
}

char* posix_sh_escape(const char* source) {
    size_t count = 0;
    int i;
    for (i = 0; source[i]; i++) {
        int j = LENGTH(ESCAPE_CHARACTERS) - 1; // = strlen(ESCAPE_CHARACTERS)
        slow_assert(j == strlen(ESCAPE_CHARACTERS));
        while (j--) {
            slow_assert(0 <= j && j < strlen(ESCAPE_CHARACTERS));
            if (source[i] == ESCAPE_CHARACTERS[j]) {
                count++;
                break;
            }
        }
    }
    size_t source_len = i;
    // special chars:
    if (source[0] == '~') {
        count++;
    }
    // if there is nothing to escape
    if (count == 0) return NULL;
    // TODO migrate to new
    char* target = (char*)malloc(sizeof(char) * (count + source_len + 1));
    if (!target) {
        die("cannot malloc - there is no memory available\n");
    }

    // do the actual escaping
    // special chars:
    int s = 0; // position in the source
    int t = 0; // position in the target
    slow_assert(s < strlen(source));
    slow_assert(t < (count + source_len));
    if (source[0] == '~') {
        target[t++] = '\\';
        target[t++] = source[s++];
    }
    slow_assert(s < strlen(source));
    slow_assert(t < (count + source_len));
    while (source[s]) {
        // check if we need to escape the next char
        int j = LENGTH(ESCAPE_CHARACTERS) - 1; // = strlen(ESCAPE_CHARACTERS)
        slow_assert(s < strlen(source));
        slow_assert(t < (count + source_len));
        while (j--) {
            if (source[s] == ESCAPE_CHARACTERS[j]) {
                // if source[s] needs to be escape, then put an backslash first
                target[t++] = '\\';
                break;
            }
        }
        slow_assert(s < strlen(source));
        slow_assert(t < (count + source_len));
        // put the actual character
        target[t++] = source[s++];
    }
    slow_assert(s == strlen(source));
    slow_assert(t == (count + source_len));
    // terminate the string
    target[t] = '\0';
    return target;
}

void posix_sh_compress_inplace(char* str) {
    int offset = 0;
    for (int i = 0; true ; i++) {
        if (str[i] == '\\' && str[i + 1] ) {
            str[i + offset] = str[i + 1];
            i++;
            offset --;
        } else {
            str[i + offset] = str[i];
        }
        if (!str[i]) {
            break;
        }
    }
}

