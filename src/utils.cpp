#include "utils.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring> // for strerror()
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "globals.h"
#include "settings.h"

#if defined(__MACH__) && ! defined(CLOCK_REALTIME)
#include <mach/clock.h>
#include <mach/mach.h>
#endif

using std::endl;
using std::shared_ptr;
using std::stringstream;
using std::string;
using std::vector;

time_t get_monotonic_timestamp() {
    struct timespec ts{};
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

int MOD(int x, int n) {
    // for the tests of this, see tests of cycle_monitor in test_monitor.py
    if (n > 0) {
        while (x < 0) {
            x += n;
        }
    }
    return (((x % n) + n) % n);
}

string window_class_to_string(Display* dpy, Window window) {
    XClassHint hint;
    if (0 == XGetClassHint(dpy, window, &hint)) {
        return "";
    }
    string str = hint.res_class ? hint.res_class : "";
    if (hint.res_name) {
        XFree(hint.res_name);
    }
    if (hint.res_class) {
        XFree(hint.res_class);
    }
    return str;
}

bool is_herbstluft_window(Display* dpy, Window window) {
    auto str = window_class_to_string(dpy, window);
    return str == HERBST_FRAME_CLASS || str == HERBST_DECORATION_CLASS;
}

// duplicates an argument-vector
char** argv_duplicate(int argc, char** argv) {
    if (argc <= 0) {
        return nullptr;
    }
    char** new_argv = new char*[argc];
    int i;
    for (i = 0; i < argc; i++) {
        new_argv[i] = strdup(argv[i]);
    }
    return new_argv;
}

// tells if the intervals [a_left, a_right) [b_left, b_right) intersect
bool intervals_intersect(int a_left, int a_right, int b_left, int b_right) {
    return (b_left < a_right) && (a_left < b_right);
}

size_t utf8_string_length(const string& str) {
   // utf-strlen from stackoverflow:
   // http://stackoverflow.com/questions/5117393/utf-8-strings-length-in-linux-c
   size_t i = 0, j = 0;
   while (str[i]) {
       // count all the non-continuation bytes
       if (!utf8_is_continuation_byte(str[i])) {
           j++;
       }
     i++;
   }
   return j;
}

string utf8_string_at(const string& str, size_t offset) {
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
    //    std::cout << "\'"<< ch << "\' -> " << ((ch&0xc0) == 0x80) << endl;
    //}
    size_t i = 0, byte_offset = 0;
    string result;
    // find the beginning of the n'th character
    // find the n'th character ch, with (ch & 0xc0) == 0x80
    while (i < offset) {
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

/**
 * \brief   emulates a double window border through the border pixmap mechanism
 */
void set_window_double_border(Display *dpy, Window win, int ibw,
                              unsigned long inner_color,
                              unsigned long outer_color)
{
    XWindowAttributes wa;

    if (!XGetWindowAttributes(dpy, win, &wa)) {
        return;
    }

    int bw = wa.border_width;

    if (bw < 2 || ibw >= bw || ibw < 1) {
        return;
    }

    int width = wa.width;
    int height = wa.height;

    auto depth = (unsigned)wa.depth;

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

    // use intermediates for casting (to avoid narrowing)
    short fw_ibw = full_width - ibw, fh_ibw = full_height - ibw;
    unsigned short uibw = ibw, h_ibw = height + ibw, w_ibw = width + ibw;
    vector<XRectangle> rectangles{
        { (short)width, 0, uibw, h_ibw },
        { fw_ibw, 0, uibw, h_ibw },
        { 0, (short)height, w_ibw, uibw },
        { 0, fh_ibw, w_ibw, uibw },
        { fw_ibw, fh_ibw, uibw, uibw }
    };

    Pixmap pix = XCreatePixmap(dpy, win, full_width, full_height, depth);
    GC gc = XCreateGC(dpy, pix, 0, nullptr);

    /* outer border */
    XSetForeground(dpy, gc, outer_color);
    XFillRectangle(dpy, pix, gc, 0, 0, full_width, full_height);

    /* inner border */
    XSetForeground(dpy, gc, inner_color);
    XFillRectangles(dpy, pix, gc, &rectangles.front(), rectangles.size());

    XSetWindowBorderPixmap(dpy, win, pix);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, pix);
}

static void subtree_print_to(shared_ptr<TreeInterface> intface, const string& indent,
                          const string& rootprefix, Output output) {
    size_t child_count = intface->childCount();
    string tree_style = g_settings->tree_style();
    if (child_count == 0) {
        output << rootprefix;
        output << utf8_string_at(tree_style, 6);
        output << utf8_string_at(tree_style, 5);
        intface->appendCaption(output);
        output << "\n";
    } else {
        output << rootprefix;
        output << utf8_string_at(tree_style, 6);
        output << utf8_string_at(tree_style, 7);
        intface->appendCaption(output);
        output << '\n';
        // append children
        string child_indent;
        string child_prefix;
        for (size_t i = 0; i < child_count; i++) {
            bool last = (i == child_count - 1);
            child_indent =  indent + " ";
            child_indent += utf8_string_at(tree_style, last ? 2 : 1);
            child_prefix = indent + " ";
            child_prefix += utf8_string_at(tree_style, last ? 4 : 3);
            shared_ptr<TreeInterface> child = intface->nthChild(i);
            subtree_print_to(child, child_indent,
                             child_prefix, output);
        }
    }
}

void tree_print_to(shared_ptr<TreeInterface> intface, Output output) {
    string rootIndicator;
    rootIndicator += utf8_string_at(g_settings->tree_style(), 0);
    subtree_print_to(intface, " ", rootIndicator, output);
}

string posix_sh_escape(const string& source) {
    if (source.empty()) {
        return "\'\'";
    }
    string target = "";
    for (char ch : source) {
        // escape everything in ESCAPE_CHARACTERS
        bool needsEscape = strchr(ESCAPE_CHARACTERS, ch);
        // and escape a tilde at the beginning of a string
        needsEscape = needsEscape || (target.empty() && ch == '~');
        if (needsEscape) {
            target += '\\';
        }
        target += ch;
    }
    return target;
}

/**
 * @brief Remove all characters at the end of 'source' that are in 'charsToRemove'
 * @param source the source string
 * @param charsToRemove characters to remove at the end of 'source'
 * @return the trimmed source
 */
string trimRight(const string& source, const string& charsToRemove)
{

    size_t endpos = source.find_last_not_of(charsToRemove);
    if (string::npos != endpos) {
        return source.substr(0, endpos + 1);
    } else {
        // 'source' only consists of characters from 'charsToRemove'
        return "";
    }
}

/**
 * @brief A c++ interface to execvp
 * @param command The command for execvp()
 * @return If there is an error, this function returns the error number.
 *         On success, this function does not return.
 */
int execvp_helper(const vector<string>& command) {
    // duplicate the vector to have space for the terminating nullptr entry:
    char** exec_args = new char*[command.size() + 1];
    for (size_t i = 0; i < command.size(); i++) {
        exec_args[i] = const_cast<char*>(command[i].c_str());
    }
    exec_args[command.size()] = nullptr;
    execvp(exec_args[0], exec_args);
    int errnum = errno;
    delete[] exec_args;
    return errnum;
}

/**
 * @brief spawn a new process for the given command
 * @param command the command to execute, this must be non-empty
 * @param retChildPid on success, the child pid is written
 * @return {} on success or an error message if
 *         the command could not be spawned.
 */
string spawnProcess(const vector<string>& command, pid_t* retChildPid)
{
    int readAndWriteFd[2]; // 0: for reading, 1: for writing
    // create a pipe to send information from the child process
    // to the parent process. Mark the file descriptors as FD_CLOEXEC
    // such that they are closed on a successful execvp().
    if (pipe(readAndWriteFd) == -1
        || fcntl(readAndWriteFd[0], F_SETFD, FD_CLOEXEC) == -1
        || fcntl(readAndWriteFd[1], F_SETFD, FD_CLOEXEC) == -1
        )
    {
        return "Can not create pipe for communication with child process";
    }
    pid_t pid = fork();
    if (pid == -1) {
        return "Can not fork";
    }
    if (pid == 0) {
        // in the child:
        setsid();
        int errnum = execvp_helper(command);
        // if exec failed: send the error number to the parent process
        close(readAndWriteFd[0]); // close the reading side
        size_t written = write(readAndWriteFd[1], &errnum, sizeof(errnum));
        if (written != sizeof(errnum)) {
            std::cerr << "Failure when writing to pipe to parent process" << endl;
        }
        close(readAndWriteFd[1]);
        exit(0);
    } else {
        // in the parent process (hlwm):
        // first close the writing end of the pipe such that
        // the reading end receives EOF if the child also closes
        // the writing end.
        close(readAndWriteFd[1]);
        int errnum;
        int count = read(readAndWriteFd[0], &errnum, sizeof(int));
        close(readAndWriteFd[0]);
        if (count == sizeof(int)) {
            // exec failed, because an error number was sent through the pipe:
            stringstream msg;
            msg << "Executing \"" << command[0] << "\" failed: "
                << trimRight(strerror(errnum), "\n");
            return msg.str();
        }
        // success, because the pipe was closed by exec
        if (retChildPid) {
            *retChildPid = pid;
        }
        return {};
    }
}

