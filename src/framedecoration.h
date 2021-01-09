#ifndef __HLWM_FRAME_DECORATION_H_
#define __HLWM_FRAME_DECORATION_H_

#include <X11/X.h>
#include <map>
#include <memory>

#include "rectangle.h"

class FrameLeaf;
class HSTag;
class Slice;
class Settings;

class FrameDecorationData {
public:
    FrameDecorationData() = default;
    bool visible = false;
    bool hasClients = false; // whether this frame holds clients at the moment
    bool hasParent = false;
    Rectangle geometry;
};

class FrameDecoration {
public:
    FrameDecoration(FrameLeaf& frame, HSTag* tag, Settings* settings);
    ~FrameDecoration();
    void render(const FrameDecorationData& data, bool isFocused);
    void updateVisibility(const FrameDecorationData& data, bool isFocused);
    void hide();
    std::shared_ptr<FrameLeaf> frame();

    static FrameDecoration* withWindow(Window winid);

private:
    static std::map<Window, FrameDecoration*> s_windowToFrameDecoration;
    FrameLeaf& frame_; //! the owner of this decoration
    Window window;
    bool visible; // whether the window is visible at the moment
    bool window_transparent; // whether the window has a mask at the moment
    Slice* slice;
    HSTag* tag;
    Settings* settings;
};

#endif

