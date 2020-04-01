#ifndef HERBSTLUFT_FRAME_TREE_H
#define HERBSTLUFT_FRAME_TREE_H

#include <functional>
#include <memory>
#include <string>

#include "types.h"

class Client;
class Completion;
class HSFrame;
class HSFrameLeaf;
class HSTag;
class RawFrameNode;
class Settings;
class TreeInterface;

/*! A class representing an entire tree of frames that provides
 * the tiling commands and other common actions on the frame tree
 */
class FrameTree : public std::enable_shared_from_this<FrameTree> {
public:
    FrameTree(HSTag* tag, Settings* settings);
    void foreachClient(std::function<void(Client*)> action);

    static void dump(std::shared_ptr<HSFrame> frame, Output output);
    static void prettyPrint(std::shared_ptr<HSFrame> frame, Output output);
    std::shared_ptr<HSFrame> lookup(const std::string& path);
    static std::shared_ptr<HSFrameLeaf> focusedFrame(std::shared_ptr<HSFrame> node);
    std::shared_ptr<HSFrameLeaf> focusedFrame();
    //! try to focus a client, and return if this was successful
    bool focusClient(Client* client);
    //! focus a frame within its tree
    static void focusFrame(std::shared_ptr<HSFrame> frame);
    bool focusInDirection(Direction dir, bool externalOnly);
    //! return a frame in the tree that holds the client
    std::shared_ptr<HSFrameLeaf> findFrameWithClient(Client* client);

    //! check whether the present FrameTree contains a given HSFrame
    //! (it requires that there are no cycles in the 'tree' containing the HSFrame
    bool contains(std::shared_ptr<HSFrame> frame) const;

    // Commands
    int cycleSelectionCommand(Input input, Output output);
    int focusNthCommand(Input input, Output output);
    int removeFrameCommand();
    int closeAndRemoveCommand();
    int closeOrRemoveCommand();
    int rotateCommand();
    int cycleAllCommand(Input input, Output output);
    int cycleFrameCommand(Input input, Output output);
    int loadCommand(Input input, Output output);
    int dumpLayoutCommand(Input input, Output output);
    void dumpLayoutCompletion(Completion& complete);
    int cycleLayoutCommand(Input input, Output output);
    void cycleLayoutCompletion(Completion& complete);
    int splitCommand(Input input, Output output);
public: // soon to be come private:
    std::shared_ptr<HSFrame> root_;
private:
    //! cycle the frames within the current tree
    void cycle_frame(int delta);
    //! try to resemble a given raw frame tree given by the FrameParser
    void applyFrameTree(std::shared_ptr<HSFrame> target,
                        std::shared_ptr<RawFrameNode> source);
    //! replace a node in the frame tree, either modifying old's parent or the root_
    void replaceNode(std::shared_ptr<HSFrame> old, std::shared_ptr<HSFrame> replacement);
    static std::shared_ptr<TreeInterface> treeInterface(
        std::shared_ptr<HSFrame> frame,
        std::shared_ptr<HSFrameLeaf> focus);
    HSTag* tag_;
    Settings* settings_;
};

#endif
