#ifndef __HERBST_UTILS_H_
#define __HERBST_UTILS_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <array>
#include <cstddef>
#include <ostream>
#include <string>

#include "optional.h"
#include "types.h"

#define LENGTH(X) (sizeof(X)/sizeof(*X))
#define SHIFT(ARGC, ARGV) (--(ARGC) && ++(ARGV))

// CLAMP taken from GLib:
#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

int MOD(int x, int n);

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

#define ATOM(A) XInternAtom(g_display, (A), False)

std::string window_class_to_string(Display* dpy, Window window);
std::experimental::optional<std::string> window_property_to_string(Display* dpy, Window window, Atom atom);
std::string window_instance_to_string(Display* dpy, Window window);
int window_pid(Display* dpy, Window window);

class TreeInterface {
public:
    TreeInterface() = default;
    virtual ~TreeInterface() = default;
    virtual std::shared_ptr<TreeInterface> nthChild(size_t idx) = 0;
    virtual size_t             childCount() = 0;
    virtual void               appendCaption(Output output) = 0;
};

void tree_print_to(std::shared_ptr<TreeInterface> intface, Output output);


bool is_herbstluft_window(Display* dpy, Window window);

bool is_window_mapable(Display* dpy, Window window);
bool is_window_mapped(Display* dpy, Window window);

bool window_has_property(Display* dpy, Window window, char* prop_name);

bool string_to_bool_error(const char* string, bool oldvalue, bool* error);
bool string_to_bool(const char* string, bool oldvalue);
bool string_to_bool(const std::string& string, bool oldvalue);

const char* strlasttoken(const char* str, const char* delim);

time_t get_monotonic_timestamp();

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv);
// frees all entries in argument-vector and then the vector itself
void argv_free(int argc, char** argv);

// tells if the intervals [a_left, a_right) [b_left, b_right) intersect
bool intervals_intersect(int a_left, int a_right, int b_left, int b_right);

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


/**
 *  Substitute for std::make_unique in C++11
 */
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

//! Joins a container of strings with a given delimiter string
template<class InContainer>
std::string join_strings(const InContainer& in, const std::string& delim) {
    auto first = in.begin();
    auto last = in.end();
    if (first == last) {
        return {};
    }

    std::stringstream out;
    out << *first;
    for (auto iter = first + 1; iter != last; iter++)
        out << delim << *iter;
    return out.str();
}


#endif

