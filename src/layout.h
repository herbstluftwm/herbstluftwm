#ifndef __HERBSTLUFT_LAYOUT_H_
#define __HERBSTLUFT_LAYOUT_H_

#include "x11-types.h"
#include "glib-backports.h"
#include "types.h"
#include <stdlib.h>
#include <X11/Xlib.h>
#include <functional>
#include "monitor.h"
#include "tag.h"
#include "floating.h"
#include "tilingresult.h"
#include "framedecoration.h"

#include <memory>

#define LAYOUT_DUMP_BRACKETS "()" /* must consist of exactly two chars */
#define LAYOUT_DUMP_WHITESPACES " \t\n" /* must be at least one char */
#define LAYOUT_DUMP_SEPARATOR_STR ":" /* must be a string with one char */
#define LAYOUT_DUMP_SEPARATOR LAYOUT_DUMP_SEPARATOR_STR[0]

#define TAG_SET_FLAG(tag, flag) \
    ((tag)->flags |= (flag))

enum {
    TAG_FLAG_URGENT = 0x01, // is there a urgent window?
    TAG_FLAG_USED   = 0x02, // the opposite of empty
};

enum {
    ALIGN_VERTICAL = 0,
    ALIGN_HORIZONTAL,
    // temporary values in split_command
    ALIGN_EXPLODE,
};

enum {
    LAYOUT_VERTICAL = 0,
    LAYOUT_HORIZONTAL,
    LAYOUT_MAX,
    LAYOUT_GRID,
    LAYOUT_COUNT,
};

extern const char* g_align_names[];
extern const char* g_layout_names[];

enum {
    TYPE_CLIENTS = 0,
    TYPE_FRAMES,
};

// execute an action on an client
// returns Success or failure.
class HSClient;
typedef std::function<void(HSClient*)> ClientAction;

#define FRACTION_UNIT 10000

struct HSSlice;
class HSTag;
class HSFrameLeaf;
class HSFrameSplit;
class Settings;

class HSFrame : public std::enable_shared_from_this<HSFrame> {
protected:
    HSFrame(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent);
    virtual ~HSFrame();
public:
    virtual void insertClient(HSClient* client) = 0;
    virtual std::shared_ptr<HSFrame> lookup(const char* path) = 0;
    virtual std::shared_ptr<HSFrameLeaf> frameWithClient(HSClient* client) = 0;
    virtual bool removeClient(HSClient* client) = 0;
    virtual void dump(Output output) = 0;

    virtual bool isFocused();
    virtual std::shared_ptr<HSFrameLeaf> getFocusedFrame() = 0;
    virtual TilingResult computeLayout(Rectangle rect) = 0;
    virtual bool focusClient(HSClient* client) = 0;
    virtual HSClient* focusedClient() = 0;

    // do recursive for each element of the (binary) frame tree
    // if order <= 0 -> action(node); action(left); action(right);
    // if order == 1 -> action(left); action(node); action(right);
    // if order >= 2 -> action(left); action(right); action(node);
    virtual void fmap(void (*onSplit)(HSFrameSplit*), void (*onLeaf)(HSFrameLeaf*), int order) = 0;
    virtual void foreachClient(ClientAction action) = 0;

    std::shared_ptr<HSFrameSplit> getParent() { return parent.lock(); };
    std::shared_ptr<HSFrame> root();
    virtual std::shared_ptr<HSFrameSplit> isSplit() { return std::shared_ptr<HSFrameSplit>(); };
    virtual std::shared_ptr<HSFrameLeaf> isLeaf() { return std::shared_ptr<HSFrameLeaf>(); };
    // count the number of splits to the root with alignment "align"
    virtual int splitsToRoot(int align);

    HSTag* getTag() { return tag; };

    void setVisibleRecursive(bool visible);

    static std::shared_ptr<HSFrameLeaf> getGloballyFocusedFrame();

    friend class HSFrameSplit;
protected:
    HSTag*   tag;
    Settings* settings;
    std::weak_ptr<HSFrameSplit> parent;
};

