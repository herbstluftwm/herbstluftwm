#ifndef __HERBST_UTILS_H_
#define __HERBST_UTILS_H_

#include <X11/X.h>
#include <X11/Xlib.h>
#include <time.h>
#include <array>
#include <cstddef>
#include <sstream>
#include <string>

#include "commandio.h"

#define LENGTH(X) (sizeof(X)/sizeof(*(X)))
#define SHIFT(ARGC, ARGV) (--(ARGC) && ++(ARGV))

// CLAMP taken from GLib:
#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

int MOD(int x, int n);

#define container_of(ptr, type, member) \
    ((type *)( (char *)(ptr)- offsetof(type,member) ))

// control structures
#define SWAP(TYPE,a,b) do { \
            TYPE TMPNAME = (a); \
            (a) = (b); \
            (b) = TMPNAME; \
        } while(0);

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

time_t get_monotonic_timestamp();

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv);

// tells if the intervals [a_left, a_right) [b_left, b_right) intersect
bool intervals_intersect(int a_left, int a_right, int b_left, int b_right);

template<class T, int S> struct ArrayInitializer {
    ArrayInitializer(std::initializer_list<std::pair<int,T> > il) {
	for (auto i = il.begin(); i != il.end(); i++) {
	    a[i->first] = i->second;
	}
    }

    std::array<T, S> a;
};

// utils for tables
void set_window_double_border(Display *dpy, Window win, int ibw,
                              unsigned long inner_color, unsigned long outer_color);

// returns the unichar in GSTR at position GSTR

size_t      utf8_string_length(const std::string& str);
std::string utf8_string_at(const std::string& str, size_t offset);
/**
 * @brief in utf8, a single unicode character may be spread over
 * multiple bytes. This function tells whether a given byte
 * is part of such a sequence but not the first one.
 * see also https://stackoverflow.com/a/9356203/4400896
 * @param ch
 * @return
 */
inline bool utf8_is_continuation_byte(char ch) { return (ch & 0xc0) == 0x80; }

#define RECTANGLE_EQUALS(a, b) (\
        (a).x == (b).x &&   \
        (a).y == (b).y &&   \
        (a).width == (b).width &&   \
        (a).height == (b).height    \
    )

// returns an posix sh escaped string or NULL if there is nothing to escape
// if a new string is returned, then the caller has to free it
std::string posix_sh_escape(const std::string& source);

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
    for (auto iter = first + 1; iter != last; iter++) {
        out << delim << *iter;
    }
    return out.str();
}

std::string trimRight(const std::string& source, const std::string& charsToRemove);

int execvp_helper(const std::vector<std::string>& command);
std::string spawnProcess(const std::vector<std::string>& command, pid_t* retChildPid = nullptr);

#endif

