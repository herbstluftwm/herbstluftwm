/** Copyright 2011-2013 Thorsten Wißmann. All rights reserved.
 *
 * This software is licensed under the "Simplified BSD License".
 * See LICENSE for details */

#ifndef __HERBST_UTILS_H_
#define __HERBST_UTILS_H_

#include "glib-backports.h"
#include "x11-types.h"

#include <stddef.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <array>
#include <memory>


struct ArgList {
    using Container = std::vector<std::string>;

    // a simple split(), as C++ doesn't have it
    static Container split(const std::string &s, char delim = '.');

    // a simple join(), as C++ doesn't have it
    static std::string join(Container::const_iterator first,
                            Container::const_iterator last,
                            char delim = '.');

    ArgList(const std::initializer_list<std::string> &l);
    ArgList(const Container &c);
    // constructor that splits the given string
    ArgList(const std::string &s, char delim = '.');
    // operator to obtain shifted version of list (shallow copy)
    ArgList operator+(size_t shift_amount);
    std::string operator[](size_t idx);

    Container::const_iterator begin() const { return begin_; }
    Container::const_iterator end() const { return c_->cend(); }
    const std::string& front() { return *begin_; }
    const std::string& back() { return c_->back(); }
    bool empty() const { return begin_ == c_->end(); }
    Container::size_type size() const { return std::distance(begin_, c_->cend()); }

    std::string join(char delim = '.');

    void reset() { begin_ = c_->cbegin(); }
    void shift(size_t amount = 1) {
        begin_ += std::min(amount, (size_t)std::distance(begin_, c_->cend()));
    }
    Container toVector() const {
        return Container(begin_, c_->cend());
    }

protected:
    static void split(Container &ret, const std::string &s, char delim = '.');

    Container::const_iterator begin_;
    /* shared pointer to make object copy-able:
     * 1. payload is shared (no redundant copies)
     * 2. begin_ stays valid
     */
    std::shared_ptr<Container> c_;
};

using Path = ArgList;


// STRTODO: move this into the herbstluftwm namespace
using Input = ArgList;
using Output = std::ostream&;

#define LENGTH(X) (sizeof(X)/sizeof(*X))
#define SHIFT(ARGC, ARGV) (--(ARGC) && ++(ARGV))
#define MOD(X, N) ((((X) % (signed)(N)) + (signed)(N)) % (signed)(N))

#define container_of(ptr, type, member) \
    ((type *)( (char *)(ptr)- offsetof(type,member) ))
#define DBGDO(X) { if(1) std::cerr << "DBGDO: " << (#X) << " = " << (X) << std::endl; }

// control structures
#define FOR(i,a,b) for (int i = (a); i < (b); i++)
#define SWAP(TYPE,a,b) do { \
            TYPE TMPNAME = (a); \
            (a) = (b); \
            (b) = TMPNAME; \
        } while(0);

/// print a printf-like message to stderr and exit
void die(const char *errstr, ...);

#define ATOM(A) XInternAtom(g_display, (A), False)

GString* window_property_to_g_string(Display* dpy, Window window, Atom atom);
GString* window_class_to_g_string(Display* dpy, Window window);
GString* window_instance_to_g_string(Display* dpy, Window window);
int window_pid(Display* dpy, Window window);

typedef void* HSTree;
struct HSTreeInterface;
typedef struct HSTreeInterface {
    struct HSTreeInterface  (*nth_child)(HSTree root, size_t idx);
    size_t                  (*child_count)(HSTree root);
    void                    (*append_caption)(HSTree root, Output output);
    HSTree                  data;
    void                    (*destructor)(HSTree data); /* how to free the data tree */
} HSTreeInterface;

class TreeInterface {
public:
    TreeInterface() {};
    virtual ~TreeInterface() {};
    virtual Ptr(TreeInterface) nthChild(size_t idx) = 0;
    virtual size_t             childCount() = 0;
    virtual void               appendCaption(Output output) = 0;
};

void tree_print_to(Ptr(TreeInterface) intface, Output output);
void tree_print_to(HSTreeInterface* intface, Output output);


bool is_herbstluft_window(Display* dpy, Window window);

bool is_window_mapable(Display* dpy, Window window);
bool is_window_mapped(Display* dpy, Window window);

bool window_has_property(Display* dpy, Window window, char* prop_name);

bool string_to_bool_error(const char* string, bool oldvalue, bool* error);
bool string_to_bool(const char* string, bool oldvalue);

const char* strlasttoken(const char* str, const char* delim);

time_t get_monotonic_timestamp();

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv);
// frees all entries in argument-vector and then the vector itself
void argv_free(int argc, char** argv);

Rectangle parse_rectangle(char* string);

void g_queue_remove_element(GQueue* queue, GList* elem);

// find an element in an array buf with elems elements of size size.
int array_find(const void* buf, size_t elems, size_t size, const void* needle);
void array_reverse(void* void_buf, size_t elems, size_t size);

template<class T, int S> struct ArrayInitializer {
    ArrayInitializer(std::initializer_list<std::pair<int,T> > il) {
	for (auto i = il.begin(); i != il.end(); i++) {
	    a[i->first] = i->second;
	}
    }

    std::array<T, S> a;
};

// utils for tables
typedef bool (*MemberEquals)(void* pmember, const void* needle);
bool memberequals_string(void* pmember, const void* needle);
bool memberequals_int(void* pmember, const void* needle);

void* table_find(void* start, size_t elem_size, size_t count,
                 size_t member_offset, MemberEquals equals, const void* needle);

void set_window_double_border(Display *dpy, Window win, int ibw,
                              unsigned long inner_color, unsigned long outer_color);

#define STATIC_TABLE_FIND(TYPE, TABLE, MEMBER, EQUALS, NEEDLE)  \
    ((TYPE*) table_find((TABLE),                                \
                        sizeof(TABLE[0]),                       \
                        LENGTH((TABLE)),                        \
                        offsetof(TYPE, MEMBER),                 \
                        EQUALS,                                 \
                        (NEEDLE)))

#define STATIC_TABLE_FIND_STR(TYPE, TABLE, MEMBER, NEEDLE)  \
    STATIC_TABLE_FIND(TYPE, TABLE, MEMBER, memberequals_string, NEEDLE)

#define INDEX_OF(ARRAY, PELEM) \
    (((char*)(PELEM) - (char*)(ARRAY)) / (sizeof (*ARRAY)))

// returns the unichar in GSTR at position GSTR

size_t      utf8_string_length(const std::string& str);
std::string utf8_string_at(const std::string& str, size_t offset);

#define RECTANGLE_EQUALS(a, b) (\
        (a).x == (b).x &&   \
        (a).y == (b).y &&   \
        (a).width == (b).width &&   \
        (a).height == (b).height    \
    )

// returns an posix sh escaped string or NULL if there is nothing to escape
// if a new string is returned, then the caller has to free it
char* posix_sh_escape(const char* source);
// does the reverse action to posix_sh_escape by modifing the string
void posix_sh_compress_inplace(char* str);

#endif