class HSFrameLeaf : public HSFrame {
public:
    HSFrameLeaf(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent);
    virtual ~HSFrameLeaf();

    // inherited:
    void insertClient(HSClient* client) override;
    std::shared_ptr<HSFrame> lookup(const char* path) override;
    std::shared_ptr<HSFrameLeaf> frameWithClient(HSClient* client) override;
    bool removeClient(HSClient* client) override;
    void moveClient(int new_index);
    void dump(Output output) override;

    std::shared_ptr<HSFrameLeaf> getFocusedFrame() override;
    TilingResult computeLayout(Rectangle rect) override;
    bool focusClient(HSClient* client) override;

    void fmap(void (*onSplit)(HSFrameSplit*), void (*onLeaf)(HSFrameLeaf*), int order) override;
    virtual void foreachClient(ClientAction action) override;


    // own members
    void setSelection(int idx);
    void select(HSClient* client);
    void cycleSelection(int delta);
    void addClients(const std::vector<HSClient*>& vec);


    HSClient* focusedClient() override;

    bool split(int alignment, int fraction, int childrenLeaving = 0);
    int getLayout() { return layout; }
    void setLayout(int l) { layout = l; }
    int getSelection() { return selection; }
    size_t clientCount() { return clients.size(); }
    std::shared_ptr<HSFrame> neighbour(char direction);
    std::vector<HSClient*> removeAllClients();

    std::shared_ptr<HSFrameLeaf> thisLeaf();
    std::shared_ptr<HSFrameLeaf> isLeaf() override { return thisLeaf(); }

    friend class HSFrame;
    void setVisible(bool visible);
    Rectangle lastRect() { return last_rect; }
private:
    // layout algorithms
    TilingResult layoutLinear(Rectangle rect, bool vertical);
    TilingResult layoutHorizontal(Rectangle rect) { return layoutLinear(rect, false); };
    TilingResult layoutVertical(Rectangle rect) { return layoutLinear(rect, true); };
    TilingResult layoutMax(Rectangle rect);
    TilingResult layoutGrid(Rectangle rect);

    // members
    std::vector<HSClient*> clients;
    int     selection;
    int     layout;

    FrameDecoration* decoration;
    Rectangle  last_rect; // last rectangle when being drawn
};

class HSFrameSplit : public HSFrame {
public:
    HSFrameSplit(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent, int align,
                 std::shared_ptr<HSFrame> a, std::shared_ptr<HSFrame> b);
    virtual ~HSFrameSplit();
    // inherited:
    void insertClient(HSClient* client) override;
    std::shared_ptr<HSFrame> lookup(const char* path) override;
    std::shared_ptr<HSFrameLeaf> frameWithClient(HSClient* client) override;
    bool removeClient(HSClient* client) override;
    void dump(Output output) override;

    std::shared_ptr<HSFrameLeaf> getFocusedFrame() override;
    TilingResult computeLayout(Rectangle rect) override;
    bool focusClient(HSClient* client) override;

    void fmap(void (*onSplit)(HSFrameSplit*), void (*onLeaf)(HSFrameLeaf*), int order) override;
    virtual void foreachClient(ClientAction action) override;

    HSClient* focusedClient() override;

    // own members
    virtual int splitsToRoot(int align) override;
    void replaceChild(std::shared_ptr<HSFrame> old, std::shared_ptr<HSFrame> newchild);
    std::shared_ptr<HSFrame> firstChild() { return a; }
    std::shared_ptr<HSFrame> secondChild() { return b; }
    std::shared_ptr<HSFrame> selectedChild() { return selection ? b : a; }
    void swapChildren();
    void adjustFraction(int delta);
    std::shared_ptr<HSFrameSplit> thisSplit();
    std::shared_ptr<HSFrameSplit> isSplit() override { return thisSplit(); }
    int getAlign() { return align; }
    void rotate();
    void swapSelection() { selection = 1 - selection; }
    void setSelection(int s) { selection = s; }
private:
    int align;         // ALIGN_VERTICAL or ALIGN_HORIZONTAL
    std::shared_ptr<HSFrame> a; // first child
    std::shared_ptr<HSFrame> b; // second child

