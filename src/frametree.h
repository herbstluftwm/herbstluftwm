#ifndef HERBSTLUFT_FRAME_TREE_H
#define HERBSTLUFT_FRAME_TREE_H

#include <functional>
#include <memory>
#include <string>

#include "types.h"

class Client;
class HSFrame;
class HSFrameLeaf;
class HSTag;
class Settings;
class TreeInterface;

/*! A class representing an entire tree of frames that provides
 * the tiling commands and other common actions on the frame tree
 */
class FrameTree {
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
    //! return a frame in the tree that holds the client
    std::shared_ptr<HSFrameLeaf> findFrameWithClient(Client* client);

    // Commands
    int cycleSelectionCommand(Input input, Output output);
    int focusNthCommand(Input input, Output output);
    int removeFrameCommand();
    int closeAndRemoveCommand();
    int closeOrRemoveCommand();
    int rotateCommand();
    int cycleAllCommand(Input input, Output output);
    int cycleFrameCommand(Input input, Output output);
public: // soon to be come private:
    std::shared_ptr<HSFrame> root_;
private:
    //! cycle the frames within the current tree
    void cycle_frame(int delta);
    static std::shared_ptr<TreeInterface> treeInterface(
        std::shared_ptr<HSFrame> frame,
        std::shared_ptr<HSFrameLeaf> focus);
    HSTag* tag_;
    Settings* settings_;
};

#endif
