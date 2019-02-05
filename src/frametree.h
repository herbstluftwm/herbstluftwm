#ifndef HERBSTLUFT_FRAME_TREE_H
#define HERBSTLUFT_FRAME_TREE_H

#include <memory>
#include <string>
#include <functional>
#include "types.h"

class Client;
class HSFrame;
class HSFrameLeaf;
class HSTag;
class Settings;

/*! A class representing an entire tree of frames that provides
 * the tiling commands and other common actions on the frame tree
 */
class FrameTree {
public:
    FrameTree(HSTag* tag, Settings* settings);
    void foreachClient(std::function<void(Client*)> action);

    static void dump(std::shared_ptr<HSFrame> frame, Output output);
    std::shared_ptr<HSFrame> lookup(const std::string& path);
    std::shared_ptr<HSFrameLeaf> focusedFrame();
public: // soon to be come private:
    std::shared_ptr<HSFrame> root_;
private:
    HSTag* tag_;
    Settings* settings_;
};

#endif