    int selection;
    int fraction; // size of first child relative to whole size
                  // FRACTION_UNIT means full size
                  // FRACTION_UNIT/2 means 50%
};

// globals
extern int* g_frame_gap;
extern int* g_window_gap;

// functions
void layout_init();
void layout_destroy();
// for frames
HSFrame* frame_create_empty(HSFrame* parent, HSTag* parenttag);
void frame_insert_client(HSFrame* frame, HSClient* client);
HSFrame* frame_current_selection();
HSFrame* frame_current_selection_below(HSFrame* frame);
// finds the subframe of frame that contains the window
HSFrameLeaf* find_frame_with_client(HSFrame* frame, HSClient* client);
// removes window from a frame/subframes
// returns true, if window was found. else: false
bool frame_remove_client(HSFrame* frame, HSClient* client);
// destroys a frame and all its childs
// then all Windows in it are collected and returned
// YOU have to g_free the resulting window-buf
void frame_destroy(HSFrame* frame, HSClient*** buf, size_t* count);
bool frame_split(HSFrame* frame, int align, int fraction);
int frame_split_command(int argc, char** argv, Output output);
int frame_change_fraction_command(int argc, char** argv, Output output);

void reset_frame_colors();
HSFrame* get_toplevel_frame(HSFrame* frame);

void print_frame_tree(std::shared_ptr<HSFrame> frame, Output output);
void dump_frame_tree(std::shared_ptr<HSFrame> frame, Output output);
// create apply a described layout to a frame and its subframes
// returns pointer to string that was not parsed yet
// or NULL on an error
char* load_frame_tree(std::shared_ptr<HSFrame> frame, char* description, Output errormsg);
int find_layout_by_name(char* name);
int find_align_by_name(char* name);

int frame_current_bring(int argc, char** argv, Output output);
int frame_current_set_selection(int argc, char** argv);
int frame_current_cycle_selection(int argc, char** argv);
int cycle_all_command(int argc, char** argv);
int cycle_frame_command(int argc, char** argv);
void cycle_frame(int direction, int new_window_index, bool skip_invisible);

void frame_unfocus(); // unfocus currently focused window

// get neighbour in a specific direction 'l' 'r' 'u' 'd' (left, right,...)
// returns the neighbour or NULL if there is no one
HSFrame* frame_neighbour(HSFrame* frame, char direction);
int frame_inner_neighbour_index(std::shared_ptr<HSFrameLeaf> frame, char direction);
int frame_focus_command(int argc, char** argv, Output output);

// follow selection to leaf and focus this frame
void frame_focus_recursive(std::shared_ptr<HSFrame> frame);
void frame_do_recursive(HSFrame* frame, void (*action)(HSFrame*), int order);
void frame_do_recursive_data(HSFrame* frame, void (*action)(HSFrame*,void*),
                             int order, void* data);
int layout_rotate_command();

int frame_current_cycle_client_layout(int argc, char** argv, Output output);
int frame_current_set_client_layout(int argc, char** argv, Output output);
int frame_split_count_to_root(HSFrame* frame, int align);

// returns the Window that is focused
// returns 0 if there is none
HSClient* frame_focused_client(HSFrame* frame);
bool frame_focus_client(HSFrame* frame, HSClient* client);
bool focus_client(HSClient* client, bool switch_tag, bool switch_monitor);
// moves a window to an other frame
int frame_move_window_command(int argc, char** argv, Output output);
/// removes the current frame
int frame_remove_command(int argc, char** argv);
int close_or_remove_command(int argc, char** argv);
int close_and_remove_command(int argc, char** argv);
void frame_set_visible(HSFrame* frame, bool visible);
void frame_update_border(Window window, unsigned long color);

int frame_focus_edge(int argc, char** argv, Output output);
int frame_move_window_edge(int argc, char** argv, Output output);

bool smart_window_surroundings_active(HSFrameLeaf* frame);

#endif

