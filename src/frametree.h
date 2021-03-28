#ifndef HERBSTLUFT_FRAME_TREE_H
#define HERBSTLUFT_FRAME_TREE_H

#include <functional>
#include <memory>
#include <string>

#include "child.h"
#include "converter.h"
#include "finite.h"
#include "fixprecdec.h"
#include "link.h"
#include "object.h"

class Client;
class Completion;
class Frame;
class FrameLeaf;
class HSTag;
class RawFrameNode;
class Settings;
class TreeInterface;

/*! A class representing an entire tree of frames that provides
 * the tiling commands and other common actions on the frame tree
 */
class FrameTree : public Object {
public:
    FrameTree(HSTag* tag, Settings* settings);
    void foreachClient(std::function<void(Client*)> action);

    static void dump(std::shared_ptr<Frame> frame, Output output);
    static void prettyPrint(std::shared_ptr<Frame> frame, Output output);
    static std::shared_ptr<FrameLeaf> findEmptyFrameNearFocus(std::shared_ptr<Frame> subtree);
    std::shared_ptr<Frame> lookup(const std::string& path);
    static std::shared_ptr<FrameLeaf> focusedFrame(std::shared_ptr<Frame> node);
    std::shared_ptr<FrameLeaf> focusedFrame();
    //! try to focus a client, and return if this was successful
    bool focusClient(Client* client);
    //! focus a frame within its tree
    static void focusFrame(std::shared_ptr<Frame> frame);
    bool focusInDirection(Direction dir, bool externalOnly);
    bool shiftInDirection(Direction direction, bool externalOnly);
    //! return a frame in the tree that holds the client
    std::shared_ptr<FrameLeaf> findFrameWithClient(Client* client);

    //! check whether the present FrameTree contains a given Frame
    //! (it requires that there are no cycles in the 'tree' containing the Frame
    bool contains(std::shared_ptr<Frame> frame) const;

    enum class CycleDelta {
        Previous,
        Next,
        Begin,
        End,
    };

    bool resizeFrame(FixPrecDec delta, Direction dir);

    // Commands
    int removeFrameCommand();
    int rotateCommand();
    enum class MirrorDirection {
        Horizontal,
        Vertical,
        Both,
    };

    int mirrorCommand(Input input, Output output);
    void mirrorCompletion(Completion& complete);
    bool cycleAll(CycleDelta cdelta, bool skip_invisible);
    int cycleFrameCommand(Input input, Output output);
    int loadCommand(Input input, Output output);
    void loadCompletion(Completion& complete);
    int dumpLayoutCommand(Input input, Output output);
    void dumpLayoutCompletion(Completion& complete);
    int cycleLayoutCommand(Input input, Output output);
    void cycleLayoutCompletion(Completion& complete);
    int splitCommand(Input input, Output output);
    int setLayoutCommand(Input input, Output output);
    void setLayoutCompletion(Completion& complete);
public: // soon to be come private:
    std::shared_ptr<Frame> root_;
    Link_<Frame> rootLink_;
    DynChild_<FrameLeaf> focused_frame_;
    //! replace a node in the frame tree, either modifying old's parent or the root_
    void replaceNode(std::shared_ptr<Frame> old, std::shared_ptr<Frame> replacement);
private:
    static std::shared_ptr<FrameLeaf> findEmptyFrameNearFocusGeometrically(std::shared_ptr<Frame> subtree);
    FrameLeaf* focusedFramePlainPtr();
    //! cycle the frames within the current tree
    void cycle_frame(std::function<size_t(size_t,size_t)> indexAndLenToIndex);
    void cycle_frame(int delta);
    void applyFrameTree(std::shared_ptr<Frame> target,
                        std::shared_ptr<RawFrameNode> source);
    static std::shared_ptr<TreeInterface> treeInterface(
        std::shared_ptr<Frame> frame,
        std::shared_ptr<FrameLeaf> focus);
    HSTag* tag_;
    Settings* settings_;
};

template <>
struct is_finite<FrameTree::MirrorDirection> : std::true_type {};
template<> Finite<FrameTree::MirrorDirection>::ValueList Finite<FrameTree::MirrorDirection>::values;

#endif
