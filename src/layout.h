#ifndef __HERBSTLUFT_LAYOUT_H_
#define __HERBSTLUFT_LAYOUT_H_

#include <cassert>
#include <cstdlib>
#include <functional>
#include <memory>

#include "glib-backports.h"
#include "tilingresult.h"
#include "types.h"
#include "x11-types.h"

#define LAYOUT_DUMP_BRACKETS "()" /* must consist of exactly two chars */
#define LAYOUT_DUMP_WHITESPACES " \t\n" /* must be at least one char */
#define LAYOUT_DUMP_SEPARATOR_STR ":" /* must be a string with one char */
#define LAYOUT_DUMP_SEPARATOR LAYOUT_DUMP_SEPARATOR_STR[0]

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
class Client;
typedef std::function<void(Client*)> ClientAction;

#define FRACTION_UNIT 10000

class HSTag;
class HSFrameLeaf;
class HSFrameSplit;
class Settings;

class HSFrame : public std::enable_shared_from_this<HSFrame> {
protected:
    HSFrame(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent);
    virtual ~HSFrame();
public:
    virtual std::shared_ptr<HSFrameLeaf> frameWithClient(Client* client) = 0;
    virtual bool removeClient(Client* client) = 0;

    virtual bool isFocused();
    virtual TilingResult computeLayout(Rectangle rect) = 0;
    virtual Client* focusedClient() = 0;

    // do recursive for each element of the (binary) frame tree
    // if order <= 0 -> action(node); action(left); action(right);
    // if order == 1 -> action(left); action(node); action(right);
    // if order >= 2 -> action(left); action(right); action(node);
    virtual void fmap(std::function<void(HSFrameSplit*)> onSplit,
                      std::function<void(HSFrameLeaf*)> onLeaf, int order) = 0;
    void fmap(std::function<void(HSFrameSplit*)> onSplit,
              std::function<void(HSFrameLeaf*)> onLeaf) {
        fmap(onSplit, onLeaf, 0);
    }
    void foreachClient(ClientAction action);

    std::shared_ptr<HSFrameSplit> getParent() { return parent_.lock(); };
    std::shared_ptr<HSFrame> root();
    // count the number of splits to the root with alignment "align"
    virtual int splitsToRoot(int align);

    HSTag* getTag() { return tag_; };

    void setVisibleRecursive(bool visible);

    static std::shared_ptr<HSFrameLeaf> getGloballyFocusedFrame();

    /*! a case distinction on the type of tree node. If `this` is a
     * HSFrameSplit, then onSplit is called, and otherwise onLeaf is called.
     * The return value is passed through.
     */
    template <typename ReturnType>
    ReturnType switchcase(std::function<ReturnType(std::shared_ptr<HSFrameLeaf>)> onLeaf,
                          std::function<ReturnType(std::shared_ptr<HSFrameSplit>)> onSplit) {
        auto s = isSplit();
        if (s) {
            return onSplit(s);
        }
        // if it is not a split, it must be a leaf
        auto l = isLeaf();
        assert(l);
        return onLeaf(l);
    }

    friend class HSFrameSplit;
    friend class FrameTree;
public: // soon will be protected:
    virtual std::shared_ptr<HSFrameSplit> isSplit() { return std::shared_ptr<HSFrameSplit>(); };
    virtual std::shared_ptr<HSFrameLeaf> isLeaf() { return std::shared_ptr<HSFrameLeaf>(); };
protected:
    HSTag* tag_;
    Settings* settings_;
    std::weak_ptr<HSFrameSplit> parent_;
};

class HSFrameLeaf : public HSFrame {
public:
    HSFrameLeaf(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent);
    ~HSFrameLeaf() override;

    // inherited:
    void insertClient(Client* client);
    std::shared_ptr<HSFrameLeaf> frameWithClient(Client* client) override;
    bool removeClient(Client* client) override;
    void moveClient(int new_index);

    TilingResult computeLayout(Rectangle rect) override;

    virtual void fmap(std::function<void(HSFrameSplit*)> onSplit,
                      std::function<void(HSFrameLeaf*)> onLeaf, int order) override;


    // own members
    void setSelection(int idx);
    void select(Client* client);
    void addClients(const std::vector<Client*>& vec);


    Client* focusedClient() override;

