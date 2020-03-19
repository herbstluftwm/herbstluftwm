#ifndef __HERBSTLUFT_LAYOUT_H_
#define __HERBSTLUFT_LAYOUT_H_

#include <cassert>
#include <cstdlib>
#include <functional>
#include <memory>

#include "framedata.h"
#include "tilingresult.h"
#include "types.h"
#include "x11-types.h"

// execute an action on an client
// returns Success or failure.
class Client;
typedef std::function<void(Client*)> ClientAction;

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
    virtual int splitsToRoot(SplitAlign align);

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

class HSFrameLeaf : public HSFrame, public FrameDataLeaf {
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
    void addClients(const std::vector<Client*>& vec, bool atFront = false);


    Client* focusedClient() override;

    bool split(SplitAlign alignment, int fraction, size_t childrenLeaving = 0);
    LayoutAlgorithm getLayout() { return layout; }
    void setLayout(LayoutAlgorithm l) { layout = l; }
    int getSelection() { return selection; }
    size_t clientCount() { return clients.size(); }
    std::shared_ptr<HSFrame> neighbour(Direction direction);
    std::vector<Client*> removeAllClients();

    std::shared_ptr<HSFrameLeaf> thisLeaf();
    std::shared_ptr<HSFrameLeaf> isLeaf() override { return thisLeaf(); }

    friend class HSFrame;
    void setVisible(bool visible);
    Rectangle lastRect() { return last_rect; }
    int getInnerNeighbourIndex(Direction direction);
private:
    friend class FrameTree;
    // layout algorithms
    TilingResult layoutLinear(Rectangle rect, bool vertical);
    TilingResult layoutHorizontal(Rectangle rect) { return layoutLinear(rect, false); };
    TilingResult layoutVertical(Rectangle rect) { return layoutLinear(rect, true); };
    TilingResult layoutMax(Rectangle rect);
    TilingResult layoutGrid(Rectangle rect);

    // members
    FrameDecoration* decoration;
    Rectangle  last_rect; // last rectangle when being drawn
                          // this is only used for 'split explode'
};

class HSFrameSplit : public HSFrame, public FrameDataSplit<HSFrame> {
public:
    HSFrameSplit(HSTag* tag, Settings* settings, std::weak_ptr<HSFrameSplit> parent, int fraction_, SplitAlign align_,
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
    int splitsToRoot(SplitAlign align_) override;
    void replaceChild(std::shared_ptr<HSFrame> old, std::shared_ptr<HSFrame> newchild);
    std::shared_ptr<HSFrame> firstChild() { return a_; }
    std::shared_ptr<HSFrame> secondChild() { return b_; }
    std::shared_ptr<HSFrame> selectedChild() { return selection_ ? b_ : a_; }
    void swapChildren();
    void adjustFraction(int delta);
    std::shared_ptr<HSFrameSplit> thisSplit();
    std::shared_ptr<HSFrameSplit> isSplit() override { return thisSplit(); }
    SplitAlign getAlign() { return align_; }
    void swapSelection() { selection_ = 1 - selection_; }
    void setSelection(int s) { selection_ = s; }
private:
    friend class FrameTree;
};

// functions
int frame_split_command(Input input, Output output);
int frame_change_fraction_command(int argc, char** argv, Output output);

int find_layout_by_name(const char* name);

int frame_current_bring(int argc, char** argv, Output output);

// get neighbour in a specific direction 'l' 'r' 'u' 'd' (left, right,...)
// returns the neighbour or NULL if there is no one
HSFrame* frame_neighbour(HSFrame* frame, char direction);
int frame_focus_command(int argc, char** argv, Output output);

int frame_current_set_client_layout(int argc, char** argv, Output output);
int frame_split_count_to_root(HSFrame* frame, int align);

bool focus_client(Client* client, bool switch_tag, bool switch_monitor);
// moves a window to an other frame
int frame_move_window_command(int argc, char** argv, Output output);

int frame_focus_edge(int argc, char** argv, Output output);
int frame_move_window_edge(int argc, char** argv, Output output);

bool smart_window_surroundings_active(HSFrameLeaf* frame);

#endif

