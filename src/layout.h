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
class FrameLeaf;
class FrameSplit;
class Settings;

class Frame : public std::enable_shared_from_this<Frame> {
protected:
    Frame(HSTag* tag, Settings* settings, std::weak_ptr<FrameSplit> parent);
    virtual ~Frame();
public:
    virtual std::shared_ptr<FrameLeaf> frameWithClient(Client* client) = 0;
    virtual bool removeClient(Client* client) = 0;

    virtual bool isFocused();
    virtual TilingResult computeLayout(Rectangle rect) = 0;
    virtual Client* focusedClient() = 0;

    // do recursive for each element of the (binary) frame tree
    // if order <= 0 -> action(node); action(left); action(right);
    // if order == 1 -> action(left); action(node); action(right);
    // if order >= 2 -> action(left); action(right); action(node);
    virtual void fmap(std::function<void(FrameSplit*)> onSplit,
                      std::function<void(FrameLeaf*)> onLeaf, int order) = 0;
    void fmap(std::function<void(FrameSplit*)> onSplit,
              std::function<void(FrameLeaf*)> onLeaf) {
        fmap(onSplit, onLeaf, 0);
    }

    std::shared_ptr<FrameSplit> getParent() { return parent_.lock(); };
    std::shared_ptr<Frame> root();
    // count the number of splits to the root with alignment "align"
    virtual int splitsToRoot(SplitAlign align);

    HSTag* getTag() { return tag_; };

    void setVisibleRecursive(bool visible);

    static std::shared_ptr<FrameLeaf> getGloballyFocusedFrame();

    /*! a case distinction on the type of tree node. If `this` is a
     * FrameSplit, then onSplit is called, and otherwise onLeaf is called.
     * The return value is passed through.
     */
    template <typename ReturnType>
    ReturnType switchcase(std::function<ReturnType(std::shared_ptr<FrameLeaf>)> onLeaf,
                          std::function<ReturnType(std::shared_ptr<FrameSplit>)> onSplit) {
        auto s = isSplit();
        if (s) {
            return onSplit(s);
        }
        // if it is not a split, it must be a leaf
        auto l = isLeaf();
        assert(l);
        return onLeaf(l);
    }

    Rectangle lastRect() { return last_rect; }

    friend class FrameSplit;
    friend class FrameTree;
    friend class HSTag; // for HSTag::foreachClient()
public: // soon will be protected:
    virtual std::shared_ptr<FrameSplit> isSplit() { return std::shared_ptr<FrameSplit>(); };
    virtual std::shared_ptr<FrameLeaf> isLeaf() { return std::shared_ptr<FrameLeaf>(); };
protected:
    void foreachClient(ClientAction action);
    HSTag* tag_;
    Settings* settings_;
    std::weak_ptr<FrameSplit> parent_;
    Rectangle  last_rect; // last rectangle when being drawn
                          // this is only used for 'split explode'
};

class FrameLeaf : public Frame, public FrameDataLeaf {
public:
    FrameLeaf(HSTag* tag, Settings* settings, std::weak_ptr<FrameSplit> parent);
    ~FrameLeaf() override;

    // inherited:
    void insertClient(Client* client, bool focus = false);
    std::shared_ptr<FrameLeaf> frameWithClient(Client* client) override;
    bool removeClient(Client* client) override;
    void moveClient(int new_index);

    TilingResult computeLayout(Rectangle rect) override;

    virtual void fmap(std::function<void(FrameSplit*)> onSplit,
                      std::function<void(FrameLeaf*)> onLeaf, int order) override;


    // own members
    void setSelection(int index);
    void select(Client* client);
    void addClients(const std::vector<Client*>& vec, bool atFront = false);


    Client* focusedClient() override;

    bool split(SplitAlign alignment, FixPrecDec fraction, size_t childrenLeaving = 0);
    LayoutAlgorithm getLayout() { return layout; }
    void setLayout(LayoutAlgorithm l) { layout = l; }
    int getSelection() { return selection; }
    size_t clientCount() { return clients.size(); }
    std::shared_ptr<Frame> neighbour(Direction direction);
    std::vector<Client*> removeAllClients();

    std::shared_ptr<FrameLeaf> thisLeaf();
    std::shared_ptr<FrameLeaf> isLeaf() override { return thisLeaf(); }

    friend class Frame;
    void setVisible(bool visible);
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
};

class FrameSplit : public Frame, public FrameDataSplit<Frame> {
public:
    FrameSplit(HSTag* tag, Settings* settings, std::weak_ptr<FrameSplit> parent, FixPrecDec fraction_, SplitAlign align_,
                 std::shared_ptr<Frame> a_, std::shared_ptr<Frame> b_);
    ~FrameSplit() override;
    // inherited:
    std::shared_ptr<FrameLeaf> frameWithClient(Client* client) override;
    bool removeClient(Client* client) override;

    TilingResult computeLayout(Rectangle rect) override;

    virtual void fmap(std::function<void(FrameSplit*)> onSplit,
                      std::function<void(FrameLeaf*)> onLeaf, int order) override;

    Client* focusedClient() override;

    // own members
    int splitsToRoot(SplitAlign align_) override;
    bool split(SplitAlign alignment, FixPrecDec fraction);
    void replaceChild(std::shared_ptr<Frame> old, std::shared_ptr<Frame> newchild);
    std::shared_ptr<Frame> firstChild() { return a_; }
    std::shared_ptr<Frame> secondChild() { return b_; }
    std::shared_ptr<Frame> selectedChild() { return selection_ ? b_ : a_; }
    void swapChildren();
    void adjustFraction(FixPrecDec delta);
    void setFraction(FixPrecDec fraction);
    FixPrecDec getFraction() const { return fraction_; }
    static FixPrecDec clampFraction(FixPrecDec fraction);
    std::shared_ptr<FrameSplit> thisSplit();
    std::shared_ptr<FrameSplit> isSplit() override { return thisSplit(); }
    SplitAlign getAlign() { return align_; }
    void swapSelection() { selection_ = selection_ == 0 ? 1 : 0; }
    void setSelection(int s) { selection_ = s; }
private:
    friend class FrameTree;
};

// functions
int frame_current_bring(int argc, char** argv, Output output);

int frame_split_count_to_root(Frame* frame, int align);

bool focus_client(Client* client, bool switch_tag, bool switch_monitor, bool raise);
// moves a window to an other frame
int frame_move_window_command(int argc, char** argv, Output output);

int frame_focus_edge(Input input, Output output);
int frame_move_window_edge(int argc, char** argv, Output output);

#endif

