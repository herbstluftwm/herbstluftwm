#ifndef __HLWM_FRAME_DECORATION_H_
#define __HLWM_FRAME_DECORATION_H_

#include <X11/X.h>

#include "x11-types.h"

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
    FrameDecoration(HSTag* tag, Settings* settings);
    ~FrameDecoration();
    void render(const FrameDecorationData& data, bool isFocused);
    void updateVisibility(const FrameDecorationData& data, bool isFocused);
    void hide();

private:
    Window window;
    bool visible; // whether the window is visible at the moment
    bool window_transparent; // whether the window has a mask at the moment
    Slice* slice;
    HSTag* tag;
    Settings* settings;
};

#endif