    bool split(int alignment, int fraction, size_t childrenLeaving = 0);
    int getLayout() { return layout; }
    void setLayout(int l) { layout = l; }
    int getSelection() { return selection; }
    size_t clientCount() { return clients.size(); }
    std::shared_ptr<HSFrame> neighbour(Direction direction);
    std::vector<Client*> removeAllClients();

    std::shared_ptr<HSFrameLeaf> thisLeaf();
    std::shared_ptr<HSFrameLeaf> isLeaf() override { return thisLeaf(); }

    friend class HSFrame;
    void setVisible(bool visible);
    Rectangle lastRect() { return last_rect; }
private:
    friend class FrameTree;
    // layout algorithms
    TilingResult layoutLinear(Rectangle rect, bool vertical);
    TilingResult layoutHorizontal(Rectangle rect) { return layoutLinear(rect, false); };
    TilingResult layoutVertical(Rectangle rect) { return layoutLinear(rect, true); };
    TilingResult layoutMax(Rectangle rect);
    TilingResult layoutGrid(Rectangle rect);

    // members
    std::vector<Client*> clients;
    int     selection;
    int     layout;

    FrameDecoration* decoration;
    Rectangle  last_rect; // last rectangle when being drawn
                          // this is only used for 'split explode'
};

class HSFrameSplit : public HSFrame {
public:
    HSFrameSplit(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent, int fraction_, int align_,
                 std::shared_ptr<HSFrame> a_, std::shared_ptr<HSFrame> b_);
    ~HSFrameSplit() override;
    // inherited:
    std::shared_ptr<HSFrameLeaf> frameWithClient(Client* client) override;
    bool removeClient(Client* client) override;

    TilingResult computeLayout(Rectangle rect) override;

    virtual void fmap(std::function<void(HSFrameSplit*)> onSplit,
                      std::function<void(HSFrameLeaf*)> onLeaf, int order) override;

    Client* focusedClient() override;

    // own members
    int splitsToRoot(int align_) override;
    void replaceChild(std::shared_ptr<HSFrame> old, std::shared_ptr<HSFrame> newchild);
    std::shared_ptr<HSFrame> firstChild() { return a_; }
    std::shared_ptr<HSFrame> secondChild() { return b_; }
    std::shared_ptr<HSFrame> selectedChild() { return selection_ ? b_ : a_; }
    void swapChildren();
    void adjustFraction(int delta);
    std::shared_ptr<HSFrameSplit> thisSplit();
    std::shared_ptr<HSFrameSplit> isSplit() override { return thisSplit(); }
    int getAlign() { return align_; }
    void swapSelection() { selection_ = 1 - selection_; }
    void setSelection(int s) { selection_ = s; }
private:
    friend class FrameTree;
    int align_;         // ALIGN_VERTICAL or ALIGN_HORIZONTAL
    std::shared_ptr<HSFrame> a_; // first child
    std::shared_ptr<HSFrame> b_; // second child

    int selection_;
    int fraction_; // size of first child relative to whole size
                  // FRACTION_UNIT means full size
                  // FRACTION_UNIT/2 means 50%
};

// globals
extern int* g_frame_gap;
extern int* g_window_gap;

// functions
void layout_init();
void layout_destroy();

int frame_split_command(Input input, Output output);
int frame_change_fraction_command(int argc, char** argv, Output output);

void reset_frame_colors();

int find_layout_by_name(char* name);
int find_align_by_name(char* name);

int frame_current_bring(int argc, char** argv, Output output);

// get neighbour in a specific direction 'l' 'r' 'u' 'd' (left, right,...)
// returns the neighbour or NULL if there is no one
HSFrame* frame_neighbour(HSFrame* frame, char direction);
int frame_inner_neighbour_index(std::shared_ptr<HSFrameLeaf> frame, Direction direction);
int frame_focus_command(int argc, char** argv, Output output);

// follow selection to leaf and focus this frame
void frame_focus_recursive(std::shared_ptr<HSFrame> frame);
int frame_current_cycle_client_layout(int argc, char** argv, Output output);
int frame_current_set_client_layout(int argc, char** argv, Output output);
int frame_split_count_to_root(HSFrame* frame, int align);

bool focus_client(Client* client, bool switch_tag, bool switch_monitor);
// moves a window to an other frame
int frame_move_window_command(int argc, char** argv, Output output);

int frame_focus_edge(int argc, char** argv, Output output);
int frame_move_window_edge(int argc, char** argv, Output output);

bool smart_window_surroundings_active(HSFrameLeaf* frame);

#endif

